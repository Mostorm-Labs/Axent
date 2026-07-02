#pragma once

#include "axent/core/adapter.hpp"

namespace axent {

class MockAdapter final : public Adapter {
public:
    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;

private:
    int next_task_id_ = 1;
};

} // namespace axent
