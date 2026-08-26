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
    return upsert_locked(std::move(snapshot), false);
}

DeviceUpsertResult DeviceManager::upsert_locked(
    DeviceSnapshot snapshot,
    bool allow_same_adapter_endpoint_conflicts)
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.adapter == snapshot.adapter && current.id == snapshot.id;
    });

    if (!snapshot.endpoint_id.empty()) {
        const auto conflict = std::find_if(
            devices_.begin(), devices_.end(), [&](const auto& current) {
                return (current.adapter != snapshot.adapter ||
                        current.id != snapshot.id) &&
                       current.endpoint_id == snapshot.endpoint_id &&
                       (!allow_same_adapter_endpoint_conflicts ||
                        current.adapter != snapshot.adapter);
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

std::vector<DeviceUpsertResult> DeviceManager::reconcile_discovery(
    const std::string& adapter,
    std::vector<DeviceSnapshot> discovered,
    const std::string& missing_reason)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto is_current = [&](const DeviceSnapshot& device) {
        return std::any_of(
            discovered.begin(), discovered.end(), [&](const DeviceSnapshot& current) {
                return current.adapter == device.adapter && current.id == device.id;
            });
    };

    std::vector<bool> endpoint_change_rejected(discovered.size(), false);
    for (std::size_t index = 0; index < discovered.size(); ++index) {
        const auto& snapshot = discovered[index];
        const auto existing = std::find_if(
            devices_.begin(), devices_.end(), [&](const DeviceSnapshot& current) {
                return current.adapter == snapshot.adapter && current.id == snapshot.id;
            });
        endpoint_change_rejected[index] =
            existing != devices_.end() &&
            !existing->endpoint_id.empty() &&
            !snapshot.endpoint_id.empty() &&
            existing->endpoint_id != snapshot.endpoint_id;
    }

    const auto current_endpoint_claim_count = [&](const std::string& endpoint_id) {
        std::size_t count = 0;
        for (std::size_t index = 0; index < discovered.size(); ++index) {
            if (!endpoint_change_rejected[index] &&
                discovered[index].adapter == adapter &&
                discovered[index].endpoint_id == endpoint_id) {
                ++count;
            }
        }
        return count;
    };

    std::vector<DeviceUpsertResult> results;
    results.reserve(discovered.size());
    for (std::size_t index = 0; index < discovered.size(); ++index) {
        const auto& snapshot = discovered[index];
        if (endpoint_change_rejected[index]) {
            results.push_back({DeviceUpsertStatus::EndpointChangeRejected});
            continue;
        }

        const auto claim_count = snapshot.endpoint_id.empty()
            ? 0
            : current_endpoint_claim_count(snapshot.endpoint_id);
        if (snapshot.adapter == adapter && claim_count == 1) {
            const auto cross_adapter_owner = std::find_if(
                devices_.begin(), devices_.end(), [&](const DeviceSnapshot& current) {
                    return current.endpoint_id == snapshot.endpoint_id &&
                           current.adapter != snapshot.adapter;
                });
            if (cross_adapter_owner == devices_.end()) {
                devices_.erase(
                    std::remove_if(
                        devices_.begin(),
                        devices_.end(),
                        [&](const DeviceSnapshot& current) {
                            return current.adapter == snapshot.adapter &&
                                   current.id != snapshot.id &&
                                   current.endpoint_id == snapshot.endpoint_id &&
                                   !is_current(current);
                        }),
                    devices_.end());
            }
        }

        results.push_back(upsert_locked(snapshot, claim_count >= 2));
    }

    for (auto& device : devices_) {
        if (device.adapter == adapter && !is_current(device)) {
            device.connection.online = false;
            device.connection.last_change_reason = missing_reason;
        }
    }
    return results;
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
