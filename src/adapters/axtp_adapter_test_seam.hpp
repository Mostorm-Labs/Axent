#pragma once

#include <functional>
#include <memory>

#include "axent/adapters/axtp_adapter.hpp"
#include "hidapi/hid_transport.hpp"

namespace axent::testing {

class AxtpAdapterTestSeam final {
public:
    using TransportFactory = std::function<std::unique_ptr<axtp::ITransport>(
        const transport::HidTransportOptions&)>;

    static std::unique_ptr<AxtpAdapter> make(AxtpAdapterConfig config,
                                             TransportFactory transport_factory);
    static transport::HidTransportOptions hid_options_from_selector(
        const TransportSelector& selector);
    static TransportDescriptor descriptor_from_hid_device(
        const transport::HidDeviceInfo& device);
    static bool matches_selector(const AxtpAdapter& adapter,
                                 const transport::HidDeviceInfo& device);
    static void record_hid_trace(AxtpAdapter& adapter,
                                 const transport::HidReportTrace& trace);
    static void disconnect_session(AxtpAdapter& adapter);
    static void bind_media_delivery_session(AxtpAdapter& adapter,
                                             const std::string& device_id,
                                             const std::string& session_id);
    static void release_session(AxtpAdapter& adapter,
                                const std::string& device_id);
    static void stop_session_pump(AxtpAdapter& adapter);
    static void enqueue_stream_payload(AxtpAdapter& adapter,
                                       const std::string& device_id,
                                       std::uint32_t stream_id,
                                       std::uint32_t sequence_id,
                                       std::uint64_t cursor,
                                       std::vector<std::uint8_t> data);
    static void reopen_media_streams(AxtpAdapter& adapter,
                                     const std::string& device_id);
    static void drain_media_callbacks(AxtpAdapter& adapter);
    static bool is_current_media_frame(const AxtpAdapter& adapter,
                                       const MediaFrame& frame);
};

} // namespace axent::testing
