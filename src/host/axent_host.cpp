#include "axent/host/axent_host.hpp"

#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/adapters/mock_adapter.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/core/session_manager.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

struct AxentHost::Impl {
    void reset();
    std::optional<SessionLease> lease_for_session_locked(const std::string& session_id) const;

    mutable std::mutex mutex;
    bool running = false;
    AxentHostOptions options;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<Adapter> axtp_adapter;
    std::unique_ptr<MockAdapter> mock_adapter;
    std::unique_ptr<DeviceManager> devices;
    std::unique_ptr<RouteManager> routes;
    std::unique_ptr<Middleware> middleware;
    std::unique_ptr<FlowControl> flow;
    std::unique_ptr<Broker> broker;
    SessionManager sessions;
    std::map<std::string, SessionLease> leases;
    std::map<std::string, std::shared_ptr<MediaStreamRelay>> relays;
    std::map<std::string, std::string> media_owner_session_by_device;
};

void AxentHost::Impl::reset()
{
    for (auto& entry : relays) {
        if (entry.second) {
            entry.second->close();
        }
    }
    relays.clear();
    leases.clear();
    media_owner_session_by_device.clear();
    broker.reset();
    flow.reset();
    middleware.reset();
    routes.reset();
    devices.reset();
    mock_adapter.reset();
    axtp_adapter.reset();
    logger.reset();
    sessions = SessionManager{};
    running = false;
}

AxentHostOptions::AxentHostOptions()
    : axtp(AxtpAdapter::na20_defaults())
{
}

std::optional<SessionLease> AxentHost::Impl::lease_for_session_locked(const std::string& session_id) const
{
    const auto it = leases.find(session_id);
    if (it == leases.end()) {
        return std::nullopt;
    }
    return it->second;
}

MediaConsumer::MediaConsumer(std::shared_ptr<MediaStreamRelay> relay)
    : relay_(std::move(relay))
{
}

std::optional<MediaFrame> MediaConsumer::read()
{
    return relay_ ? relay_->read() : std::nullopt;
}

MediaRelayStats MediaConsumer::stats() const
{
    return relay_ ? relay_->stats() : MediaRelayStats{};
}

AxentHost::AxentHost()
    : impl_(std::make_unique<Impl>())
{
}

AxentHost::~AxentHost()
{
    stop();
}

bool AxentHost::start(AxentHostOptions options)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);

    impl_->reset();
    impl_->options = options;
    impl_->logger = std::make_unique<Logger>();
    impl_->devices = std::make_unique<DeviceManager>();
    impl_->routes = std::make_unique<RouteManager>(*impl_->devices);
    impl_->middleware = std::make_unique<Middleware>(*impl_->logger);
    impl_->flow = std::make_unique<FlowControl>();
    impl_->broker = std::make_unique<Broker>(*impl_->routes, *impl_->middleware, *impl_->flow);

    if (impl_->options.enable_mock_adapter) {
        impl_->mock_adapter = std::make_unique<MockAdapter>();
        for (const auto& device : impl_->mock_adapter->discover()) {
            impl_->devices->upsert(device);
        }
        impl_->broker->register_adapter(*impl_->mock_adapter);
    }
    if (impl_->options.enable_axtp_adapter) {
        if (impl_->options.axtp_adapter_factory) {
            impl_->axtp_adapter = impl_->options.axtp_adapter_factory(impl_->options.axtp);
        } else {
            impl_->axtp_adapter = std::make_unique<AxtpAdapter>(impl_->options.axtp);
        }
        for (const auto& device : impl_->axtp_adapter->discover()) {
            impl_->devices->upsert(device);
        }
        impl_->broker->register_adapter(*impl_->axtp_adapter);
    }

    impl_->running = true;
    return true;
}

void AxentHost::stop()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running && !impl_->broker) {
        return;
    }
    impl_->reset();
}

bool AxentHost::running() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->running;
}

std::vector<DeviceSnapshot> AxentHost::discover_devices() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->devices ? impl_->devices->list() : std::vector<DeviceSnapshot>{};
}

void AxentHost::upsert_device(DeviceSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->devices) {
        impl_->devices->upsert(std::move(snapshot));
    }
}

SessionLease AxentHost::acquire_session(const SessionAcquireRequest& request)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running || !impl_->devices) {
        return {false, "", request.device_id, request.client_id, false, "host not running"};
    }
    const auto device = impl_->devices->get(request.device_id);
    if (!device.has_value()) {
        return {false, "", request.device_id, request.client_id, false, "device not found"};
    }
    if (request.media
        && impl_->media_owner_session_by_device.find(request.device_id)
               != impl_->media_owner_session_by_device.end()) {
        return {false, "", request.device_id, request.client_id, false, "media lease busy"};
    }

    const std::string session_id = impl_->sessions.device().open(request.device_id, device->adapter);
    SessionLease lease{true, session_id, request.device_id, request.client_id, request.media, ""};
    impl_->leases[session_id] = lease;
    if (request.media) {
        impl_->media_owner_session_by_device[request.device_id] = session_id;
    }
    return lease;
}

void AxentHost::release_session(const std::string& session_id, const std::string&)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (lease.has_value() && lease->media) {
        const auto owner = impl_->media_owner_session_by_device.find(lease->device_id);
        if (owner != impl_->media_owner_session_by_device.end() && owner->second == session_id) {
            impl_->media_owner_session_by_device.erase(owner);
        }
    }
    const auto relay = impl_->relays.find(session_id);
    if (relay != impl_->relays.end()) {
        relay->second->close();
        impl_->relays.erase(relay);
    }
    if (lease.has_value()) {
        impl_->sessions.close_device_session(session_id);
    }
    impl_->leases.erase(session_id);
}

std::unique_ptr<MediaConsumer> AxentHost::create_media_consumer(const std::string& session_id,
                                                                MediaRelayOptions options)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value() || !lease->media) {
        return nullptr;
    }

    auto& relay = impl_->relays[session_id];
    if (!relay) {
        relay = std::make_shared<MediaStreamRelay>(options);
    }
    return std::unique_ptr<MediaConsumer>(new MediaConsumer(relay));
}

bool AxentHost::publish_media_frame(const std::string& session_id, MediaFrame frame)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value() || !lease->media) {
        return false;
    }
    const auto relay = impl_->relays.find(session_id);
    if (relay == impl_->relays.end() || !relay->second) {
        return false;
    }
    frame.session_id = session_id;
    relay->second->publish(std::move(frame));
    return true;
}

ControlResult AxentHost::call(const std::string& session_id,
                              const std::string& method,
                              const nlohmann::json& params)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->broker) {
        return {ControlStatus::Unavailable, {{"error", "host not running"}}};
    }
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value()) {
        return {ControlStatus::NotFound, {{"error", "session not found"}}};
    }

    ControlCommand command;
    command.request_id = session_id + ":" + method;
    command.control_session_id = lease->client_id;
    command.device_id = lease->device_id;
    command.method = method;
    command.params = params;
    return impl_->broker->dispatch(command);
}

Broker& AxentHost::broker()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running || !impl_->broker) {
        throw std::logic_error("AxentHost broker is unavailable while host is not running");
    }
    return *impl_->broker;
}

} // namespace axent
