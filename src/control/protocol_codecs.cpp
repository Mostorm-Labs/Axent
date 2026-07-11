#include "axent/control/protocol_codecs.hpp"

#include <map>

#include "axent/core/json.hpp"

namespace axent {
namespace {

std::string map_legacy_method(const std::string& method)
{
    static const std::map<std::string, std::string> mapping = {
        {"GetDeviceList", "devices.list"},
        {"GetDeviceInfo", "device.info"},
        {"StartDeviceUpgrade", "firmware.update"},
        {"StartAgentUpgrade", "agent.update"},
        {"StartDeviceReboot", "control.reboot"},
        {"StartAgentReboot", "agent.reboot"},
        {"SetDeviceMute", "audio.mute.set"},
        {"GetDeviceMute", "audio.mute.get"},
        {"SetDeviceVolume", "audio.volume.set"},
        {"GetDeviceVolume", "audio.volume.get"},
    };
    const auto found = mapping.find(method);
    return found == mapping.end() ? method : found->second;
}

bool is_success(ControlStatus status)
{
    return status == ControlStatus::Ok || status == ControlStatus::Accepted;
}

std::string optional_string(const nlohmann::json& object, const char* key)
{
    if (!object.is_object()) {
        return "";
    }
    const auto found = object.find(key);
    if (found == object.end() || !found->is_string()) {
        return "";
    }
    return found->get<std::string>();
}

std::string request_id_for_log(const nlohmann::json& value)
{
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        return value.dump();
    }
    return "";
}

nlohmann::json object_or_empty(const nlohmann::json& object, const char* key)
{
    if (!object.is_object()) {
        return nlohmann::json::object();
    }
    const auto found = object.find(key);
    if (found == object.end() || !found->is_object()) {
        return nlohmann::json::object();
    }
    return *found;
}

int int_or_default(const nlohmann::json& object, const char* key, int default_value)
{
    if (!object.is_object()) {
        return default_value;
    }
    const auto found = object.find(key);
    if (found == object.end() || !found->is_number_integer()) {
        return default_value;
    }
    return found->get<int>();
}

int json_rpc_error_code(ControlStatus status)
{
    switch (status) {
    case ControlStatus::Ok: return 0;
    case ControlStatus::Accepted: return 1;
    case ControlStatus::NotFound: return -32004;
    case ControlStatus::Forbidden: return -32003;
    case ControlStatus::InvalidArgument: return -32602;
    case ControlStatus::Busy: return -32005;
    case ControlStatus::Unavailable: return -32001;
    case ControlStatus::InternalError: return -32603;
    }
    return -32603;
}

} // namespace

DecodedControlMessage decode_control_message(const nlohmann::json& message)
{
    DecodedControlMessage decoded;
    decoded.original = message;

    if (message.contains("jsonrpc")) {
        decoded.command.source = ProtocolSource::JsonRpc;
        decoded.json_rpc_id = message.contains("id") ? message.at("id") : nullptr;
        decoded.command.request_id = request_id_for_log(decoded.json_rpc_id);
        decoded.command.method = optional_string(message, "method");
        decoded.command.params = object_or_empty(message, "params");
        decoded.command.device_id = optional_string(decoded.command.params, "deviceId");
        decoded.wire_method = decoded.command.method;
        return decoded;
    }

    decoded.command.source = ProtocolSource::LegacyOp;
    const auto d = object_or_empty(message, "d");
    decoded.command.request_id = optional_string(d, "id");
    decoded.wire_method = optional_string(d, "method");
    decoded.command.method = map_legacy_method(decoded.wire_method);
    decoded.command.params = object_or_empty(d, "params");
    decoded.command.device_id = optional_string(decoded.command.params, "serialNumber");
    if (decoded.command.device_id.empty()) {
        decoded.command.device_id = optional_string(decoded.command.params, "deviceId");
    }
    return decoded;
}

nlohmann::json encode_control_response(const DecodedControlMessage& decoded, const ControlResult& result)
{
    if (decoded.command.source == ProtocolSource::JsonRpc) {
        if (is_success(result.status)) {
            return {
                {"jsonrpc", "2.0"},
                {"id", decoded.json_rpc_id},
                {"result", result.body},
            };
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", decoded.json_rpc_id},
            {"error", {
                {"code", json_rpc_error_code(result.status)},
                {"message", control_status_name(result.status)},
                {"data", result.body},
            }},
        };
    }

    return {
        {"op", 8},
        {"sid", int_or_default(decoded.original, "sid", 0)},
        {"d", {
            {"id", decoded.command.request_id},
            {"method", decoded.wire_method},
            {"status", {
                {"result", is_success(result.status)},
                {"code", is_success(result.status) ? 100 : 500},
                {"comment", control_status_name(result.status)},
            }},
            {"result", result.body},
        }},
    };
}

} // namespace axent
