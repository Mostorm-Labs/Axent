#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace axent {

enum class VideoStreamParamsStateKind {
    Idle,
    Pending,
    Applied,
    Failed,
    RolledBack,
    Unchanged,
};

enum class VideoStreamParamsPhase {
    Idle,
    Pending,
    Closing,
    Opening,
    Streaming,
    Failed,
    RolledBack,
};

struct VideoStreamParamsError {
    std::uint32_t code = 0;
    std::string message;
};

struct VideoStreamParamsRequest {
    std::optional<std::uint32_t> frame_rate;
    bool reset_frame_rate = false;
};

struct VideoStreamParamsState {
    std::string session_id;
    std::string source;
    std::optional<std::uint32_t> desired_frame_rate;
    std::optional<std::uint32_t> effective_frame_rate;
    std::string stream_profile;
    std::string reconfigure_id;
    VideoStreamParamsStateKind state = VideoStreamParamsStateKind::Idle;
    VideoStreamParamsPhase phase = VideoStreamParamsPhase::Idle;
    std::optional<std::uint32_t> previous_stream_id;
    std::optional<std::uint32_t> active_stream_id;
    bool rollback_applied = false;
    std::optional<VideoStreamParamsError> last_error;
    std::vector<std::string> changed_fields;
};

struct VideoStreamParamsResult {
    std::uint32_t status_code = 0;
    bool accepted = false;
    VideoStreamParamsState state;
};

using VideoStreamParamsObserver = std::function<void(const VideoStreamParamsState&)>;

class VideoStreamParamsSubscription {
public:
    virtual ~VideoStreamParamsSubscription() = default;
    virtual void cancel() noexcept = 0;
};

using VideoStreamParamsSubscriptionPtr = std::unique_ptr<VideoStreamParamsSubscription>;

const char* video_stream_params_state_name(VideoStreamParamsStateKind state) noexcept;
const char* video_stream_params_phase_name(VideoStreamParamsPhase phase) noexcept;

} // namespace axent
