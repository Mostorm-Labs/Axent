#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "axent/core/types.hpp"

namespace axent {

enum class DeviceUpsertStatus {
    Inserted,
    Refreshed,
    EndpointBound,
    EndpointConflict,
    EndpointChangeRejected,
};

struct DeviceUpsertResult {
    DeviceUpsertStatus status = DeviceUpsertStatus::Inserted;

    bool accepted() const noexcept
    {
        return status == DeviceUpsertStatus::Inserted ||
               status == DeviceUpsertStatus::Refreshed ||
               status == DeviceUpsertStatus::EndpointBound;
    }
};

class DeviceManager {
public:
    DeviceManager() = default;
    DeviceManager(const DeviceManager& other);
    DeviceManager& operator=(const DeviceManager& other);
    DeviceManager(DeviceManager&& other) noexcept;
    DeviceManager& operator=(DeviceManager&& other) noexcept;

    DeviceUpsertResult upsert(DeviceSnapshot snapshot);
    void mark_offline(const std::string& adapter,
                      const std::string& id,
                      const std::string& reason);
    // Compatibility operation for hosts that only have a provider-local ID.
    // It is a no-op when more than one adapter owns that ID.
    void mark_offline(const std::string& id, const std::string& reason);
    std::optional<DeviceSnapshot> get(const std::string& adapter,
                                      const std::string& id) const;
    // Compatibility lookup. It fails closed when the ID is ambiguous across
    // adapters; callers resolving an Endpoint receive an adapter-scoped target.
    std::optional<DeviceSnapshot> get(const std::string& id) const;
    std::optional<DeviceSnapshot> find_by_serial_number(const std::string& serial_number) const;
    std::vector<DeviceSnapshot> list() const;

private:
    mutable std::mutex mutex_;
    std::vector<DeviceSnapshot> devices_;
};

} // namespace axent
