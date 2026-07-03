#include "axent/media/media_relay.hpp"

#include <utility>

namespace axent {

MediaStreamRelay::MediaStreamRelay(MediaRelayOptions options)
    : options_(options)
{
}

void MediaStreamRelay::publish(MediaFrame frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
        return;
    }

    queued_bytes_ += frame.payload.size();
    queue_.push_back(std::move(frame));
    ++stats_.published_frames;

    bool dropped = false;
    while (over_limit_locked() && !queue_.empty()) {
        drop_front_locked();
        dropped = true;
    }

    if (dropped && !queue_.empty()) {
        queue_.front().flags |= MediaFrameFlag::Discontinuity;
    }

    stats_.queued_frames = queue_.size();
    stats_.queued_bytes = queued_bytes_;
}

std::optional<MediaFrame> MediaStreamRelay::read()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }

    MediaFrame frame = std::move(queue_.front());
    queued_bytes_ -= frame.payload.size();
    queue_.pop_front();
    ++stats_.read_frames;
    stats_.queued_frames = queue_.size();
    stats_.queued_bytes = queued_bytes_;
    return frame;
}

void MediaStreamRelay::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    queue_.clear();
    queued_bytes_ = 0;
    stats_.queued_frames = 0;
    stats_.queued_bytes = 0;
}

bool MediaStreamRelay::closed() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

MediaRelayStats MediaStreamRelay::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    MediaRelayStats snapshot = stats_;
    snapshot.queued_frames = queue_.size();
    snapshot.queued_bytes = queued_bytes_;
    return snapshot;
}

bool MediaStreamRelay::over_limit_locked() const
{
    const bool frame_limited = options_.max_frames != 0 && queue_.size() > options_.max_frames;
    const bool byte_limited = options_.max_bytes != 0 && queued_bytes_ > options_.max_bytes;
    return frame_limited || byte_limited;
}

void MediaStreamRelay::drop_front_locked()
{
    const auto dropped_bytes = queue_.front().payload.size();
    queued_bytes_ -= dropped_bytes;
    queue_.pop_front();
    ++stats_.dropped_frames;
    stats_.dropped_bytes += dropped_bytes;
}

} // namespace axent
