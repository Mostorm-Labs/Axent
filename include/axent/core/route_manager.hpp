#pragma once

#include <optional>
#include <string>
#include <vector>

#include "axent/core/device_manager.hpp"

namespace axent {

struct RouteTarget {
    std::string adapter;
    std::string device_id;
    std::string endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode = EndpointDeliveryMode::LocalProjection;
};

enum class RouteResolutionStatus {
    Found,
    NotFound,
    Unavailable,
    Conflict,
};

struct RouteResolution {
    RouteResolutionStatus status = RouteResolutionStatus::NotFound;
    std::optional<RouteTarget> target;
};

class RouteManager {
public:
    explicit RouteManager(const DeviceManager& devices);

    // Resolve a logical endpoint ID.  Only online devices are routable.
    RouteResolution resolve_endpoint_route(const std::string& endpoint_id) const;
    std::optional<RouteTarget> resolve_endpoint(const std::string& endpoint_id) const;
    // Resolve a legacy device ID or serial number.  Kept for existing callers.
    std::optional<RouteTarget> resolve_device(const std::string& device_id) const;
    // Resolve either an endpoint ID (preferred) or a legacy device selector.
    std::optional<RouteTarget> resolve(const std::string& destination) const;
    std::vector<DeviceSnapshot> list_devices() const;

private:
    const DeviceManager& devices_;
};

} // namespace axent
