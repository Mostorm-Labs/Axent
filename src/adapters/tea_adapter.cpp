#include "axent/adapters/tea_adapter.hpp"

namespace axent {

AdapterMetadata TeaAdapter::metadata() const
{
    return {"tea", "TEA Adapter", false, "TEA SDK wrapper skeleton only"};
}

std::vector<Capability> TeaAdapter::capabilities() const
{
    return {
        {"tea.device",
         "tea",
         false,
         "TEA SDK wrapper skeleton only",
         {{"status.get", RiskLevel::Safe, false, false},
          {"audio.mute.get", RiskLevel::Safe, false, false},
          {"firmware.update", RiskLevel::Dangerous, true, true}},
         {"tea.device.changed"}},
    };
}

std::vector<DeviceSnapshot> TeaAdapter::discover()
{
    return {};
}

ControlResult TeaAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "TEA adapter skeleton only"}}};
}

ControlResult TeaAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "TEA firmware update skeleton only"}}};
}

} // namespace axent
