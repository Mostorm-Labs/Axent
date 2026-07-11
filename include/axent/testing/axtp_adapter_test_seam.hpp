#pragma once

#include <functional>
#include <memory>

#include "axent/adapters/axtp_adapter.hpp"

namespace axtp {
class ITransport;
struct HidDeviceInfo;
struct HidReportTrace;
struct HidTransportOptions;
} // namespace axtp

namespace axent::testing {

class AxtpAdapterTestSeam final {
public:
    using TransportFactory =
        std::function<std::unique_ptr<axtp::ITransport>(const axtp::HidTransportOptions&)>;

    static std::unique_ptr<AxtpAdapter> make(AxtpAdapterConfig config,
                                             TransportFactory transport_factory);
    static axtp::HidTransportOptions hid_options_from_selector(const TransportSelector& selector);
    static TransportDescriptor descriptor_from_hid_device(const axtp::HidDeviceInfo& device);
    static bool matches_selector(const AxtpAdapter& adapter, const axtp::HidDeviceInfo& device);
    static void record_hid_trace(AxtpAdapter& adapter, const axtp::HidReportTrace& trace);
    static void disconnect_session(AxtpAdapter& adapter);
};

} // namespace axent::testing
