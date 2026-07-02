#include "axent/core/broker.hpp"

namespace axent {

Broker::Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control)
    : routes_(routes), middleware_(middleware), flow_control_(flow_control)
{
}

void Broker::register_adapter(Adapter* adapter)
{
    if (adapter != nullptr) {
        adapters_[adapter->metadata().name] = adapter;
    }
}

ControlResult Broker::dispatch(const ControlCommand& command)
{
    middleware_.before_dispatch(command);

    ControlResult result;
    const auto target = routes_.resolve(command.device_id);
    if (!target) {
        result = {ControlStatus::NotFound, {{"error", "route not found"}}};
    } else {
        const auto adapter = adapters_.find(target->adapter);
        if (adapter == adapters_.end()) {
            result = {ControlStatus::Unavailable, {{"error", "adapter unavailable"}}};
        } else if (command.method == "firmware.update") {
            result = adapter->second->start_firmware_update(target->device_id, command.params.value("file", ""));
        } else {
            result = adapter->second->call(target->device_id, command.method, command.params);
        }
    }

    if (flow_control_.snapshot().paused) {
        flow_control_.record_drop();
    }

    middleware_.after_dispatch(command, result);
    return result;
}

} // namespace axent
