#pragma once

#include <memory>

#include "axent/transport/types.hpp"
#include "transports/hidapi/hid_transport.hpp"

namespace axent::detail {

class AxtpAdapterRuntimeFactory {
public:
    virtual ~AxtpAdapterRuntimeFactory() = default;
    virtual std::unique_ptr<axtp::ITransport> create(const axtp::HidTransportOptions& options) = 0;
};

std::shared_ptr<AxtpAdapterRuntimeFactory> make_default_axtp_runtime_factory();
axtp::HidTransportOptions hid_options_from_selector(const TransportSelector& selector);
TransportDescriptor descriptor_from_hid_device(const axtp::HidDeviceInfo& device);
bool matches_selector(const TransportSelector& selector, const axtp::HidDeviceInfo& device);

} // namespace axent::detail
