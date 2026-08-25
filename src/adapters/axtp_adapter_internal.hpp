#pragma once

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "axent/core/types.hpp"
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

struct AxtpDiscoveryProjection {
    std::vector<DeviceSnapshot> devices;
    std::map<std::string, TransportDescriptor> descriptors;
    std::set<std::string> ambiguous_device_ids;
};

AxtpDiscoveryProjection project_hid_devices(
    const TransportSelector& selector,
    const std::vector<axent::transport::HidDeviceInfo>& hid_devices,
    EndpointDeliveryMode endpoint_delivery_mode);

} // namespace axent::detail
