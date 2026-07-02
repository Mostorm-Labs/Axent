#include "axent/adapters/axdp_adapter.hpp"

namespace axent {

AdapterMetadata AxdpAdapter::metadata() const
{
    return {"axdp", "AXDP Adapter", false, "real AXDP device session not connected in skeleton"};
}

std::vector<Capability> AxdpAdapter::capabilities() const
{
    return {
        {"axdp.device",
         "axdp",
         false,
         "AXDP runtime wiring pending",
         {{"status.get", RiskLevel::Safe, false, false}, {"firmware.update", RiskLevel::Dangerous, true, true}},
         {"axdp.device.changed"}},
    };
}

std::vector<DeviceSnapshot> AxdpAdapter::discover()
{
    return {};
}

ControlResult AxdpAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "AXDP adapter skeleton only"}}};
}

ControlResult AxdpAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "AXDP firmware update skeleton only"}}};
}

} // namespace axent
