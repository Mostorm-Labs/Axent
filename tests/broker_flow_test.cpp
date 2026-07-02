#include <stdexcept>
#include <string>
#include <vector>

#include "axent/adapters/mock_adapter.hpp"
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
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }
    for (const auto& device : throwing_adapter.discover()) {
        devices.upsert(device);
    }

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(adapter);
    broker.register_adapter(throwing_adapter);

    axent::ControlCommand command;
    command.request_id = "req-1";
    command.method = "status.get";
    command.device_id = "mock-device-001";
    const auto result = broker.dispatch(command);

    require(result.status == axent::ControlStatus::Ok, "status.get should succeed");
    require(result.body.at("health") == "ok", "status.get health mismatch");
    require(logger.records().size() == 2, "broker should emit request and response audit records");
    require(logger.records()[0].channel == "audit", "request log channel mismatch");
    require(logger.records()[0].message == "control.request", "request log message mismatch");
    require(logger.records()[0].fields.at("requestId") == "req-1", "request log requestId mismatch");
    require(logger.records()[0].fields.at("method") == "status.get", "request log method mismatch");
    require(logger.records()[0].fields.at("deviceId") == "mock-device-001", "request log deviceId mismatch");
    require(logger.records()[0].fields.at("source") == "json-rpc", "request log source mismatch");
    require(logger.records()[1].channel == "audit", "response log channel mismatch");
    require(logger.records()[1].message == "control.response", "response log message mismatch");
    require(logger.records()[1].fields.at("requestId") == "req-1", "response log requestId mismatch");
    require(logger.records()[1].fields.at("method") == "status.get", "response log method mismatch");
    require(logger.records()[1].fields.at("deviceId") == "mock-device-001", "response log deviceId mismatch");
    require(logger.records()[1].fields.at("source") == "json-rpc", "response log source mismatch");
    require(logger.records()[1].fields.at("status") == "ok", "response log status mismatch");
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
