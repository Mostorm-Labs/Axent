#pragma once

#include <nlohmann/json.hpp>

#include "axent/core/types.hpp"

namespace axent {

nlohmann::json to_json(const DeviceSnapshot& device);
nlohmann::json to_json(const Capability& capability);
nlohmann::json to_json(const ControlResult& result);

} // namespace axent
