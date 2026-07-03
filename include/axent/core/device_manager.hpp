#pragma once

#include <optional>
#include <string>
#include <vector>

#include "axent/core/types.hpp"

namespace axent {

class DeviceManager {
public:
    void upsert(DeviceSnapshot snapshot);
    void mark_offline(const std::string& id, const std::string& reason);
    std::optional<DeviceSnapshot> get(const std::string& id) const;
    std::optional<DeviceSnapshot> find_by_serial_number(const std::string& serial_number) const;
    std::vector<DeviceSnapshot> list() const;

private:
    std::vector<DeviceSnapshot> devices_;
};

} // namespace axent
