#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "axent/media/media_relay.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

axent::MediaFrame make_frame(std::uint64_t sequence_id, std::size_t bytes)
{
    axent::MediaFrame frame;
    frame.session_id = "session-1";
    frame.stream_id = 0x1001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = sequence_id;
    frame.cursor = sequence_id * 1000U;
    frame.flags = sequence_id == 1 ? axent::MediaFrameFlag::KeyFrame : axent::MediaFrameFlag::EndOfFrame;
    frame.payload.assign(bytes, static_cast<std::uint8_t>(sequence_id));
    return frame;
}

} // namespace

int main()
{
    axent::MediaRelayOptions options;
    options.max_frames = 2;
    options.max_bytes = 0;

    axent::MediaStreamRelay relay(options);
    relay.publish(make_frame(1, 4));
    relay.publish(make_frame(2, 5));
    relay.publish(make_frame(3, 6));

    const auto stats_after_drop = relay.stats();
    require(stats_after_drop.published_frames == 3, "published frame count mismatch");
    require(stats_after_drop.dropped_frames == 1, "drop count mismatch");
    require(stats_after_drop.dropped_bytes == 4, "drop byte count mismatch");
    require(stats_after_drop.queued_frames == 2, "queued frame count mismatch");
    require(stats_after_drop.queued_bytes == 11, "queued byte count mismatch");

    auto first = relay.read();
    require(first.has_value(), "first frame missing");
    require(first->sequence_id == 2, "oldest retained frame should be sequence 2");
    require(axent::has_flag(first->flags, axent::MediaFrameFlag::Discontinuity),
            "first retained frame should carry discontinuity");

    auto second = relay.read();
    require(second.has_value(), "second frame missing");
    require(second->sequence_id == 3, "second retained frame should be sequence 3");

    auto empty = relay.read();
    require(!empty.has_value(), "relay should be empty");

    relay.close();
    require(relay.closed(), "relay should report closed");
    relay.publish(make_frame(4, 7));
    require(!relay.read().has_value(), "closed relay should not accept frames");

    return 0;
}
