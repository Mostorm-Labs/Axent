#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/core/types.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_relay.hpp"

namespace axent {

class Broker;
class MediaStreamRelay;

struct AxentHostOptions {
    bool enable_mock_adapter = true;
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
    void stop();
    bool running() const;

    std::vector<DeviceSnapshot> discover_devices() const;
    void upsert_device(DeviceSnapshot snapshot);
    SessionLease acquire_session(const SessionAcquireRequest& request);
    void release_session(const std::string& session_id, const std::string& reason);

    std::unique_ptr<MediaConsumer> create_media_consumer(const std::string& session_id,
                                                         MediaRelayOptions options);
    bool publish_media_frame(const std::string& session_id, MediaFrame frame);

    ControlResult call(const std::string& session_id,
                       const std::string& method,
                       const nlohmann::json& params);

    // AxentHost serializes its own lifecycle/session/media state. The returned
    // reference is valid only until stop() or the next start(), and those calls
    // must not race with users such as ControlPlane that hold this reference.
    Broker& broker();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
