#include <cstdint>
#include <stdexcept>
#include <string>

#include "axent/media/media_frame.hpp"
#include "axent/transport/types.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    axent::TransportSelector selector;
    selector.kind = axent::TransportKind::Hid;
    selector.vendor_id = 0x0581;
    selector.product_id = 0x2581;
    selector.usage_page = 0x0081;
    selector.report_id = 0x05;
    selector.input_report_size = 0;
    selector.output_report_size = 0;

    require(selector.kind == axent::TransportKind::Hid, "selector kind mismatch");
    require(selector.input_report_size == 0, "zero input report size should mean auto");
    require(selector.output_report_size == 0, "zero output report size should mean auto");

    axent::TransportDescriptor descriptor;
    descriptor.id = "hid:mock";
    descriptor.kind = axent::TransportKind::Hid;
    descriptor.path = "mock-path";
    descriptor.vendor_id = selector.vendor_id;
    descriptor.product_id = selector.product_id;
    descriptor.online = true;

    require(descriptor.id == "hid:mock", "descriptor id mismatch");
    require(descriptor.online, "descriptor online mismatch");

    axent::MediaFrame frame;
    frame.session_id = "session-1";
    frame.stream_id = 0x1001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = 7;
    frame.cursor = 123456;
    frame.flags = axent::MediaFrameFlag::KeyFrame | axent::MediaFrameFlag::EndOfFrame;
    frame.payload = {0x00, 0x00, 0x01, 0x65};

    require(axent::has_flag(frame.flags, axent::MediaFrameFlag::KeyFrame), "keyframe flag missing");
    require(axent::has_flag(frame.flags, axent::MediaFrameFlag::EndOfFrame), "end-of-frame flag missing");
    require(!axent::has_flag(
                axent::MediaFrameFlag::KeyFrame,
                axent::MediaFrameFlag::KeyFrame | axent::MediaFrameFlag::EndOfFrame),
            "composite flag check should require all bits");
    require(!axent::has_flag(frame.flags, axent::MediaFrameFlag::Discontinuity), "unexpected discontinuity");
    require(frame.payload.size() == 4, "payload size mismatch");

    return 0;
}
