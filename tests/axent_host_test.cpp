#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axent/host/axent_host.hpp"

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

    void blockAfterNextStream()
    {
        std::lock_guard<std::mutex> lock(block_mutex_);
        block_after_next_stream_ = true;
        stream_blocked_ = false;
        unblock_stream_ = false;
    }

    bool waitForStreamBlocked()
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        return block_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return stream_blocked_;
        });
    }

    void unblockStream()
    {
        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            unblock_stream_ = true;
        }
        block_cv_.notify_all();
    }

    void sendBytes(const axtp::Byte* data, std::size_t size) override
    {
        CapturingPayloadSink payload_sink;
        axtp::InboundProcessor inbound(payload_sink);
        inbound.onBytes(data, size);

        for (const auto& control : payload_sink.controls) {
            if (control.opcode == axtp::ControlOpcode::Open) {
                axtp::ControlPayload accept;
                accept.opcode = axtp::ControlOpcode::Accept;
                accept.controlId = control.controlId;
                accept.statusCode = axtp::ErrorCode::Success;
                inject(encode_control(accept));
                inject(encode_rpc(axtp::JsonRpcEncoder::makeHello()));
            }
        }

        for (const auto& rpc : payload_sink.rpcs) {
            if (rpc.op == axtp::RpcOp::Identify) {
                inject(encode_rpc(axtp::JsonRpcEncoder::makeIdentified("axent-host-session")));
                continue;
            }
            if (rpc.op == axtp::RpcOp::Request) {
                saw_business_request = true;
                axtp::RpcPayload response;
                response.encoding = axtp::RpcEncoding::Json;
                response.op = axtp::RpcOp::RequestResponse;
                response.requestId = rpc.requestId;
                response.methodOrEventId = rpc.methodOrEventId;
                response.statusCode = axtp::ErrorCode::Success;
                response.bodyEncoding = axtp::RpcBodyEncoding::None;
                response.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
                response.meta.jsonSid = rpc.meta.jsonSid;
                const std::string body = R"({"via":"axtp"})";
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

    bool saw_business_request = false;

private:
    void inject(const axtp::Bytes& bytes)
    {
        if (sink_ != nullptr) {
            sink_->onBytes(bytes.data(), bytes.size());
        }
        maybeBlockAfterStreamInject();
    }

    void maybeBlockAfterStreamInject()
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        if (!block_after_next_stream_) {
            return;
        }
        block_after_next_stream_ = false;
        stream_blocked_ = true;
        block_cv_.notify_all();
        block_cv_.wait(lock, [this]() {
            return unblock_stream_;
        });
    }

    axtp::IByteSink* sink_ = nullptr;
    std::mutex rx_mutex_;
    std::queue<axtp::Bytes> rx_queue_;
    std::mutex block_mutex_;
    std::condition_variable block_cv_;
    bool block_after_next_stream_ = false;
    bool stream_blocked_ = false;
    bool unblock_stream_ = false;
    bool open_ = false;
};

axent::MediaFrame make_media_frame(std::uint64_t sequence_id)
{
    axent::MediaFrame frame;
    frame.session_id = "filled-by-test";
    frame.stream_id = 0x2001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = sequence_id;
    frame.cursor = sequence_id * 1000U;
    frame.flags = sequence_id == 1 ? axent::MediaFrameFlag::KeyFrame : axent::MediaFrameFlag::EndOfFrame;
    frame.payload = {0, 0, 1, static_cast<std::uint8_t>(sequence_id)};
    return frame;
}

axent::DeviceSnapshot make_second_mock_device()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-002";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK002";
    device.identity.firmware_version = "mock-fw-1.0.0";
    device.identity.hardware_version = "mock-hw-revA";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.connection.last_change_reason = "test-upserted";
    device.status.health = "ok";
    return device;
}

std::optional<axent::MediaFrame> wait_for_media_frame(axent::MediaConsumer& consumer)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        auto frame = consumer.read();
        if (frame.has_value()) {
            return frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::nullopt;
}

class RecordingMediaSink final : public axent::IMediaFrameSink {
public:
    void on_media_frame(axent::MediaFrame frame) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        frames_.push_back(std::move(frame));
        if (block_next_frame_) {
            block_next_frame_ = false;
            frame_blocked_ = true;
            cv_.notify_all();
            cv_.wait(lock, [this]() {
                return unblock_frames_;
            });
        }
        cv_.notify_all();
    }

    void on_media_event(axent::MediaEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(event);
        cv_.notify_all();
    }

    void block_next_frame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        block_next_frame_ = true;
        frame_blocked_ = false;
        unblock_frames_ = false;
    }

    bool wait_for_frames(std::size_t minimum)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this, minimum]() {
            return frames_.size() >= minimum;
        });
    }

    bool wait_for_frame_blocked()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return frame_blocked_;
        });
    }

    void unblock_frames()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            unblock_frames_ = true;
        }
        cv_.notify_all();
    }

    bool wait_for_closed()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            for (const auto& event : events_) {
                if (event.kind == axent::MediaEventKind::Closed) {
                    return true;
                }
            }
            return false;
        });
    }

    bool has_drop_event() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& event : events_) {
            if (event.kind == axent::MediaEventKind::Dropped) {
                return true;
            }
        }
        return false;
    }

    std::vector<axent::MediaFrame> frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<axent::MediaFrame> frames_;
    std::vector<axent::MediaEvent> events_;
    bool block_next_frame_ = false;
    bool frame_blocked_ = false;
    bool unblock_frames_ = false;
};

} // namespace

int main()
{
    axent::AxentHost host;
    axent::SessionAcquireRequest stopped_request;
    stopped_request.client_id = "stopped-client";
    stopped_request.device_id = "mock-device-001";
    stopped_request.media = true;
    const auto stopped_lease = host.acquire_session(stopped_request);
    require(!stopped_lease.acquired, "stopped host should not acquire");
    require(stopped_lease.reason == "host not running", "stopped acquire reason mismatch");

    require(host.create_media_consumer("missing-session", {}) == nullptr,
            "missing session should not create consumer");
    auto missing_frame = make_media_frame(99);
    require(!host.publish_media_frame("missing-session", missing_frame),
            "missing session publish should fail");
    const auto missing_call = host.call("missing-session", "status.get", {});
    require(missing_call.status == axent::ControlStatus::Unavailable,
            "call before start should return unavailable");

    bool broker_threw = false;
    try {
        (void)host.broker();
    } catch (const std::logic_error&) {
        broker_threw = true;
    }
    require(broker_threw, "broker should throw before start");

    axent::AxentHostOptions options;
    options.enable_mock_adapter = true;
    options.enable_axtp_adapter = false;
    require(host.start(options), "host should start");

    const auto devices = host.discover_devices();
    require(devices.size() >= 1, "host should discover at least the mock device");
    require(devices[0].id == "mock-device-001", "mock device id mismatch");
    host.upsert_device(make_second_mock_device());
    const auto devices_after_upsert = host.discover_devices();
    require(devices_after_upsert.size() == 2, "host should allow upserting a second device");

    axent::SessionAcquireRequest unknown_device_request;
    unknown_device_request.client_id = "nearcast-test";
    unknown_device_request.device_id = "missing-device";
    unknown_device_request.media = true;
    const auto unknown_device_lease = host.acquire_session(unknown_device_request);
    require(!unknown_device_lease.acquired, "unknown device should not acquire");
    require(unknown_device_lease.reason == "device not found", "unknown device reason mismatch");
    require(host.create_media_consumer("missing-session", {}) == nullptr,
            "running host missing session should not create consumer");
    const auto missing_session_call = host.call("missing-session", "status.get", {});
    require(missing_session_call.status == axent::ControlStatus::NotFound,
            "missing session call should return not found");

    axent::SessionAcquireRequest request;
    request.client_id = "nearcast-test";
    request.device_id = "mock-device-001";
    request.media = true;
    const auto lease = host.acquire_session(request);
    require(lease.acquired, "media lease should be acquired");
    require(lease.media, "media lease should be marked media");
    require(!lease.session_id.empty(), "session id should be assigned");

    auto same_client_control_request = request;
    same_client_control_request.media = false;
    const auto same_client_control_lease = host.acquire_session(same_client_control_request);
    require(same_client_control_lease.acquired, "same client control lease should be acquired");
    require(!same_client_control_lease.media, "control lease should not be marked media");
    require(host.create_media_consumer(same_client_control_lease.session_id, {}) == nullptr,
            "control lease should not create media consumer");
    auto control_frame = make_media_frame(77);
    require(!host.publish_media_frame(same_client_control_lease.session_id, control_frame),
            "control lease publish should fail");
    require(host.subscribe_media(same_client_control_lease.session_id,
                                 std::make_shared<RecordingMediaSink>()) == nullptr,
            "control lease should not create media subscription");

    host.release_session(same_client_control_lease.session_id, "control session done");

    auto denied_request = request;
    denied_request.client_id = "second-renderer";
    const auto denied = host.acquire_session(denied_request);
    require(!denied.acquired, "second media owner should still be denied");
    require(denied.reason == "media lease busy", "denied reason mismatch");

    auto second_device_media_request = denied_request;
    second_device_media_request.device_id = "mock-device-002";
    const auto second_device_media_lease = host.acquire_session(second_device_media_request);
    require(second_device_media_lease.acquired, "second device media lease should be acquired");
    require(second_device_media_lease.media, "second device lease should be marked media");

    auto denied_second_device_request = second_device_media_request;
    denied_second_device_request.client_id = "third-renderer";
    const auto denied_second_device = host.acquire_session(denied_second_device_request);
    require(!denied_second_device.acquired, "second media owner for second device should be denied");
    require(denied_second_device.reason == "media lease busy", "second device denied reason mismatch");

    host.release_session(second_device_media_lease.session_id, "second device media done");

    auto early_frame = make_media_frame(42);
    require(!host.publish_media_frame(lease.session_id, early_frame),
            "publish before media consumer should fail");

    require(host.subscribe_media("missing-session", std::make_shared<RecordingMediaSink>()) == nullptr,
            "missing session should not create media subscription");

    auto push_sink = std::make_shared<RecordingMediaSink>();
    axent::MediaSubscriptionOptions push_options;
    push_options.max_frames = 4;
    auto push_subscription = host.subscribe_media(lease.session_id, push_sink, push_options);
    require(push_subscription != nullptr, "media subscription should be created");

    auto push_frame = make_media_frame(100);
    require(host.publish_media_frame(lease.session_id, push_frame),
            "push-only publish should succeed");
    require(push_sink->wait_for_frames(1), "push sink should receive a frame without relay");
    auto push_frames = push_sink->frames();
    require(push_frames.front().sequence_id == 100, "push sink sequence mismatch");
    require(push_frames.front().session_id == lease.session_id, "push sink session mismatch");

    axent::MediaRelayOptions relay_options;
    relay_options.max_frames = 1;
    auto consumer = host.create_media_consumer(lease.session_id, relay_options);
    require(consumer != nullptr, "media consumer should be created");

    auto frame = make_media_frame(1);
    frame.session_id = lease.session_id;
    require(host.publish_media_frame(lease.session_id, frame), "publish should succeed");
    auto second_frame = make_media_frame(2);
    require(host.publish_media_frame(lease.session_id, second_frame), "second publish should succeed");

    const auto received = consumer->read();
    require(received.has_value(), "consumer should read a frame");
    require(received->sequence_id == 2, "custom relay should retain newest frame");
    require(received->session_id == lease.session_id, "received session mismatch");
    require(axent::has_flag(received->flags, axent::MediaFrameFlag::Discontinuity),
            "retained frame should mark discontinuity");
    const auto relay_stats = consumer->stats();
    require(relay_stats.published_frames == 2, "relay published count mismatch");
    require(relay_stats.dropped_frames == 1, "relay dropped count mismatch");
    require(push_sink->wait_for_frames(3), "push sink should keep receiving alongside relay");

    auto slow_sink = std::make_shared<RecordingMediaSink>();
    axent::MediaSubscriptionOptions slow_options;
    slow_options.max_frames = 1;
    auto slow_subscription = host.subscribe_media(lease.session_id, slow_sink, slow_options);
    require(slow_subscription != nullptr, "slow media subscription should be created");
    slow_sink->block_next_frame();
    auto slow_first_frame = make_media_frame(200);
    require(host.publish_media_frame(lease.session_id, slow_first_frame),
            "slow subscription first publish should succeed");
    require(slow_sink->wait_for_frame_blocked(), "slow sink should block on first frame");

    const auto publish_start = std::chrono::steady_clock::now();
    for (std::uint64_t sequence = 201; sequence <= 205; ++sequence) {
        require(host.publish_media_frame(lease.session_id, make_media_frame(sequence)),
                "publish should not require relay read or sink progress");
    }
    const auto publish_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - publish_start);
    require(publish_elapsed < std::chrono::milliseconds(200),
            "slow sink should not block media publishing");

    slow_sink->unblock_frames();
    require(slow_sink->wait_for_frames(2), "slow sink should receive latest queued frame after unblock");
    require(slow_sink->has_drop_event(), "slow sink should observe a drop event");
    const auto slow_stats = slow_subscription->stats();
    require(slow_stats.received_frames >= 6, "slow subscription received count mismatch");
    require(slow_stats.dropped_frames >= 4, "slow subscription should drop stale frames");
    auto slow_frames = slow_sink->frames();
    require(slow_frames.back().sequence_id == 205, "slow subscription should retain newest frame");
    require(axent::has_flag(slow_frames.back().flags, axent::MediaFrameFlag::Discontinuity),
            "slow subscription retained frame should mark discontinuity");

    const auto result = host.call(lease.session_id, "status.get", {});
    require(result.status == axent::ControlStatus::Ok, "host call should dispatch through broker");
    require(result.body.at("health") == "ok", "host call result mismatch");

    host.release_session(lease.session_id, "test complete");
    require(push_sink->wait_for_closed(), "push subscription should close on release");
    require(slow_sink->wait_for_closed(), "slow subscription should close on release");
    require(!host.publish_media_frame(lease.session_id, frame), "publish after release should fail");
    require(!consumer->read().has_value(), "consumer should not read after media release");

    const auto second_lease = host.acquire_session(denied_request);
    require(second_lease.acquired, "second media owner should acquire after release");
    require(second_lease.media, "second media lease should be marked media");
    host.release_session(second_lease.session_id, "second test complete");

    host.stop();
    const auto stopped_call = host.call(second_lease.session_id, "status.get", {});
    require(stopped_call.status == axent::ControlStatus::Unavailable,
            "call after stop should return unavailable");

    broker_threw = false;
    try {
        (void)host.broker();
    } catch (const std::logic_error&) {
        broker_threw = true;
    }
    require(broker_threw, "broker should throw after stop");

    axent::AxentHost real_host;
    ScriptedAxtpTransport* scripted = nullptr;
    axent::AxentHostOptions real_options;
    real_options.enable_mock_adapter = false;
    real_options.enable_axtp_adapter = true;
    real_options.axtp_adapter_factory = [&](axent::AxtpAdapterConfig config) {
        return std::make_unique<axent::AxtpAdapter>(std::move(config), [&](const axtp::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            scripted = transport.get();
            return transport;
        });
    };
    require(real_host.start(std::move(real_options)), "real AXTP host should start");

    axent::DeviceSnapshot real_device;
    real_device.id = "hid:0581:2581:NA20-SERIAL";
    real_device.adapter = "axtp";
    real_device.identity.vendor = "Mostorm";
    real_device.identity.model = "NA20";
    real_device.identity.serial_number = "NA20-SERIAL";
    real_device.connection.online = true;
    real_device.connection.transport = "hid";
    real_device.status.health = "ready";
    real_host.upsert_device(real_device);

    axent::SessionAcquireRequest real_request;
    real_request.client_id = "nearcast-real";
    real_request.device_id = real_device.id;
    const auto real_lease = real_host.acquire_session(real_request);
    require(real_lease.acquired, "real AXTP device lease should be acquired");
    const auto real_call = real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(real_call.status == axent::ControlStatus::Ok, "real AXTP host call should dispatch");
    require(real_call.body.at("via") == "axtp", "real AXTP host call response mismatch");
    require(scripted != nullptr && scripted->saw_business_request,
            "real AXTP host call should use scripted transport");

    real_request.media = true;
    const auto media_lease = real_host.acquire_session(real_request);
    require(media_lease.acquired, "real AXTP media lease should be acquired");

    axent::MediaRelayOptions real_relay_options;
    real_relay_options.max_frames = 4;
    auto real_consumer = real_host.create_media_consumer(media_lease.session_id, real_relay_options);
    require(real_consumer != nullptr, "real media consumer should be created");

    scripted->injectStream(0x1001, 9, 9000, {0x00, 0x00, 0x01, 0x65});

    const auto media_frame = wait_for_media_frame(*real_consumer);
    require(media_frame.has_value(), "real adapter stream should reach host media relay");
    require(media_frame->session_id == media_lease.session_id, "media relay session mismatch");
    require(media_frame->device_id == real_device.id, "media relay device mismatch");
    require(media_frame->stream_id == 0x1001, "media relay stream mismatch");
    require(media_frame->codec == axent::MediaCodec::H264, "media relay codec mismatch");

    scripted->blockAfterNextStream();
    scripted->injectStream(0x1001, 10, 10000, {0x00, 0x00, 0x01, 0x41});
    require(scripted->waitForStreamBlocked(), "stale stream should be queued before adapter drain");
    std::thread release_thread([&real_host, &media_lease]() {
        real_host.release_session(media_lease.session_id, "replace media lease");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    scripted->unblockStream();
    release_thread.join();

    const auto replacement_lease = real_host.acquire_session(real_request);
    require(replacement_lease.acquired, "replacement media lease should be acquired");
    auto replacement_consumer = real_host.create_media_consumer(replacement_lease.session_id, real_relay_options);
    require(replacement_consumer != nullptr, "replacement media consumer should be created");

    const auto stale_frame = wait_for_media_frame(*replacement_consumer);
    require(!stale_frame.has_value(), "queued frame from released media lease must not reach replacement lease");

    real_host.release_session(replacement_lease.session_id, "replacement media lease done");
    real_host.stop();

    return 0;
}
