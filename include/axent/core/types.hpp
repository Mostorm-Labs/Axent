#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

enum class RiskLevel {
    Safe,
    Confirm,
    Dangerous,
};

enum class ProtocolSource {
    JsonRpc,
    LegacyOp,
    LocalCli,
};

enum class ControlStatus {
    Ok,
    Accepted,
    NotFound,
    NotSupported,
    Forbidden,
    InvalidArgument,
    Unavailable,
    InternalError,
    Busy,
};

enum class EndpointDeliveryMode {
    LocalProjection,
    NativeRelay,
};

struct DeviceIdentity {
    std::string vendor;
    std::string model;
    std::string serial_number;
    std::string firmware_version;
    std::string hardware_version;
};

struct DeviceConnection {
    bool online = false;
    std::string transport;
    std::string last_change_reason;
};

struct DeviceStatus {
    std::string health = "unknown";
};

struct DeviceSnapshot {
    std::string id;
    std::string adapter;
    DeviceIdentity identity;
    DeviceConnection connection;
    DeviceStatus status;
    // Stable logical endpoint used by control-plane routing. The Endpoint
    // owner (an adapter or deployment Host) supplies this binding only from
    // persistent identity evidence; DeviceManager never synthesizes it from a
    // provider-local ID or transport path. Kept at the end so existing
    // aggregate initializers remain source-compatible.
    std::string endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode = EndpointDeliveryMode::LocalProjection;
};

struct CapabilityMethod {
    std::string name;
    RiskLevel risk = RiskLevel::Safe;
    bool async = false;
    bool requires_confirmation = false;
};

struct Capability {
    std::string name;
    std::string domain;
    bool available = true;
    std::string unavailable_reason;
    std::vector<CapabilityMethod> methods;
    std::vector<std::string> events;
};

struct ControlCommand {
    std::string request_id;
    std::string control_session_id;
    std::string method;
    std::string device_id;
    ProtocolSource source = ProtocolSource::JsonRpc;
    nlohmann::json params = nlohmann::json::object();
    // JSON-RPC routing envelope.  These are logical endpoint names, not
    // physical device identifiers.  device_id remains as a legacy fallback.
    // Kept at the end so existing aggregate initializers remain compatible.
    std::string src;
    std::string dst;
};

struct ControlResult {
    ControlStatus status = ControlStatus::Ok;
    nlohmann::json body = nlohmann::json::object();
};

const char* risk_name(RiskLevel risk);
const char* protocol_source_name(ProtocolSource source);
const char* control_status_name(ControlStatus status);

} // namespace axent
