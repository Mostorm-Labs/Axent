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

} // namespace

DecodedControlMessage decode_control_message(const nlohmann::json& message)
{
    DecodedControlMessage decoded;
    decoded.original = message;

    if (message.contains("jsonrpc")) {
        decoded.command.source = ProtocolSource::JsonRpc;
        decoded.command.request_id = message.value("id", "");
        decoded.command.method = message.value("method", "");
        decoded.command.params = message.value("params", nlohmann::json::object());
        decoded.command.device_id = decoded.command.params.value("deviceId", "");
        decoded.wire_method = decoded.command.method;
        return decoded;
    }

    decoded.command.source = ProtocolSource::LegacyOp;
    const auto d = message.value("d", nlohmann::json::object());
    decoded.command.request_id = d.value("id", "");
    decoded.wire_method = d.value("method", "");
    decoded.command.method = map_legacy_method(decoded.wire_method);
    decoded.command.params = d.value("params", nlohmann::json::object());
    decoded.command.device_id =
        decoded.command.params.value("serialNumber", decoded.command.params.value("deviceId", ""));
    return decoded;
}

nlohmann::json encode_control_response(const DecodedControlMessage& decoded, const ControlResult& result)
{
    if (decoded.command.source == ProtocolSource::JsonRpc) {
        if (is_success(result.status)) {
            return {
                {"jsonrpc", "2.0"},
                {"id", decoded.command.request_id},
                {"result", result.body},
            };
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", decoded.command.request_id},
            {"error", {
                {"message", control_status_name(result.status)},
                {"data", result.body},
            }},
        };
    }

    return {
        {"op", 8},
        {"sid", decoded.original.value("sid", 0)},
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
