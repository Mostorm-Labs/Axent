#include "axent/host/axent_host.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

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

namespace {

class MediaSubscriptionState final : public MediaSubscription {
public:
    MediaSubscriptionState(std::shared_ptr<IMediaFrameSink> sink, MediaSubscriptionOptions options)
        : core_(std::make_shared<Core>(std::move(sink), options))
    {
        core_->start();
    }

    ~MediaSubscriptionState() override
    {
        cancel();
    }

    MediaSubscriptionState(const MediaSubscriptionState&) = delete;
    MediaSubscriptionState& operator=(const MediaSubscriptionState&) = delete;

    void publish(MediaFrame frame)
    {
        core_->publish(std::move(frame));
    }

    void cancel() override
    {
        core_->cancel();
    }

    MediaDeliveryStats stats() const override
    {
        return core_->stats();
    }

    bool active() const
    {
        return core_->active();
    }

private:
    class Core final : public std::enable_shared_from_this<Core> {
    public:
        Core(std::shared_ptr<IMediaFrameSink> sink, MediaSubscriptionOptions options)
            : sink_(std::move(sink))
            , options_(options)
        {
        }

        ~Core()
        {
            cancel();
        }

        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        void start()
        {
            if (options_.dispatch == MediaSubscriptionDispatch::Direct) {
                return;
            }
            auto self = shared_from_this();
            worker_ = std::thread([self]() { self->run(); });
        }

        void publish(MediaFrame frame)
        {
            if (options_.dispatch == MediaSubscriptionDispatch::Direct) {
                publish_direct(std::move(frame));
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }

                queued_bytes_ += frame.payload.size();
                queue_.push_back(std::move(frame));
                ++stats_.received_frames;

                bool dropped = false;
                while (over_limit_locked() && !queue_.empty()) {
                    drop_front_locked();
                    dropped = true;
                }
                if (dropped && !queue_.empty()) {
                    queue_.front().flags |= MediaFrameFlag::Discontinuity;
                }
                if (dropped) {
                    MediaEvent event{
                        MediaEventKind::Dropped,
                        stats_.dropped_frames,
                        stats_.dropped_bytes,
                    };
                    if (!events_.empty() && events_.back().kind == MediaEventKind::Dropped) {
                        events_.back() = event;
                    } else {
                        events_.push_back(event);
                    }
                }
                refresh_stats_locked();
            }
            cv_.notify_one();
        }

        void cancel()
        {
            std::thread worker;
            std::shared_ptr<IMediaFrameSink> sink;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }
                closed_ = true;
                queue_.clear();
                events_.clear();
                queued_bytes_ = 0;
                refresh_stats_locked();
                worker = std::move(worker_);
                sink = sink_;
            }
            cv_.notify_all();
            if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) {
                worker.join();
            } else if (worker.joinable()) {
                worker.detach();
            }
            if (sink) {
                sink->on_media_event(MediaEvent{MediaEventKind::Closed, 0, 0});
            }
        }

        MediaDeliveryStats stats() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto snapshot = stats_;
            snapshot.queued_frames = queue_.size();
            snapshot.queued_bytes = queued_bytes_;
            return snapshot;
        }

        bool active() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return !closed_;
        }

    private:
        void publish_direct(MediaFrame frame)
        {
            std::shared_ptr<IMediaFrameSink> sink;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }
                ++stats_.received_frames;
                sink = sink_;
            }

            if (sink) {
                sink->on_media_frame(std::move(frame));
                std::lock_guard<std::mutex> lock(mutex_);
                ++stats_.delivered_frames;
            }
        }

        bool over_limit_locked() const
        {
            const bool frame_limited = options_.max_frames != 0 && queue_.size() > options_.max_frames;
            const bool byte_limited = options_.max_bytes != 0 && queued_bytes_ > options_.max_bytes;
            return frame_limited || byte_limited;
        }

        void drop_front_locked()
        {
            const auto dropped_bytes = queue_.front().payload.size();
            queued_bytes_ -= dropped_bytes;
            queue_.pop_front();
            ++stats_.dropped_frames;
            stats_.dropped_bytes += dropped_bytes;
        }

        void refresh_stats_locked()
        {
            stats_.queued_frames = queue_.size();
            stats_.queued_bytes = queued_bytes_;
        }

        void run()
        {
            for (;;) {
                MediaFrame frame;
                MediaEvent event;
                bool has_frame = false;
                bool has_event = false;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() {
                        return closed_ || !queue_.empty() || !events_.empty();
                    });
                    if (!events_.empty()) {
                        event = events_.front();
                        events_.pop_front();
                        has_event = true;
                    } else if (!queue_.empty()) {
                        frame = std::move(queue_.front());
                        queued_bytes_ -= frame.payload.size();
                        queue_.pop_front();
                        refresh_stats_locked();
                        has_frame = true;
                    } else {
                        if (closed_) {
                            break;
                        }
                        continue;
                    }
                }

                if (has_event && sink_) {
                    sink_->on_media_event(event);
                    continue;
                }

                if (has_frame && sink_) {
                    sink_->on_media_frame(std::move(frame));
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (has_frame) {
                        ++stats_.delivered_frames;
                    }
                }
            }
        }

        std::shared_ptr<IMediaFrameSink> sink_;
        MediaSubscriptionOptions options_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::deque<MediaFrame> queue_;
        std::deque<MediaEvent> events_;
        std::size_t queued_bytes_ = 0;
        MediaDeliveryStats stats_;
        bool closed_ = false;
        std::thread worker_;
    };

    std::shared_ptr<Core> core_;
};

} // namespace

struct AxentHost::Impl {
    std::vector<std::shared_ptr<MediaSubscriptionState>> reset();
    std::optional<SessionLease> lease_for_session_locked(const std::string& session_id) const;
    std::vector<std::shared_ptr<MediaSubscriptionState>> take_session_subscriptions_locked(
        const std::string& session_id);
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_for_session_locked(const std::string& session_id);
    bool is_axtp_device_locked(const std::string& device_id) const;
    std::optional<std::string> other_axtp_lease_device_locked(const std::string& device_id) const;
    bool has_lease_for_device_locked(const std::string& device_id) const;

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
    std::map<std::string, std::vector<std::weak_ptr<MediaSubscriptionState>>> subscriptions;
    std::map<std::string, std::string> media_owner_session_by_device;
    std::mutex dispatch_mutex;
};

std::vector<std::shared_ptr<MediaSubscriptionState>> AxentHost::Impl::reset()
{
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    for (auto& entry : subscriptions) {
        for (auto& weak_subscription : entry.second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.push_back(std::move(subscription));
            }
        }
    }
    subscriptions.clear();
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
    return subscriptions_to_close;
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

std::vector<std::shared_ptr<MediaSubscriptionState>> AxentHost::Impl::take_session_subscriptions_locked(
    const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    const auto it = subscriptions.find(session_id);
    if (it != subscriptions.end()) {
        for (auto& weak_subscription : it->second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.push_back(std::move(subscription));
            }
        }
        subscriptions.erase(it);
    }
    return subscriptions_to_close;
}

std::vector<std::shared_ptr<MediaSubscriptionState>> AxentHost::Impl::subscriptions_for_session_locked(
    const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaSubscriptionState>> result;
    const auto it = subscriptions.find(session_id);
    if (it == subscriptions.end()) {
        return result;
    }
    auto& session_subscriptions = it->second;
    for (auto weak_subscription = session_subscriptions.begin();
         weak_subscription != session_subscriptions.end();) {
        if (auto subscription = weak_subscription->lock()) {
            if (subscription->active()) {
                result.push_back(subscription);
            }
            ++weak_subscription;
        } else {
            weak_subscription = session_subscriptions.erase(weak_subscription);
        }
    }
    return result;
}

bool AxentHost::Impl::is_axtp_device_locked(const std::string& device_id) const
{
    if (!devices) {
        return false;
    }
    const auto device = devices->get(device_id);
    return device.has_value() && device->adapter == "axtp";
}

std::optional<std::string> AxentHost::Impl::other_axtp_lease_device_locked(
    const std::string& device_id) const
{
    for (const auto& entry : leases) {
        const auto& lease = entry.second;
        if (lease.device_id != device_id && is_axtp_device_locked(lease.device_id)) {
            return lease.device_id;
        }
    }
    return std::nullopt;
}

bool AxentHost::Impl::has_lease_for_device_locked(const std::string& device_id) const
{
    for (const auto& entry : leases) {
        if (entry.second.device_id == device_id) {
            return true;
        }
    }
    return false;
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
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::unique_ptr<Adapter> previous_axtp_adapter;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        previous_axtp_adapter = std::move(impl_->axtp_adapter);
        subscriptions_to_close = impl_->reset();
    }
    for (auto& subscription : subscriptions_to_close) {
        subscription->cancel();
    }
    if (auto* axtp_adapter = dynamic_cast<AxtpAdapter*>(previous_axtp_adapter.get())) {
        axtp_adapter->set_media_frame_callback({});
    }
    previous_axtp_adapter.reset();

    std::lock_guard<std::mutex> lock(impl_->mutex);
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
        if (auto* axtp_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get())) {
            axtp_adapter->set_media_frame_callback(
                [this](std::string device_id, MediaFrame frame) {
                    this->publish_media_frame_for_device(std::move(device_id), std::move(frame));
                });
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
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::unique_ptr<Adapter> axtp_adapter;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running && !impl_->broker) {
            return;
        }
        axtp_adapter = std::move(impl_->axtp_adapter);
        subscriptions_to_close = impl_->reset();
    }
    for (auto& subscription : subscriptions_to_close) {
        subscription->cancel();
    }
    if (auto* adapter = dynamic_cast<AxtpAdapter*>(axtp_adapter.get())) {
        adapter->set_media_frame_callback({});
    }
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

TransportDiagnostics AxentHost::transport_diagnostics() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (const auto* adapter = dynamic_cast<const AxtpAdapter*>(impl_->axtp_adapter.get())) {
        return adapter->diagnostics();
    }
    return {};
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
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    AxtpAdapter* media_adapter = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running || !impl_->devices) {
            return {false, "", request.device_id, request.client_id, false, "host not running",
                    ControlStatus::Unavailable};
        }
        const auto device = impl_->devices->get(request.device_id);
        if (!device.has_value()) {
            return {false, "", request.device_id, request.client_id, false, "device not found",
                    ControlStatus::NotFound};
        }
        if (request.media
            && impl_->media_owner_session_by_device.find(request.device_id)
                   != impl_->media_owner_session_by_device.end()) {
            return {false, "", request.device_id, request.client_id, false, "media lease busy",
                    ControlStatus::Busy};
        }
        if (device->adapter == "axtp") {
            const auto other_device =
                impl_->other_axtp_lease_device_locked(request.device_id);
            if (other_device.has_value()) {
                return {false,
                        "",
                        request.device_id,
                        request.client_id,
                        request.media,
                        "AXTP session busy for active device " + *other_device,
                        ControlStatus::Busy};
            }
        }
        if (request.media && device->adapter == "axtp") {
            media_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        }
    }

    if (media_adapter != nullptr) {
        std::string error;
        const auto status = media_adapter->open_session_status(request.device_id, error);
        if (status != ControlStatus::Ok) {
            return {false, "", request.device_id, request.client_id, true, error, status};
        }
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto device = impl_->devices->get(request.device_id);
    if (!device.has_value()) {
        return {false, "", request.device_id, request.client_id, false, "device not found",
                ControlStatus::NotFound};
    }
    if (request.media
        && impl_->media_owner_session_by_device.find(request.device_id)
               != impl_->media_owner_session_by_device.end()) {
        return {false, "", request.device_id, request.client_id, false, "media lease busy",
                ControlStatus::Busy};
    }
    if (device->adapter == "axtp") {
        const auto other_device = impl_->other_axtp_lease_device_locked(request.device_id);
        if (other_device.has_value()) {
            return {false,
                    "",
                    request.device_id,
                    request.client_id,
                    request.media,
                    "AXTP session busy for active device " + *other_device,
                    ControlStatus::Busy};
        }
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
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string reset_device_id;
    AxtpAdapter* reset_adapter = nullptr;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
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
        subscriptions_to_close = impl_->take_session_subscriptions_locked(session_id);
        if (lease.has_value()) {
            impl_->sessions.close_device_session(session_id);
        }
        impl_->leases.erase(session_id);
        if (lease.has_value() &&
            impl_->is_axtp_device_locked(lease->device_id) &&
            !impl_->has_lease_for_device_locked(lease->device_id)) {
            reset_device_id = lease->device_id;
            reset_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        }
    }
    for (auto& subscription : subscriptions_to_close) {
        subscription->cancel();
    }
    if (reset_adapter != nullptr && !reset_device_id.empty()) {
        reset_adapter->reset_session_for_device(reset_device_id);
    }
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

MediaSubscriptionPtr AxentHost::subscribe_media(const std::string& session_id,
                                                std::shared_ptr<IMediaFrameSink> sink,
                                                MediaSubscriptionOptions options)
{
    if (!sink) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value() || !lease->media) {
        return nullptr;
    }

    auto subscription = std::make_shared<MediaSubscriptionState>(std::move(sink), options);
    impl_->subscriptions[session_id].push_back(subscription);
    return subscription;
}

bool AxentHost::publish_media_frame(const std::string& session_id, MediaFrame frame)
{
    std::shared_ptr<MediaStreamRelay> relay;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return false;
        }
        const auto relay_it = impl_->relays.find(session_id);
        subscriptions = impl_->subscriptions_for_session_locked(session_id);
        if ((relay_it == impl_->relays.end() || !relay_it->second) && subscriptions.empty()) {
            return false;
        }
        frame.session_id = session_id;
        if (relay_it != impl_->relays.end()) {
            relay = relay_it->second;
        }
    }
    if (relay) {
        relay->publish(frame);
    }
    for (const auto& subscription : subscriptions) {
        subscription->publish(frame);
    }
    return true;
}

bool AxentHost::publish_media_frame_for_device(std::string device_id, MediaFrame frame)
{
    std::shared_ptr<MediaStreamRelay> relay;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto owner = impl_->media_owner_session_by_device.find(device_id);
        if (owner == impl_->media_owner_session_by_device.end()) {
            return false;
        }
        const auto lease = impl_->lease_for_session_locked(owner->second);
        if (!lease.has_value() || !lease->media) {
            return false;
        }
        const auto relay_it = impl_->relays.find(owner->second);
        subscriptions = impl_->subscriptions_for_session_locked(owner->second);
        if ((relay_it == impl_->relays.end() || !relay_it->second) && subscriptions.empty()) {
            return false;
        }
        frame.session_id = owner->second;
        frame.device_id = std::move(device_id);
        if (relay_it != impl_->relays.end()) {
            relay = relay_it->second;
        }
    }
    if (relay) {
        relay->publish(frame);
    }
    for (const auto& subscription : subscriptions) {
        subscription->publish(frame);
    }
    return true;
}

ControlResult AxentHost::call(const std::string& session_id,
                              const std::string& method,
                              const nlohmann::json& params)
{
    ControlCommand command;
    Broker* broker = nullptr;
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->broker) {
            return {ControlStatus::Unavailable, {{"error", "host not running"}}};
        }
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value()) {
            return {ControlStatus::NotFound, {{"error", "session not found"}}};
        }

        broker = impl_->broker.get();
        command.request_id = session_id + ":" + method;
        command.control_session_id = lease->client_id;
        command.device_id = lease->device_id;
        command.method = method;
        command.params = params;
    }
    return broker->dispatch(command);
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
