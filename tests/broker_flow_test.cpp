#include <stdexcept>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/logging/logger.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    axent::MockAdapter adapter;
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(&adapter);

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

    return 0;
}
