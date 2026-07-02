#include "axent/core/broker.hpp"

#include <exception>

#include "axent/core/json.hpp"

namespace axent {
namespace {

nlohmann::json device_list_body(const std::vector<DeviceSnapshot>& devices)
{
    nlohmann::json body = {{"devices", nlohmann::json::array()}};
    for (const auto& device : devices) {
        body["devices"].push_back(to_json(device));
    }
    return body;
}

} // namespace

Broker::Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control)
    : routes_(routes), middleware_(middleware), flow_control_(flow_control)
{
}

void Broker::register_adapter(Adapter& adapter)
{
    adapters_[adapter.metadata().name] = &adapter;
}

void Broker::unregister_adapter(const std::string& name)
{
    adapters_.erase(name);
}

ControlResult Broker::dispatch(const ControlCommand& command)
{
    middleware_.before_dispatch(command);

    ControlResult result;
    try {
        if (flow_control_.snapshot().paused) {
            flow_control_.record_drop();
            result = {ControlStatus::Unavailable, {{"error", "flow paused"}}};
        } else if (command.method == "devices.list") {
            result = {ControlStatus::Ok, device_list_body(routes_.list_devices())};
        } else {
            const auto target = routes_.resolve(command.device_id);
            if (!target) {
                result = {ControlStatus::NotFound, {{"error", "route not found"}}};
            } else {
                const auto adapter = adapters_.find(target->adapter);
                if (adapter == adapters_.end()) {
                    result = {ControlStatus::Unavailable, {{"error", "adapter unavailable"}}};
                } else if (command.method == "firmware.update") {
                    if (!command.params.is_object() || !command.params.contains("file")
                        || !command.params.at("file").is_string()) {
                        result = {ControlStatus::InvalidArgument, {{"error", "invalid firmware file"}}};
                    } else {
                        result = adapter->second->start_firmware_update(target->device_id,
                                                                        command.params.at("file").get<std::string>());
                    }
                } else {
                    result = adapter->second->call(target->device_id, command.method, command.params);
                }
            }
        }
    } catch (const std::exception& error) {
        result = {ControlStatus::InternalError, {{"error", error.what()}}};
    } catch (...) {
        result = {ControlStatus::InternalError, {{"error", "unknown error"}}};
    }

    middleware_.after_dispatch(command, result);
    return result;
}

} // namespace axent
