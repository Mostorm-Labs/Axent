#include "axent/core/device_manager.hpp"

#include <algorithm>
#include <map>
#include <tuple>
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

    using DeviceKey = std::pair<std::string, std::string>;
    struct ClaimPlan {
        DeviceSnapshot snapshot;
        std::vector<std::size_t> row_indices;
        bool valid = true;
        bool has_authoritative_claim = false;
        DeviceUpsertStatus rejection_status =
            DeviceUpsertStatus::DiscoveryClaimConflict;
    };

    std::map<DeviceKey, std::vector<std::size_t>> rows_by_key;
    for (std::size_t index = 0; index < discovered.size(); ++index) {
        rows_by_key[{discovered[index].adapter, discovered[index].id}]
            .push_back(index);
    }

    const auto snapshots_equivalent = [](const DeviceSnapshot& lhs,
                                         const DeviceSnapshot& rhs) {
        return std::tie(lhs.id,
                        lhs.adapter,
                        lhs.identity.vendor,
                        lhs.identity.model,
                        lhs.identity.serial_number,
                        lhs.identity.firmware_version,
                        lhs.identity.hardware_version,
                        lhs.connection.online,
                        lhs.connection.transport,
                        lhs.connection.last_change_reason,
                        lhs.status.health,
                        lhs.endpoint_id,
                        lhs.endpoint_delivery_mode) ==
               std::tie(rhs.id,
                        rhs.adapter,
                        rhs.identity.vendor,
                        rhs.identity.model,
                        rhs.identity.serial_number,
                        rhs.identity.firmware_version,
                        rhs.identity.hardware_version,
                        rhs.connection.online,
                        rhs.connection.transport,
                        rhs.connection.last_change_reason,
                        rhs.status.health,
                        rhs.endpoint_id,
                        rhs.endpoint_delivery_mode);
    };

    std::map<DeviceKey, ClaimPlan> plans;
    for (const auto& entry : rows_by_key) {
        ClaimPlan plan;
        plan.row_indices = entry.second;

        const auto existing = std::find_if(
            devices_.begin(), devices_.end(), [&](const DeviceSnapshot& current) {
                return current.adapter == entry.first.first &&
                       current.id == entry.first.second;
            });
        std::string effective_endpoint = existing == devices_.end()
            ? std::string{}
            : existing->endpoint_id;
        for (const auto index : entry.second) {
            const auto& endpoint = discovered[index].endpoint_id;
            if (endpoint.empty()) {
                continue;
            }
            if (!effective_endpoint.empty() && endpoint != effective_endpoint) {
                plan.valid = false;
                plan.rejection_status = existing == devices_.end()
                    ? DeviceUpsertStatus::DiscoveryClaimConflict
                    : DeviceUpsertStatus::EndpointChangeRejected;
                break;
            }
            effective_endpoint = endpoint;
        }

        if (plan.valid) {
            plan.snapshot = discovered[entry.second.front()];
            plan.snapshot.endpoint_id = effective_endpoint;
            for (const auto index : entry.second) {
                auto normalized = discovered[index];
                normalized.endpoint_id = effective_endpoint;
                if (!snapshots_equivalent(plan.snapshot, normalized)) {
                    plan.valid = false;
                    plan.rejection_status =
                        DeviceUpsertStatus::DiscoveryClaimConflict;
                    break;
                }
            }
        }
        if (!plan.valid && existing != devices_.end() &&
            !existing->endpoint_id.empty()) {
            plan.snapshot = *existing;
            plan.has_authoritative_claim = true;
        }
        plans.emplace(entry.first, std::move(plan));
    }

    std::map<std::string, std::size_t> claim_counts;
    for (const auto& entry : plans) {
        const auto& plan = entry.second;
        if ((plan.valid || plan.has_authoritative_claim) &&
            plan.snapshot.adapter == adapter &&
            !plan.snapshot.endpoint_id.empty()) {
            ++claim_counts[plan.snapshot.endpoint_id];
        }
    }

    const auto is_current = [&](const DeviceSnapshot& device) {
        return rows_by_key.find({device.adapter, device.id}) !=
               rows_by_key.end();
    };

    std::vector<DeviceUpsertResult> results(discovered.size());
    for (const auto& entry : plans) {
        const auto& plan = entry.second;
        if (!plan.valid && !plan.has_authoritative_claim) {
            for (const auto index : plan.row_indices) {
                results[index] = {plan.rejection_status};
            }
            continue;
        }

        const auto& snapshot = plan.snapshot;
        const auto claim_count = snapshot.endpoint_id.empty()
            ? 0
            : claim_counts[snapshot.endpoint_id];
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

        if (!plan.valid) {
            for (const auto index : plan.row_indices) {
                results[index] = {plan.rejection_status};
            }
            continue;
        }

        const auto result = upsert_locked(snapshot, claim_count >= 2);
        for (const auto index : plan.row_indices) {
            results[index] = result;
        }
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
