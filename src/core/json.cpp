#include "axent/core/json.hpp"

namespace axent {

const char* risk_name(RiskLevel risk)
{
    switch (risk) {
    case RiskLevel::Safe: return "safe";
    case RiskLevel::Confirm: return "confirm";
    case RiskLevel::Dangerous: return "dangerous";
    }
    return "dangerous";
}

const char* protocol_source_name(ProtocolSource source)
{
    switch (source) {
    case ProtocolSource::JsonRpc: return "json-rpc";
    case ProtocolSource::LegacyOp: return "legacy-op";
    case ProtocolSource::LocalCli: return "local-cli";
    }
    return "json-rpc";
}

const char* control_status_name(ControlStatus status)
{
    switch (status) {
    case ControlStatus::Ok: return "ok";
    case ControlStatus::Accepted: return "accepted";
    case ControlStatus::NotFound: return "not_found";
    case ControlStatus::Forbidden: return "forbidden";
    case ControlStatus::InvalidArgument: return "invalid_argument";
    case ControlStatus::Busy: return "busy";
    case ControlStatus::Unavailable: return "unavailable";
    case ControlStatus::InternalError: return "internal_error";
    }
    return "internal_error";
}

nlohmann::json to_json(const DeviceSnapshot& device)
{
    return {
        {"id", device.id},
        {"adapter", device.adapter},
        {"identity", {
            {"vendor", device.identity.vendor},
            {"model", device.identity.model},
            {"serialNumber", device.identity.serial_number},
            {"firmwareVersion", device.identity.firmware_version},
            {"hardwareVersion", device.identity.hardware_version}
        }},
        {"connection", {
            {"online", device.connection.online},
            {"transport", device.connection.transport},
            {"lastChangeReason", device.connection.last_change_reason}
        }},
        {"status", {{"health", device.status.health}}}
    };
}

nlohmann::json to_json(const Capability& capability)
{
    nlohmann::json methods = nlohmann::json::array();
    for (const auto& method : capability.methods) {
        methods.push_back({
            {"name", method.name},
            {"risk", risk_name(method.risk)},
            {"async", method.async},
            {"requiresConfirmation", method.requires_confirmation}
        });
    }
    return {
        {"name", capability.name},
        {"domain", capability.domain},
        {"available", capability.available},
        {"unavailableReason", capability.unavailable_reason},
        {"methods", methods},
        {"events", capability.events}
    };
}

nlohmann::json to_json(const ControlResult& result)
{
    return {
        {"status", control_status_name(result.status)},
        {"body", result.body}
    };
}

} // namespace axent
