#include <stdexcept>
#include <string>
#include <optional>
#include <vector>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/logging/logger.hpp"

namespace {

class ThrowingAdapter final : public axent::Adapter {
public:
    axent::AdapterMetadata metadata() const override
    {
        return {"throwing", "Throwing Adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        axent::DeviceSnapshot device;
        device.id = "throw-device-001";
        device.adapter = "throwing";
        device.connection.online = true;
        return {device};
    }

    axent::ControlResult call(const std::string&, const std::string&, const nlohmann::json&) override
    {
        throw std::runtime_error("adapter exploded");
    }

    axent::ControlResult start_firmware_update(const std::string&, const std::string&) override
    {
        throw std::runtime_error("firmware exploded");
    }
};

class SyncOnlyAdapter final : public axent::Adapter {
public:
    axent::AdapterMetadata metadata() const override
    {
        return {"sync-only", "Sync-only lazy session adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        axent::DeviceSnapshot device;
        device.id = "physical-sync-only";
        device.endpoint_id = "endpoint/sync-only";
        device.adapter = "sync-only";
        device.connection.online = true;
        return {device};
    }

    axent::ControlResult call(
        const std::string&, const std::string&, const nlohmann::json&) override
    {
        return {axent::ControlStatus::Ok, {{"path", "sync"}}};
    }

    axent::ControlOperationPtr call_async(
        const std::string&,
        const std::string&,
        const nlohmann::json&,
        axent::ControlCallOptions) override
    {
        return axent::make_completed_control_operation(
            {axent::ControlStatus::Unavailable, {{"path", "async"}}});
    }

    axent::ControlResult start_firmware_update(
        const std::string&, const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }
};

class RoutedCaptureAdapter final : public axent::Adapter {
public:
    axent::AdapterMetadata metadata() const override
    {
        return {"routed-capture", "Routed capture adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        axent::DeviceSnapshot device;
        device.id = "provider-device-1";
        device.adapter = "routed-capture";
        device.endpoint_id = "ep_device";
        device.endpoint_delivery_mode = axent::EndpointDeliveryMode::NativeRelay;
        device.connection.online = true;
        return {device};
    }

    axent::ControlResult call(
        const std::string&, const std::string&, const nlohmann::json&) override
    {
        return {axent::ControlStatus::InternalError, {{"error", "legacy call used"}}};
    }

    axent::ControlResult call(const axent::AdapterControlRequest& request) override
    {
        ++call_count;
        captured = request;
        return {axent::ControlStatus::Ok, {{"path", "routed"}}};
    }

    axent::ControlOperationPtr call_async(
        const axent::AdapterControlRequest& request,
        axent::ControlCallOptions) override
    {
        captured = request;
        return axent::make_completed_control_operation(
            {axent::ControlStatus::Ok, {{"path", "routed-async"}}});
    }

    axent::ControlResult start_firmware_update(
        const std::string&, const std::string&) override
    {
        return {axent::ControlStatus::InternalError, {{"error", "legacy firmware used"}}};
    }

    axent::ControlResult start_firmware_update(
        const axent::AdapterControlRequest& request,
        const std::string& file_path) override
    {
        captured = request;
        return {axent::ControlStatus::Accepted, {{"file", file_path}}};
    }

    std::optional<axent::AdapterControlRequest> captured;
    std::size_t call_count = 0;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_result(const axent::ControlResult& result,
                    axent::ControlStatus status,
                    const std::string& error,
                    const char* message)
{
    require(result.status == status, message);
    if (!error.empty()) {
        require(result.body.at("error") == error, message);
    }
}

} // namespace

int main()
{
    axent::MockAdapter adapter;
    ThrowingAdapter throwing_adapter;
    RoutedCaptureAdapter routed_capture_adapter;
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }
    for (const auto& device : throwing_adapter.discover()) {
        devices.upsert(device);
    }
    for (const auto& device : routed_capture_adapter.discover()) {
        devices.upsert(device);
    }
    auto unavailable_endpoint_device = routed_capture_adapter.discover().front();
    unavailable_endpoint_device.id = "provider-device-offline";
    unavailable_endpoint_device.endpoint_id = "ep_offline";
    unavailable_endpoint_device.connection.online = false;
    devices.upsert(unavailable_endpoint_device);

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(adapter);
    broker.register_adapter(throwing_adapter);
    broker.register_adapter(routed_capture_adapter);
    axent::ControlPlane control_plane(broker);

    const auto legacy_device_list = control_plane.handle_text({
        {"op", 7},
        {"sid", 1001},
        {"d", {
            {"id", "legacy-list"},
            {"method", "GetDeviceList"},
            {"params", nlohmann::json::object()},
        }},
    });
    require(legacy_device_list.at("d").at("status").at("result") == true,
            "legacy GetDeviceList should succeed");
    require(legacy_device_list.at("d").at("result").at("devices").size() == 4,
            "legacy GetDeviceList should return managed devices");
    require(legacy_device_list.at("d").at("result").at("devices").at(0).contains("endpointId"),
            "managed device snapshots should expose a stable endpointId");

    const auto make_selector_collision = [](
                                             std::string adapter_name,
                                             std::string id,
                                             std::string serial,
                                             std::string endpoint) {
        axent::DeviceSnapshot device;
        device.adapter = std::move(adapter_name);
        device.id = std::move(id);
        device.identity.serial_number = std::move(serial);
        device.endpoint_id = std::move(endpoint);
        device.connection.online = true;
        return device;
    };
    devices.upsert(make_selector_collision(
        "mock", "CROSS-ID", "mock-id-owner", "endpoint/cross-id-mock"));
    devices.upsert(make_selector_collision(
        "routed-capture",
        "CROSS-ID",
        "routed-id-owner",
        "endpoint/cross-id-routed"));
    devices.upsert(make_selector_collision(
        "routed-capture",
        "cross-id-serial-owner",
        "CROSS-ID",
        "endpoint/cross-id-serial"));
    devices.upsert(make_selector_collision(
        "mock",
        "cross-serial-mock",
        "CROSS-SERIAL",
        "endpoint/cross-serial-mock"));
    devices.upsert(make_selector_collision(
        "routed-capture",
        "cross-serial-routed",
        "CROSS-SERIAL",
        "endpoint/cross-serial-routed"));
    devices.upsert(make_selector_collision(
        "routed-capture",
        "CROSS-SERIAL",
        "id-shadow-owner",
        "endpoint/cross-serial-id"));

    const auto ambiguous_device_id = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "cross-namespace-device-id"},
        {"method", "status.get"},
        {"params", {{"deviceId", "CROSS-ID"}}},
    });
    require(ambiguous_device_id.at("error").at("code") == -32004,
            "ambiguous deviceId must not fall through to a unique serial match");
    const auto ambiguous_serial = control_plane.handle_text({
        {"op", 7},
        {"sid", 1004},
        {"d", {
            {"id", "cross-namespace-serial"},
            {"method", "GetDeviceInfo"},
            {"params", {{"serialNumber", "CROSS-SERIAL"}}},
        }},
    });
    require(ambiguous_serial.at("d").at("status").at("result") == false,
            "ambiguous serialNumber must not fall through to a unique device ID");

    const auto mock_route = routes.resolve_endpoint("endpoint/mock-primary");
    require(mock_route.has_value(), "mock endpoint should resolve");
    require(mock_route->device_id == "mock-device-001",
            "mock endpoint should resolve to the physical mock device");
    require(mock_route->endpoint_id == "endpoint/mock-primary",
            "resolved route should preserve the logical endpoint");
    require(mock_route->endpoint_delivery_mode == axent::EndpointDeliveryMode::LocalProjection,
            "default endpoint route should use local projection");
    const auto found_endpoint = routes.resolve_endpoint_route("endpoint/mock-primary");
    require(found_endpoint.status == axent::RouteResolutionStatus::Found &&
                found_endpoint.target.has_value(),
            "online endpoint should report Found");
    require(routes.resolve_endpoint_route("endpoint/missing").status ==
                axent::RouteResolutionStatus::NotFound,
            "unknown endpoint should report NotFound");

    axent::DeviceManager offline_devices;
    auto offline_device = adapter.discover().front();
    offline_device.id = "offline-device";
    offline_device.endpoint_id = "endpoint/offline";
    offline_device.connection.online = false;
    offline_devices.upsert(offline_device);
    axent::RouteManager offline_routes(offline_devices);
    require(offline_routes.resolve_endpoint_route("endpoint/offline").status ==
                axent::RouteResolutionStatus::Unavailable,
            "offline endpoint should report Unavailable");
    require(!offline_routes.resolve_endpoint("endpoint/offline").has_value(),
            "compatibility endpoint resolver must omit unavailable targets");

    // Duplicate logical endpoints are rejected before they can make routing
    // ambiguous; the original binding remains usable.
    axent::DeviceManager collision_devices;
    auto collision_a = adapter.discover().front();
    auto collision_b = collision_a;
    collision_a.id = "collision-a";
    collision_b.id = "collision-b";
    collision_a.endpoint_id = "endpoint/duplicate";
    collision_b.endpoint_id = "endpoint/duplicate";
    collision_a.endpoint_delivery_mode = axent::EndpointDeliveryMode::NativeRelay;
    require(collision_devices.upsert(collision_a).accepted(),
            "first endpoint owner should be accepted");
    require(collision_devices.upsert(collision_b).status ==
                axent::DeviceUpsertStatus::EndpointConflict,
            "second endpoint owner should be rejected");
    axent::RouteManager collision_routes(collision_devices);
    const auto collision_route =
        collision_routes.resolve_endpoint_route("endpoint/duplicate");
    require(collision_route.status == axent::RouteResolutionStatus::Found &&
                collision_route.target->device_id == "collision-a" &&
                collision_route.target->endpoint_delivery_mode ==
                    axent::EndpointDeliveryMode::NativeRelay,
            "rejected duplicate must preserve the original route");

    const auto routed_status = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "endpoint-status"},
        {"src", "controller:nearcast"},
        {"dst", "endpoint/mock-primary"},
        {"method", "status.get"},
        {"params", nlohmann::json::object()},
    });
    require(routed_status.at("result").at("health") == "ok",
            "endpoint-routed JSON-RPC call should reach the mock device");
    require(routed_status.at("src") == "endpoint/mock-primary" &&
                routed_status.at("dst") == "controller:nearcast",
            "endpoint-routed JSON-RPC response should reverse src and dst");

    const auto routed_capture = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "endpoint-capture"},
        {"src", "ep_app"},
        {"dst", "ep_device"},
        {"method", "status.get"},
        {"params", {{"deviceId", "wrong-device"},
                    {"serialNumber", "wrong-serial"},
                    {"detail", "business"}}},
    });
    require(routed_capture.at("result").at("path") == "routed",
            "routed overload should handle addressed control");
    require(routed_capture_adapter.captured.has_value(),
            "routed adapter should capture the request");
    require(routed_capture_adapter.captured->device_id == "provider-device-1" &&
                routed_capture_adapter.captured->source_endpoint_id == "ep_app" &&
                routed_capture_adapter.captured->destination_endpoint_id == "ep_device" &&
                routed_capture_adapter.captured->endpoint_delivery_mode ==
                    axent::EndpointDeliveryMode::NativeRelay &&
                routed_capture_adapter.captured->params ==
                    nlohmann::json{{"detail", "business"}},
            "broker must carry the resolved endpoint route without legacy selectors");
    axent::ControlCommand routed_lane;
    routed_lane.request_id = "lane";
    routed_lane.method = "status.get";
    routed_lane.src = "ep_app";
    routed_lane.dst = "ep_device";
    require(broker.route_key(routed_lane) ==
                "physical:routed-capture:provider-device-1",
            "endpoint aliases must share the physical device lane");

    const auto routed_async = broker.dispatch_async(routed_lane)->wait();
    require(routed_async.status == axent::ControlStatus::Ok &&
                routed_async.body.at("path") == "routed-async",
            "async dispatch must use the routed adapter overload");
    require(routed_capture_adapter.captured->source_endpoint_id == "ep_app" &&
                routed_capture_adapter.captured->destination_endpoint_id == "ep_device",
            "async routed dispatch must retain endpoint addressing");

    auto routed_firmware = routed_lane;
    routed_firmware.method = "firmware.update";
    routed_firmware.params = {{"file", "/tmp/routed-fw.bin"}};
    const auto routed_firmware_result = broker.dispatch(routed_firmware);
    require(routed_firmware_result.status == axent::ControlStatus::Accepted &&
                routed_firmware_result.body.at("file") == "/tmp/routed-fw.bin",
            "firmware dispatch must use the routed adapter overload");
    require(routed_capture_adapter.captured->method == "firmware.update" &&
                routed_capture_adapter.captured->endpoint_delivery_mode ==
                    axent::EndpointDeliveryMode::NativeRelay,
            "firmware routed dispatch must retain route metadata");

    const auto dst_takes_precedence = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "endpoint-precedence"},
        {"src", "controller:nearcast"},
        {"dst", "endpoint/mock-primary"},
        {"method", "status.get"},
        {"params", {{"deviceId", "throw-device-001"}}},
    });
    require(dst_takes_precedence.at("result").at("health") == "ok",
            "logical dst must take precedence over legacy deviceId");

    const auto missing_endpoint = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "missing-endpoint"},
        {"src", "controller:nearcast"},
        {"dst", "endpoint/missing"},
        {"method", "status.get"},
        {"params", nlohmann::json::object()},
    });
    require(missing_endpoint.at("error").at("code") == -32004,
            "unknown logical endpoint should report a stable NotFound error");
    require(missing_endpoint.at("src") == "endpoint/missing" &&
                missing_endpoint.at("dst") == "controller:nearcast",
            "unknown endpoint errors should retain the reversible routing envelope");

    const auto offline_endpoint = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "offline-endpoint"},
        {"src", "ep_app"},
        {"dst", "ep_offline"},
        {"method", "status.get"},
        {"params", nlohmann::json::object()},
    });
    require(offline_endpoint.at("error").at("code") == -32001,
            "known offline endpoint should report Unavailable");

    axent::ControlCommand async_missing;
    async_missing.request_id = "async-missing";
    async_missing.method = "status.get";
    async_missing.src = "ep_app";
    async_missing.dst = "ep_missing_async";
    require(broker.dispatch_async(async_missing)->wait().status ==
                axent::ControlStatus::NotFound,
            "async unknown endpoint should report NotFound");
    async_missing.dst = "ep_offline";
    require(broker.dispatch_async(async_missing)->wait().status ==
                axent::ControlStatus::Unavailable,
            "async offline endpoint should report Unavailable");

    const auto incomplete_envelope = control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "incomplete-envelope"},
        {"dst", "endpoint/mock-primary"},
        {"method", "status.get"},
        {"params", nlohmann::json::object()},
    });
    require(incomplete_envelope.at("error").at("code") == -32602,
            "endpoint-routed JSON-RPC must provide src and dst together");

    const auto capture_calls_before_malformed = routed_capture_adapter.call_count;
    for (const auto& malformed_fields : std::vector<nlohmann::json>{
             {{"src", "ep_app"}},
             {{"dst", "ep_device"}},
             {{"src", 7}, {"dst", "ep_device"}},
             {{"src", "ep_app"}, {"dst", false}},
             {{"src", ""}, {"dst", "ep_device"}},
             {{"src", "ep_app"}, {"dst", ""}},
             {{"src", 7}, {"dst", false}},
         }) {
        auto malformed = malformed_fields;
        malformed["jsonrpc"] = "2.0";
        malformed["id"] = "malformed-routing";
        malformed["method"] = "status.get";
        malformed["params"] = {{"deviceId", "provider-device-1"}};
        const auto malformed_response = control_plane.handle_text(malformed);
        require(malformed_response.at("error").at("code") == -32602,
                "malformed endpoint envelope must be rejected before Broker dispatch");
        require(!malformed_response.contains("src") &&
                    !malformed_response.contains("dst"),
                "malformed endpoint envelope must not produce a half response envelope");
    }
    require(routed_capture_adapter.call_count == capture_calls_before_malformed,
            "malformed endpoint envelopes must not invoke an adapter");

    // WebSocket/ControlPlane dispatch is the synchronous lazy-connect
    // boundary. A real AxtpAdapter uses this path to open the destination's
    // HID session; Host lease calls use Broker::dispatch_async separately.
    SyncOnlyAdapter sync_only_adapter;
    axent::DeviceManager sync_only_devices;
    sync_only_devices.upsert(sync_only_adapter.discover().front());
    axent::RouteManager sync_only_routes(sync_only_devices);
    axent::Logger sync_only_logger;
    axent::Middleware sync_only_middleware(sync_only_logger);
    axent::FlowControl sync_only_flow;
    axent::Broker sync_only_broker(
        sync_only_routes, sync_only_middleware, sync_only_flow);
    sync_only_broker.register_adapter(sync_only_adapter);
    axent::ControlPlane sync_only_control_plane(sync_only_broker);
    const auto lazy_connect_response = sync_only_control_plane.handle_text({
        {"jsonrpc", "2.0"},
        {"id", "lazy-connect"},
        {"src", "controller:nearcast"},
        {"dst", "endpoint/sync-only"},
        {"method", "status.get"},
        {"params", nlohmann::json::object()},
    });
    require(lazy_connect_response.at("result").at("path") == "sync",
            "ControlPlane must use the adapter sync path for lazy session open");

    const auto legacy_info_by_serial = control_plane.handle_text({
        {"op", 7},
        {"sid", 1002},
        {"d", {
            {"id", "legacy-info-serial"},
            {"method", "GetDeviceInfo"},
            {"params", {{"serialNumber", "MOCK001"}}},
        }},
    });
    require(legacy_info_by_serial.at("d").at("status").at("result") == true,
            "legacy GetDeviceInfo by serialNumber should succeed");
    require(legacy_info_by_serial.at("d").at("result").at("identity").at("serialNumber") == "MOCK001",
            "legacy GetDeviceInfo by serialNumber should return mock identity");

    const auto legacy_info_by_device_id = control_plane.handle_text({
        {"op", 7},
        {"sid", 1003},
        {"d", {
            {"id", "legacy-info-device"},
            {"method", "GetDeviceInfo"},
            {"params", {{"deviceId", "mock-device-001"}}},
        }},
    });
    require(legacy_info_by_device_id.at("d").at("status").at("result") == true,
            "legacy GetDeviceInfo by deviceId should succeed");
    require(legacy_info_by_device_id.at("d").at("result").at("id") == "mock-device-001",
            "legacy GetDeviceInfo by deviceId should return mock device");

    axent::ControlCommand command;
    command.request_id = "req-1";
    command.method = "status.get";
    command.device_id = "mock-device-001";
    const auto audit_start = logger.records().size();
    const auto result = broker.dispatch(command);

    require(result.status == axent::ControlStatus::Ok, "status.get should succeed");
    require(result.body.at("health") == "ok", "status.get health mismatch");
    require(logger.records().size() == audit_start + 2, "broker should emit request and response audit records");
    require(logger.records()[audit_start].channel == "audit", "request log channel mismatch");
    require(logger.records()[audit_start].message == "control.request", "request log message mismatch");
    require(logger.records()[audit_start].fields.at("requestId") == "req-1", "request log requestId mismatch");
    require(logger.records()[audit_start].fields.at("method") == "status.get", "request log method mismatch");
    require(logger.records()[audit_start].fields.at("deviceId") == "mock-device-001", "request log deviceId mismatch");
    require(logger.records()[audit_start].fields.at("source") == "json-rpc", "request log source mismatch");
    require(logger.records()[audit_start + 1].channel == "audit", "response log channel mismatch");
    require(logger.records()[audit_start + 1].message == "control.response", "response log message mismatch");
    require(logger.records()[audit_start + 1].fields.at("requestId") == "req-1", "response log requestId mismatch");
    require(logger.records()[audit_start + 1].fields.at("method") == "status.get", "response log method mismatch");
    require(logger.records()[audit_start + 1].fields.at("deviceId") == "mock-device-001", "response log deviceId mismatch");
    require(logger.records()[audit_start + 1].fields.at("source") == "json-rpc", "response log source mismatch");
    require(logger.records()[audit_start + 1].fields.at("status") == "ok", "response log status mismatch");
    require(!flow.snapshot().paused, "flow should not be paused");
    require(flow.snapshot().dropped == 0, "flow should not record drops while running");

    flow.pause();
    const auto paused = broker.dispatch(command);
    require_result(paused, axent::ControlStatus::Unavailable, "flow paused", "paused flow should be unavailable");
    require(flow.snapshot().dropped == 1, "paused flow should record one drop");
    require(logger.records().back().message == "control.response", "paused dispatch should emit response audit");
    require(logger.records().back().fields.at("status") == "unavailable", "paused response status mismatch");
    flow.resume();

    axent::ControlCommand paused_throwing = command;
    paused_throwing.request_id = "req-paused-throwing";
    paused_throwing.device_id = "throw-device-001";
    flow.pause();
    const auto paused_throwing_result = broker.dispatch(paused_throwing);
    require_result(paused_throwing_result,
                   axent::ControlStatus::Unavailable,
                   "flow paused",
                   "paused flow should not invoke adapter");
    require(flow.snapshot().dropped == 2, "paused throwing dispatch should record another drop");
    flow.resume();

    axent::ControlCommand missing_route = command;
    missing_route.request_id = "req-missing-route";
    missing_route.device_id = "missing-device";
    const auto missing_route_result = broker.dispatch(missing_route);
    require_result(missing_route_result,
                   axent::ControlStatus::NotFound,
                   "route not found",
                   "missing route should be NotFound");

    broker.unregister_adapter("mock");
    const auto unavailable = broker.dispatch(command);
    require_result(unavailable,
                   axent::ControlStatus::Unavailable,
                   "adapter unavailable",
                   "unregistered adapter should be unavailable");
    broker.register_adapter(adapter);

    axent::ControlCommand firmware = command;
    firmware.request_id = "req-firmware";
    firmware.method = "firmware.update";
    firmware.params = {{"file", "/tmp/mock-fw.bin"}};
    const auto firmware_result = broker.dispatch(firmware);
    require(firmware_result.status == axent::ControlStatus::Accepted, "firmware update should be accepted");
    require(firmware_result.body.at("file") == "/tmp/mock-fw.bin", "firmware file should be routed");

    axent::ControlCommand malformed_firmware = firmware;
    malformed_firmware.request_id = "req-bad-firmware";
    malformed_firmware.params = {{"file", 12}};
    const auto malformed_result = broker.dispatch(malformed_firmware);
    require_result(malformed_result,
                   axent::ControlStatus::InvalidArgument,
                   "invalid firmware file",
                   "malformed firmware params should be InvalidArgument");

    axent::ControlCommand throwing = command;
    throwing.request_id = "req-throw";
    throwing.device_id = "throw-device-001";
    const auto throwing_result = broker.dispatch(throwing);
    require_result(throwing_result,
                   axent::ControlStatus::InternalError,
                   "adapter exploded",
                   "adapter exception should be InternalError");
    require(logger.records().back().message == "control.response", "exception path should emit response audit");
    require(logger.records().back().fields.at("status") == "internal_error", "exception status audit mismatch");

    return 0;
}
