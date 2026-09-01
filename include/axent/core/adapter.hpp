#pragma once

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/core/adapter_registry.hpp"
#include "axent/core/control_operation.hpp"
#include "axent/core/types.hpp"

namespace axent {

struct AdapterControlRequest {
    std::string device_id;
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode = EndpointDeliveryMode::LocalProjection;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
};

class Adapter {
public:
    virtual ~Adapter() = default;

    virtual AdapterMetadata metadata() const = 0;
    virtual std::vector<Capability> capabilities() const = 0;
    virtual std::vector<DeviceSnapshot> discover() = 0;
    virtual ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) = 0;
    virtual ControlResult call(const AdapterControlRequest& request)
    {
        return call(request.device_id, request.method, request.params);
    }
    virtual ControlOperationPtr call_async(
        const std::string& device_id,
        const std::string& method,
        const nlohmann::json& params,
        ControlCallOptions options = {})
    {
        (void)options;
        return make_completed_control_operation(call(device_id, method, params));
    }
    virtual ControlOperationPtr call_async(
        const AdapterControlRequest& request,
        ControlCallOptions options = {})
    {
        return call_async(request.device_id, request.method, request.params, std::move(options));
    }
    virtual ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) = 0;
    virtual ControlResult start_firmware_update(
        const AdapterControlRequest& request,
        const std::string& file_path)
    {
        return start_firmware_update(request.device_id, file_path);
    }
};

} // namespace axent
