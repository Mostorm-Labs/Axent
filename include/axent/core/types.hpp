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
    Forbidden,
    InvalidArgument,
    Unavailable,
    InternalError,
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
};

struct ControlResult {
    ControlStatus status = ControlStatus::Ok;
    nlohmann::json body = nlohmann::json::object();
};

const char* risk_name(RiskLevel risk);
const char* protocol_source_name(ProtocolSource source);
const char* control_status_name(ControlStatus status);

} // namespace axent
