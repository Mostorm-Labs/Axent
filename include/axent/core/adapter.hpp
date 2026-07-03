#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/core/adapter_registry.hpp"
#include "axent/core/types.hpp"

namespace axent {

class Adapter {
public:
    virtual ~Adapter() = default;

    virtual AdapterMetadata metadata() const = 0;
    virtual std::vector<Capability> capabilities() const = 0;
    virtual std::vector<DeviceSnapshot> discover() = 0;
    virtual ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) = 0;
    virtual ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) = 0;
};

} // namespace axent
