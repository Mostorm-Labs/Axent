#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "axent/core/adapter.hpp"
#include "axent/media/media_frame.hpp"
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
    std::string video_source = "wireless_cast";
    std::string audio_source = "wireless_cast_audio";
};

class AxentHost;

class AxtpAdapter final : public Adapter {
public:
    using MediaFrameCallback = std::function<void(std::string device_id, MediaFrame frame)>;

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
    ControlStatus open_session_status(const std::string& device_id, std::string& error);
    bool open_session(const std::string& device_id, std::string& error);

private:
    friend class AxentHost;
    friend class testing::AxtpAdapterTestSeam;

    AxtpAdapter(AxtpAdapterConfig config,
                std::shared_ptr<detail::AxtpAdapterRuntimeFactory> runtime_factory);

    std::thread request_stop_session_pump_locked();
    bool ensure_session_locked(const std::string& device_id,
                               std::string& error,
                               ControlStatus& status,
                               bool configure_media);
    void refresh_diagnostics_locked();
    void record_transport_trace(const std::string& event_name,
                                bool accepted_read,
                                bool write_report,
                                bool read_error,
                                bool write_error,
                                bool dropped_report,
                                const std::string& message);
    void drop_pending_media_frames_for_device(const std::string& device_id);
    void reset_session_for_device(const std::string& device_id);
    bool media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const;
    void configure_media_streams();
    void clear_media_streams();
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
    void drain_pending_media_callbacks();

    AxtpAdapterConfig config_;
    struct RuntimeState;
    std::unique_ptr<RuntimeState> runtime_;
    MediaFrameCallback media_frame_callback_;
    mutable std::mutex media_callback_mutex_;
    struct ActiveMediaStream {
        MediaKind kind = MediaKind::Unknown;
        MediaCodec codec = MediaCodec::Opaque;
        std::string source;
    };
    mutable std::mutex media_stream_mutex_;
    std::map<std::uint32_t, ActiveMediaStream> active_media_streams_;
    std::mutex pending_media_mutex_;
    std::queue<std::pair<std::string, MediaFrame>> pending_media_frames_;
    mutable std::mutex mutex_;
    mutable std::mutex session_mutex_;
    mutable std::mutex client_mutex_;
    TransportDiagnostics diagnostics_;
    std::chrono::steady_clock::time_point next_media_configure_attempt_;
    std::uint32_t media_configure_attempts_ = 0;
    std::string active_device_id_;
    std::atomic<bool> stop_session_pump_{false};
    std::thread session_pump_;
};

} // namespace axent
