#include "axent/core/capability_registry.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void CapabilityRegistry::register_core_capabilities()
{
    capabilities_.clear();
    register_capability({"identity", "core", true, "", {{"identity.get", RiskLevel::Safe, false, false}}, {"identity.changed"}});
    register_capability({"connection", "core", true, "", {{"connection.get", RiskLevel::Safe, false, false}}, {"connection.changed"}});
    register_capability({"status", "core", true, "", {{"status.get", RiskLevel::Safe, false, false}}, {"status.changed"}});
    register_capability({"control", "core", true, "", {{"control.reboot", RiskLevel::Dangerous, false, true}}, {"control.completed"}});
    register_capability({"events", "core", true, "", {{"events.subscribe", RiskLevel::Safe, false, false}}, {"events.subscriptionChanged"}});
    register_capability({"stream.flowControl", "core", true, "", {{"stream.flowControl.get", RiskLevel::Safe, false, false}, {"stream.flowControl.set", RiskLevel::Confirm, false, true}}, {"stream.flowControl.changed"}});
    register_capability({"diagnostics", "core", true, "", {{"diagnostics.collect", RiskLevel::Safe, true, false}}, {"diagnostics.ready"}});
    register_capability({"config", "core", true, "", {{"config.export", RiskLevel::Safe, true, false}, {"config.import", RiskLevel::Dangerous, true, true}}, {"config.changed"}});
    register_capability({"firmware", "core", true, "", {{"firmware.update", RiskLevel::Dangerous, true, true}, {"firmware.task", RiskLevel::Safe, false, false}}, {"firmware.progress"}});
}

void CapabilityRegistry::register_capability(Capability capability)
{
    auto existing = std::find_if(capabilities_.begin(), capabilities_.end(), [&](const auto& current) {
        return current.name == capability.name;
    });
    if (existing == capabilities_.end()) {
        capabilities_.push_back(std::move(capability));
        return;
    }
    *existing = std::move(capability);
}

bool CapabilityRegistry::has(const std::string& name) const
{
    return std::any_of(capabilities_.begin(), capabilities_.end(), [&](const auto& current) {
        return current.name == name;
    });
}

bool CapabilityRegistry::has_method(const std::string& method) const
{
    return find_method(method).has_value();
}

std::optional<CapabilityMethod> CapabilityRegistry::find_method(const std::string& method) const
{
    for (const auto& capability : capabilities_) {
        for (const auto& candidate : capability.methods) {
            if (candidate.name == method) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

const std::vector<Capability>& CapabilityRegistry::all() const
{
    return capabilities_;
}

} // namespace axent
