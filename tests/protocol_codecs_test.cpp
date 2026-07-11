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

template <typename Func>
void require_no_throw(Func&& func, const char* message)
{
    try {
        func();
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(message) + ": " + error.what());
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

    const auto numeric_id_json_rpc = axent::decode_control_message({
        {"jsonrpc", "2.0"},
        {"id", 7},
        {"method", "status.get"},
        {"params", {{"deviceId", "mock-device-001"}}}
    });
    const auto numeric_id_response = axent::encode_control_response(numeric_id_json_rpc, result);
    require(numeric_id_response.at("id") == 7, "json-rpc numeric response id mismatch");

    axent::ControlResult error;
    error.status = axent::ControlStatus::InvalidArgument;
    error.body = {{"detail", "bad params"}};
    const auto json_error = axent::encode_control_response(json_rpc, error);
    require(json_error.at("jsonrpc") == "2.0", "json-rpc error version mismatch");
    require(json_error.at("id") == "jr-1", "json-rpc error id mismatch");
    require(json_error.at("error").at("code") == -32602, "json-rpc error code mismatch");
    require(json_error.at("error").at("message") == "invalid_argument", "json-rpc error message mismatch");
    require(json_error.at("error").at("data").at("detail") == "bad params", "json-rpc error data mismatch");

    axent::ControlResult busy_error;
    busy_error.status = axent::ControlStatus::Busy;
    const auto json_busy = axent::encode_control_response(json_rpc, busy_error);
    require(json_busy.at("error").at("code") == -32005,
            "json-rpc Busy must have a stable code distinct from NotFound");
    require(json_busy.at("error").at("message") == "busy", "json-rpc Busy message mismatch");

    const auto legacy_error = axent::encode_control_response(legacy, error);
    require(legacy_error.at("d").at("status").at("result") == false, "legacy error result mismatch");
    require(legacy_error.at("d").at("status").at("code") == 500, "legacy error code mismatch");
    require(legacy_error.at("d").at("status").at("comment") == "invalid_argument", "legacy error comment mismatch");

    require_no_throw([] {
        const auto malformed_json_rpc = axent::decode_control_message({
            {"jsonrpc", "2.0"},
            {"id", 12},
            {"method", {"status.get"}},
            {"params", {{"deviceId", 42}}}
        });
        require(malformed_json_rpc.command.source == axent::ProtocolSource::JsonRpc,
                "malformed json-rpc source mismatch");
        require(malformed_json_rpc.command.request_id == "12", "numeric json-rpc id should be available for logs");
        require(malformed_json_rpc.command.method.empty(), "malformed json-rpc method should default empty");
        require(malformed_json_rpc.command.device_id.empty(), "malformed json-rpc device id should default empty");
        require(malformed_json_rpc.command.params.is_object(), "malformed json-rpc params should stay object");
    }, "malformed json-rpc decode should not throw");

    require_no_throw([] {
        const auto scalar_params_json_rpc = axent::decode_control_message({
            {"jsonrpc", "2.0"},
            {"id", "jr-2"},
            {"method", "status.get"},
            {"params", "not-object"}
        });
        require(scalar_params_json_rpc.command.request_id == "jr-2", "scalar params json-rpc id mismatch");
        require(scalar_params_json_rpc.command.method == "status.get", "scalar params json-rpc method mismatch");
        require(scalar_params_json_rpc.command.device_id.empty(), "scalar params json-rpc device id should default empty");
        require(scalar_params_json_rpc.command.params.is_object(), "scalar params json-rpc params should default object");
        require(scalar_params_json_rpc.command.params.empty(), "scalar params json-rpc params should be empty");
    }, "scalar params json-rpc decode should not throw");

    require_no_throw([] {
        const auto malformed_legacy = axent::decode_control_message({
            {"op", 7},
            {"sid", "not-int"},
            {"d", "not-object"}
        });
        require(malformed_legacy.command.source == axent::ProtocolSource::LegacyOp,
                "malformed legacy source mismatch");
        require(malformed_legacy.command.request_id.empty(), "malformed legacy id should default empty");
        require(malformed_legacy.command.method.empty(), "malformed legacy method should default empty");
        require(malformed_legacy.command.device_id.empty(), "malformed legacy device id should default empty");
        require(malformed_legacy.command.params.is_object(), "malformed legacy params should default object");
        require(malformed_legacy.wire_method.empty(), "malformed legacy wire method should default empty");
    }, "malformed legacy d decode should not throw");

    require_no_throw([] {
        const auto malformed_legacy_params = axent::decode_control_message({
            {"op", 7},
            {"d", {
                {"id", 99},
                {"method", 42},
                {"params", {{"serialNumber", 123}, {"deviceId", false}}}
            }}
        });
        require(malformed_legacy_params.command.source == axent::ProtocolSource::LegacyOp,
                "malformed legacy params source mismatch");
        require(malformed_legacy_params.command.request_id.empty(), "malformed legacy params id should default empty");
        require(malformed_legacy_params.command.method.empty(), "malformed legacy params method should default empty");
        require(malformed_legacy_params.command.device_id.empty(),
                "malformed legacy params device id should default empty");
        require(malformed_legacy_params.command.params.is_object(), "malformed legacy params should stay object");
    }, "malformed legacy params decode should not throw");

    require_no_throw([&] {
        axent::DecodedControlMessage malformed_original = legacy;
        malformed_original.original = {{"op", 7}, {"sid", {"not-int"}}};
        const auto response = axent::encode_control_response(malformed_original, result);
        require(response.at("sid") == 0, "malformed legacy response sid should default zero");
    }, "malformed legacy response encode should not throw");

    return 0;
}
