#include "axent/core/route_manager.hpp"

namespace axent {

RouteManager::RouteManager(const DeviceManager& devices)
    : devices_(devices)
{
}

std::optional<RouteTarget> RouteManager::resolve(const std::string& destination) const
{
    if (const auto endpoint = resolve_endpoint(destination)) {
        return endpoint;
    }
    return resolve_device(destination);
}

std::optional<RouteTarget> RouteManager::resolve_endpoint(const std::string& endpoint_id) const
{
    if (endpoint_id.empty()) {
        return std::nullopt;
    }
    const auto devices = devices_.list();
    auto found = devices.end();
    for (auto current = devices.begin(); current != devices.end(); ++current) {
        if (current->endpoint_id != endpoint_id) {
            continue;
        }
        // Endpoint IDs are routing identities.  Ambiguity must fail closed
        // instead of selecting an arbitrary physical device.
        if (found != devices.end()) {
            return std::nullopt;
        }
        found = current;
    }
    if (found == devices.end() || !found->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{found->adapter, found->id, found->endpoint_id};
}

std::optional<RouteTarget> RouteManager::resolve_device(const std::string& device_id) const
{
    auto device = devices_.get(device_id);
    if (!device) {
        device = devices_.find_by_serial_number(device_id);
    }
    if (!device || !device->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{device->adapter, device->id, device->endpoint_id};
}

std::vector<DeviceSnapshot> RouteManager::list_devices() const
{
    return devices_.list();
}

} // namespace axent
