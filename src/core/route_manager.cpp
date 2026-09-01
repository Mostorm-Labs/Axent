#include "axent/core/route_manager.hpp"

namespace axent {

RouteManager::RouteManager(const DeviceManager& devices)
    : devices_(devices)
{
}

std::optional<RouteTarget> RouteManager::resolve(const std::string& destination) const
{
    const auto endpoint = resolve_endpoint_route(destination);
    if (endpoint.status == RouteResolutionStatus::Found) {
        return endpoint.target;
    }
    if (endpoint.status != RouteResolutionStatus::NotFound) {
        return std::nullopt;
    }
    return resolve_device(destination);
}

RouteResolution RouteManager::resolve_endpoint_route(const std::string& endpoint_id) const
{
    if (endpoint_id.empty()) {
        return {RouteResolutionStatus::NotFound, std::nullopt};
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
            return {RouteResolutionStatus::Conflict, std::nullopt};
        }
        found = current;
    }
    if (found == devices.end()) {
        return {RouteResolutionStatus::NotFound, std::nullopt};
    }
    if (!found->connection.online) {
        return {RouteResolutionStatus::Unavailable, std::nullopt};
    }
    return {RouteResolutionStatus::Found,
            RouteTarget{found->adapter,
                        found->id,
                        found->endpoint_id,
                        found->endpoint_delivery_mode}};
}

std::optional<RouteTarget> RouteManager::resolve_endpoint(const std::string& endpoint_id) const
{
    const auto resolution = resolve_endpoint_route(endpoint_id);
    if (resolution.status != RouteResolutionStatus::Found) {
        return std::nullopt;
    }
    return resolution.target;
}

std::optional<RouteTarget> RouteManager::resolve_device(const std::string& device_id) const
{
    if (device_id.empty()) {
        return std::nullopt;
    }
    const auto devices = devices_.list();
    auto found = devices.end();
    for (auto current = devices.begin(); current != devices.end(); ++current) {
        if (current->id != device_id &&
            current->identity.serial_number != device_id) {
            continue;
        }
        if (found != devices.end()) {
            return std::nullopt;
        }
        found = current;
    }
    if (found == devices.end() || !found->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{found->adapter,
                       found->id,
                       found->endpoint_id,
                       found->endpoint_delivery_mode};
}

std::optional<RouteTarget> RouteManager::resolve_device(
    const std::string& selector,
    DeviceSelectorKind selector_kind) const
{
    if (selector_kind == DeviceSelectorKind::Unspecified) {
        return resolve_device(selector);
    }
    const auto device = selector_kind == DeviceSelectorKind::ProviderLocalId
        ? devices_.get(selector)
        : devices_.find_by_serial_number(selector);
    if (!device || !device->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{device->adapter,
                       device->id,
                       device->endpoint_id,
                       device->endpoint_delivery_mode};
}

std::vector<DeviceSnapshot> RouteManager::list_devices() const
{
    return devices_.list();
}

} // namespace axent
