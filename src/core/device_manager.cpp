#include "axent/core/device_manager.hpp"

#include <algorithm>
#include <utility>

namespace axent {

DeviceManager::DeviceManager(const DeviceManager& other)
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    devices_ = other.devices_;
}

DeviceManager& DeviceManager::operator=(const DeviceManager& other)
{
    if (this == &other) {
        return *this;
    }
    std::scoped_lock lock(mutex_, other.mutex_);
    devices_ = other.devices_;
    return *this;
}

DeviceManager::DeviceManager(DeviceManager&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    devices_ = std::move(other.devices_);
}

DeviceManager& DeviceManager::operator=(DeviceManager&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    std::scoped_lock lock(mutex_, other.mutex_);
    devices_ = std::move(other.devices_);
    return *this;
}

DeviceUpsertResult DeviceManager::upsert(DeviceSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.adapter == snapshot.adapter && current.id == snapshot.id;
    });

    if (!snapshot.endpoint_id.empty()) {
        const auto conflict = std::find_if(
            devices_.begin(), devices_.end(), [&](const auto& current) {
                return (current.adapter != snapshot.adapter ||
                        current.id != snapshot.id) &&
                       current.endpoint_id == snapshot.endpoint_id;
            });
        if (conflict != devices_.end()) {
            return {DeviceUpsertStatus::EndpointConflict};
        }
    }

    if (existing == devices_.end()) {
        devices_.push_back(std::move(snapshot));
        return {DeviceUpsertStatus::Inserted};
    }

    const bool binds_endpoint = existing->endpoint_id.empty() && !snapshot.endpoint_id.empty();
    if (!existing->endpoint_id.empty()) {
        if (snapshot.endpoint_id.empty()) {
            snapshot.endpoint_id = existing->endpoint_id;
        } else if (snapshot.endpoint_id != existing->endpoint_id) {
            return {DeviceUpsertStatus::EndpointChangeRejected};
        }
    }

    *existing = std::move(snapshot);
    return {binds_endpoint ? DeviceUpsertStatus::EndpointBound
                           : DeviceUpsertStatus::Refreshed};
}

void DeviceManager::mark_offline(const std::string& adapter,
                                 const std::string& id,
                                 const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.adapter == adapter && current.id == id;
    });
    if (existing != devices_.end()) {
        existing->connection.online = false;
        existing->connection.last_change_reason = reason;
    }
}

void DeviceManager::mark_offline(const std::string& id, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = devices_.end();
    for (auto current = devices_.begin(); current != devices_.end(); ++current) {
        if (current->id != id) {
            continue;
        }
        if (existing != devices_.end()) {
            return;
        }
        existing = current;
    }
    if (existing == devices_.end()) {
        return;
    }
    existing->connection.online = false;
    existing->connection.last_change_reason = reason;
}

std::optional<DeviceSnapshot> DeviceManager::get(const std::string& adapter,
                                                  const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto existing = std::find_if(
        devices_.begin(), devices_.end(), [&](const auto& current) {
            return current.adapter == adapter && current.id == id;
        });
    if (existing == devices_.end()) {
        return std::nullopt;
    }
    return *existing;
}

std::optional<DeviceSnapshot> DeviceManager::get(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = devices_.end();
    for (auto current = devices_.begin(); current != devices_.end(); ++current) {
        if (current->id != id) {
            continue;
        }
        if (existing != devices_.end()) {
            return std::nullopt;
        }
        existing = current;
    }
    if (existing == devices_.end()) {
        return std::nullopt;
    }
    return *existing;
}

std::optional<DeviceSnapshot> DeviceManager::find_by_serial_number(const std::string& serial_number) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (serial_number.empty()) {
        return std::nullopt;
    }
    auto existing = devices_.end();
    for (auto current = devices_.begin(); current != devices_.end(); ++current) {
        if (current->identity.serial_number != serial_number) {
            continue;
        }
        if (existing != devices_.end()) {
            return std::nullopt;
        }
        existing = current;
    }
    if (existing == devices_.end()) {
        return std::nullopt;
    }
    return *existing;
}

std::vector<DeviceSnapshot> DeviceManager::list() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

} // namespace axent
