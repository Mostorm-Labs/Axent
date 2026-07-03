#include "axent/core/route_manager.hpp"

namespace axent {

RouteManager::RouteManager(const DeviceManager& devices)
    : devices_(devices)
{
}

std::optional<RouteTarget> RouteManager::resolve(const std::string& device_id) const
{
    auto device = devices_.get(device_id);
    if (!device) {
        device = devices_.find_by_serial_number(device_id);
    }
    if (!device || !device->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{device->adapter, device->id};
}

std::vector<DeviceSnapshot> RouteManager::list_devices() const
{
    return devices_.list();
}

} // namespace axent
