#include <stdexcept>
#include <string>

#include "axent/control/protocol_codecs.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_eq(const nlohmann::json& actual, const nlohmann::json& expected, const char* message)
{
    if (actual != expected) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    const auto json_rpc = axent::decode_control_message({
        {"jsonrpc", "2.0"},
        {"id", "jr-1"},
        {"method", "status.get"},
        {"params", {{"deviceId", "mock-device-001"}}}
    });
    require(json_rpc.command.request_id == "jr-1", "json-rpc request id mismatch");
    require(json_rpc.command.method == "status.get", "json-rpc method mismatch");
    require(json_rpc.command.device_id == "mock-device-001", "json-rpc device id mismatch");
    require(json_rpc.command.source == axent::ProtocolSource::JsonRpc, "json-rpc source mismatch");
    require(json_rpc.wire_method == "status.get", "json-rpc wire method mismatch");
    require_eq(json_rpc.command.params, {{"deviceId", "mock-device-001"}}, "json-rpc params mismatch");

    const auto legacy = axent::decode_control_message({
        {"op", 7},
        {"sid", 123},
        {"d", {
            {"id", "legacy-1"},
            {"method", "GetDeviceList"},
            {"params", {{"serialNumber", "MOCK001"}}}
        }}
    });
    require(legacy.command.request_id == "legacy-1", "legacy request id mismatch");
    require(legacy.command.method == "devices.list", "legacy method mapping mismatch");
    require(legacy.command.device_id == "MOCK001", "legacy serialNumber device id mismatch");
    require(legacy.command.source == axent::ProtocolSource::LegacyOp, "legacy source mismatch");
    require(legacy.wire_method == "GetDeviceList", "legacy wire method mismatch");

    const auto legacy_device_id = axent::decode_control_message({
        {"op", 7},
        {"d", {
            {"id", "legacy-2"},
            {"method", "GetDeviceInfo"},
            {"params", {{"deviceId", "mock-device-001"}}}
        }}
    });
    require(legacy_device_id.command.method == "device.info", "legacy GetDeviceInfo mapping mismatch");
    require(legacy_device_id.command.device_id == "mock-device-001", "legacy deviceId fallback mismatch");

    axent::ControlResult result;
    result.status = axent::ControlStatus::Ok;
    result.body = {{"devices", nlohmann::json::array()}};

    const auto legacy_response = axent::encode_control_response(legacy, result);
    require(legacy_response.at("op") == 8, "legacy response op mismatch");
    require(legacy_response.at("sid") == 123, "legacy response sid mismatch");
    require(legacy_response.at("d").at("id") == "legacy-1", "legacy response id mismatch");
    require(legacy_response.at("d").at("method") == "GetDeviceList", "legacy response wire method mismatch");
    require(legacy_response.at("d").at("status").at("result") == true, "legacy response status result mismatch");
    require(legacy_response.at("d").at("status").at("code") == 100, "legacy response status code mismatch");
    require(legacy_response.at("d").at("status").at("comment") == "ok", "legacy response status comment mismatch");
    require(legacy_response.at("d").at("result").at("devices").is_array(), "legacy response body mismatch");

    const auto json_response = axent::encode_control_response(json_rpc, result);
    require(json_response.at("jsonrpc") == "2.0", "json-rpc response version mismatch");
    require(json_response.at("id") == "jr-1", "json-rpc response id mismatch");
    require(json_response.at("result").at("devices").is_array(), "json-rpc response result mismatch");

    axent::ControlResult error;
    error.status = axent::ControlStatus::InvalidArgument;
    error.body = {{"detail", "bad params"}};
    const auto json_error = axent::encode_control_response(json_rpc, error);
    require(json_error.at("jsonrpc") == "2.0", "json-rpc error version mismatch");
    require(json_error.at("id") == "jr-1", "json-rpc error id mismatch");
    require(json_error.at("error").at("message") == "invalid_argument", "json-rpc error message mismatch");
    require(json_error.at("error").at("data").at("detail") == "bad params", "json-rpc error data mismatch");

    const auto legacy_error = axent::encode_control_response(legacy, error);
    require(legacy_error.at("d").at("status").at("result") == false, "legacy error result mismatch");
    require(legacy_error.at("d").at("status").at("code") == 500, "legacy error code mismatch");
    require(legacy_error.at("d").at("status").at("comment") == "invalid_argument", "legacy error comment mismatch");

    return 0;
}
