#include "axent/adapters/mock_adapter.hpp"

#include <sstream>

namespace axent {

AdapterMetadata MockAdapter::metadata() const
{
    return {"mock", "Mock Adapter", true, ""};
}

std::vector<Capability> MockAdapter::capabilities() const
{
    return {
        {"mock.identity", "mock", true, "", {{"identity.get", RiskLevel::Safe, false, false}}, {"identity.changed"}},
        {"mock.status", "mock", true, "", {{"status.get", RiskLevel::Safe, false, false}}, {"status.changed"}},
        {"mock.firmware", "mock", true, "", {{"firmware.update", RiskLevel::Dangerous, true, true}}, {"firmware.progress"}},
        {"mock.stream",
         "mock",
         true,
         "",
         {{"stream.flowControl.get", RiskLevel::Safe, false, false}},
         {"stream.flowControl.changed"}},
    };
}

std::vector<DeviceSnapshot> MockAdapter::discover()
{
    DeviceSnapshot device;
    device.id = "mock-device-001";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK001";
    device.identity.firmware_version = "mock-fw-1.0.0";
    device.identity.hardware_version = "mock-hw-revA";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.connection.last_change_reason = "mock-discovered";
    device.status.health = "ok";
    return {device};
}

ControlResult MockAdapter::call(const std::string& device_id, const std::string& method, const nlohmann::json&)
{
    if (device_id != "mock-device-001") {
        return {ControlStatus::NotFound, {{"error", "device not found"}}};
    }
    if (method == "status.get") {
        return {ControlStatus::Ok, {{"health", "ok"}}};
    }
    if (method == "identity.get") {
        return {ControlStatus::Ok, {{"serialNumber", "MOCK001"}, {"model", "MockCam"}}};
    }
    if (method == "stream.flowControl.get") {
        return {ControlStatus::Ok, {{"paused", false}, {"dropped", 0}}};
    }
    return {ControlStatus::NotFound, {{"error", "method not found"}}};
}

ControlResult MockAdapter::start_firmware_update(const std::string& device_id, const std::string& file_path)
{
    if (device_id != "mock-device-001") {
        return {ControlStatus::NotFound, {{"error", "device not found"}}};
    }

    std::ostringstream task_id;
    task_id << "fw-mock-" << next_task_id_++;
    return {ControlStatus::Accepted, {{"taskId", task_id.str()}, {"file", file_path}, {"state", "queued"}}};
}

} // namespace axent
