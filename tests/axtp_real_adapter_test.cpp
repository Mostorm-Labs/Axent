#include <chrono>
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axent/adapters/axtp_adapter.hpp"

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
                const std::string body = R"({"ok":true})";
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

    const auto descriptor = axent::AxtpAdapter::descriptor_from_hid_device(hid_device);
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
    require(adapter.matches_selector(hid_device), "default adapter should match NA20 device");
    auto wrong_usage = hid_device;
    wrong_usage.usagePage = 0x1234;
    require(!adapter.matches_selector(wrong_usage), "usage page filter should reject mismatches");

    auto path_config = defaults;
    path_config.selector = na20_selector();
    path_config.selector.path = "specific-path";
    axent::AxtpAdapter path_adapter(path_config);
    require(!path_adapter.matches_selector(hid_device), "path filter should reject different path");
    hid_device.path = "specific-path";
    require(path_adapter.matches_selector(hid_device), "path filter should accept matching path");

    axtp::HidTransportOptions options;
    options.inputReportSize = 0;
    options.outputReportSize = 0;
    options.readBufferSize = 4096;
    options.reportId = 0x05;
    auto mapped_options = axent::AxtpAdapter::hid_options_from_selector(na20_selector());
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
    adapter.record_hid_trace(timeout);
    axtp::HidReportTrace raw_read;
    raw_read.kind = axtp::HidReportTraceKind::ReadReport;
    adapter.record_hid_trace(raw_read);
    axtp::HidReportTrace accepted;
    accepted.kind = axtp::HidReportTraceKind::AcceptedReport;
    adapter.record_hid_trace(accepted);
    axtp::HidReportTrace read_error;
    read_error.kind = axtp::HidReportTraceKind::ReadError;
    read_error.message = "read failed";
    adapter.record_hid_trace(read_error);
    axtp::HidReportTrace write_error;
    write_error.kind = axtp::HidReportTraceKind::WriteError;
    write_error.message = "write failed";
    adapter.record_hid_trace(write_error);
    axtp::HidReportTrace dropped;
    dropped.kind = axtp::HidReportTraceKind::DroppedReportId;
    dropped.reportId = 0x06;
    dropped.expectedReportId = 0x05;
    adapter.record_hid_trace(dropped);

    const auto diagnostics = adapter.diagnostics();
    require(diagnostics.read_reports == 1, "accepted report count should not double-count raw reads");
    require(diagnostics.read_errors == 1, "read error count mismatch");
    require(diagnostics.write_errors == 1, "write error count mismatch");
    require(diagnostics.dropped_reports == 1, "dropped report count mismatch");
    require(diagnostics.last_event == "dropped-report-id", "last event should track last trace");

    axent::AxtpAdapter unavailable_adapter(defaults, [](const axtp::HidTransportOptions&) {
        return std::unique_ptr<axtp::ITransport>{};
    });
    const auto result = unavailable_adapter.call("hid:0581:2581:NA20-SERIAL", "status.get", {});
    require(result.status == axent::ControlStatus::Unavailable,
            "real adapter without a transport should be unavailable");
    require(result.body.at("error") == "AXTP HID transport target is unavailable",
            "unavailable transport message mismatch");

    ScriptedAxtpTransport* scripted = nullptr;
    axent::AxtpAdapter session_adapter(defaults, [&](const axtp::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        scripted = transport.get();
        return transport;
    });
    const auto call_result = session_adapter.call("hid:0581:2581:NA20-SERIAL", "audio.getAlgorithmConfig", {});
    require(call_result.status == axent::ControlStatus::Ok,
            std::string("scripted AXTP call should succeed: ") + call_result.body.dump());
    require(call_result.body.at("ok") == true, "scripted AXTP response should be parsed");
    require(scripted != nullptr, "scripted transport should be constructed");
    require(scripted->saw_control_open, "AXTP session should send control open");
    require(scripted->saw_identify, "AXTP session should send identify");
    require(scripted->saw_business_request, "AXTP call should send a business request");
    require(scripted->last_business_sid == "axent-session-1", "business request should use app-ready sid");

    std::mutex frames_mutex;
    std::vector<axent::MediaFrame> frames;
    axent::AxtpAdapterConfig media_config = axent::AxtpAdapter::na20_defaults();
    ScriptedAxtpTransport* media_scripted = nullptr;
    axent::AxtpAdapter media_adapter(media_config, [&](const axtp::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        media_scripted = transport.get();
        return transport;
    });

    media_adapter.set_media_frame_callback([&frames, &frames_mutex](std::string device_id, axent::MediaFrame frame) {
        frame.device_id = std::move(device_id);
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames.push_back(std::move(frame));
    });

    std::string error;
    require(media_adapter.open_session_for_test("hid:0581:2581:NA20-SERIAL", error),
            "scripted adapter session should open");
    require(media_scripted != nullptr, "scripted media transport should be constructed");

    media_scripted->injectStream(0x1001, 3, 777000, {0x00, 0x00, 0x01, 0x65});
    require(wait_for_frames(frames, frames_mutex, 1), "video stream should publish a frame");

    axent::MediaFrame received_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 1, "stream payload should publish one media frame");
        received_frame = frames.front();
    }
    require(received_frame.device_id == "hid:0581:2581:NA20-SERIAL", "device id mismatch");
    require(received_frame.stream_id == 0x1001, "stream id mismatch");
    require(received_frame.kind == axent::MediaKind::Video, "stream kind mismatch");
    require(received_frame.codec == axent::MediaCodec::H264, "codec mismatch");
    require(received_frame.sequence_id == 3, "sequence mismatch");
    require(received_frame.cursor == 777000, "cursor mismatch");
    require(received_frame.timestamp_us == 777000, "timestamp mismatch");
    require(axent::has_flag(received_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "end-of-frame flag missing");

    media_scripted->injectStream(0x2001, 4, 888000, {0x11, 0x22, 0x33, 0x44});
    require(wait_for_frames(frames, frames_mutex, 2), "audio stream should publish a frame");

    axent::MediaFrame received_audio_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 2, "audio stream should append one media frame");
        received_audio_frame = frames.back();
    }
    require(received_audio_frame.device_id == "hid:0581:2581:NA20-SERIAL", "audio device id mismatch");
    require(received_audio_frame.stream_id == 0x2001, "audio stream id mismatch");
    require(received_audio_frame.kind == axent::MediaKind::Audio, "audio stream kind mismatch");
    require(received_audio_frame.codec == axent::MediaCodec::Aac, "audio codec mismatch");
    require(received_audio_frame.sequence_id == 4, "audio sequence mismatch");
    require(received_audio_frame.cursor == 888000, "audio cursor mismatch");
    require(received_audio_frame.timestamp_us == 888000, "audio timestamp mismatch");
    require(axent::has_flag(received_audio_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "audio end-of-frame flag missing");

    std::mutex reentrant_frames_mutex;
    std::vector<axent::MediaFrame> reentrant_frames;
    ScriptedAxtpTransport* reentrant_scripted = nullptr;
    axent::AxtpAdapter reentrant_adapter(media_config, [&](const axtp::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        reentrant_scripted = transport.get();
        return transport;
    });
    std::atomic<bool> reentrant_call_succeeded{false};
    reentrant_adapter.set_media_frame_callback(
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
                reentrant_adapter.call("hid:0581:2581:NA20-SERIAL", "audio.getAlgorithmConfig", {});
            reentrant_call_succeeded.store(result.status == axent::ControlStatus::Ok
                && result.body.value("ok", false));
        });

    require(reentrant_adapter.open_session_for_test("hid:0581:2581:NA20-SERIAL", error),
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
