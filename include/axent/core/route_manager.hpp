#pragma once

#include <optional>
#include <string>

#include "axent/core/device_manager.hpp"

namespace axent {

struct RouteTarget {
    std::string adapter;
    std::string device_id;
};

class RouteManager {
public:
    explicit RouteManager(const DeviceManager& devices);

    std::optional<RouteTarget> resolve(const std::string& device_id) const;

private:
    const DeviceManager& devices_;
};

} // namespace axent
