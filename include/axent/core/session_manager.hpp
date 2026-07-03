#pragma once

#include <map>
#include <optional>
#include <string>

namespace axent {

struct ControlSession {
    std::string id;
    std::string protocol;
};

struct DeviceSession {
    std::string id;
    std::string device_id;
    std::string adapter;
};

class ControlSessionManager {
public:
    std::string open(const std::string& protocol);
    std::optional<ControlSession> get(const std::string& id) const;

private:
    int next_id_ = 1;
    std::map<std::string, ControlSession> sessions_;
};

class DeviceSessionManager {
public:
    std::string open(const std::string& device_id, const std::string& adapter);
    std::optional<DeviceSession> get(const std::string& id) const;
    void close(const std::string& id);

private:
    int next_id_ = 1;
    std::map<std::string, DeviceSession> sessions_;
};

class SessionManager {
public:
    ControlSessionManager& control();
    DeviceSessionManager& device();
    void map_control_to_device(std::string control_session_id, std::string device_session_id);
    std::optional<std::string> device_session_for_control(const std::string& control_session_id) const;
    void close_device_session(const std::string& device_session_id);

private:
    ControlSessionManager control_;
    DeviceSessionManager device_;
    std::map<std::string, std::string> control_to_device_;
};

} // namespace axent
