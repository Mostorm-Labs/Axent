#include <cassert>

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
    assert(devices.list().size() == 1);
    assert(devices.get("dev-1")->adapter == "mock");
    devices.mark_offline("dev-1", "test-remove");
    assert(!devices.get("dev-1")->connection.online);

    axent::CapabilityRegistry capabilities;
    capabilities.register_core_capabilities();
    assert(capabilities.has("identity"));
    assert(capabilities.has_method("firmware.update"));
    assert(capabilities.all().size() == 9);

    axent::SessionManager sessions;
    const auto control = sessions.control().open("json-rpc");
    const auto device_session = sessions.device().open("dev-1", "mock");
    sessions.map_control_to_device(control, device_session);
    assert(sessions.device_session_for_control(control) == device_session);

    axent::AdapterRegistry adapters;
    adapters.register_adapter({"mock", "Mock Adapter", true, ""});
    adapters.register_adapter({"tea", "TEA Adapter", false, "SDK not loaded"});
    assert(adapters.find("mock")->available);
    assert(!adapters.find("tea")->available);
    return 0;
}
