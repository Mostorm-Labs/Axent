#include <cassert>
#include <string>

#include "axent/core/json.hpp"
#include "axent/core/types.hpp"
#include "test_json.hpp"

int main()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-001";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK001";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.status.health = "ok";

    assert_json_eq(axent::to_json(device), {
        {"id", "mock-device-001"},
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

    axent::Capability capability;
    capability.name = "firmware";
    capability.domain = "core";
    capability.available = true;
    capability.methods.push_back({"firmware.update", axent::RiskLevel::Dangerous, true, true});
    assert(axent::risk_name(axent::RiskLevel::Dangerous) == std::string("dangerous"));
    assert(axent::to_json(capability).at("methods").at(0).at("name") == "firmware.update");
    return 0;
}
