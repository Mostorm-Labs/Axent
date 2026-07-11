#include "axent/testing/axtp_adapter_test_seam.hpp"

#include <utility>

#include "axtp_adapter_internal.hpp"

namespace axent::testing {
namespace {

class InjectedRuntimeFactory final : public detail::AxtpAdapterRuntimeFactory {
public:
    explicit InjectedRuntimeFactory(AxtpAdapterTestSeam::TransportFactory factory)
        : factory_(std::move(factory))
    {
    }

    std::unique_ptr<axtp::ITransport> create(const axtp::HidTransportOptions& options) override
    {
        return factory_ ? factory_(options) : nullptr;
    }

private:
    AxtpAdapterTestSeam::TransportFactory factory_;
};

const char* trace_event_name(axtp::HidReportTraceKind kind)
{
    switch (kind) {
    case axtp::HidReportTraceKind::ReadReport: return "read-report";
    case axtp::HidReportTraceKind::ReadTimeout: return "read-timeout";
    case axtp::HidReportTraceKind::ReadError: return "read-error";
    case axtp::HidReportTraceKind::WriteFrame: return "write-frame";
    case axtp::HidReportTraceKind::WriteReport: return "write-report";
    case axtp::HidReportTraceKind::WriteError: return "write-error";
    case axtp::HidReportTraceKind::AcceptedReport: return "accepted-report";
    case axtp::HidReportTraceKind::DroppedReportId: return "dropped-report-id";
    }
    return "unknown";
}

} // namespace

std::unique_ptr<AxtpAdapter> AxtpAdapterTestSeam::make(
    AxtpAdapterConfig config,
    TransportFactory transport_factory)
{
    auto factory = std::make_shared<InjectedRuntimeFactory>(std::move(transport_factory));
    return std::unique_ptr<AxtpAdapter>(new AxtpAdapter(std::move(config), std::move(factory)));
}

axtp::HidTransportOptions AxtpAdapterTestSeam::hid_options_from_selector(
    const TransportSelector& selector)
{
    return detail::hid_options_from_selector(selector);
}

TransportDescriptor AxtpAdapterTestSeam::descriptor_from_hid_device(
    const axtp::HidDeviceInfo& device)
{
    return detail::descriptor_from_hid_device(device);
}

bool AxtpAdapterTestSeam::matches_selector(const AxtpAdapter& adapter,
                                           const axtp::HidDeviceInfo& device)
{
    return detail::matches_selector(adapter.config_.selector, device);
}

void AxtpAdapterTestSeam::record_hid_trace(AxtpAdapter& adapter,
                                           const axtp::HidReportTrace& trace)
{
    adapter.record_transport_trace(
        trace_event_name(trace.kind),
        trace.kind == axtp::HidReportTraceKind::AcceptedReport,
        trace.kind == axtp::HidReportTraceKind::WriteReport,
        trace.kind == axtp::HidReportTraceKind::ReadError,
        trace.kind == axtp::HidReportTraceKind::WriteError,
        trace.kind == axtp::HidReportTraceKind::DroppedReportId,
        trace.message);
}

} // namespace axent::testing
