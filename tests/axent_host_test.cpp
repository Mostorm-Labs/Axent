#include <cstdint>
#include <stdexcept>

#include "axent/host/axent_host.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

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
    require(host.start(options), "host should start");

    const auto devices = host.discover_devices();
    require(devices.size() == 1, "mock host should discover one device");
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

    const auto result = host.call(lease.session_id, "status.get", {});
    require(result.status == axent::ControlStatus::Ok, "host call should dispatch through broker");
    require(result.body.at("health") == "ok", "host call result mismatch");

    host.release_session(lease.session_id, "test complete");
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

    return 0;
}
