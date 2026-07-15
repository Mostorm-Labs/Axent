#include "axent/media/video_stream_params.hpp"

namespace axent {

const char* video_stream_params_state_name(VideoStreamParamsStateKind state) noexcept
{
    switch (state) {
    case VideoStreamParamsStateKind::Idle: return "idle";
    case VideoStreamParamsStateKind::Pending: return "pending";
    case VideoStreamParamsStateKind::Applied: return "applied";
    case VideoStreamParamsStateKind::Failed: return "failed";
    case VideoStreamParamsStateKind::RolledBack: return "rolledBack";
    case VideoStreamParamsStateKind::Unchanged: return "unchanged";
    }
    return "idle";
}

const char* video_stream_params_phase_name(VideoStreamParamsPhase phase) noexcept
{
    switch (phase) {
    case VideoStreamParamsPhase::Idle: return "idle";
    case VideoStreamParamsPhase::Pending: return "pending";
    case VideoStreamParamsPhase::Closing: return "closing";
    case VideoStreamParamsPhase::Opening: return "opening";
    case VideoStreamParamsPhase::Streaming: return "streaming";
    case VideoStreamParamsPhase::Failed: return "failed";
    case VideoStreamParamsPhase::RolledBack: return "rolledBack";
    }
    return "idle";
}

} // namespace axent
