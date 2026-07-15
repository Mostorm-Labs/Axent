#include <algorithm>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
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
#include "axtp_adapter_test_seam.hpp"

#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include "hidapi/hid_transport.hpp"

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

    void injectStreamOnNextRequest(std::uint32_t stream_id,
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
        stream_on_next_request_ = encode_stream(std::move(stream));
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
                std::optional<axtp::Bytes> request_stream;
                {
                    std::lock_guard<std::mutex> lock(rx_mutex_);
                    request_stream.swap(stream_on_next_request_);
                }
                if (request_stream.has_value()) {
                    inject(*request_stream);
                }
                axtp::RpcPayload response;
                response.encoding = axtp::RpcEncoding::Json;
                response.op = axtp::RpcOp::RequestResponse;
                response.requestId = rpc.requestId;
                response.methodOrEventId = rpc.methodOrEventId;
                response.statusCode = axtp::ErrorCode::Success;
                response.bodyEncoding = axtp::RpcBodyEncoding::None;
                response.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
                response.meta.jsonSid = rpc.meta.jsonSid;
                std::string body = R"({"via":"axtp"})";
                if (rpc.methodOrEventId ==
                    static_cast<std::uint32_t>(axtp::MethodId::VideoGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast","currentState":"receiving"}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast_audio","currentState":"receiving","channels":[2],"sampleRates":[48000]}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoOpenStream)) {
                    body = R"({"streamId":4097,"state":"streaming","source":"wireless_cast","codec":"h264","width":1920,"height":1080,"streamProfile":"media.video","cursorUnit":"timestampUs"})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioOpenStream)) {
                    body = R"({"streamId":8193,"state":"streaming","source":"wireless_cast_audio","codec":"aac","transportFormat":"adts","sampleRate":48000,"channels":2,"streamProfile":"media.audio","cursorUnit":"timestampUs"})";
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
    std::optional<axtp::Bytes> stream_on_next_request_;
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

enum class StreamCallbackKind {
    Opened,
    Frame,
    Closed,
    DeliveryDropped,
    SubscriptionClosed,
};

struct StreamCallbackRecord {
    StreamCallbackKind kind = StreamCallbackKind::Frame;
    axent::StreamKey key;
    axent::MediaStreamDescriptor descriptor;
    axent::MediaFrame frame;
    axent::MediaDeliveryEvent delivery;
    std::thread::id thread_id;
};

class RecordingMediaStreamSink final : public axent::IMediaStreamSink {
public:
    void on_media_stream_event(axent::MediaStreamEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = event.kind == axent::MediaStreamEventKind::Opened
            ? StreamCallbackKind::Opened
            : StreamCallbackKind::Closed;
        record.key = event.descriptor.key;
        record.descriptor = std::move(event.descriptor);
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        cv_.notify_all();
    }

    void on_media_stream_frame(axent::MediaFrame frame) override
    {
        std::function<void()> hook;
        std::unique_lock<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = StreamCallbackKind::Frame;
        record.key = axent::stream_key(frame);
        record.frame = std::move(frame);
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        hook = frame_hook_;
        if (block_next_frame_) {
            block_next_frame_ = false;
            frame_blocked_ = true;
            cv_.notify_all();
            cv_.wait(lock, [this]() { return unblock_frames_; });
        }
        cv_.notify_all();
        lock.unlock();
        if (hook) {
            hook();
        }
    }

    void on_media_delivery_event(axent::MediaDeliveryEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = event.kind == axent::MediaDeliveryEventKind::DeliveryDropped
            ? StreamCallbackKind::DeliveryDropped
            : StreamCallbackKind::SubscriptionClosed;
        record.delivery = event;
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        cv_.notify_all();
    }

    void set_frame_hook(std::function<void()> hook)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_hook_ = std::move(hook);
    }

    void block_next_frame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        block_next_frame_ = true;
        frame_blocked_ = false;
        unblock_frames_ = false;
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

    bool wait_for_records(std::size_t minimum)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this, minimum]() {
            return records_.size() >= minimum;
        });
    }

    std::vector<StreamCallbackRecord> records() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<StreamCallbackRecord> records_;
    std::function<void()> frame_hook_;
    bool block_next_frame_ = false;
    bool frame_blocked_ = false;
    bool unblock_frames_ = false;
};

class CallbackExitGate {
public:
    void enter_and_wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        entered_ = true;
        cv_.notify_all();
        cv_.wait(lock, [this]() { return released_; });
    }

    bool wait_until_entered()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return entered_;
        });
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            released_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_ = false;
    bool released_ = false;
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

    auto provider_sink = std::make_shared<RecordingMediaStreamSink>();
    auto provider_subscription =
        host.subscribe_media_stream(lease.session_id, provider_sink);
    require(provider_subscription != nullptr && provider_sink->records().empty(),
            "custom provider subscription should begin without a descriptor snapshot");
    axent::MediaStreamDescriptor provider_descriptor;
    provider_descriptor.key = {"adapter-owned-placeholder", 0x3001, 1};
    provider_descriptor.device_id = "adapter-owned-placeholder";
    provider_descriptor.kind = axent::MediaKind::Video;
    provider_descriptor.codec = axent::MediaCodec::H264;
    require(host.publish_media_stream_event(
                lease.session_id,
                {axent::MediaStreamEventKind::Opened, provider_descriptor}),
            "custom provider should publish an Opened lifecycle event through Host");
    auto provider_frame = make_media_frame(43);
    provider_frame.stream_id = provider_descriptor.key.stream_id;
    provider_frame.generation = provider_descriptor.key.generation;
    require(host.publish_media_frame(lease.session_id, provider_frame) &&
                provider_sink->wait_for_records(2),
            "custom provider frame should follow its Host-bound descriptor");
    require(host.publish_media_stream_event(
                lease.session_id,
                {axent::MediaStreamEventKind::Closed, provider_descriptor}) &&
                provider_sink->wait_for_records(3),
            "custom provider should close its stream through Host");
    const auto provider_records = provider_sink->records();
    require(provider_records[0].key.session_id == lease.session_id &&
                provider_records[0].descriptor.device_id == lease.device_id &&
                provider_records[1].kind == StreamCallbackKind::Frame &&
                provider_records[1].key == provider_records[0].key &&
                provider_records[2].kind == StreamCallbackKind::Closed &&
                provider_records[2].key == provider_records[0].key,
            "Host must bind and order custom provider lifecycle and frames");
    provider_subscription->cancel();

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
    auto immediate_push_frames = push_sink->frames();
    require(immediate_push_frames.size() == 1, "direct push sink should receive before publish returns");
    const auto push_stats = push_subscription->stats();
    require(push_stats.received_frames == 1, "direct push received count mismatch");
    require(push_stats.delivered_frames == 1, "direct push delivered count mismatch");
    require(push_stats.queued_frames == 0, "direct push should not queue frames");
    require(push_stats.queued_bytes == 0, "direct push should not queue bytes");
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
    slow_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
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
    auto stop_sink = std::make_shared<RecordingMediaSink>();
    auto stop_subscription = host.subscribe_media(second_lease.session_id, stop_sink);
    require(stop_subscription != nullptr,
            "stop quiescence subscription should be created");
    stop_sink->block_next_frame();
    std::thread stop_publisher([&]() {
        (void)host.publish_media_frame(second_lease.session_id, make_media_frame(300));
    });
    require(stop_sink->wait_for_frame_blocked(),
            "stop quiescence callback should become in-flight");
    std::atomic<bool> stop_finished{false};
    std::thread stop_thread([&]() {
        host.stop();
        stop_finished.store(true);
    });
    const auto stop_reset_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (host.running() && std::chrono::steady_clock::now() < stop_reset_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool stop_waited_for_callback =
        !host.running() && !stop_finished.load();
    stop_sink->unblock_frames();
    stop_publisher.join();
    stop_thread.join();
    require(stop_waited_for_callback && stop_finished.load(),
            "Host stop must wait for an in-flight legacy media callback");
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
    axent::AxtpAdapter* real_adapter = nullptr;
    int transport_factory_calls = 0;
    axent::AxentHostOptions real_options;
    real_options.enable_mock_adapter = false;
    real_options.enable_axtp_adapter = true;
    real_options.axtp_adapter_factory = [&](axent::AxtpAdapterConfig config) {
        auto adapter = axent::testing::AxtpAdapterTestSeam::make(
            std::move(config),
            [&](const axent::transport::HidTransportOptions&) {
                ++transport_factory_calls;
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                scripted = transport.get();
                return transport;
            });
        real_adapter = adapter.get();
        return adapter;
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
    const auto initial_video_params = real_host.video_stream_params_state(
        media_lease.session_id);
    require(initial_video_params.session_id == media_lease.session_id &&
                initial_video_params.active_stream_id.has_value(),
            "Host video stream parameter state should bind the media lease session");
    auto video_params_subscription = real_host.subscribe_video_stream_params(
        media_lease.session_id,
        [](const axent::VideoStreamParamsState&) {});
    require(video_params_subscription != nullptr,
            "Host should expose an RAII video parameter subscription for a media lease");
    axent::VideoStreamParamsRequest invalid_video_params;
    invalid_video_params.frame_rate = 0;
    const auto invalid_video_params_result = real_host.set_video_stream_params(
        media_lease.session_id, invalid_video_params);
    require(invalid_video_params_result.status_code == 0x000A &&
                !invalid_video_params_result.accepted &&
                invalid_video_params_result.state.session_id == media_lease.session_id,
            "Host should preserve typed validation status and bind result session id");

    auto stream_sink = std::make_shared<RecordingMediaStreamSink>();
    auto stream_subscription =
        real_host.subscribe_media_stream(media_lease.session_id, stream_sink);
    require(stream_subscription != nullptr,
            "real media stream subscription should be created");
    require(stream_sink->wait_for_records(2),
            "late media stream subscription should replay active descriptors");
    const auto replayed_streams = stream_sink->records();
    require(replayed_streams.size() == 2,
            "video and audio descriptors should be replayed exactly once");
    require(replayed_streams[0].kind == StreamCallbackKind::Opened &&
                replayed_streams[1].kind == StreamCallbackKind::Opened,
            "descriptor replay must begin with Opened events");
    require(replayed_streams[0].key.session_id == media_lease.session_id &&
                replayed_streams[1].key.session_id == media_lease.session_id,
            "descriptor replay must bind the Host media session id");
    require(replayed_streams[0].key.stream_id == 0x1001 &&
                replayed_streams[0].key.generation != 0 &&
                replayed_streams[0].descriptor.kind == axent::MediaKind::Video &&
                replayed_streams[0].descriptor.codec == axent::MediaCodec::H264 &&
                replayed_streams[0].descriptor.width == 1920 &&
                replayed_streams[0].descriptor.height == 1080 &&
                replayed_streams[0].descriptor.stream_profile == "media.video",
            "video descriptor mapping mismatch");
    require(replayed_streams[1].key.stream_id == 0x2001 &&
                replayed_streams[1].key.generation != 0 &&
                replayed_streams[1].descriptor.kind == axent::MediaKind::Audio &&
                replayed_streams[1].descriptor.codec == axent::MediaCodec::Aac &&
                replayed_streams[1].descriptor.transport_format == "adts" &&
                replayed_streams[1].descriptor.sample_rate == 48000 &&
                replayed_streams[1].descriptor.channels == 2 &&
                replayed_streams[1].descriptor.stream_profile == "media.audio",
            "audio descriptor mapping mismatch");

    axent::DeviceSnapshot second_real_device = real_device;
    second_real_device.id = "hid:0581:2581:NA20-SECOND";
    second_real_device.identity.serial_number = "NA20-SECOND";
    real_host.upsert_device(second_real_device);
    axent::SessionAcquireRequest second_real_request;
    second_real_request.client_id = "nearcast-second-real";
    second_real_request.device_id = second_real_device.id;
    second_real_request.media = true;
    const auto busy_lease = real_host.acquire_session(second_real_request);
    require(!busy_lease.acquired, "second real AXTP device must fail fast");
    require(busy_lease.status == axent::ControlStatus::Busy,
            "second real AXTP device must return typed Busy");
    require(busy_lease.reason.find("AXTP session busy") != std::string::npos,
            "second real AXTP Busy reason mismatch");
    require(transport_factory_calls == 1,
            "Busy acquisition must not construct a replacement transport");
    const auto original_after_busy =
        real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(original_after_busy.status == axent::ControlStatus::Ok,
            "Busy acquisition must not disconnect the original AXTP session");
    require(transport_factory_calls == 1,
            "original session should remain attached after Busy acquisition");

    axent::MediaRelayOptions real_relay_options;
    real_relay_options.max_frames = 4;
    auto real_consumer = real_host.create_media_consumer(media_lease.session_id, real_relay_options);
    require(real_consumer != nullptr, "real media consumer should be created");
    auto real_push_sink = std::make_shared<RecordingMediaSink>();
    auto real_push_subscription = real_host.subscribe_media(media_lease.session_id, real_push_sink);
    require(real_push_subscription != nullptr, "real media direct subscription should be created");

    scripted->injectStream(0x1001, 9, 9000, {0x00, 0x00, 0x01, 0x65});
    require(real_push_sink->wait_for_frames(1), "real adapter stream should reach direct subscription");
    require(stream_sink->wait_for_records(3),
            "descriptor-driven subscription should receive the video frame");
    const auto stream_records_after_frame = stream_sink->records();
    require(stream_records_after_frame[2].kind == StreamCallbackKind::Frame &&
                stream_records_after_frame[2].key == replayed_streams[0].key,
            "Opened must precede a frame carrying the same StreamKey");
    const auto real_push_frames = real_push_sink->frames();
    require(real_push_frames.front().session_id == media_lease.session_id,
            "direct subscription session mismatch");
    require(real_push_frames.front().device_id == real_device.id,
            "direct subscription device mismatch");

    const auto media_frame = wait_for_media_frame(*real_consumer);
    require(media_frame.has_value(), "real adapter stream should reach host media relay");
    require(media_frame->session_id == media_lease.session_id, "media relay session mismatch");
    require(media_frame->device_id == real_device.id, "media relay device mismatch");
    require(media_frame->stream_id == 0x1001, "media relay stream mismatch");
    require(media_frame->codec == axent::MediaCodec::H264, "media relay codec mismatch");

    axent::MediaStreamDescriptor extension_descriptor;
    extension_descriptor.key = {"placeholder", 0x3001, 1};
    extension_descriptor.kind = axent::MediaKind::Video;
    extension_descriptor.codec = axent::MediaCodec::Opaque;
    extension_descriptor.source = "extension-source";
    require(real_host.publish_media_stream_event(
                media_lease.session_id,
                {axent::MediaStreamEventKind::Opened, extension_descriptor}),
            "an extension provider should coexist with AXTP media streams");
    auto late_extension_sink = std::make_shared<RecordingMediaStreamSink>();
    auto late_extension_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, late_extension_sink);
    require(late_extension_subscription != nullptr &&
                late_extension_sink->wait_for_records(3),
            "late subscription should merge AXTP and extension descriptors");
    const auto late_extension_records = late_extension_sink->records();
    const auto extension_snapshot = std::find_if(
        late_extension_records.begin(), late_extension_records.end(),
        [](const StreamCallbackRecord& record) {
            return record.key.stream_id == 0x3001;
        });
    require(extension_snapshot != late_extension_records.end() &&
                extension_snapshot->descriptor.source == "extension-source",
            "AXTP snapshot refresh must not erase an extension-owned descriptor");
    extension_descriptor.source = "tampered-close";
    require(real_host.publish_media_stream_event(
                media_lease.session_id,
                {axent::MediaStreamEventKind::Closed, extension_descriptor}),
            "extension provider close should succeed");
    require(late_extension_sink->wait_for_records(4),
            "extension close should reach the late subscriber");
    const auto canonical_close = late_extension_sink->records().back();
    require(canonical_close.kind == StreamCallbackKind::Closed &&
                canonical_close.descriptor.source == "extension-source",
            "Closed must reuse the canonical descriptor stored by Opened");
    late_extension_subscription->cancel();

    auto reentrant_direct_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_direct_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_direct_sink);
    require(reentrant_direct_subscription != nullptr &&
                reentrant_direct_sink->wait_for_records(2),
            "direct reentrant-call subscription should replay descriptors");
    std::atomic<bool> reentrant_direct_hook_ran{false};
    std::atomic<bool> reentrant_direct_call_succeeded{false};
    std::atomic<bool> direct_nested_frame_was_deferred{false};
    std::atomic<bool> direct_queued_publisher_enqueued{false};
    std::atomic<bool> direct_queued_publish_succeeded{false};
    std::atomic<int> reentrant_direct_depth{0};
    std::atomic<int> reentrant_direct_max_depth{0};
    std::thread direct_queued_publisher;
    auto direct_queued_frame = make_media_frame(81);
    direct_queued_frame.stream_id = 0x1001;
    direct_queued_frame.generation = replayed_streams[0].key.generation;
    reentrant_direct_sink->set_frame_hook([&]() {
        const auto depth = reentrant_direct_depth.fetch_add(1) + 1;
        if (depth > reentrant_direct_max_depth.load()) {
            reentrant_direct_max_depth.store(depth);
        }
        if (!reentrant_direct_hook_ran.exchange(true)) {
            direct_queued_publisher = std::thread([&]() {
                direct_queued_publish_succeeded.store(
                    real_host.publish_media_frame(
                        media_lease.session_id, direct_queued_frame));
            });
            const auto queued_deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (reentrant_direct_subscription->stats().received_frames < 2 &&
                   std::chrono::steady_clock::now() < queued_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            direct_queued_publisher_enqueued.store(
                reentrant_direct_subscription->stats().received_frames >= 2);
            scripted->injectStreamOnNextRequest(
                0x1001, 82, 82000, {0x00, 0x00, 0x01, 0x41});
            const auto result = real_host.call(
                real_lease.session_id, "audio.getAlgorithmConfig", {});
            reentrant_direct_call_succeeded.store(
                result.status == axent::ControlStatus::Ok);
            direct_nested_frame_was_deferred.store(
                reentrant_direct_sink->records().size() == 3);
        }
        reentrant_direct_depth.fetch_sub(1);
    });
    auto direct_reentrant_frame = make_media_frame(80);
    direct_reentrant_frame.stream_id = 0x1001;
    direct_reentrant_frame.generation = replayed_streams[0].key.generation;
    const bool direct_reentrant_publish_succeeded = real_host.publish_media_frame(
        media_lease.session_id, direct_reentrant_frame);
    if (direct_queued_publisher.joinable()) {
        direct_queued_publisher.join();
    }
    require(direct_reentrant_publish_succeeded &&
                reentrant_direct_sink->wait_for_records(5),
            "Direct reentrant adapter media should drain after the current callback");
    auto direct_followup_frame = direct_reentrant_frame;
    direct_followup_frame.sequence_id = 83;
    require(real_host.publish_media_frame(
                media_lease.session_id, direct_followup_frame) &&
                reentrant_direct_sink->wait_for_records(6),
            "Direct subscription should remain usable after deferred reentrant media");
    const auto reentrant_direct_records = reentrant_direct_sink->records();
    require(reentrant_direct_call_succeeded.load() &&
                direct_nested_frame_was_deferred.load() &&
                direct_queued_publisher_enqueued.load() &&
                direct_queued_publish_succeeded.load() &&
                reentrant_direct_max_depth.load() == 1 &&
                reentrant_direct_records[2].frame.sequence_id == 80 &&
                reentrant_direct_records[3].frame.sequence_id == 81 &&
                reentrant_direct_records[4].frame.sequence_id == 82 &&
                reentrant_direct_records[5].frame.sequence_id == 83,
            "Direct reentrant call media must remain non-nested and FIFO");
    reentrant_direct_subscription->cancel();

    axent::MediaSubscriptionOptions reentrant_async_options;
    reentrant_async_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    reentrant_async_options.max_frames = 4;
    auto reentrant_async_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_async_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_async_sink, reentrant_async_options);
    require(reentrant_async_subscription != nullptr &&
                reentrant_async_sink->wait_for_records(2),
            "async reentrant-call subscription should replay descriptors");
    std::atomic<bool> reentrant_async_hook_ran{false};
    std::atomic<bool> reentrant_async_call_succeeded{false};
    reentrant_async_sink->set_frame_hook([&]() {
        if (reentrant_async_hook_ran.exchange(true)) {
            return;
        }
        scripted->injectStreamOnNextRequest(
            0x1001, 71, 71000, {0x00, 0x00, 0x01, 0x41});
        const auto result = real_host.call(
            real_lease.session_id, "audio.getAlgorithmConfig", {});
        reentrant_async_call_succeeded.store(result.status == axent::ControlStatus::Ok);
    });
    scripted->injectStream(0x1001, 70, 70000, {0x00, 0x00, 0x01, 0x41});
    require(reentrant_async_sink->wait_for_records(4),
            "media drained by an async sink reentrant call must not be lost");
    const auto reentrant_async_records = reentrant_async_sink->records();
    require(reentrant_async_call_succeeded.load() &&
                reentrant_async_records[2].kind == StreamCallbackKind::Frame &&
                reentrant_async_records[2].frame.sequence_id == 70 &&
                reentrant_async_records[3].kind == StreamCallbackKind::Frame &&
                reentrant_async_records[3].frame.sequence_id == 71,
            "async callback call() must defer and preserve nested adapter media order");
    reentrant_async_subscription->cancel();

    auto concurrent_direct_sink = std::make_shared<RecordingMediaStreamSink>();
    auto concurrent_direct_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, concurrent_direct_sink);
    require(concurrent_direct_subscription != nullptr &&
                concurrent_direct_sink->wait_for_records(2),
            "concurrent direct subscription should replay descriptors");
    concurrent_direct_sink->block_next_frame();
    auto direct_frame = make_media_frame(60);
    direct_frame.stream_id = 0x1001;
    direct_frame.generation = replayed_streams[0].key.generation;
    std::thread::id first_publisher_id;
    std::thread::id second_publisher_id;
    std::thread::id third_publisher_id;
    std::atomic<bool> first_publish_succeeded{false};
    std::atomic<bool> second_publish_succeeded{false};
    std::atomic<bool> third_publish_succeeded{false};
    std::atomic<bool> second_publish_finished{false};
    std::atomic<bool> third_publish_finished{false};
    std::thread first_publisher([&]() {
        first_publisher_id = std::this_thread::get_id();
        first_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, direct_frame));
    });
    require(concurrent_direct_sink->wait_for_frame_blocked(),
            "first direct publisher should enter its callback");
    auto second_direct_frame = direct_frame;
    second_direct_frame.sequence_id = 61;
    std::thread second_publisher([&]() {
        second_publisher_id = std::this_thread::get_id();
        second_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, second_direct_frame));
        second_publish_finished.store(true);
    });
    const auto second_enqueue_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (concurrent_direct_subscription->stats().received_frames < 2 &&
           std::chrono::steady_clock::now() < second_enqueue_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool second_enqueued =
        concurrent_direct_subscription->stats().received_frames >= 2;
    auto third_direct_frame = direct_frame;
    third_direct_frame.sequence_id = 62;
    std::thread third_publisher([&]() {
        third_publisher_id = std::this_thread::get_id();
        third_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, third_direct_frame));
        third_publish_finished.store(true);
    });
    const auto third_enqueue_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (concurrent_direct_subscription->stats().received_frames < 3 &&
           std::chrono::steady_clock::now() < third_enqueue_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool third_enqueued =
        concurrent_direct_subscription->stats().received_frames >= 3;
    const bool queued_publishers_waited =
        !second_publish_finished.load() && !third_publish_finished.load();
    concurrent_direct_sink->unblock_frames();
    first_publisher.join();
    second_publisher.join();
    third_publisher.join();
    require(second_enqueued && third_enqueued &&
                first_publish_succeeded.load() && second_publish_succeeded.load() &&
                third_publish_succeeded.load(),
            "all concurrent direct publishes should succeed");
    require(queued_publishers_waited,
            "queued Direct publishes must not return before their callbacks run");
    require(concurrent_direct_sink->wait_for_records(5),
            "all concurrent Direct frames should be delivered");
    const auto concurrent_direct_records = concurrent_direct_sink->records();
    require(concurrent_direct_records[2].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[2].frame.sequence_id == 60 &&
                concurrent_direct_records[2].thread_id == first_publisher_id &&
                concurrent_direct_records[3].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[3].frame.sequence_id == 61 &&
                concurrent_direct_records[3].thread_id == second_publisher_id,
            "the first two Direct callbacks must remain FIFO on their publisher threads");
    require(concurrent_direct_records[4].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[4].frame.sequence_id == 62 &&
                concurrent_direct_records[4].thread_id == third_publisher_id,
            "Direct callbacks must remain synchronous on their publishing threads");
    concurrent_direct_subscription->cancel();

    axent::MediaSubscriptionOptions drop_options;
    drop_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    drop_options.max_frames = 1;
    drop_options.max_bytes = 1024 * 1024;
    auto drop_sink = std::make_shared<RecordingMediaStreamSink>();
    auto drop_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, drop_sink, drop_options);
    require(drop_subscription != nullptr && drop_sink->wait_for_records(2),
            "async stream subscription should replay descriptors");
    drop_sink->block_next_frame();
    scripted->injectStream(0x1001, 20, 20000, {0x00, 0x00, 0x01, 0x41});
    require(drop_sink->wait_for_frame_blocked(),
            "async stream sink should block its first frame callback");
    scripted->injectStream(0x1001, 21, 21000, {0x00, 0x00, 0x01, 0x41});
    scripted->injectStream(0x1001, 22, 22000, {0x00, 0x00, 0x01, 0x41});
    scripted->injectStream(0x1001, 23, 23000, {0x00, 0x00, 0x01, 0x41});
    const auto drop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (drop_subscription->stats().dropped_frames < 2 &&
           std::chrono::steady_clock::now() < drop_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto dropped_while_blocked = drop_subscription->stats().dropped_frames;
    drop_sink->unblock_frames();
    require(dropped_while_blocked == 2,
            "async max_frames=1 must drop the two oldest queued frames");
    require(drop_sink->wait_for_records(5),
            "async stream sink should receive ordered drop and retained frame callbacks");
    const auto drop_records = drop_sink->records();
    require(drop_records[2].kind == StreamCallbackKind::Frame &&
                drop_records[2].frame.sequence_id == 20,
            "in-flight async frame should remain first");
    require(drop_records[3].kind == StreamCallbackKind::DeliveryDropped &&
                drop_records[3].delivery.dropped_frames == 2,
            "DeliveryDropped should precede the retained frame with cumulative counts");
    require(drop_records[4].kind == StreamCallbackKind::Frame &&
                drop_records[4].frame.sequence_id == 23 &&
                axent::has_flag(drop_records[4].frame.flags,
                                axent::MediaFrameFlag::Discontinuity),
            "latest retained frame must carry Discontinuity after drop-oldest");
    drop_subscription->cancel();

    auto cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, cancel_sink, drop_options);
    require(cancel_subscription != nullptr && cancel_sink->wait_for_records(2),
            "cancel test subscription should replay descriptors");
    cancel_sink->block_next_frame();
    scripted->injectStream(0x1001, 30, 30000, {0x00, 0x00, 0x01, 0x41});
    require(cancel_sink->wait_for_frame_blocked(),
            "cancel test should enter an in-flight async callback");
    std::atomic<bool> cancel_finished{false};
    std::thread cancel_thread([&]() {
        cancel_subscription->cancel();
        cancel_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool cancel_waited_for_callback = !cancel_finished.load();
    cancel_sink->unblock_frames();
    cancel_thread.join();
    require(cancel_waited_for_callback,
            "external cancel must wait for an in-flight stream callback");
    require(cancel_finished.load() && cancel_sink->wait_for_records(4),
            "external cancel should finish with SubscriptionClosed");
    const auto cancel_records = cancel_sink->records();
    require(cancel_records.back().kind == StreamCallbackKind::SubscriptionClosed,
            "SubscriptionClosed must be the terminal external-cancel callback");
    const auto records_after_cancel = cancel_records.size();
    scripted->injectStream(0x1001, 31, 31000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(cancel_sink->records().size() == records_after_cancel,
            "cancel return must prevent future stream callbacks");

    auto self_cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto self_cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, self_cancel_sink);
    require(self_cancel_subscription != nullptr && self_cancel_sink->wait_for_records(2),
            "self-cancel stream subscription should replay descriptors");
    std::weak_ptr<axent::MediaStreamSubscription> weak_self_cancel =
        self_cancel_subscription;
    CallbackExitGate direct_self_cancel_gate;
    std::atomic<bool> reentrant_publish_rejected{false};
    self_cancel_sink->set_frame_hook([&]() {
        auto nested_frame = make_media_frame(32001);
        nested_frame.stream_id = 0x1001;
        nested_frame.generation = replayed_streams[0].key.generation;
        reentrant_publish_rejected.store(
            !real_host.publish_media_frame(media_lease.session_id, nested_frame));
        if (auto subscription = weak_self_cancel.lock()) {
            subscription->cancel();
        }
        direct_self_cancel_gate.enter_and_wait();
    });
    scripted->injectStream(0x1001, 32, 32000, {0x00, 0x00, 0x01, 0x41});
    require(direct_self_cancel_gate.wait_until_entered(),
            "direct self-cancel should return inside its in-flight callback");
    std::mutex direct_external_cancel_mutex;
    std::condition_variable direct_external_cancel_cv;
    bool direct_external_cancel_started = false;
    std::atomic<bool> direct_external_cancel_finished{false};
    std::thread direct_external_cancel_thread([&]() {
        {
            std::lock_guard<std::mutex> lock(direct_external_cancel_mutex);
            direct_external_cancel_started = true;
        }
        direct_external_cancel_cv.notify_all();
        self_cancel_subscription->cancel();
        direct_external_cancel_finished.store(true);
    });
    bool direct_external_cancel_observed = false;
    {
        std::unique_lock<std::mutex> lock(direct_external_cancel_mutex);
        direct_external_cancel_observed = direct_external_cancel_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() {
                return direct_external_cancel_started;
            });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool direct_external_cancel_waited =
        !direct_external_cancel_finished.load();
    direct_self_cancel_gate.release();
    direct_external_cancel_thread.join();
    require(direct_external_cancel_observed && direct_external_cancel_waited &&
                direct_external_cancel_finished.load() &&
                reentrant_publish_rejected.load(),
            "Direct self-cancel must reject nested publish and remain externally quiescent");
    require(self_cancel_sink->wait_for_records(3),
            "direct sink should receive the frame that triggers self-cancel");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto self_cancel_count = self_cancel_sink->records().size();
    scripted->injectStream(0x1001, 33, 33000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(self_cancel_sink->records().size() == self_cancel_count,
            "self-cancel must not nest terminal callbacks or allow future frames");

    auto async_self_cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto async_self_cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, async_self_cancel_sink, drop_options);
    require(async_self_cancel_subscription != nullptr &&
                async_self_cancel_sink->wait_for_records(2),
            "async self-cancel subscription should replay descriptors");
    std::weak_ptr<axent::MediaStreamSubscription> weak_async_self_cancel =
        async_self_cancel_subscription;
    CallbackExitGate async_self_cancel_gate;
    async_self_cancel_sink->set_frame_hook(
        [weak_async_self_cancel, &async_self_cancel_gate]() {
            if (auto subscription = weak_async_self_cancel.lock()) {
                subscription->cancel();
            }
            async_self_cancel_gate.enter_and_wait();
        });
    scripted->injectStream(0x1001, 34, 34000, {0x00, 0x00, 0x01, 0x41});
    require(async_self_cancel_gate.wait_until_entered(),
            "async self-cancel should return inside its in-flight callback");
    std::mutex async_external_cancel_mutex;
    std::condition_variable async_external_cancel_cv;
    bool async_external_cancel_started = false;
    std::atomic<bool> async_external_cancel_finished{false};
    std::thread async_external_cancel_thread([&]() {
        {
            std::lock_guard<std::mutex> lock(async_external_cancel_mutex);
            async_external_cancel_started = true;
        }
        async_external_cancel_cv.notify_all();
        async_self_cancel_subscription->cancel();
        async_external_cancel_finished.store(true);
    });
    bool async_external_cancel_observed = false;
    {
        std::unique_lock<std::mutex> lock(async_external_cancel_mutex);
        async_external_cancel_observed = async_external_cancel_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() {
                return async_external_cancel_started;
            });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool async_external_cancel_waited =
        !async_external_cancel_finished.load();
    async_self_cancel_gate.release();
    async_external_cancel_thread.join();
    require(async_external_cancel_observed && async_external_cancel_waited &&
                async_external_cancel_finished.load(),
            "external cancel must wait until an async self-cancel callback exits");
    require(async_self_cancel_sink->wait_for_records(3),
            "async sink should receive the frame that triggers self-cancel");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto async_self_cancel_count = async_self_cancel_sink->records().size();
    scripted->injectStream(0x1001, 35, 35000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(async_self_cancel_sink->records().size() == async_self_cancel_count,
            "async self-cancel must not detach future callbacks");

    auto reentrant_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_sink);
    require(reentrant_subscription != nullptr && reentrant_sink->wait_for_records(2),
            "reentrant release subscription should replay descriptors");
    std::atomic<axent::ControlStatus> reentrant_status{axent::ControlStatus::InternalError};
    std::atomic<bool> reentrant_release_rejected{false};
    std::atomic<bool> reentrant_stop_rejected{false};
    std::atomic<axent::ControlStatus> reentrant_acquire_status{
        axent::ControlStatus::InternalError};
    reentrant_sink->set_frame_hook([&]() {
        try {
            real_host.release_session(media_lease.session_id, "illegal callback release");
        } catch (const std::logic_error&) {
            reentrant_release_rejected.store(true);
        }
        try {
            real_host.stop();
        } catch (const std::logic_error&) {
            reentrant_stop_rejected.store(true);
        }
        reentrant_acquire_status.store(
            real_host.acquire_session(second_real_request).status);
        const auto result = real_host.call(
            real_lease.session_id, "audio.getAlgorithmConfig", {});
        reentrant_status.store(result.status);
    });
    reentrant_sink->block_next_frame();
    scripted->injectStream(0x1001, 40, 40000, {0x00, 0x00, 0x01, 0x41});
    require(reentrant_sink->wait_for_frame_blocked(),
            "reentrant release test should enter the direct sink callback");
    const auto stream_records_before_release = stream_sink->records().size();
    std::atomic<bool> release_finished{false};
    std::thread release_thread([&real_host, &media_lease, &release_finished]() {
        real_host.release_session(media_lease.session_id, "replace media lease");
        release_finished.store(true);
    });
    const bool release_entered_subscription_close =
        stream_sink->wait_for_records(stream_records_before_release + 3);
    const bool release_waited_for_callback = !release_finished.load();
    reentrant_sink->unblock_frames();
    release_thread.join();
    require(release_entered_subscription_close && release_waited_for_callback,
            "release must enter stream close and wait for an in-flight callback");
    require(release_finished.load(), "release should complete after the callback returns");
    require(reentrant_status.load() == axent::ControlStatus::Busy,
            "callback re-entry during release must fail fast with Busy instead of deadlocking");
    require(reentrant_release_rejected.load() && reentrant_stop_rejected.load() &&
                reentrant_acquire_status.load() == axent::ControlStatus::Busy,
            "Host lifecycle mutation must fail fast inside a media sink callback");

    const auto released_stream_records = stream_sink->records();
    require(released_stream_records.size() >= 3 &&
                released_stream_records[released_stream_records.size() - 3].kind ==
                    StreamCallbackKind::Closed &&
                released_stream_records[released_stream_records.size() - 2].kind ==
                    StreamCallbackKind::Closed &&
                released_stream_records.back().kind == StreamCallbackKind::SubscriptionClosed,
            "release must end each stream lifecycle before SubscriptionClosed");

    const auto factory_calls_after_media_release = transport_factory_calls;
    const auto busy_after_media_release = real_host.acquire_session(second_real_request);
    require(!busy_after_media_release.acquired,
            "releasing A media lease must not let B replace a remaining A control lease");
    require(busy_after_media_release.status == axent::ControlStatus::Busy,
            "B must remain typed Busy while A still has a control lease");
    require(transport_factory_calls == factory_calls_after_media_release,
            "B Busy after A media release must not replace A transport");
    const auto control_after_media_release =
        real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(control_after_media_release.status == axent::ControlStatus::Ok,
            "A control lease must remain usable after its media lease is released");
    require(transport_factory_calls == factory_calls_after_media_release,
            "A control call after media release must keep the existing transport");

    const auto replacement_lease = real_host.acquire_session(real_request);
    require(replacement_lease.acquired, "replacement media lease should be acquired");
    auto replacement_consumer = real_host.create_media_consumer(replacement_lease.session_id, real_relay_options);
    require(replacement_consumer != nullptr, "replacement media consumer should be created");

    auto replacement_stream_sink = std::make_shared<RecordingMediaStreamSink>();
    auto replacement_stream_subscription = real_host.subscribe_media_stream(
        replacement_lease.session_id, replacement_stream_sink);
    require(replacement_stream_subscription != nullptr &&
                replacement_stream_sink->wait_for_records(2),
            "replacement media lease should replay the still-open physical descriptors");
    const auto replacement_opened = replacement_stream_sink->records();
    require(replacement_opened[0].key.session_id == replacement_lease.session_id &&
                replacement_opened[0].key.generation == replayed_streams[0].key.generation,
            "logical media lease handoff must preserve physical generation and bind a new session id");

    scripted->blockAfterNextStream();
    scripted->injectStream(0x1001, 41, 41000, {0x00, 0x00, 0x01, 0x41});
    require(scripted->waitForStreamBlocked(),
            "stale stream should be queued before adapter drain");
    std::thread stale_release_thread([&real_host, &replacement_lease]() {
        real_host.release_session(replacement_lease.session_id, "replace stale media lease");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    scripted->unblockStream();
    stale_release_thread.join();

    const auto post_stale_lease = real_host.acquire_session(real_request);
    require(post_stale_lease.acquired,
            "media lease should reacquire after the stale-frame release");
    auto post_stale_consumer =
        real_host.create_media_consumer(post_stale_lease.session_id, real_relay_options);
    require(post_stale_consumer != nullptr,
            "post-stale media consumer should be created");

    const auto stale_frame = wait_for_media_frame(*post_stale_consumer);
    require(!stale_frame.has_value(), "queued frame from released media lease must not reach replacement lease");

    scripted->injectStream(0x1001, 42, 42000, {0x00, 0x00, 0x01, 0x41});
    const auto post_handoff_frame = wait_for_media_frame(*post_stale_consumer);
    require(post_handoff_frame.has_value() &&
                post_handoff_frame->session_id == post_stale_lease.session_id &&
                post_handoff_frame->sequence_id == 42,
            "frame captured after handoff must carry the replacement lease token");

    axent::MediaSubscriptionOptions lifecycle_options;
    lifecycle_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    lifecycle_options.max_frames = 4;
    auto lifecycle_sink = std::make_shared<RecordingMediaStreamSink>();
    auto lifecycle_subscription = real_host.subscribe_media_stream(
        post_stale_lease.session_id, lifecycle_sink, lifecycle_options);
    require(lifecycle_subscription != nullptr && lifecycle_sink->wait_for_records(2),
            "terminal lifecycle subscription should replay active descriptors");
    lifecycle_sink->block_next_frame();
    scripted->injectStream(0x1001, 43, 43000, {0x00, 0x00, 0x01, 0x41});
    require(lifecycle_sink->wait_for_frame_blocked(),
            "terminal lifecycle test should hold an in-flight frame callback");
    require(real_adapter != nullptr, "real adapter test seam should remain available");
    axent::testing::AxtpAdapterTestSeam::stop_session_pump(*real_adapter);
    axent::testing::AxtpAdapterTestSeam::reopen_media_streams(
        *real_adapter, real_device.id);
    axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*real_adapter);

    std::atomic<bool> lifecycle_release_finished{false};
    std::thread lifecycle_release_thread([&]() {
        real_host.release_session(
            post_stale_lease.session_id, "terminal lifecycle reconciliation");
        lifecycle_release_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool lifecycle_release_waited = !lifecycle_release_finished.load();
    lifecycle_sink->unblock_frames();
    lifecycle_release_thread.join();
    require(lifecycle_release_waited && lifecycle_release_finished.load(),
            "release must wait for the in-flight lifecycle sink callback");

    const auto terminal_records = lifecycle_sink->records();
    require(terminal_records.size() == 10,
            "release must reconcile both old and pending stream generations");
    require(terminal_records[3].kind == StreamCallbackKind::Closed &&
                terminal_records[3].key.stream_id == 0x1001 &&
                terminal_records[3].key.generation == 1 &&
                terminal_records[4].kind == StreamCallbackKind::Opened &&
                terminal_records[4].key.stream_id == 0x1001 &&
                terminal_records[4].key.generation == 2 &&
                terminal_records[5].kind == StreamCallbackKind::Closed &&
                terminal_records[5].key == terminal_records[4].key &&
                terminal_records[6].kind == StreamCallbackKind::Closed &&
                terminal_records[6].key.stream_id == 0x2001 &&
                terminal_records[6].key.generation == 1 &&
                terminal_records[7].kind == StreamCallbackKind::Opened &&
                terminal_records[7].key.stream_id == 0x2001 &&
                terminal_records[7].key.generation == 2 &&
                terminal_records[8].kind == StreamCallbackKind::Closed &&
                terminal_records[8].key == terminal_records[7].key &&
                terminal_records[9].kind == StreamCallbackKind::SubscriptionClosed,
            "terminal reconciliation must preserve Closed/Open/Closed ordering per stream");

    const auto factory_calls_before_last_control_release = transport_factory_calls;
    real_host.release_session(real_lease.session_id, "last A control lease done");
    axent::SessionAcquireRequest second_control_request = second_real_request;
    second_control_request.media = false;
    const auto second_control_lease = real_host.acquire_session(second_control_request);
    require(second_control_lease.acquired,
            "B should acquire after A's last control-only lease is released");
    const auto second_control_call =
        real_host.call(second_control_lease.session_id, "audio.getAlgorithmConfig", {});
    require(second_control_call.status == axent::ControlStatus::Ok,
            "B control lease should connect after A ownership is released");
    require(transport_factory_calls == factory_calls_before_last_control_release + 1,
            "B takeover should create exactly one new transport after A's final release");
    real_host.release_session(second_control_lease.session_id, "B control lease done");
    real_host.stop();

    return 0;
}
