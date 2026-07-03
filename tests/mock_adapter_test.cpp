#include <stdexcept>
#include <string>

#include "axent/adapters/mock_adapter.hpp"

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

    const auto metadata = adapter.metadata();
    require(metadata.name == "mock", "adapter name mismatch");
    require(metadata.display_name == "Mock Adapter", "adapter display name mismatch");
    require(metadata.available, "adapter should be available");
    require(metadata.unavailable_reason.empty(), "adapter unavailable reason should be empty");

    const auto capabilities = adapter.capabilities();
    require(capabilities.size() == 4, "mock capability count mismatch");
    require(capabilities[0].name == "mock.identity", "identity capability name mismatch");
    require(capabilities[0].methods.size() == 1, "identity method count mismatch");
    require(capabilities[0].methods[0].name == "identity.get", "identity method name mismatch");
    require(capabilities[1].methods[0].name == "status.get", "status method name mismatch");
    require(capabilities[2].methods[0].name == "firmware.update", "firmware method name mismatch");
    require(capabilities[2].methods[0].risk == axent::RiskLevel::Dangerous, "firmware risk mismatch");
    require(capabilities[2].methods[0].async, "firmware method should be async");
    require(capabilities[2].methods[0].requires_confirmation, "firmware method should require confirmation");
    require(capabilities[3].methods[0].name == "stream.flowControl.get", "stream method name mismatch");

    const auto devices = adapter.discover();
    require(devices.size() == 1, "mock device count mismatch");
    require(devices[0].id == "mock-device-001", "mock device id mismatch");
    require(devices[0].adapter == "mock", "mock device adapter mismatch");
    require(devices[0].identity.vendor == "Mostorm", "mock device vendor mismatch");
    require(devices[0].identity.model == "MockCam", "mock device model mismatch");
    require(devices[0].identity.serial_number == "MOCK001", "mock device serial mismatch");
    require(devices[0].connection.online, "mock device should be online");
    require(devices[0].connection.transport == "mock", "mock device transport mismatch");
    require(devices[0].status.health == "ok", "mock device health mismatch");

    const auto status = adapter.call("mock-device-001", "status.get", {});
    require(status.status == axent::ControlStatus::Ok, "status.get should succeed");
    require(status.body.at("health") == "ok", "status.get health mismatch");

    const auto identity = adapter.call("mock-device-001", "identity.get", {});
    require(identity.status == axent::ControlStatus::Ok, "identity.get should succeed");
    require(identity.body.at("serialNumber") == "MOCK001", "identity serial mismatch");
    require(identity.body.at("model") == "MockCam", "identity model mismatch");

    const auto flow_control = adapter.call("mock-device-001", "stream.flowControl.get", {});
    require(flow_control.status == axent::ControlStatus::Ok, "stream.flowControl.get should succeed");
    require(flow_control.body.at("paused") == false, "stream paused mismatch");
    require(flow_control.body.at("dropped") == 0, "stream dropped mismatch");

    const auto missing_method = adapter.call("mock-device-001", "missing.method", {});
    require(missing_method.status == axent::ControlStatus::NotFound, "missing method should be NotFound");
    require(missing_method.body.at("error") == "method not found", "missing method error mismatch");

    const auto missing_device = adapter.call("missing-device", "status.get", {});
    require(missing_device.status == axent::ControlStatus::NotFound, "missing device should be NotFound");
    require(missing_device.body.at("error") == "device not found", "missing device error mismatch");

    const auto firmware = adapter.start_firmware_update("mock-device-001", "/tmp/mock.bin");
    require(firmware.status == axent::ControlStatus::Accepted, "firmware update should be accepted");
    require(firmware.body.at("taskId").get<std::string>().find("fw-mock-") == 0, "firmware task id prefix mismatch");
    require(firmware.body.at("file") == "/tmp/mock.bin", "firmware file mismatch");
    require(firmware.body.at("state") == "queued", "firmware state mismatch");

    const auto second_firmware = adapter.start_firmware_update("mock-device-001", "/tmp/mock2.bin");
    require(second_firmware.body.at("taskId") != firmware.body.at("taskId"), "firmware task ids should be unique");

    const auto missing_firmware_device = adapter.start_firmware_update("missing-device", "/tmp/mock.bin");
    require(missing_firmware_device.status == axent::ControlStatus::NotFound, "missing firmware device should be NotFound");
    require(missing_firmware_device.body.at("error") == "device not found", "missing firmware device error mismatch");

    return 0;
}
