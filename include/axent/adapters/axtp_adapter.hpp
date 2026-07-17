#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "axent/core/adapter.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_stream.hpp"
#include "axent/media/video_stream_params.hpp"
#include "axent/transport/types.hpp"

namespace axent {

namespace detail {
class AxtpAdapterRuntimeFactory;
} // namespace detail

namespace testing {
class AxtpAdapterTestSeam;
} // namespace testing

struct AxtpAdapterConfig {
    TransportSelector selector;
    bool enable_media = true;
    bool enable_video = true;
    bool enable_audio = true;
    std::uint32_t audio_sample_rate = 48000;
    std::uint32_t audio_channels = 2;
    std::optional<std::uint32_t> video_frame_rate;
    std::string video_source = "wireless_cast";
    std::string audio_source = "wireless_cast_audio";
    bool enable_session_health_probe = true;
    std::uint32_t session_health_probe_interval_ms = 1000;
    std::uint32_t session_health_probe_timeout_ms = 250;
    std::uint32_t session_health_failure_threshold = 2;
    std::uint32_t session_recovery_backoff_initial_ms = 1000;
    std::uint32_t session_recovery_backoff_max_ms = 5000;
};

class AxentHost;

class AxtpAdapter final : public Adapter {
public:
    using MediaFrameCallback = std::function<void(std::string device_id, MediaFrame frame)>;
    using MediaStreamEventCallback = std::function<void(MediaStreamEvent event)>;

    AxtpAdapter();
    explicit AxtpAdapter(AxtpAdapterConfig config);
    ~AxtpAdapter() override;

    static AxtpAdapterConfig na20_defaults();
    static DeviceSnapshot snapshot_from_descriptor(const TransportDescriptor& descriptor);

    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;

    TransportDiagnostics diagnostics() const;
    void set_media_frame_callback(MediaFrameCallback callback);
    void set_media_stream_event_callback(MediaStreamEventCallback callback);
    std::vector<MediaStreamDescriptor> active_media_stream_descriptors() const;
    ControlStatus open_session_status(const std::string& device_id, std::string& error);
    bool open_session(const std::string& device_id, std::string& error);
    VideoStreamParamsResult set_video_stream_params(
        const std::string& device_id,
        const VideoStreamParamsRequest& request);
    VideoStreamParamsState video_stream_params_state(const std::string& device_id) const;
    VideoStreamParamsSubscriptionPtr subscribe_video_stream_params(
        const std::string& device_id,
        VideoStreamParamsObserver observer);

private:
    friend class AxentHost;
    friend class testing::AxtpAdapterTestSeam;

    AxtpAdapter(AxtpAdapterConfig config,
                std::shared_ptr<detail::AxtpAdapterRuntimeFactory> runtime_factory);

    std::thread request_stop_session_pump_locked();
    bool ensure_session_locked(const std::string& device_id,
                               std::string& error,
                               ControlStatus& status,
                               bool configure_media,
                               MediaStreamEventReason open_reason =
                                   MediaStreamEventReason::InitialOpen);
    void refresh_diagnostics_locked();
    void record_transport_trace(const std::string& event_name,
                                bool accepted_read,
                                bool write_report,
                                bool read_error,
                                bool write_error,
                                bool dropped_report,
                                const std::string& message);
    void drop_pending_media_frames_for_device(const std::string& device_id);
    void bind_media_delivery_session(const std::string& device_id,
                                     const std::string& session_id);
    void unbind_media_delivery_session(const std::string& device_id,
                                       const std::string& session_id);
    void reset_session_for_device(const std::string& device_id);
    bool media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const;
    void configure_media_streams(
        const std::string& device_id,
        MediaStreamEventReason open_reason = MediaStreamEventReason::InitialOpen);
    bool configure_media_stream_kind(const std::string& device_id,
                                     MediaKind kind,
                                     bool update_retry_state,
                                     bool from_video_reconfigure,
                                     MediaStreamEventReason open_reason =
                                         MediaStreamEventReason::InitialOpen);
    void advance_video_reconfigure(const std::string& device_id);
    void clear_video_stream_params_session(bool preserve_logical_session = false);
    void notify_video_stream_params_state(VideoStreamParamsState state);
    void retry_pending_media_source_recoveries(
        const std::string& device_id,
        std::chrono::steady_clock::time_point now);
    void clear_media_streams(MediaStreamEventReason reason = MediaStreamEventReason::Shutdown);
    void enqueue_media_stream_events(std::vector<MediaStreamEvent> events);
    struct MediaSourceStateEvent {
        std::uint32_t event_id = 0;
        std::string event_name;
        MediaKind kind = MediaKind::Unknown;
        std::string source;
        std::string state;
        std::string reason;
        std::uint32_t active_stream_id = 0;
        bool has_active_stream_id = false;
        bool valid = false;
    };
    void enqueue_media_source_state_event(MediaSourceStateEvent event);
    void process_pending_media_source_state_events(const std::string& device_id);
    void process_media_source_state_event(const std::string& device_id,
                                          const MediaSourceStateEvent& event);
    void handle_stream_payload(const std::string& device_id,
                               std::uint32_t stream_id,
                               std::uint32_t sequence_id,
                               std::uint64_t cursor,
                               std::vector<std::uint8_t> data);
    MediaFrame frame_from_stream(const std::string& device_id,
                                 std::uint32_t stream_id,
                                 std::uint32_t sequence_id,
                                 std::uint64_t cursor,
                                 std::vector<std::uint8_t> data) const;
    bool is_current_media_frame(const MediaFrame& frame) const;
    void drain_pending_media_callbacks();
    void request_session_recovery(const std::string& device_id, std::string reason);
    void run_session_recovery_worker();
    bool recover_session_once(const std::string& device_id,
                              std::string& error,
                              std::uint64_t recovery_generation);
    bool session_health_probe_due(std::chrono::steady_clock::time_point now);
    bool run_session_health_probe(const std::string& device_id);
    void set_session_health(SessionHealthState state,
                            std::uint32_t probe_failures,
                            std::string reason = {});
    bool has_media_delivery_session(const std::string& device_id) const;

    AxtpAdapterConfig config_;
    struct RuntimeState;
    std::unique_ptr<RuntimeState> runtime_;
    MediaFrameCallback media_frame_callback_;
    MediaStreamEventCallback media_stream_event_callback_;
    mutable std::mutex media_callback_mutex_;
    std::recursive_mutex media_callback_dispatch_mutex_;
    bool draining_media_callbacks_ = false;
    struct ActiveMediaStream {
        MediaStreamDescriptor descriptor;
    };
    mutable std::mutex media_stream_mutex_;
    std::map<std::uint32_t, ActiveMediaStream> active_media_streams_;
    std::map<std::uint32_t, std::uint64_t> media_stream_generations_;
    // Source recovery closes both receiver-pull legs once before reopening.
    std::optional<MediaStreamDescriptor> pending_video_source_recovery_close_;
    std::optional<MediaStreamDescriptor> pending_audio_source_recovery_close_;
    bool source_recovery_cycle_active_ = false;
    bool source_recovery_close_sent_ = false;
    std::uint8_t source_recovery_reopen_mask_ = 0;
    std::mutex pending_media_event_mutex_;
    std::queue<MediaStreamEvent> pending_media_stream_events_;
    std::mutex pending_media_source_state_mutex_;
    std::queue<MediaSourceStateEvent> pending_media_source_state_events_;
    std::mutex pending_media_mutex_;
    std::queue<std::pair<std::string, MediaFrame>> pending_media_frames_;
    mutable std::mutex media_delivery_session_mutex_;
    std::map<std::string, std::string> media_delivery_sessions_;
    mutable std::mutex mutex_;
    mutable std::mutex session_mutex_;
    mutable std::mutex client_mutex_;
    TransportDiagnostics diagnostics_;
    std::chrono::steady_clock::time_point next_media_configure_attempt_;
    std::uint32_t media_configure_attempts_ = 0;
    bool video_source_terminal_ = false;
    bool audio_source_terminal_ = false;
    bool video_source_recovery_pending_ = false;
    bool audio_source_recovery_pending_ = false;
    std::chrono::steady_clock::time_point next_video_source_recovery_attempt_;
    std::chrono::steady_clock::time_point next_audio_source_recovery_attempt_;
    std::string active_device_id_;
    std::atomic<bool> stop_session_pump_{false};
    std::thread session_pump_;
    std::mutex recovery_mutex_;
    std::condition_variable recovery_cv_;
    std::thread recovery_worker_;
    bool stop_recovery_worker_ = false;
    bool recovery_requested_ = false;
    std::string recovery_device_id_;
    std::string recovery_reason_;
    std::uint32_t recovery_attempt_ = 0;
    std::chrono::steady_clock::time_point next_health_probe_;
    std::chrono::steady_clock::time_point last_transport_activity_;
    SessionHealthState session_health_ = SessionHealthState::Healthy;
    std::uint32_t health_probe_failures_ = 0;
    std::uint64_t session_recoveries_ = 0;
    std::string last_session_recovery_reason_;
    std::atomic<std::uint64_t> recovery_generation_{0};
};

} // namespace axent
