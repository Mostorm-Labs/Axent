#pragma once

#include <cstdint>
#include <string>

#include "axent/media/media_frame.hpp"

namespace axent {

// Stable, JSON-free description of one opened media stream generation.
// Fields that are not present in the AXTP open result retain the documented
// neutral values below instead of exposing transport-specific payloads.
struct MediaStreamDescriptor {
    StreamKey key;
    std::string device_id;
    MediaKind kind = MediaKind::Unknown;
    MediaCodec codec = MediaCodec::Unknown;
    std::string source;
    std::string transport_format;
    std::string stream_profile;
    std::string cursor_unit;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frame_rate = 0;
};

enum class MediaStreamEventKind {
    Opened,
    Closed,
};

enum class MediaStreamEventReason {
    Unspecified,
    Snapshot,
    InitialOpen,
    SourceRecovery,
    SessionRecovery,
    ParameterReconfigure,
    Shutdown,
};

struct MediaStreamEvent {
    MediaStreamEventKind kind = MediaStreamEventKind::Opened;
    MediaStreamDescriptor descriptor;
    MediaStreamEventReason reason = MediaStreamEventReason::Unspecified;
};

enum class MediaDeliveryEventKind {
    DeliveryDropped,
    SubscriptionClosed,
};

struct MediaDeliveryEvent {
    MediaDeliveryEventKind kind = MediaDeliveryEventKind::SubscriptionClosed;
    // DeliveryDropped values are cumulative for the subscription lifetime.
    std::uint64_t dropped_frames = 0;
    std::uint64_t dropped_bytes = 0;
};

} // namespace axent
