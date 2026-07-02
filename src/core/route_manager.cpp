#include "axent/core/route_manager.hpp"

namespace axent {

RouteManager::RouteManager(const DeviceManager& devices)
    : devices_(devices)
{
}

std::optional<RouteTarget> RouteManager::resolve(const std::string& device_id) const
{
    const auto device = devices_.get(device_id);
    if (!device || !device->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{device->adapter, device->id};
}

} // namespace axent
