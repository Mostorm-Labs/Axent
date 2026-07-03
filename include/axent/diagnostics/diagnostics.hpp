#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

class Diagnostics {
public:
    Diagnostics(const DeviceManager& devices, const CapabilityRegistry& capabilities, const Logger& logger);

    nlohmann::json collect(const std::string& device_id, bool include_sensitive) const;

private:
    const DeviceManager& devices_;
    const CapabilityRegistry& capabilities_;
    const Logger& logger_;
};

} // namespace axent
