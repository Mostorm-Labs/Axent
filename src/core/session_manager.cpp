#include "axent/core/session_manager.hpp"

#include <sstream>
#include <utility>

namespace axent {

std::string ControlSessionManager::open(const std::string& protocol)
{
    std::ostringstream id;
    id << "ctrl-" << next_id_++;
    sessions_[id.str()] = {id.str(), protocol};
    return id.str();
}

std::optional<ControlSession> ControlSessionManager::get(const std::string& id) const
{
    auto found = sessions_.find(id);
    if (found == sessions_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string DeviceSessionManager::open(const std::string& device_id, const std::string& adapter)
{
    std::ostringstream id;
    id << "devsess-" << next_id_++;
    sessions_[id.str()] = {id.str(), device_id, adapter};
    return id.str();
}

std::optional<DeviceSession> DeviceSessionManager::get(const std::string& id) const
{
    auto found = sessions_.find(id);
    if (found == sessions_.end()) {
        return std::nullopt;
    }
    return found->second;
}

ControlSessionManager& SessionManager::control()
{
    return control_;
}

DeviceSessionManager& SessionManager::device()
{
    return device_;
}

void SessionManager::map_control_to_device(std::string control_session_id, std::string device_session_id)
{
    control_to_device_[std::move(control_session_id)] = std::move(device_session_id);
}

std::optional<std::string> SessionManager::device_session_for_control(const std::string& control_session_id) const
{
    auto found = control_to_device_.find(control_session_id);
    if (found == control_to_device_.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace axent
