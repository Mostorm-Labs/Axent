#include <stdexcept>
#include <string>

#include "axent/core/adapter_registry.hpp"
#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/session_manager.hpp"

int main()
{
    axent::DeviceManager devices;
    axent::DeviceSnapshot device;
    device.id = "dev-1";
    device.adapter = "mock";
    device.connection.online = true;
    devices.upsert(device);
    if (devices.list().size() != 1) {
        throw std::runtime_error("device list size mismatch");
    }
    if (!devices.get("dev-1") || devices.get("dev-1")->adapter != "mock") {
        throw std::runtime_error("device lookup mismatch");
    }
    devices.mark_offline("dev-1", "test-remove");
    if (!devices.get("dev-1") || devices.get("dev-1")->connection.online) {
        throw std::runtime_error("device must be marked offline");
    }

    axent::CapabilityRegistry capabilities;
    capabilities.register_core_capabilities();
    if (!capabilities.has("identity")) {
        throw std::runtime_error("identity capability missing");
    }
    if (!capabilities.has_method("firmware.update")) {
        throw std::runtime_error("firmware.update method missing");
    }
    if (capabilities.all().size() != 9) {
        throw std::runtime_error("core capability count mismatch");
    }

    axent::SessionManager sessions;
    const auto control = sessions.control().open("json-rpc");
    const auto device_session = sessions.device().open("dev-1", "mock");
    sessions.map_control_to_device(control, device_session);
    const auto mapped_session = sessions.device_session_for_control(control);
    if (!mapped_session || *mapped_session != device_session) {
        throw std::runtime_error("mapped control session mismatch");
    }
    if (sessions.device_session_for_control("ctrl-missing").has_value()) {
        throw std::runtime_error("missing control session should not map to a device session");
    }

    axent::AdapterRegistry adapters;
    adapters.register_adapter({"mock", "Mock Adapter", true, ""});
    adapters.register_adapter({"tea", "TEA Adapter", false, "SDK not loaded"});
    if (!adapters.find("mock") || !adapters.find("mock")->available) {
        throw std::runtime_error("mock adapter should be available");
    }
    if (!adapters.find("tea") || adapters.find("tea")->available) {
        throw std::runtime_error("tea adapter should be unavailable");
    }
    return 0;
}
