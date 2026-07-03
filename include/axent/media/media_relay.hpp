#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

#include "axent/media/media_frame.hpp"

namespace axent {

struct MediaRelayOptions {
    std::size_t max_frames = 64;
    std::size_t max_bytes = 16 * 1024 * 1024;
};

struct MediaRelayStats {
    std::uint64_t published_frames = 0;
    std::uint64_t read_frames = 0;
    std::uint64_t dropped_frames = 0;
    std::uint64_t dropped_bytes = 0;
    std::size_t queued_frames = 0;
    std::size_t queued_bytes = 0;
};

class MediaStreamRelay {
public:
    explicit MediaStreamRelay(MediaRelayOptions options = {});

    void publish(MediaFrame frame);
    std::optional<MediaFrame> read();
    void close();
    bool closed() const;
    MediaRelayStats stats() const;

private:
    bool over_limit_locked() const;
    void drop_front_locked();

    MediaRelayOptions options_;
    mutable std::mutex mutex_;
    std::deque<MediaFrame> queue_;
    std::size_t queued_bytes_ = 0;
    MediaRelayStats stats_;
    bool closed_ = false;
};

} // namespace axent
