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
    device.identity.serial_number = "SERIAL-1";
    device.connection.online = true;
    if (devices.upsert(device).status != axent::DeviceUpsertStatus::Inserted) {
        throw std::runtime_error("device insert status mismatch");
    }
    if (devices.list().size() != 1) {
        throw std::runtime_error("device list size mismatch");
    }
    if (!devices.get("dev-1") || devices.get("dev-1")->adapter != "mock") {
        throw std::runtime_error("device lookup mismatch");
    }
    if (!devices.get("dev-1")->endpoint_id.empty()) {
        throw std::runtime_error("device manager must not synthesize an endpoint");
    }

    auto bound = device;
    bound.endpoint_id = "ep_explicit";
    if (devices.upsert(bound).status != axent::DeviceUpsertStatus::EndpointBound) {
        throw std::runtime_error("explicit endpoint binding status mismatch");
    }

    auto refreshed = bound;
    refreshed.endpoint_id.clear();
    refreshed.status.health = "healthy";
    if (devices.upsert(refreshed).status != axent::DeviceUpsertStatus::Refreshed) {
        throw std::runtime_error("endpoint-preserving refresh status mismatch");
    }
    if (!devices.get("dev-1") || devices.get("dev-1")->endpoint_id != "ep_explicit") {
        throw std::runtime_error("device endpoint must remain stable across refreshes");
    }

    auto changed = bound;
    changed.endpoint_id = "ep_changed";
    if (devices.upsert(changed).status !=
        axent::DeviceUpsertStatus::EndpointChangeRejected) {
        throw std::runtime_error("endpoint change must be rejected");
    }
    if (devices.get("dev-1")->endpoint_id != "ep_explicit" ||
        devices.get("dev-1")->status.health != "healthy") {
        throw std::runtime_error("rejected endpoint change must not mutate stored state");
    }

    axent::DeviceSnapshot conflict = bound;
    conflict.id = "dev-2";
    conflict.identity.serial_number = "SERIAL-2";
    if (devices.upsert(conflict).status != axent::DeviceUpsertStatus::EndpointConflict) {
        throw std::runtime_error("duplicate endpoint must be rejected");
    }
    if (devices.get("dev-2").has_value() || devices.list().size() != 1) {
        throw std::runtime_error("endpoint conflict must not insert or mutate devices");
    }
    if (!devices.find_by_serial_number("SERIAL-1") || devices.find_by_serial_number("SERIAL-1")->id != "dev-1") {
        throw std::runtime_error("device serial lookup mismatch");
    }
    devices.mark_offline("dev-1", "test-remove");
    if (!devices.get("dev-1") || devices.get("dev-1")->connection.online ||
        devices.get("dev-1")->endpoint_id != "ep_explicit") {
        throw std::runtime_error("device must be marked offline");
    }

    axent::DeviceSnapshot managed;
    managed.id = "deployment-slot-a";
    managed.adapter = "external";
    managed.endpoint_id = "ep_20d54d9fc87018d571995be978620d21";
    if (devices.upsert(managed).status != axent::DeviceUpsertStatus::Inserted ||
        devices.get(managed.id)->endpoint_id != managed.endpoint_id) {
        throw std::runtime_error("deployment-owned endpoint binding must be accepted");
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
    sessions.close_device_session(device_session);
    if (sessions.device().get(device_session).has_value()) {
        throw std::runtime_error("closed device session should not be returned");
    }
    if (sessions.device_session_for_control(control).has_value()) {
        throw std::runtime_error("closed device session should not remain mapped");
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
