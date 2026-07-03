#include "axent/adapters/axtp_adapter.hpp"

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "axtp_runtime.hpp"
#include "axtp_sdk.hpp"
#include "transports/hidapi/hid_transport.hpp"

namespace axent {
namespace {

std::string hex4(std::uint16_t value)
{
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(4)
        << static_cast<unsigned int>(value);
    return out.str();
}

std::string descriptor_id_for(const axtp::HidDeviceInfo& device)
{
    if (!device.serialNumber.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.serialNumber;
    }
    if (!device.path.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.path;
    }
    return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId);
}

std::string trace_event_name(axtp::HidReportTraceKind kind)
{
    switch (kind) {
    case axtp::HidReportTraceKind::ReadReport:
        return "read-report";
    case axtp::HidReportTraceKind::ReadTimeout:
        return "read-timeout";
    case axtp::HidReportTraceKind::ReadError:
        return "read-error";
    case axtp::HidReportTraceKind::WriteFrame:
        return "write-frame";
    case axtp::HidReportTraceKind::WriteReport:
        return "write-report";
    case axtp::HidReportTraceKind::WriteError:
        return "write-error";
    case axtp::HidReportTraceKind::AcceptedReport:
        return "accepted-report";
    case axtp::HidReportTraceKind::DroppedReportId:
        return "dropped-report-id";
    }
    return "unknown";
}

std::string error_name(axtp::ErrorCode code)
{
    switch (code) {
    case axtp::ErrorCode::Success:
        return "success";
    case axtp::ErrorCode::NotSupported:
        return "not-supported";
    case axtp::ErrorCode::Unavailable:
        return "unavailable";
    case axtp::ErrorCode::RpcMethodNotFound:
        return "rpc-method-not-found";
    case axtp::ErrorCode::RpcResponseTimeout:
        return "rpc-response-timeout";
    default:
        return "axtp-error-" + std::to_string(static_cast<std::uint32_t>(code));
    }
}

std::unique_ptr<axtp::ITransport> make_default_hid_transport(axtp::HidTransportOptions options)
{
#if AXENT_HAS_AXTP_HID_TRANSPORT
    return std::make_unique<axtp::HidTransport>(std::move(options));
#else
    (void)options;
    return nullptr;
#endif
}

} // namespace

AxtpAdapter::AxtpAdapter()
    : AxtpAdapter(na20_defaults())
{
}

AxtpAdapter::AxtpAdapter(AxtpAdapterConfig config)
    : AxtpAdapter(std::move(config), {})
{
}

AxtpAdapter::AxtpAdapter(AxtpAdapterConfig config, TransportFactory transport_factory)
    : config_(std::move(config))
    , transport_factory_(std::move(transport_factory))
{
    if (!transport_factory_) {
        transport_factory_ = [](const axtp::HidTransportOptions& options) {
            return make_default_hid_transport(options);
        };
    }
}

AxtpAdapter::~AxtpAdapter() = default;

AxtpAdapterConfig AxtpAdapter::na20_defaults()
{
    AxtpAdapterConfig config;
    config.selector.kind = TransportKind::Hid;
    config.selector.vendor_id = 0x0581;
    config.selector.product_id = 0x2581;
    config.selector.usage_page = 0x0081;
    config.selector.usage = 0;
    config.selector.report_id = 0x05;
    config.selector.input_report_size = 0;
    config.selector.output_report_size = 0;
    config.selector.read_buffer_size = 4096;
    config.selector.max_reports_per_poll = 32;
    return config;
}

axtp::HidTransportOptions AxtpAdapter::hid_options_from_selector(const TransportSelector& selector)
{
    axtp::HidTransportOptions options;
    options.vendorId = selector.vendor_id;
    options.productId = selector.product_id;
    options.usagePage = selector.usage_page;
    options.usage = selector.usage;
    options.devicePath = selector.path;
    options.serialNumber = selector.serial_number;
    options.reportId = selector.report_id;
    options.inputReportSize = selector.input_report_size;
    options.outputReportSize = selector.output_report_size;
    options.readBufferSize = selector.read_buffer_size;
    options.maxReportsPerPoll = selector.max_reports_per_poll;
    options.useReadThread = true;
    options.readThreadTimeoutMs = 1000;
    options.reportTrace = [](const axtp::HidReportTrace&) {};
    return options;
}

TransportDescriptor AxtpAdapter::descriptor_from_hid_device(const axtp::HidDeviceInfo& device)
{
    TransportDescriptor descriptor;
    descriptor.id = descriptor_id_for(device);
    descriptor.kind = TransportKind::Hid;
    descriptor.online = true;
    descriptor.vendor_id = device.vendorId;
    descriptor.product_id = device.productId;
    descriptor.usage_page = device.usagePage;
    descriptor.usage = device.usage;
    descriptor.interface_number = device.interfaceNumber;
    descriptor.path = device.path;
    descriptor.serial_number = device.serialNumber;
    descriptor.manufacturer = device.manufacturer;
    descriptor.product = device.product;
    descriptor.bus_type = device.busType;
    return descriptor;
}

DeviceSnapshot AxtpAdapter::snapshot_from_descriptor(const TransportDescriptor& descriptor)
{
    DeviceSnapshot snapshot;
    snapshot.id = descriptor.id;
    snapshot.adapter = "axtp";
    snapshot.identity.vendor = descriptor.manufacturer;
    snapshot.identity.model = descriptor.product;
    snapshot.identity.serial_number = descriptor.serial_number;
    snapshot.connection.online = descriptor.online;
    snapshot.connection.transport = "hid";
    snapshot.connection.last_change_reason = "hid-discovered";
    snapshot.status.health = descriptor.online ? "ready" : "offline";
    return snapshot;
}

AdapterMetadata AxtpAdapter::metadata() const
{
    return {"axtp", "AXTP Runtime Adapter", true, ""};
}

std::vector<Capability> AxtpAdapter::capabilities() const
{
    return {
        {"axtp.runtime",
         "axtp",
         true,
         "",
         {{"status.get", RiskLevel::Safe, false, false},
          {"stream.flowControl.get", RiskLevel::Safe, false, false},
          {"firmware.update", RiskLevel::Dangerous, true, true}},
         {"axtp.session.changed"}},
    };
}

std::vector<DeviceSnapshot> AxtpAdapter::discover()
{
    std::vector<DeviceSnapshot> devices;
    if (config_.selector.kind != TransportKind::Hid) {
        return devices;
    }

#if AXENT_HAS_AXTP_HID_TRANSPORT
    const auto hid_devices = axtp::enumerateHidDevices(config_.selector.vendor_id, config_.selector.product_id);
    for (const auto& device : hid_devices) {
        if (matches_selector(device)) {
            devices.push_back(snapshot_from_descriptor(descriptor_from_hid_device(device)));
        }
    }
#endif
    return devices;
}

ControlResult AxtpAdapter::call(const std::string& device_id, const std::string& method, const nlohmann::json& params)
{
    std::string error;
    if (!ensure_session_locked(device_id, error)) {
        return {ControlStatus::Unavailable, {{"error", error}}};
    }

    axtp::sdk::CallOptions options;
    options.timeout = std::chrono::milliseconds(5000);
    const std::string params_text = params.is_null() ? std::string("{}") : params.dump();
    const std::string body = client_->callJson(method, params_text, options);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        refresh_diagnostics_locked();
    }

    const auto& last_error = client_->lastError();
    if (!last_error.ok()) {
        return {ControlStatus::Unavailable,
                {{"error", last_error.message.empty() ? error_name(last_error.code) : last_error.message},
                 {"axtp_code", static_cast<std::uint32_t>(last_error.code)}}};
    }

    if (body.empty()) {
        return {ControlStatus::Ok, nlohmann::json::object()};
    }
    try {
        return {ControlStatus::Ok, nlohmann::json::parse(body)};
    } catch (const std::exception&) {
        return {ControlStatus::Ok, {{"body", body}}};
    }
}

ControlResult AxtpAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "AXTP firmware update skeleton only"}}};
}

bool AxtpAdapter::matches_selector(const axtp::HidDeviceInfo& device) const
{
    const auto& selector = config_.selector;
    if (selector.kind != TransportKind::Hid) {
        return false;
    }
    if (selector.vendor_id != 0 && device.vendorId != selector.vendor_id) {
        return false;
    }
    if (selector.product_id != 0 && device.productId != selector.product_id) {
        return false;
    }
    if (selector.usage_page != 0 && device.usagePage != selector.usage_page) {
        return false;
    }
    if (selector.usage != 0 && device.usage != selector.usage) {
        return false;
    }
    if (!selector.path.empty() && device.path != selector.path) {
        return false;
    }
    if (!selector.serial_number.empty() && device.serialNumber != selector.serial_number) {
        return false;
    }
    return true;
}

void AxtpAdapter::record_hid_trace(const axtp::HidReportTrace& trace)
{
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.last_event = trace_event_name(trace.kind);
    switch (trace.kind) {
    case axtp::HidReportTraceKind::ReadReport:
        break;
    case axtp::HidReportTraceKind::AcceptedReport:
        ++diagnostics_.read_reports;
        break;
    case axtp::HidReportTraceKind::WriteReport:
        ++diagnostics_.write_reports;
        break;
    case axtp::HidReportTraceKind::ReadError:
        ++diagnostics_.read_errors;
        break;
    case axtp::HidReportTraceKind::WriteError:
        ++diagnostics_.write_errors;
        break;
    case axtp::HidReportTraceKind::DroppedReportId:
        ++diagnostics_.dropped_reports;
        break;
    case axtp::HidReportTraceKind::ReadTimeout:
    case axtp::HidReportTraceKind::WriteFrame:
        break;
    }
}

TransportDiagnostics AxtpAdapter::diagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_;
}

bool AxtpAdapter::ensure_session_locked(const std::string& device_id, std::string& error)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (client_ != nullptr && client_->isConnected() && client_->isAppReady() && active_device_id_ == device_id) {
            return true;
        }
    }
    if (config_.selector.kind != TransportKind::Hid) {
        error = "AXTP adapter is configured for a non-HID selector";
        return false;
    }

    auto hid_options = hid_options_from_selector(config_.selector);
    hid_options.reportTrace = [this](const axtp::HidReportTrace& trace) {
        record_hid_trace(trace);
    };
    auto transport = transport_factory_ ? transport_factory_(hid_options) : nullptr;
    if (transport == nullptr) {
        error = "AXTP HID transport target is unavailable";
        return false;
    }

    axtp::sdk::ClientOptions client_options;
    client_options.autoOpen = true;
    client_options.autoIdentify = false;
    auto client = std::make_unique<axtp::sdk::AxtpClient>(client_options);
    auto* active_transport = transport.get();
    client->attachTransport(std::move(transport));

    axtp::sdk::AppReadyOptions ready_options;
    ready_options.timeout = std::chrono::milliseconds(5000);
    std::string last_ready_event;
    ready_options.trace = [&last_ready_event](const axtp::sdk::AppReadyTraceEvent& event) {
        last_ready_event = "app-ready:" + event.stage + ":" + event.action;
    };
    const auto ready = client->ensureAppReady(ready_options);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client_ = std::move(client);
        active_transport_ = active_transport;
        if (!last_ready_event.empty()) {
            diagnostics_.last_event = last_ready_event;
        }
        refresh_diagnostics_locked();
    }
    if (!ready.ok) {
        error = "AXTP app-ready failed at " + ready.stage + ": " + error_name(ready.statusCode);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            client_->close();
            client_.reset();
            active_transport_ = nullptr;
            active_device_id_.clear();
            diagnostics_.open = false;
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_device_id_ = device_id;
        diagnostics_.open = true;
        diagnostics_.last_event = "app-ready";
    }
    return true;
}

void AxtpAdapter::refresh_diagnostics_locked()
{
#if AXENT_HAS_AXTP_HID_TRANSPORT
    if (client_ == nullptr || !client_->isConnected()) {
        diagnostics_.open = false;
        return;
    }
    const auto* transport = dynamic_cast<const axtp::HidTransport*>(active_transport_);
    if (transport == nullptr) {
        return;
    }
    const auto& options = transport->options();
    const auto profile = transport->profile();
    const auto stats = transport->stats();
    diagnostics_.open = transport->isOpen();
    diagnostics_.negotiated_input_report_size = options.inputReportSize;
    diagnostics_.negotiated_output_report_size = options.outputReportSize;
    diagnostics_.read_buffer_size = options.readBufferSize;
    diagnostics_.preferred_frame_size = profile.preferredFrameSize;
    diagnostics_.read_reports = stats.acceptedReports;
    diagnostics_.write_reports = stats.writeReports;
    diagnostics_.read_errors = stats.readErrors;
    diagnostics_.write_errors = stats.writeErrors;
    diagnostics_.dropped_reports = stats.droppedReportId;
    diagnostics_.queued_reports = stats.queuedReports;
#else
    diagnostics_.open = client_ != nullptr && client_->isConnected() && client_->isAppReady();
#endif
}

} // namespace axent
