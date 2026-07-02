#include "axent/core/device_manager.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void DeviceManager::upsert(DeviceSnapshot snapshot)
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == snapshot.id;
    });
    if (existing == devices_.end()) {
        devices_.push_back(std::move(snapshot));
        return;
    }
    *existing = std::move(snapshot);
}

void DeviceManager::mark_offline(const std::string& id, const std::string& reason)
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == id;
    });
    if (existing != devices_.end()) {
        existing->connection.online = false;
        existing->connection.last_change_reason = reason;
    }
}

std::optional<DeviceSnapshot> DeviceManager::get(const std::string& id) const
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == id;
    });
    if (existing == devices_.end()) {
        return std::nullopt;
    }
    return *existing;
}

std::vector<DeviceSnapshot> DeviceManager::list() const
{
    return devices_;
}

} // namespace axent
