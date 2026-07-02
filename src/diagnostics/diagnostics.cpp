#include "axent/diagnostics/diagnostics.hpp"

#include <utility>

#include "axent/core/json.hpp"

namespace axent {

Diagnostics::Diagnostics(const DeviceManager& devices, const CapabilityRegistry& capabilities, const Logger& logger)
    : devices_(devices), capabilities_(capabilities), logger_(logger)
{
}

nlohmann::json Diagnostics::collect(const std::string& device_id, bool include_sensitive) const
{
    nlohmann::json capability_list = nlohmann::json::array();
    for (const auto& capability : capabilities_.all()) {
        capability_list.push_back(to_json(capability));
    }

    nlohmann::json audit_log = nlohmann::json::array();
    for (const auto& record : logger_.records()) {
        if (record.channel == "audit") {
            nlohmann::json audit_record = {{"message", record.message}};
            if (include_sensitive) {
                audit_record["fields"] = record.fields;
            }
            audit_log.push_back(std::move(audit_record));
        }
    }

    nlohmann::json bundle = {
        {"sanitized", !include_sensitive},
        {"deviceId", device_id},
        {"capabilities", capability_list},
        {"auditLog", audit_log},
        {"flowControl", {{"dropped", 0}, {"paused", false}}},
        {"firmwareTasks", nlohmann::json::array()},
        {"platform", {{"product", "Axent"}}},
    };

    const auto device = devices_.get(device_id);
    bundle["device"] = device ? to_json(*device) : nlohmann::json(nullptr);

    if (include_sensitive) {
        bundle["debug"] = {{"sensitiveIncluded", true}};
    }

    return bundle;
}

} // namespace axent
