#include "axent/core/device_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace axent {
namespace {

std::string default_endpoint_id(const DeviceSnapshot& snapshot)
{
    // The control-plane identity must not require a physical device ID, HID
    // path, or serial number. Hash the internal binding into a stable endpoint
    // token which does not embed those values; callers may still provide a
    // product-friendly explicit endpoint_id (for example endpoint/receiver-a).
    if (snapshot.id.empty() && snapshot.identity.serial_number.empty()) {
        return {};
    }
    std::string binding = snapshot.adapter;
    binding.push_back('\0');
    binding += snapshot.id;
    if (snapshot.id.empty()) {
        binding.push_back('\0');
        binding += snapshot.identity.serial_number;
    }
    constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    std::uint64_t hash = kFnvOffset;
    for (const unsigned char byte : binding) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    std::ostringstream endpoint;
    endpoint << "endpoint/" << std::hex << std::nouppercase
             << std::setfill('0') << std::setw(16) << hash;
    return endpoint.str();
}

} // namespace

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

void DeviceManager::upsert(DeviceSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == snapshot.id;
    });

    // Keep an adapter-provided endpoint stable across refreshes.  For normal
    // devices the internal binding is stable and gives us a deterministic,
    // opaque fallback.
    if (snapshot.endpoint_id.empty()) {
        if (existing != devices_.end() && !existing->endpoint_id.empty()) {
            snapshot.endpoint_id = existing->endpoint_id;
        } else {
            snapshot.endpoint_id = default_endpoint_id(snapshot);
        }
    }

    if (existing == devices_.end()) {
        devices_.push_back(std::move(snapshot));
        return;
    }
    *existing = std::move(snapshot);
}

void DeviceManager::mark_offline(const std::string& id, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == id;
    });
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
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.identity.serial_number == serial_number;
    });
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
