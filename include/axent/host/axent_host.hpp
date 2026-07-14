#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/core/types.hpp"
#include "axent/firmware/firmware_update_service.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_relay.hpp"
#include "axent/media/media_subscription.hpp"

namespace axent {

class Broker;
class MediaStreamRelay;

struct AxentHostOptions {
    AxentHostOptions();

    using AdapterFactory = std::function<std::unique_ptr<Adapter>(AxtpAdapterConfig)>;

    bool enable_mock_adapter = true;
    bool enable_axtp_adapter = false;
    AxtpAdapterConfig axtp;
    AdapterFactory axtp_adapter_factory;
};

struct SessionAcquireRequest {
    std::string client_id;
    std::string device_id;
    bool media = false;
};

struct SessionLease {
    bool acquired = false;
    std::string session_id;
    std::string device_id;
    std::string client_id;
    bool media = false;
    std::string reason;
    ControlStatus status = ControlStatus::Ok;
};

class MediaConsumer {
public:
    std::optional<MediaFrame> read();
    MediaRelayStats stats() const;

private:
    friend class AxentHost;

    explicit MediaConsumer(std::shared_ptr<MediaStreamRelay> relay);

    std::shared_ptr<MediaStreamRelay> relay_;
};

class AxentHost {
public:
    AxentHost();
    ~AxentHost();

    AxentHost(const AxentHost&) = delete;
    AxentHost& operator=(const AxentHost&) = delete;
    AxentHost(AxentHost&&) = delete;
    AxentHost& operator=(AxentHost&&) = delete;

    bool start(AxentHostOptions options = {});
    // Host lifecycle mutation is not re-entrant from media sink callbacks:
    // start returns false, acquire returns Busy, and stop/release throw
    // std::logic_error. Media publication also returns false. Subscription
    // cancel and call() remain supported.
    void stop();
    bool running() const;

    std::vector<DeviceSnapshot> discover_devices() const;
    TransportDiagnostics transport_diagnostics() const;
    void upsert_device(DeviceSnapshot snapshot);
    SessionLease acquire_session(const SessionAcquireRequest& request);
    void release_session(const std::string& session_id, const std::string& reason);
    firmware::MaintenanceLeaseProvider& maintenance_lease_provider();

    std::unique_ptr<MediaConsumer> create_media_consumer(const std::string& session_id,
                                                         MediaRelayOptions options);
    MediaSubscriptionPtr subscribe_media(const std::string& session_id,
                                         std::shared_ptr<IMediaFrameSink> sink,
                                         MediaSubscriptionOptions options = {});
    MediaStreamSubscriptionPtr subscribe_media_stream(
        const std::string& session_id,
        std::shared_ptr<IMediaStreamSink> sink,
        MediaSubscriptionOptions options = {});
    bool publish_media_stream_event(const std::string& session_id,
                                    MediaStreamEvent event);
    bool publish_media_frame(const std::string& session_id, MediaFrame frame);

    ControlResult call(const std::string& session_id,
                       const std::string& method,
                       const nlohmann::json& params);

    // AxentHost serializes its own lifecycle/session/media state. The returned
    // reference is valid only until stop() or the next start(), and those calls
    // must not race with users such as ControlPlane that hold this reference.
    Broker& broker();

private:
    bool reserve_maintenance(const std::string& device_id,
                             std::string& reason,
                             std::function<void()>& release);
    bool publish_media_stream_event_for_session(const std::string& session_id,
                                                MediaStreamEvent event);
    bool publish_media_frame_for_device(std::string device_id, MediaFrame frame);
    bool publish_media_stream_event_for_device(MediaStreamEvent event);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
