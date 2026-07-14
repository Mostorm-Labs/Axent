#pragma once

#include <memory>

#include "axent/transport/types.hpp"
#include "hidapi/hid_transport.hpp"

namespace axent::detail {

class AxtpAdapterRuntimeFactory {
public:
    virtual ~AxtpAdapterRuntimeFactory() = default;
    virtual std::unique_ptr<axtp::ITransport> create(const axent::transport::HidTransportOptions& options) = 0;
};

std::shared_ptr<AxtpAdapterRuntimeFactory> make_default_axtp_runtime_factory();
axent::transport::HidTransportOptions hid_options_from_selector(const TransportSelector& selector);
TransportDescriptor descriptor_from_hid_device(const axent::transport::HidDeviceInfo& device);
bool matches_selector(const TransportSelector& selector, const axent::transport::HidDeviceInfo& device);

} // namespace axent::detail
