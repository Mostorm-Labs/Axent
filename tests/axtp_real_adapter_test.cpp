#include <chrono>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/testing/axtp_adapter_test_seam.hpp"

#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include "transports/hidapi/hid_transport.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct CapturingByteWriter : axtp::IByteWriter {
    axtp::Bytes bytes;

    void writeBytes(const axtp::Byte* data, std::size_t size) override
    {
        bytes.insert(bytes.end(), data, data + size);
    }
};

struct CapturingPayloadSink : axtp::IPayloadSink {
    std::vector<axtp::ControlPayload> controls;
    std::vector<axtp::RpcPayload> rpcs;
    std::vector<axtp::StreamPayload> streams;

    void onControl(axtp::ControlPayload payload) override
    {
        controls.push_back(std::move(payload));
    }

    void onRpc(axtp::RpcPayload payload) override
    {
        rpcs.push_back(std::move(payload));
    }

    void onStream(axtp::StreamPayload payload) override
    {
        streams.push_back(std::move(payload));
    }
};

axtp::Bytes encode_control(axtp::ControlPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendControl(std::move(payload));
    return writer.bytes;
}

axtp::Bytes encode_rpc(axtp::RpcPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendRpc(std::move(payload));
    return writer.bytes;
}

axtp::Bytes encode_stream(axtp::StreamPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendStream(std::move(payload));
    return writer.bytes;
}

class ScriptedAxtpTransport : public axtp::ITransport {
public:
    void bind(axtp::IByteSink& sink) override
    {
        sink_ = &sink;
    }

    void open() override
    {
        open_ = true;
    }

    void close() override
    {
        open_ = false;
    }

    void poll() override
    {
        std::queue<axtp::Bytes> pending;
        {
            std::lock_guard<std::mutex> lock(rx_mutex_);
            pending.swap(rx_queue_);
        }
        while (!pending.empty()) {
            inject(pending.front());
            pending.pop();
        }
    }

    void injectStream(std::uint32_t stream_id,
                      std::uint32_t sequence_id,
                      std::uint64_t cursor,
                      axtp::Bytes data)
    {
        axtp::StreamPayload stream;
        stream.streamId = stream_id;
        stream.seqId = sequence_id;
        stream.cursor = cursor;
        stream.data = std::move(data);
        std::lock_guard<std::mutex> lock(rx_mutex_);
        rx_queue_.push(encode_stream(std::move(stream)));
    }

    void sendBytes(const axtp::Byte* data, std::size_t size) override
    {
        CapturingPayloadSink payload_sink;
        axtp::InboundProcessor inbound(payload_sink);
        inbound.onBytes(data, size);

        for (const auto& control : payload_sink.controls) {
            if (control.opcode != axtp::ControlOpcode::Open) {
                continue;
            }
            saw_control_open = true;
            axtp::ControlPayload accept;
            accept.opcode = axtp::ControlOpcode::Accept;
            accept.controlId = control.controlId;
            accept.statusCode = axtp::ErrorCode::Success;
            inject(encode_control(accept));
            inject(encode_rpc(axtp::JsonRpcEncoder::makeHello()));
        }

        for (const auto& rpc : payload_sink.rpcs) {
            if (rpc.op == axtp::RpcOp::Identify) {
                saw_identify = true;
                inject(encode_rpc(axtp::JsonRpcEncoder::makeIdentified("axent-session-1")));
                continue;
            }
            if (rpc.op == axtp::RpcOp::Request) {
                saw_business_request = true;
                last_business_sid = rpc.meta.jsonSid;
                axtp::RpcPayload response;
                response.encoding = axtp::RpcEncoding::Json;
                response.op = axtp::RpcOp::RequestResponse;
                response.requestId = rpc.requestId;
                response.methodOrEventId = rpc.methodOrEventId;
                response.statusCode = axtp::ErrorCode::Success;
                response.bodyEncoding = axtp::RpcBodyEncoding::None;
                response.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
                response.meta.jsonSid = rpc.meta.jsonSid;
                std::string body = R"({"ok":true})";
                if (rpc.methodOrEventId ==
                    static_cast<std::uint32_t>(axtp::MethodId::VideoGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast","currentState":"receiving"}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast_audio","currentState":"receiving","channels":[2],"sampleRates":[48000]}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoOpenStream)) {
                    body = R"({"streamId":1,"state":"streaming","source":"wireless_cast","codec":"h264"})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioOpenStream)) {
                    body = R"({"streamId":2,"state":"streaming","source":"wireless_cast_audio","codec":"aac","transportFormat":"adts"})";
                }
                response.body = axtp::Bytes(body.begin(), body.end());
                inject(encode_rpc(response));
            }
        }
    }

    axtp::TransportProfile profile() const override
    {
        return axtp::TransportProfile{
            axtp::TransportKind::Hid,
            axtp::AxtpWireMode::FramedBinary,
            axtp::jsonBinaryRpcEncoding(),
            false,
            false,
            true,
            4096,
        };
    }

    bool saw_control_open = false;
    bool saw_identify = false;
    bool saw_business_request = false;
    std::string last_business_sid;

private:
    void inject(const axtp::Bytes& bytes)
    {
        if (sink_ != nullptr) {
            sink_->onBytes(bytes.data(), bytes.size());
        }
    }

    axtp::IByteSink* sink_ = nullptr;
    std::mutex rx_mutex_;
    std::queue<axtp::Bytes> rx_queue_;
    bool open_ = false;
};

axent::TransportSelector na20_selector()
{
    axent::TransportSelector selector;
    selector.kind = axent::TransportKind::Hid;
    selector.vendor_id = 0x0581;
    selector.product_id = 0x2581;
    selector.usage_page = 0x0081;
    selector.report_id = 0x05;
    selector.input_report_size = 0;
    selector.output_report_size = 0;
    return selector;
}

bool wait_for_frames(const std::vector<axent::MediaFrame>& frames,
                     std::mutex& frames_mutex,
                     std::size_t expected_count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            if (frames.size() >= expected_count) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool wait_for_stream_events(const std::vector<axent::MediaStreamEvent>& events,
                            std::mutex& events_mutex,
                            std::size_t expected_count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(events_mutex);
            if (events.size() >= expected_count) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

} // namespace

int main()
{
    const auto defaults = axent::AxtpAdapter::na20_defaults();
    require(defaults.selector.kind == axent::TransportKind::Hid, "NA20 should default to HID transport");
    require(defaults.selector.vendor_id == 0x0581, "NA20 VID mismatch");
    require(defaults.selector.product_id == 0x2581, "NA20 PID mismatch");
    require(defaults.selector.usage_page == 0x0081, "NA20 usage page mismatch");
    require(defaults.selector.report_id == 0x05, "NA20 report id mismatch");
    require(defaults.selector.input_report_size == 0, "NA20 input report size should be auto");
    require(defaults.selector.output_report_size == 0, "NA20 output report size should be auto");

    axtp::HidDeviceInfo hid_device;
    hid_device.path = "hid-path-001";
    hid_device.vendorId = 0x0581;
    hid_device.productId = 0x2581;
    hid_device.serialNumber = "NA20-SERIAL";
    hid_device.manufacturer = "Mostorm";
    hid_device.product = "NA20";
    hid_device.usagePage = 0x0081;
    hid_device.usage = 0x0001;
    hid_device.interfaceNumber = 3;
    hid_device.busType = "usb";

    const auto descriptor = axent::testing::AxtpAdapterTestSeam::descriptor_from_hid_device(hid_device);
    require(descriptor.id == "hid:0581:2581:NA20-SERIAL", "descriptor id should include VID/PID/serial");
    require(descriptor.online, "descriptor should be online");
    require(descriptor.kind == axent::TransportKind::Hid, "descriptor kind mismatch");
    require(descriptor.path == "hid-path-001", "descriptor path mismatch");
    require(descriptor.serial_number == "NA20-SERIAL", "descriptor serial mismatch");

    const auto snapshot = axent::AxtpAdapter::snapshot_from_descriptor(descriptor);
    require(snapshot.id == descriptor.id, "snapshot id mismatch");
    require(snapshot.adapter == "axtp", "snapshot adapter mismatch");
    require(snapshot.identity.vendor == "Mostorm", "snapshot vendor mismatch");
    require(snapshot.identity.model == "NA20", "snapshot model mismatch");
    require(snapshot.identity.serial_number == "NA20-SERIAL", "snapshot serial mismatch");
    require(snapshot.connection.online, "snapshot should be online");
    require(snapshot.connection.transport == "hid", "snapshot transport mismatch");
    require(snapshot.status.health == "ready", "snapshot health mismatch");

    axent::AxtpAdapter adapter(defaults);
    require(axent::testing::AxtpAdapterTestSeam::matches_selector(adapter, hid_device),
            "default adapter should match NA20 device");
    auto wrong_usage = hid_device;
    wrong_usage.usagePage = 0x1234;
    require(!axent::testing::AxtpAdapterTestSeam::matches_selector(adapter, wrong_usage),
            "usage page filter should reject mismatches");

    auto path_config = defaults;
    path_config.selector = na20_selector();
    path_config.selector.path = "specific-path";
    axent::AxtpAdapter path_adapter(path_config);
    require(!axent::testing::AxtpAdapterTestSeam::matches_selector(path_adapter, hid_device),
            "path filter should reject different path");
    hid_device.path = "specific-path";
    require(axent::testing::AxtpAdapterTestSeam::matches_selector(path_adapter, hid_device),
            "path filter should accept matching path");

    axtp::HidTransportOptions options;
    options.inputReportSize = 0;
    options.outputReportSize = 0;
    options.readBufferSize = 4096;
    options.reportId = 0x05;
    auto mapped_options =
        axent::testing::AxtpAdapterTestSeam::hid_options_from_selector(na20_selector());
    require(mapped_options.vendorId == 0x0581, "mapped VID mismatch");
    require(mapped_options.productId == 0x2581, "mapped PID mismatch");
    require(mapped_options.usagePage == 0x0081, "mapped usage page mismatch");
    require(mapped_options.reportId == 0x05, "mapped report id mismatch");
    require(mapped_options.inputReportSize == 0, "mapped input report size should remain auto");
    require(mapped_options.outputReportSize == 0, "mapped output report size should remain auto");
    require(mapped_options.useReadThread, "real HID adapter should use read thread like NearCast");
    bool trace_called = false;
    mapped_options.reportTrace = [&](const axtp::HidReportTrace&) {
        trace_called = true;
    };
    axtp::HidReportTrace mapped_trace;
    mapped_options.reportTrace(mapped_trace);
    require(trace_called, "mapped HID options should preserve report trace callback storage");

    axtp::HidReportTrace timeout;
    timeout.kind = axtp::HidReportTraceKind::ReadTimeout;
    timeout.timeoutMs = 1000;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, timeout);
    axtp::HidReportTrace raw_read;
    raw_read.kind = axtp::HidReportTraceKind::ReadReport;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, raw_read);
    axtp::HidReportTrace accepted;
    accepted.kind = axtp::HidReportTraceKind::AcceptedReport;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, accepted);
    axtp::HidReportTrace read_error;
    read_error.kind = axtp::HidReportTraceKind::ReadError;
    read_error.message = "read failed";
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, read_error);
    axtp::HidReportTrace write_error;
    write_error.kind = axtp::HidReportTraceKind::WriteError;
    write_error.message = "write failed";
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, write_error);
    axtp::HidReportTrace dropped;
    dropped.kind = axtp::HidReportTraceKind::DroppedReportId;
    dropped.reportId = 0x06;
    dropped.expectedReportId = 0x05;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, dropped);

    const auto diagnostics = adapter.diagnostics();
    require(diagnostics.read_reports == 1, "accepted report count should not double-count raw reads");
    require(diagnostics.read_errors == 1, "read error count mismatch");
    require(diagnostics.write_errors == 1, "write error count mismatch");
    require(diagnostics.dropped_reports == 1, "dropped report count mismatch");
    require(diagnostics.last_event == "dropped-report-id", "last event should track last trace");
    require(diagnostics.last_error == "write failed", "last HID error should keep the latest error message");

    auto unavailable_adapter = axent::testing::AxtpAdapterTestSeam::make(
        defaults, [](const axtp::HidTransportOptions&) {
        return std::unique_ptr<axtp::ITransport>{};
    });
    const auto result = unavailable_adapter->call("hid:0581:2581:NA20-SERIAL", "status.get", {});
    require(result.status == axent::ControlStatus::Unavailable,
            "real adapter without a transport should be unavailable");
    require(result.body.at("error") == "AXTP HID transport target is unavailable",
            "unavailable transport message mismatch");

    ScriptedAxtpTransport* scripted = nullptr;
    int session_transport_factory_calls = 0;
    auto session_adapter = axent::testing::AxtpAdapterTestSeam::make(
        defaults, [&](const axtp::HidTransportOptions&) {
        ++session_transport_factory_calls;
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        scripted = transport.get();
        return transport;
    });
    const auto call_result = session_adapter->call("hid:0581:2581:NA20-SERIAL", "audio.getAlgorithmConfig", {});
    require(call_result.status == axent::ControlStatus::Ok,
            std::string("scripted AXTP call should succeed: ") + call_result.body.dump());
    require(call_result.body.at("ok") == true, "scripted AXTP response should be parsed");
    require(scripted != nullptr, "scripted transport should be constructed");
    require(scripted->saw_control_open, "AXTP session should send control open");
    require(scripted->saw_identify, "AXTP session should send identify");
    require(scripted->saw_business_request, "AXTP call should send a business request");
    require(scripted->last_business_sid == "axent-session-1", "business request should use app-ready sid");

    axent::testing::AxtpAdapterTestSeam::disconnect_session(*session_adapter);
    std::string disconnected_error;
    const auto disconnected_busy = session_adapter->open_session_status(
        "hid:0581:2581:NA20-SECOND", disconnected_error);
    require(disconnected_busy == axent::ControlStatus::Busy,
            "a disconnected but unreleased AXTP owner must still reject a second device");
    require(disconnected_error.find("AXTP session busy") != std::string::npos,
            "disconnected AXTP owner Busy reason mismatch");
    require(session_transport_factory_calls == 1,
            "disconnected ownership must not construct a second transport before release");

    std::mutex frames_mutex;
    std::vector<axent::MediaFrame> frames;
    std::mutex stream_events_mutex;
    std::condition_variable stream_events_cv;
    std::vector<axent::MediaStreamEvent> stream_events;
    bool block_next_stream_event = false;
    bool stream_event_blocked = false;
    bool unblock_stream_event = false;
    axent::AxtpAdapterConfig media_config = axent::AxtpAdapter::na20_defaults();
    ScriptedAxtpTransport* media_scripted = nullptr;
    auto media_adapter = axent::testing::AxtpAdapterTestSeam::make(
        media_config, [&](const axtp::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        media_scripted = transport.get();
        return transport;
    });

    media_adapter->set_media_frame_callback([&frames, &frames_mutex](std::string device_id, axent::MediaFrame frame) {
        frame.device_id = std::move(device_id);
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames.push_back(std::move(frame));
    });
    media_adapter->set_media_stream_event_callback(
        [&](axent::MediaStreamEvent event) {
            std::unique_lock<std::mutex> lock(stream_events_mutex);
            stream_events.push_back(std::move(event));
            stream_events_cv.notify_all();
            if (block_next_stream_event) {
                block_next_stream_event = false;
                stream_event_blocked = true;
                stream_events_cv.notify_all();
                stream_events_cv.wait(lock, [&]() { return unblock_stream_event; });
            }
        });

    std::string error;
    require(media_adapter->open_session("hid:0581:2581:NA20-SERIAL", error),
            "scripted adapter session should open");
    require(media_scripted != nullptr, "scripted media transport should be constructed");
    const auto media_diagnostics = media_adapter->diagnostics();
    require(media_diagnostics.active_video_stream_id == 1, "video stream id should be registered from openStream");
    require(media_diagnostics.active_audio_stream_id == 2, "audio stream id should be registered from openStream");
    require(media_diagnostics.active_media_streams == 2, "active media stream count mismatch");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 2),
            "openStream should publish video and audio Opened events");

    const auto descriptors = media_adapter->active_media_stream_descriptors();
    require(descriptors.size() == 2, "active descriptor snapshot should contain video and audio");
    require(descriptors[0].key.session_id.empty() &&
                descriptors[0].key.stream_id == 1 &&
                descriptors[0].key.generation == 1 &&
                descriptors[0].device_id == "hid:0581:2581:NA20-SERIAL" &&
                descriptors[0].kind == axent::MediaKind::Video &&
                descriptors[0].codec == axent::MediaCodec::H264 &&
                descriptors[0].source == "wireless_cast" &&
                descriptors[0].stream_profile == "media.video" &&
                descriptors[0].cursor_unit == "timestampUs",
            "video open result descriptor mismatch");
    require(descriptors[1].key.session_id.empty() &&
                descriptors[1].key.stream_id == 2 &&
                descriptors[1].key.generation == 1 &&
                descriptors[1].kind == axent::MediaKind::Audio &&
                descriptors[1].codec == axent::MediaCodec::Aac &&
                descriptors[1].transport_format == "adts" &&
                descriptors[1].sample_rate == 48000 &&
                descriptors[1].channels == 2 &&
                descriptors[1].stream_profile == "media.audio" &&
                descriptors[1].cursor_unit == "timestampUs",
            "audio open result descriptor mismatch");

    media_scripted->injectStream(1, 3, 777000, {0x00, 0x00, 0x01, 0x65});
    require(wait_for_frames(frames, frames_mutex, 1), "video stream should publish a frame");

    axent::MediaFrame received_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 1, "stream payload should publish one media frame");
        received_frame = frames.front();
    }
    require(received_frame.device_id == "hid:0581:2581:NA20-SERIAL", "device id mismatch");
    require(received_frame.stream_id == 1, "stream id mismatch");
    require(received_frame.kind == axent::MediaKind::Video, "stream kind mismatch");
    require(received_frame.codec == axent::MediaCodec::H264, "codec mismatch");
    require(received_frame.sequence_id == 3, "sequence mismatch");
    require(received_frame.cursor == 777000, "cursor mismatch");
    require(received_frame.timestamp_us == 777000, "timestamp mismatch");
    require(received_frame.generation == 1, "video frame generation mismatch");
    require(received_frame.session_id.empty(),
            "standalone adapter frames must remain unbound without a Host media lease");
    require(axent::has_flag(received_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "end-of-frame flag missing");

    media_scripted->injectStream(2, 4, 888000, {0x11, 0x22, 0x33, 0x44});
    require(wait_for_frames(frames, frames_mutex, 2), "audio stream should publish a frame");

    axent::MediaFrame received_audio_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 2, "audio stream should append one media frame");
        received_audio_frame = frames.back();
    }
    require(received_audio_frame.device_id == "hid:0581:2581:NA20-SERIAL", "audio device id mismatch");
    require(received_audio_frame.stream_id == 2, "audio stream id mismatch");
    require(received_audio_frame.kind == axent::MediaKind::Audio, "audio stream kind mismatch");
    require(received_audio_frame.codec == axent::MediaCodec::Aac, "audio codec mismatch");
    require(received_audio_frame.sequence_id == 4, "audio sequence mismatch");
    require(received_audio_frame.cursor == 888000, "audio cursor mismatch");
    require(received_audio_frame.timestamp_us == 888000, "audio timestamp mismatch");
    require(received_audio_frame.generation == 1, "audio frame generation mismatch");
    require(axent::has_flag(received_audio_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "audio end-of-frame flag missing");

    axent::testing::AxtpAdapterTestSeam::stop_session_pump(*media_adapter);
    axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
        *media_adapter,
        "hid:0581:2581:NA20-SERIAL",
        1,
        5,
        999000,
        {0x00, 0x00, 0x01, 0x41});
    axent::testing::AxtpAdapterTestSeam::reopen_media_streams(
        *media_adapter, "hid:0581:2581:NA20-SERIAL");
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        block_next_stream_event = true;
        stream_event_blocked = false;
        unblock_stream_event = false;
    }
    std::thread first_drain([&]() {
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*media_adapter);
    });
    bool first_event_blocked = false;
    {
        std::unique_lock<std::mutex> lock(stream_events_mutex);
        first_event_blocked = stream_events_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() { return stream_event_blocked; });
    }
    axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
        *media_adapter,
        "hid:0581:2581:NA20-SERIAL",
        1,
        6,
        1000000,
        {0x00, 0x00, 0x01, 0x65});
    std::atomic<bool> second_drain_finished{false};
    std::thread second_drain([&]() {
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*media_adapter);
        second_drain_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool second_drain_waited_for_lifecycle = !second_drain_finished.load();
    std::size_t frames_before_opened_unblocked = 0;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames_before_opened_unblocked = frames.size();
    }
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        unblock_stream_event = true;
    }
    stream_events_cv.notify_all();
    first_drain.join();
    second_drain.join();
    require(first_event_blocked && second_drain_waited_for_lifecycle &&
                frames_before_opened_unblocked == 2,
            "concurrent media drains must not deliver a frame ahead of lifecycle events");

    require(wait_for_stream_events(stream_events, stream_events_mutex, 6),
            "same-ID reopen should publish Closed/Open pairs for video and audio");
    const auto reopened_descriptors = media_adapter->active_media_stream_descriptors();
    require(reopened_descriptors.size() == 2 &&
                reopened_descriptors[0].key.generation == 2 &&
                reopened_descriptors[1].key.generation == 2,
            "same-ID reopen should increment each physical generation");
    require(!axent::testing::AxtpAdapterTestSeam::is_current_media_frame(
                *media_adapter, received_frame),
            "old generation frame must become stale after same-ID reopen");

    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 3,
                "queued old generation must be dropped while the new generation frame is delivered");
        require(frames.back().sequence_id == 6 && frames.back().generation == 2,
                "reopened stream frame should carry the incremented generation");
        require(axent::testing::AxtpAdapterTestSeam::is_current_media_frame(
                    *media_adapter, frames.back()),
                "new generation frame should remain current");
    }

    std::mutex reentrant_frames_mutex;
    std::vector<axent::MediaFrame> reentrant_frames;
    ScriptedAxtpTransport* reentrant_scripted = nullptr;
    auto reentrant_adapter = axent::testing::AxtpAdapterTestSeam::make(
        media_config, [&](const axtp::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        reentrant_scripted = transport.get();
        return transport;
    });
    std::atomic<bool> reentrant_call_succeeded{false};
    reentrant_adapter->set_media_frame_callback(
        [&reentrant_adapter,
         &reentrant_call_succeeded,
         &reentrant_frames,
         &reentrant_frames_mutex](std::string device_id, axent::MediaFrame frame) {
            frame.device_id = std::move(device_id);
            {
                std::lock_guard<std::mutex> lock(reentrant_frames_mutex);
                reentrant_frames.push_back(std::move(frame));
            }
            const auto result =
                reentrant_adapter->call("hid:0581:2581:NA20-SERIAL", "audio.getAlgorithmConfig", {});
            reentrant_call_succeeded.store(result.status == axent::ControlStatus::Ok
                && result.body.value("ok", false));
        });

    require(reentrant_adapter->open_session("hid:0581:2581:NA20-SERIAL", error),
            "reentrant scripted adapter session should open");
    require(reentrant_scripted != nullptr, "reentrant scripted transport should be constructed");
    reentrant_scripted->injectStream(0x1001, 5, 999000, {0x00, 0x00, 0x01, 0x65});
    const auto reentrant_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!reentrant_call_succeeded.load() && std::chrono::steady_clock::now() < reentrant_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard<std::mutex> lock(reentrant_frames_mutex);
        require(reentrant_frames.size() == 1, "reentrant callback should receive one media frame");
    }
    require(reentrant_call_succeeded.load(), "media callback should be able to re-enter adapter call");

    return 0;
}
