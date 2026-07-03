#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include "axent/core/adapter.hpp"
#include "axent/transport/types.hpp"

namespace axtp {
class ITransport;
class HidTransport;
struct HidDeviceInfo;
struct HidReportTrace;
struct HidTransportOptions;
namespace sdk {
class AxtpClient;
} // namespace sdk
} // namespace axtp

namespace axent {

struct AxtpAdapterConfig {
    TransportSelector selector;
};

class AxtpAdapter final : public Adapter {
public:
    using TransportFactory = std::function<std::unique_ptr<axtp::ITransport>(const axtp::HidTransportOptions&)>;

    AxtpAdapter();
    explicit AxtpAdapter(AxtpAdapterConfig config);
    AxtpAdapter(AxtpAdapterConfig config, TransportFactory transport_factory);
    ~AxtpAdapter() override;

    static AxtpAdapterConfig na20_defaults();
    static axtp::HidTransportOptions hid_options_from_selector(const TransportSelector& selector);
    static TransportDescriptor descriptor_from_hid_device(const axtp::HidDeviceInfo& device);
    static DeviceSnapshot snapshot_from_descriptor(const TransportDescriptor& descriptor);

    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;

    bool matches_selector(const axtp::HidDeviceInfo& device) const;
    void record_hid_trace(const axtp::HidReportTrace& trace);
    TransportDiagnostics diagnostics() const;

private:
    bool ensure_session_locked(const std::string& device_id, std::string& error);
    void refresh_diagnostics_locked();

    AxtpAdapterConfig config_;
    TransportFactory transport_factory_;
    mutable std::mutex mutex_;
    TransportDiagnostics diagnostics_;
    std::unique_ptr<axtp::sdk::AxtpClient> client_;
    axtp::ITransport* active_transport_ = nullptr;
    std::string active_device_id_;
};

} // namespace axent
