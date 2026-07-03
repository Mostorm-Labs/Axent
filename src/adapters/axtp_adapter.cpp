#include "axent/adapters/axtp_adapter.hpp"

#include "axtp_runtime.hpp"
#include "axtp_sdk.hpp"

namespace axent {

AdapterMetadata AxtpAdapter::metadata() const
{
    return {"axtp", "AXTP Runtime Adapter", true, ""};
}

std::vector<Capability> AxtpAdapter::capabilities() const
{
    return {
        {"axtp.runtime",
         "axtp",
         true,
         "",
         {{"status.get", RiskLevel::Safe, false, false},
          {"stream.flowControl.get", RiskLevel::Safe, false, false},
          {"firmware.update", RiskLevel::Dangerous, true, true}},
         {"axtp.session.changed"}},
    };
}

std::vector<DeviceSnapshot> AxtpAdapter::discover()
{
    return {};
}

ControlResult AxtpAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "AXTP device session not open"}}};
}

ControlResult AxtpAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "AXTP firmware update skeleton only"}}};
}

} // namespace axent
