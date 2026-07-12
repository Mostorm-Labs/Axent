#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "axent/media/media_frame.hpp"
#include "axent/media/media_stream.hpp"

namespace axent {

enum class MediaDeliveryMode {
    LatestDecodable,
    Realtime,
    LosslessBounded,
};

enum class MediaSubscriptionDispatch {
    // Callbacks are serialized inline on their publishing thread; a publisher
    // does not return before its item is consumed. Host media publication from
    // inside a media sink callback is rejected to keep FIFO non-reentrant.
    Direct,
    // One subscription-owned worker drains the bounded FIFO.
    AsyncQueued,
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
    MediaSubscriptionDispatch dispatch = MediaSubscriptionDispatch::Direct;
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

class IMediaStreamSink {
public:
    virtual ~IMediaStreamSink() = default;

    virtual void on_media_stream_event(MediaStreamEvent event) = 0;
    virtual void on_media_stream_frame(MediaFrame frame) = 0;
    virtual void on_media_delivery_event(MediaDeliveryEvent event) = 0;
};

class MediaStreamSubscription {
public:
    virtual ~MediaStreamSubscription() = default;

    // From an external thread, return guarantees that no callback remains in
    // flight and no future callback will start. Self-cancel returns inside the
    // current callback and suppresses nested terminal callbacks.
    virtual void cancel() = 0;
    virtual MediaDeliveryStats stats() const = 0;
};

using MediaStreamSubscriptionPtr = std::shared_ptr<MediaStreamSubscription>;

} // namespace axent
