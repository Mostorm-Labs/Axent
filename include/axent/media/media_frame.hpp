#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace axent {

using Bytes = std::vector<std::uint8_t>;

struct StreamKey {
    // Logical Host media lease. A lease handoff changes this value without
    // changing the physical stream generation.
    std::string session_id;
    std::uint32_t stream_id = 0;
    // Monotonic for each physical stream id within one adapter lifetime.
    // Zero is reserved for frames without descriptor lifecycle provenance.
    std::uint64_t generation = 0;
};

inline bool operator==(const StreamKey& lhs, const StreamKey& rhs)
{
    return lhs.session_id == rhs.session_id &&
        lhs.stream_id == rhs.stream_id &&
        lhs.generation == rhs.generation;
}

inline bool operator!=(const StreamKey& lhs, const StreamKey& rhs)
{
    return !(lhs == rhs);
}

enum class MediaKind {
    Unknown,
    Video,
    Audio,
};

enum class MediaCodec {
    Unknown,
    H264,
    Aac,
    Pcm,
    Opaque,
};

enum class MediaFrameFlag : std::uint32_t {
    None = 0,
    KeyFrame = 1U << 0U,
    Config = 1U << 1U,
    Discontinuity = 1U << 2U,
    EndOfFrame = 1U << 3U,
};

inline MediaFrameFlag operator|(MediaFrameFlag lhs, MediaFrameFlag rhs)
{
    return static_cast<MediaFrameFlag>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

inline MediaFrameFlag& operator|=(MediaFrameFlag& lhs, MediaFrameFlag rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool has_flag(MediaFrameFlag flags, MediaFrameFlag flag)
{
    const auto bits = static_cast<std::uint32_t>(flags);
    const auto mask = static_cast<std::uint32_t>(flag);
    return (bits & mask) == mask;
}

struct MediaFrame {
    std::string session_id;
    std::string device_id;
    std::uint32_t stream_id = 0;
    MediaKind kind = MediaKind::Unknown;
    MediaCodec codec = MediaCodec::Unknown;
    std::uint64_t sequence_id = 0;
    std::uint64_t cursor = 0;
    std::uint64_t timestamp_us = 0;
    MediaFrameFlag flags = MediaFrameFlag::None;
    Bytes payload;
    // Appended for source compatibility with positional aggregate users of
    // the legacy frame façade. Zero means no lifecycle generation is bound.
    std::uint64_t generation = 0;
};

inline StreamKey stream_key(const MediaFrame& frame)
{
    return {frame.session_id, frame.stream_id, frame.generation};
}

} // namespace axent
