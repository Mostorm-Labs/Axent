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

struct CommandRoute {
    std::optional<RouteTarget> target;
    ControlStatus failure_status = ControlStatus::NotFound;
    const char* failure_message = "route not found";
};

CommandRoute resolve_command_route(RouteManager& routes, const ControlCommand& command)
{
    if (command.dst.empty()) {
        return {routes.resolve_device(command.device_id),
                ControlStatus::NotFound,
                "route not found"};
    }

    const auto resolution = routes.resolve_endpoint_route(command.dst);
    switch (resolution.status) {
    case RouteResolutionStatus::Found:
        return {resolution.target, ControlStatus::Ok, ""};
    case RouteResolutionStatus::NotFound:
        return {std::nullopt, ControlStatus::NotFound, "route not found"};
    case RouteResolutionStatus::Unavailable:
    case RouteResolutionStatus::Conflict:
        return {std::nullopt, ControlStatus::Unavailable, "route unavailable"};
    }
    return {std::nullopt, ControlStatus::Unavailable, "route unavailable"};
}

AdapterControlRequest make_adapter_request(const ControlCommand& command,
                                           const RouteTarget& target)
{
    AdapterControlRequest request;
    request.device_id = target.device_id;
    request.source_endpoint_id = command.dst.empty() ? "" : command.src;
    request.destination_endpoint_id = command.dst.empty() ? "" : target.endpoint_id;
    request.endpoint_delivery_mode = target.endpoint_delivery_mode;
    request.method = command.method;
    request.params = command.params;
    return request;
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
    std::lock_guard<std::mutex> lock(adapters_mutex_);
    adapters_[adapter.metadata().name] = &adapter;
}

void Broker::unregister_adapter(const std::string& name)
{
    std::lock_guard<std::mutex> lock(adapters_mutex_);
    adapters_.erase(name);
}

std::string Broker::route_key(const ControlCommand& command) const
{
    if (command.method == "devices.list") {
        return "control-plane";
    }
    const auto target = !command.dst.empty()
        ? routes_.resolve_endpoint(command.dst)
        : routes_.resolve_device(command.device_id);
    if (target.has_value()) {
        return "physical:" + target->adapter + ":" + target->device_id;
    }
    if (!command.dst.empty()) {
        return "endpoint:" + command.dst;
    }
    if (!command.device_id.empty()) {
        return "legacy-device:" + command.device_id;
    }
    return "control-plane";
}

ControlResult Broker::dispatch(const ControlCommand& command)
{
    // The synchronous control plane is also the lazy connection boundary for
    // daemon/WebSocket callers. In particular AxtpAdapter::call() may open the
    // destination's physical session, whereas call_async() intentionally
    // never blocks its submitting thread on a HID handshake. Host/session
    // callers continue to use dispatch_async() after lease acquisition.
    middleware_.before_dispatch(command);
    ControlResult result;
    try {
        if (flow_control_.snapshot().paused) {
            flow_control_.record_drop();
            result = {ControlStatus::Unavailable, {{"error", "flow paused"}}};
        } else if (command.source == ProtocolSource::JsonRpc &&
                   command.src.empty() != command.dst.empty()) {
            result = {ControlStatus::InvalidArgument,
                      {{"error", "JSON-RPC src and dst must be provided together"}}};
        } else if (command.method == "devices.list") {
            result = {ControlStatus::Ok, device_list_body(routes_.list_devices())};
        } else {
            const auto route = resolve_command_route(routes_, command);
            if (!route.target) {
                result = {route.failure_status, {{"error", route.failure_message}}};
            } else {
                const auto routed = make_adapter_request(command, *route.target);
                Adapter* adapter = nullptr;
                {
                    std::lock_guard<std::mutex> lock(adapters_mutex_);
                    const auto found = adapters_.find(route.target->adapter);
                    if (found != adapters_.end()) {
                        adapter = found->second;
                    }
                }
                if (adapter == nullptr) {
                    result = {ControlStatus::Unavailable,
                              {{"error", "adapter unavailable"}}};
                } else if (command.method == "firmware.update") {
                    if (!command.params.is_object() ||
                        !command.params.contains("file") ||
                        !command.params.at("file").is_string()) {
                        result = {ControlStatus::InvalidArgument,
                                  {{"error", "invalid firmware file"}}};
                    } else {
                        result = adapter->start_firmware_update(
                            routed,
                            command.params.at("file").get<std::string>());
                    }
                } else {
                    result = adapter->call(routed);
                }
            }
        }
    } catch (const std::exception& error) {
        result = {ControlStatus::InternalError, {{"error", error.what()}}};
    } catch (...) {
        result = {ControlStatus::InternalError,
                  {{"error", "unknown error"}}};
    }
    middleware_.after_dispatch(command, result);
    return result;
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
        } else if (command.source == ProtocolSource::JsonRpc &&
                   command.src.empty() != command.dst.empty()) {
            operation = make_completed_control_operation(
                {ControlStatus::InvalidArgument,
                 {{"error", "JSON-RPC src and dst must be provided together"}}});
        } else if (command.method == "devices.list") {
            operation = make_completed_control_operation(
                {ControlStatus::Ok, device_list_body(routes_.list_devices())});
        } else {
            // `dst` is the logical endpoint contract for JSON-RPC.  Route it
            // before consulting the legacy physical selector kept in
            // device_id (which may contain a serial number).
            const auto route = resolve_command_route(routes_, command);
            if (!route.target) {
                operation = make_completed_control_operation(
                    {route.failure_status, {{"error", route.failure_message}}});
            } else {
                const auto routed = make_adapter_request(command, *route.target);
                Adapter* adapter = nullptr;
                {
                    std::lock_guard<std::mutex> lock(adapters_mutex_);
                    const auto found = adapters_.find(route.target->adapter);
                    if (found != adapters_.end()) {
                        adapter = found->second;
                    }
                }
                if (adapter == nullptr) {
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
                            adapter->start_firmware_update(
                                routed,
                                command.params.at("file").get<std::string>()));
                    }
                } else {
                    operation = adapter->call_async(
                        routed,
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
