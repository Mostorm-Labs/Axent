#include "axent/core/broker.hpp"

#include <chrono>
#include <exception>
#include <mutex>
#include <utility>

#include "axent/core/json.hpp"
#include "control_operation_internal.hpp"

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

struct Broker::AsyncState {
    std::mutex mutex;
    Middleware* middleware = nullptr;
};

Broker::Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control)
    : routes_(routes)
    , middleware_(middleware)
    , flow_control_(flow_control)
    , async_state_(std::make_shared<AsyncState>())
{
    async_state_->middleware = &middleware_;
}

Broker::~Broker()
{
    std::lock_guard<std::mutex> lock(async_state_->mutex);
    async_state_->middleware = nullptr;
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
    auto operation = dispatch_async(command);
    return operation
        ? operation->wait()
        : ControlResult{
              ControlStatus::InternalError,
              {{"error", "adapter returned no control operation"}}};
}

ControlOperationPtr Broker::dispatch_async(
    const ControlCommand& command,
    ControlCallOptions options)
{
    if (!options.deadline.has_value()) {
        const auto now = std::chrono::steady_clock::now();
        options.deadline = options.timeout <= std::chrono::milliseconds::zero()
            ? now
            : now + options.timeout;
    }
    middleware_.before_dispatch(command);

    ControlOperationPtr operation;
    try {
        if (flow_control_.snapshot().paused) {
            flow_control_.record_drop();
            operation = make_completed_control_operation(
                {ControlStatus::Unavailable, {{"error", "flow paused"}}});
        } else if (command.method == "devices.list") {
            operation = make_completed_control_operation(
                {ControlStatus::Ok, device_list_body(routes_.list_devices())});
        } else {
            const auto target = routes_.resolve(command.device_id);
            if (!target) {
                operation = make_completed_control_operation(
                    {ControlStatus::NotFound, {{"error", "route not found"}}});
            } else {
                const auto adapter = adapters_.find(target->adapter);
                if (adapter == adapters_.end()) {
                    operation = make_completed_control_operation(
                        {ControlStatus::Unavailable,
                         {{"error", "adapter unavailable"}}});
                } else if (command.method == "firmware.update") {
                    if (!command.params.is_object() || !command.params.contains("file")
                        || !command.params.at("file").is_string()) {
                        operation = make_completed_control_operation(
                            {ControlStatus::InvalidArgument,
                             {{"error", "invalid firmware file"}}});
                    } else {
                        operation = make_completed_control_operation(
                            adapter->second->start_firmware_update(
                                target->device_id,
                                command.params.at("file").get<std::string>()));
                    }
                } else {
                    operation = adapter->second->call_async(
                        target->device_id,
                        command.method,
                        command.params,
                        options);
                }
            }
        }
    } catch (const std::exception& error) {
        operation = make_completed_control_operation(
            {ControlStatus::InternalError, {{"error", error.what()}}});
    } catch (...) {
        operation = make_completed_control_operation(
            {ControlStatus::InternalError, {{"error", "unknown error"}}});
    }

    if (!operation) {
        operation = make_completed_control_operation(
            {ControlStatus::InternalError,
             {{"error", "adapter returned no control operation"}}});
    }
    const auto async_state = async_state_;
    ControlOperationSource::observe(
        operation,
        [async_state, command](const ControlResult& result) {
            std::lock_guard<std::mutex> lock(async_state->mutex);
            if (async_state->middleware != nullptr) {
                async_state->middleware->after_dispatch(command, result);
            }
        });
    return operation;
}

} // namespace axent
