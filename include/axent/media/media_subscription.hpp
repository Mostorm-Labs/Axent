#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "axent/media/media_frame.hpp"

namespace axent {

enum class MediaDeliveryMode {
    LatestDecodable,
    Realtime,
    LosslessBounded,
};

enum class MediaEventKind {
    Closed,
    Dropped,
};

struct MediaEvent {
    MediaEventKind kind = MediaEventKind::Closed;
    std::uint64_t dropped_frames = 0;
    std::uint64_t dropped_bytes = 0;
};

struct MediaSubscriptionOptions {
    MediaDeliveryMode delivery_mode = MediaDeliveryMode::LatestDecodable;
    std::size_t max_frames = 2;
    std::size_t max_bytes = 2 * 1024 * 1024;
};

struct MediaDeliveryStats {
    std::uint64_t received_frames = 0;
    std::uint64_t delivered_frames = 0;
    std::uint64_t dropped_frames = 0;
    std::uint64_t dropped_bytes = 0;
    std::uint64_t slow_callbacks = 0;
    std::size_t queued_frames = 0;
    std::size_t queued_bytes = 0;
};

class IMediaFrameSink {
public:
    virtual ~IMediaFrameSink() = default;

    virtual void on_media_frame(MediaFrame frame) = 0;
    virtual void on_media_event(MediaEvent event) = 0;
};

class MediaSubscription {
public:
    virtual ~MediaSubscription() = default;

    virtual void cancel() = 0;
    virtual MediaDeliveryStats stats() const = 0;
};

using MediaSubscriptionPtr = std::shared_ptr<MediaSubscription>;

} // namespace axent
