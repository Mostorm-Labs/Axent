#include <stdexcept>
#include <string>

#include "axent/core/json.hpp"
#include "axent/core/types.hpp"
#include "test_json.hpp"

int main()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-001";
    device.endpoint_id = "ep_20d54d9fc87018d571995be978620d21";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK001";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.status.health = "ok";

    assert_json_eq(axent::to_json(device), {
        {"id", "mock-device-001"},
        {"endpointId", "ep_20d54d9fc87018d571995be978620d21"},
        {"adapter", "mock"},
        {"identity", {
            {"vendor", "Mostorm"},
            {"model", "MockCam"},
            {"serialNumber", "MOCK001"},
            {"firmwareVersion", ""},
            {"hardwareVersion", ""}
        }},
        {"connection", {
            {"online", true},
            {"transport", "mock"},
            {"lastChangeReason", ""}
        }},
        {"status", {{"health", "ok"}}}
    });

    auto unbound = device;
    unbound.endpoint_id.clear();
    if (axent::to_json(unbound).contains("endpointId")) {
        throw std::runtime_error("unbound device JSON must omit endpointId");
    }

    axent::Capability capability;
    capability.name = "firmware";
    capability.domain = "core";
    capability.available = true;
    capability.methods.push_back({"firmware.update", axent::RiskLevel::Dangerous, true, true});
    if (axent::risk_name(axent::RiskLevel::Dangerous) != std::string("dangerous")) {
        throw std::runtime_error("dangerous risk name mismatch");
    }
    if (axent::risk_name(static_cast<axent::RiskLevel>(999)) != std::string("dangerous")) {
        throw std::runtime_error("invalid risk level must fail closed to dangerous");
    }
    if (axent::to_json(capability).at("methods").at(0).at("name") != "firmware.update") {
        throw std::runtime_error("capability method name mismatch");
    }
    return 0;
}
