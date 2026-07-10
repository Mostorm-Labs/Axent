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

namespace axtp {
struct BrokerContext;
class ITransport;
class HidTransport;
struct HidDeviceInfo;
struct HidReportTrace;
struct HidTransportOptions;
struct StreamPayload;
namespace sdk {
class AxtpClient;
} // namespace sdk
} // namespace axtp

namespace axent {

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
    using TransportFactory = std::function<std::unique_ptr<axtp::ITransport>(const axtp::HidTransportOptions&)>;
    using MediaFrameCallback = std::function<void(std::string device_id, MediaFrame frame)>;

    AxtpAdapter();
    explicit AxtpAdapter(AxtpAdapterConfig config);
    AxtpAdapter(AxtpAdapterConfig config, TransportFactory transport_factory);
    ~AxtpAdapter() override;

    static AxtpAdapterConfig na20_defaults();
    static axtp::HidTransportOptions hid_options_from_selector(const TransportSelector& selector);
    static TransportDescriptor descriptor_from_hid_device(const axtp::HidDeviceInfo& device);
    static DeviceSnapshot snapshot_from_descriptor(const TransportDescriptor& descriptor);

    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;

    bool matches_selector(const axtp::HidDeviceInfo& device) const;
    void record_hid_trace(const axtp::HidReportTrace& trace);
    TransportDiagnostics diagnostics() const;
    void set_media_frame_callback(MediaFrameCallback callback);
    bool open_session(const std::string& device_id, std::string& error);
    bool open_session_for_test(const std::string& device_id, std::string& error);

private:
    friend class AxentHost;

    std::thread request_stop_session_pump_locked();
    bool ensure_session_locked(const std::string& device_id, std::string& error, bool configure_media);
    void refresh_diagnostics_locked();
    void drop_pending_media_frames_for_device(const std::string& device_id);
    void reset_session_for_device(const std::string& device_id);
    bool media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const;
    void configure_media_streams(axtp::sdk::AxtpClient& client);
    void clear_media_streams();
    void handle_stream_payload(const std::string& device_id,
                               const axtp::BrokerContext& context,
                               const axtp::StreamPayload& stream);
    MediaFrame frame_from_stream(const std::string& device_id,
                                 const axtp::StreamPayload& stream) const;
    void drain_pending_media_callbacks();

    AxtpAdapterConfig config_;
    TransportFactory transport_factory_;
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
    std::unique_ptr<axtp::sdk::AxtpClient> client_;
    axtp::ITransport* active_transport_ = nullptr;
    std::string active_device_id_;
    std::atomic<bool> stop_session_pump_{false};
    std::thread session_pump_;
};

} // namespace axent
