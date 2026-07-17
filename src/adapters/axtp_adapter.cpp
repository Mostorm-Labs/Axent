#include "axent/adapters/axtp_adapter.hpp"
#include "axtp_adapter_test_seam.hpp"

#include "axtp_adapter_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axtp_runtime.hpp"
#include "axtp_sdk.hpp"
#include "hidapi/hid_transport.hpp"

namespace axent {
namespace {

constexpr auto kMediaConfigureRetryInterval = std::chrono::seconds(1);

std::string hex4(std::uint16_t value)
{
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(4)
        << static_cast<unsigned int>(value);
    return out.str();
}

std::string descriptor_id_for(const axent::transport::HidDeviceInfo& device)
{
    if (!device.serialNumber.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.serialNumber;
    }
    if (!device.path.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.path;
    }
    return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId);
}

std::string trace_event_name(axent::transport::HidReportTraceKind kind)
{
    switch (kind) {
    case axent::transport::HidReportTraceKind::ReadReport:
        return "read-report";
    case axent::transport::HidReportTraceKind::ReadTimeout:
        return "read-timeout";
    case axent::transport::HidReportTraceKind::ReadError:
        return "read-error";
    case axent::transport::HidReportTraceKind::WriteFrame:
        return "write-frame";
    case axent::transport::HidReportTraceKind::WriteReport:
        return "write-report";
    case axent::transport::HidReportTraceKind::WriteError:
        return "write-error";
    case axent::transport::HidReportTraceKind::AcceptedReport:
        return "accepted-report";
    case axent::transport::HidReportTraceKind::DroppedReportId:
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

const char* media_kind_name(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video:
        return "video";
    case MediaKind::Audio:
        return "audio";
    case MediaKind::Unknown:
    default:
        return "unknown";
    }
}

std::string capabilities_method_name(MediaKind kind)
{
    return std::string(media_kind_name(kind)) + ".getStreamCapabilities";
}

std::string open_stream_method_name(MediaKind kind)
{
    return std::string(media_kind_name(kind)) + ".openStream";
}

std::optional<nlohmann::json> parse_json_object(const std::string& text)
{
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        auto parsed = nlohmann::json::parse(text);
        if (parsed.is_object()) {
            return parsed;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::string json_string_or(const nlohmann::json& object, const char* key, std::string fallback = {})
{
    if (object.is_object() && object.contains(key) && object[key].is_string()) {
        return object[key].get<std::string>();
    }
    return fallback;
}

std::uint32_t json_u32_or(const nlohmann::json& object, const char* key, std::uint32_t fallback = 0)
{
    if (!object.is_object() || !object.contains(key)) {
        return fallback;
    }
    const auto& value = object[key];
    if (value.is_number_unsigned()) {
        return value.get<std::uint32_t>();
    }
    if (value.is_number_integer()) {
        const auto signed_value = value.get<std::int64_t>();
        return signed_value > 0 ? static_cast<std::uint32_t>(signed_value) : fallback;
    }
    return fallback;
}

std::string ascii_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

struct ParsedMediaSourceStateEvent {
    std::uint32_t event_id = 0;
    std::string event_name;
    MediaKind kind = MediaKind::Unknown;
    std::string source;
    std::string state;
    std::string reason;
    std::uint32_t active_stream_id = 0;
    bool has_active_stream_id = false;
    bool valid = false;
};

ParsedMediaSourceStateEvent parse_media_source_state_event(
    const axtp::RpcPayload& payload,
    MediaKind kind,
    std::uint32_t event_id,
    const char* expected_name)
{
    ParsedMediaSourceStateEvent event;
    event.event_id = event_id;
    event.event_name = payload.meta.jsonMethodOrEventName.empty()
        ? expected_name
        : payload.meta.jsonMethodOrEventName;
    event.kind = kind;
    const auto body = parse_json_object(
        std::string(payload.body.begin(), payload.body.end()));
    if (!body.has_value()) {
        return event;
    }
    event.source = json_string_or(*body, "source");
    event.state = json_string_or(*body, "state");
    event.reason = json_string_or(*body, "reason");
    bool active_stream_id_valid = true;
    if (body->contains("activeStreamId")) {
        const auto& active_stream_id = (*body)["activeStreamId"];
        if (active_stream_id.is_number_unsigned()) {
            const auto value = active_stream_id.get<std::uint64_t>();
            if (value <= std::numeric_limits<std::uint32_t>::max()) {
                event.has_active_stream_id = true;
                event.active_stream_id = static_cast<std::uint32_t>(value);
            } else {
                active_stream_id_valid = false;
            }
        } else if (active_stream_id.is_number_integer()) {
            const auto value = active_stream_id.get<std::int64_t>();
            if (value >= 0 &&
                static_cast<std::uint64_t>(value) <=
                    std::numeric_limits<std::uint32_t>::max()) {
                event.has_active_stream_id = true;
                event.active_stream_id = static_cast<std::uint32_t>(value);
            } else {
                active_stream_id_valid = false;
            }
        } else {
            active_stream_id_valid = false;
        }
    }
    event.valid = !event.state.empty() && active_stream_id_valid;
    return event;
}

bool is_terminal_source_state(const std::string& state, const std::string& reason)
{
    const auto normalized_state = ascii_lower(state);
    const auto normalized_reason = ascii_lower(reason);
    return normalized_state == "idle" || normalized_state == "stopped" ||
        normalized_state == "unavailable" || normalized_state == "failed" ||
        normalized_reason == "source_disconnected";
}

bool is_streamable_source_state(const std::string& state)
{
    const auto normalized = ascii_lower(state);
    return normalized == "receiving" || normalized == "available";
}

bool source_state_is_streamable(const nlohmann::json& body)
{
    if (!body.is_object()) {
        return false;
    }
    const auto state = json_string_or(body, "state", json_string_or(body, "currentState"));
    if (state == "available" || state == "receiving") {
        return true;
    }
    if (body.contains("available") && body["available"].is_boolean() && body["available"].get<bool>()) {
        return true;
    }
    return false;
}

bool source_matches(const nlohmann::json& body, const std::string& source)
{
    if (source.empty()) {
        return true;
    }
    const auto source_id = json_string_or(body, "sourceId", json_string_or(body, "source"));
    return source_id.empty() || source_id == source;
}

bool capabilities_are_streamable(const nlohmann::json& capabilities, const std::string& source)
{
    if (!capabilities.is_object()) {
        return false;
    }
    if (capabilities.contains("sourceState") &&
        source_state_is_streamable(capabilities["sourceState"])) {
        return true;
    }
    if (capabilities.contains("sources") && capabilities["sources"].is_array()) {
        for (const auto& entry : capabilities["sources"]) {
            if (source_matches(entry, source) && source_state_is_streamable(entry)) {
                return true;
            }
        }
    }
    if (capabilities.value("supported", false) &&
        capabilities.contains("openModes") &&
        capabilities["openModes"].is_array()) {
        for (const auto& mode : capabilities["openModes"]) {
            if (mode.is_string() && mode.get<std::string>() == "receiver_pull") {
                return true;
            }
        }
    }
    return false;
}

std::uint32_t choose_audio_channels(const nlohmann::json& capabilities,
                                    const std::string& source,
                                    std::uint32_t requested)
{
    if (requested == 0) {
        requested = 2;
    }
    if (!capabilities.is_object() || !capabilities.contains("sources") ||
        !capabilities["sources"].is_array()) {
        return requested;
    }
    for (const auto& entry : capabilities["sources"]) {
        if (!source_matches(entry, source) || !entry.contains("channels") ||
            !entry["channels"].is_array()) {
            continue;
        }
        for (const auto& channel : entry["channels"]) {
            if (channel.is_number_unsigned() && channel.get<std::uint32_t>() == requested) {
                return requested;
            }
            if (channel.is_number_integer() &&
                channel.get<std::int64_t>() == static_cast<std::int64_t>(requested)) {
                return requested;
            }
        }
        for (const auto& channel : entry["channels"]) {
            if (channel.is_number_unsigned() && channel.get<std::uint32_t>() > 0) {
                return channel.get<std::uint32_t>();
            }
            if (channel.is_number_integer() && channel.get<std::int64_t>() > 0) {
                return static_cast<std::uint32_t>(channel.get<std::int64_t>());
            }
        }
    }
    return requested;
}

MediaCodec codec_for_open_result(MediaKind kind, const nlohmann::json& result)
{
    const auto codec = json_string_or(result, "codec", json_string_or(result, "format"));
    if (codec == "h264") {
        return MediaCodec::H264;
    }
    if (codec == "aac") {
        return MediaCodec::Aac;
    }
    if (codec == "pcm" || codec == "lpcm") {
        return MediaCodec::Pcm;
    }
    if (!codec.empty()) {
        return MediaCodec::Opaque;
    }
    return kind == MediaKind::Video ? MediaCodec::H264 :
        (kind == MediaKind::Audio ? MediaCodec::Aac : MediaCodec::Unknown);
}

std::unique_ptr<axtp::ITransport> make_default_hid_transport(axent::transport::HidTransportOptions options)
{
#if AXENT_HAS_AXTP_HID_TRANSPORT
    return std::make_unique<axent::transport::HidTransport>(std::move(options));
#else
    (void)options;
    return nullptr;
#endif
}

} // namespace

namespace detail {
namespace {

class DefaultAxtpAdapterRuntimeFactory final : public AxtpAdapterRuntimeFactory {
public:
    std::unique_ptr<axtp::ITransport> create(const axent::transport::HidTransportOptions& options) override
    {
        return make_default_hid_transport(options);
    }
};

} // namespace

std::shared_ptr<AxtpAdapterRuntimeFactory> make_default_axtp_runtime_factory()
{
    return std::make_shared<DefaultAxtpAdapterRuntimeFactory>();
}

} // namespace detail

namespace {

class VideoStreamParamsObserverSlot final {
public:
    explicit VideoStreamParamsObserverSlot(VideoStreamParamsObserver next)
        : observer(std::move(next))
    {
    }

    void publish(const VideoStreamParamsState& state)
    {
        VideoStreamParamsObserver current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            current = observer;
        }
        if (current) {
            current(state);
        }
    }

    void cancel() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        observer = {};
    }

    std::mutex mutex;
    VideoStreamParamsObserver observer;
};

class VideoStreamParamsSubscriptionState final : public VideoStreamParamsSubscription {
public:
    explicit VideoStreamParamsSubscriptionState(
        std::shared_ptr<VideoStreamParamsObserverSlot> next)
        : slot_(std::move(next))
    {
    }

    ~VideoStreamParamsSubscriptionState() override
    {
        cancel();
    }

    void cancel() noexcept override
    {
        if (slot_) {
            slot_->cancel();
            slot_.reset();
        }
    }

private:
    std::shared_ptr<VideoStreamParamsObserverSlot> slot_;
};

struct VideoReconfigureOperation {
    std::optional<std::uint32_t> previous_frame_rate;
    std::optional<std::uint32_t> requested_frame_rate;
    std::optional<MediaStreamDescriptor> previous_video_descriptor;
    std::optional<MediaStreamDescriptor> previous_audio_descriptor;
    nlohmann::json previous_video_open_params = nlohmann::json::object();
    nlohmann::json previous_audio_open_params = nlohmann::json::object();
    bool video_close_sent = false;
    bool audio_close_sent = false;
    bool video_close_terminal = false;
    bool audio_close_terminal = false;
    bool video_opened = false;
    bool audio_opened = false;
    bool rollback = false;
    std::chrono::steady_clock::time_point close_deadline;
};

constexpr std::uint32_t kStatusSuccess = 0x0000;
constexpr std::uint32_t kStatusNotSupported = 0x0003;
constexpr std::uint32_t kStatusInvalidState = 0x0004;
constexpr std::uint32_t kStatusBusy = 0x0005;
constexpr std::uint32_t kStatusInvalidArgument = 0x000A;
constexpr std::uint32_t kStatusMediaSourceUnavailable = 0x0802;
constexpr std::uint32_t kStatusMediaFrameRateUnsupported = 0x0805;
constexpr std::uint32_t kStatusMediaStreamStartFailed = 0x0807;
constexpr std::uint32_t kStatusMediaStreamStopFailed = 0x0808;

} // namespace

struct AxtpAdapter::RuntimeState {
    explicit RuntimeState(std::shared_ptr<detail::AxtpAdapterRuntimeFactory> next_factory)
        : factory(std::move(next_factory))
    {
    }

    std::shared_ptr<detail::AxtpAdapterRuntimeFactory> factory;
    std::unique_ptr<axtp::sdk::AxtpClient> client;
    axtp::ITransport* active_transport = nullptr;
    mutable std::mutex video_params_mutex;
    VideoStreamParamsState video_params_state;
    std::optional<std::uint32_t> session_video_frame_rate;
    std::optional<VideoReconfigureOperation> video_reconfigure;
    nlohmann::json active_video_open_params = nlohmann::json::object();
    nlohmann::json active_audio_open_params = nlohmann::json::object();
    std::vector<std::weak_ptr<VideoStreamParamsObserverSlot>> video_params_observers;
    std::string health_probe_method;
    nlohmann::json health_probe_params = nlohmann::json::object();
    std::uint64_t next_video_reconfigure_id = 1;
    bool suppress_video_auto_open = false;
    std::vector<std::uint32_t> video_frame_rates;
    bool video_supports_active_reconfigure = true;
};

AxtpAdapter::AxtpAdapter()
    : AxtpAdapter(na20_defaults())
{
}

AxtpAdapter::AxtpAdapter(AxtpAdapterConfig config)
    : AxtpAdapter(std::move(config), detail::make_default_axtp_runtime_factory())
{
}

AxtpAdapter::AxtpAdapter(
    AxtpAdapterConfig config,
    std::shared_ptr<detail::AxtpAdapterRuntimeFactory> runtime_factory)
    : config_(std::move(config))
    , runtime_(std::make_unique<RuntimeState>(std::move(runtime_factory)))
{
    if (!runtime_->factory) {
        runtime_->factory = detail::make_default_axtp_runtime_factory();
    }
    recovery_worker_ = std::thread(&AxtpAdapter::run_session_recovery_worker, this);
}

AxtpAdapter::~AxtpAdapter()
{
    std::thread pump;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pump = request_stop_session_pump_locked();
    }
    if (pump.joinable()) {
        pump.join();
    }
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        stop_recovery_worker_ = true;
        recovery_requested_ = false;
    }
    recovery_cv_.notify_all();
    if (recovery_worker_.joinable()) {
        recovery_worker_.join();
    }
    std::lock_guard<std::mutex> client_lock(client_mutex_);
    if (runtime_->client) {
        runtime_->client->close();
    }
}

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

axent::transport::HidTransportOptions detail::hid_options_from_selector(const TransportSelector& selector)
{
    axent::transport::HidTransportOptions options;
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
    options.reportTrace = [](const axent::transport::HidReportTrace&) {};
    return options;
}

TransportDescriptor detail::descriptor_from_hid_device(const axent::transport::HidDeviceInfo& device)
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
    const auto hid_devices = axent::transport::enumerateHidDevices(config_.selector.vendor_id, config_.selector.product_id);
    for (const auto& device : hid_devices) {
        if (detail::matches_selector(config_.selector, device)) {
            devices.push_back(snapshot_from_descriptor(detail::descriptor_from_hid_device(device)));
        }
    }
#endif
    return devices;
}

ControlResult AxtpAdapter::call(const std::string& device_id, const std::string& method, const nlohmann::json& params)
{
    std::string error;
    std::string body;
    axtp::sdk::SdkError last_error;
    {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        ControlStatus session_status = ControlStatus::Unavailable;
        if (!ensure_session_locked(device_id, error, session_status, false)) {
            return {session_status, {{"error", error}}};
        }

        {
            std::lock_guard<std::mutex> client_lock(client_mutex_);
            if (runtime_->client == nullptr) {
                return {ControlStatus::Unavailable, {{"error", "AXTP session is unavailable"}}};
            }
            axtp::sdk::CallOptions options;
            options.timeout = std::chrono::milliseconds(5000);
            const std::string params_text = params.is_null() ? std::string("{}") : params.dump();
            body = runtime_->client->callJson(method, params_text, options);
            last_error = runtime_->client->lastError();
            // callJson() polls the endpoint and may dispatch source lifecycle
            // events and STREAM payloads. Reconcile descriptors before the
            // queued callbacks are drained below so wire ordering is retained.
            process_pending_media_source_state_events(device_id);
        }
    }
    drain_pending_media_callbacks();

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

bool detail::matches_selector(const TransportSelector& selector, const axent::transport::HidDeviceInfo& device)
{
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

void AxtpAdapter::record_transport_trace(const std::string& event_name,
                                         bool accepted_read,
                                         bool write_report,
                                         bool read_error,
                                         bool write_error,
                                         bool dropped_report,
                                         const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.last_event = event_name;
    if (accepted_read) {
        ++diagnostics_.read_reports;
        last_transport_activity_ = std::chrono::steady_clock::now();
    }
    if (write_report) {
        ++diagnostics_.write_reports;
    }
    if (read_error) {
        ++diagnostics_.read_errors;
    }
    if (write_error) {
        ++diagnostics_.write_errors;
    }
    if (dropped_report) {
        ++diagnostics_.dropped_reports;
    }
    if ((read_error || write_error) && !message.empty()) {
        diagnostics_.last_error = message;
    }
}

TransportDiagnostics AxtpAdapter::diagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_;
}

void AxtpAdapter::set_media_frame_callback(MediaFrameCallback callback)
{
    std::lock_guard<std::mutex> lock(media_callback_mutex_);
    media_frame_callback_ = std::move(callback);
}

void AxtpAdapter::set_media_stream_event_callback(MediaStreamEventCallback callback)
{
    std::lock_guard<std::mutex> lock(media_callback_mutex_);
    media_stream_event_callback_ = std::move(callback);
}

std::vector<MediaStreamDescriptor> AxtpAdapter::active_media_stream_descriptors() const
{
    std::vector<MediaStreamDescriptor> descriptors;
    std::lock_guard<std::mutex> lock(media_stream_mutex_);
    descriptors.reserve(active_media_streams_.size());
    for (const auto& entry : active_media_streams_) {
        descriptors.push_back(entry.second.descriptor);
    }
    return descriptors;
}

VideoStreamParamsResult AxtpAdapter::set_video_stream_params(
    const std::string& device_id,
    const VideoStreamParamsRequest& request)
{
    VideoStreamParamsResult result;
    if ((!request.frame_rate.has_value() && !request.reset_frame_rate) ||
        (request.frame_rate.has_value() && request.reset_frame_rate) ||
        (request.frame_rate.has_value() && *request.frame_rate == 0)) {
        result.status_code = kStatusInvalidArgument;
        return result;
    }

    std::optional<MediaStreamDescriptor> active_video;
    std::optional<MediaStreamDescriptor> active_audio;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto active = std::find_if(
            active_media_streams_.begin(), active_media_streams_.end(),
            [](const auto& entry) {
                return entry.second.descriptor.kind == MediaKind::Video;
            });
        if (active != active_media_streams_.end()) {
            active_video = active->second.descriptor;
        }
        const auto audio = std::find_if(
            active_media_streams_.begin(), active_media_streams_.end(),
            [](const auto& entry) {
                return entry.second.descriptor.kind == MediaKind::Audio;
            });
        if (audio != active_media_streams_.end()) {
            active_audio = audio->second.descriptor;
        }
    }
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }

    VideoStreamParamsState state;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!session_active) {
            result.status_code = kStatusInvalidState;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (runtime_->video_reconfigure.has_value()) {
            result.status_code = kStatusBusy;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (active_video.has_value() &&
            !runtime_->video_supports_active_reconfigure) {
            result.status_code = kStatusNotSupported;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (request.frame_rate.has_value() &&
            !runtime_->video_frame_rates.empty() &&
            std::find(runtime_->video_frame_rates.begin(),
                      runtime_->video_frame_rates.end(),
                      *request.frame_rate) == runtime_->video_frame_rates.end()) {
            result.status_code = kStatusMediaFrameRateUnsupported;
            result.state = runtime_->video_params_state;
            return result;
        }

        const auto requested = request.reset_frame_rate
            ? std::optional<std::uint32_t>{}
            : request.frame_rate;
        if (runtime_->session_video_frame_rate == requested) {
            runtime_->video_params_state.state = VideoStreamParamsStateKind::Unchanged;
            runtime_->video_params_state.phase = active_video.has_value()
                ? VideoStreamParamsPhase::Streaming
                : VideoStreamParamsPhase::Idle;
            runtime_->video_params_state.changed_fields.clear();
            state = runtime_->video_params_state;
            result.status_code = kStatusSuccess;
            result.accepted = true;
            result.state = state;
            return result;
        }

        VideoReconfigureOperation operation;
        operation.previous_frame_rate = runtime_->session_video_frame_rate;
        operation.requested_frame_rate = requested;
        operation.previous_video_descriptor = active_video;
        operation.previous_audio_descriptor = active_audio;
        operation.previous_video_open_params = runtime_->active_video_open_params;
        operation.previous_audio_open_params = runtime_->active_audio_open_params;
        runtime_->video_reconfigure = std::move(operation);
        runtime_->session_video_frame_rate = requested;
        runtime_->suppress_video_auto_open = true;

        auto& current = runtime_->video_params_state;
        current.source = config_.video_source;
        current.desired_frame_rate = requested;
        current.effective_frame_rate = active_video.has_value() && active_video->frame_rate != 0
            ? std::optional<std::uint32_t>(active_video->frame_rate)
            : current.effective_frame_rate;
        current.reconfigure_id = "vr-" + std::to_string(runtime_->next_video_reconfigure_id++);
        current.state = VideoStreamParamsStateKind::Pending;
        current.phase = active_video.has_value() || active_audio.has_value()
            ? VideoStreamParamsPhase::Closing
            : VideoStreamParamsPhase::Opening;
        current.previous_stream_id = active_video.has_value()
            ? std::optional<std::uint32_t>(active_video->key.stream_id)
            : std::optional<std::uint32_t>{};
        current.active_stream_id = active_video.has_value()
            ? std::optional<std::uint32_t>(active_video->key.stream_id)
            : std::optional<std::uint32_t>{};
        current.rollback_applied = false;
        current.last_error.reset();
        current.changed_fields = {"frameRate"};
        state = current;
    }

    result.status_code = kStatusSuccess;
    result.accepted = true;
    result.state = state;
    notify_video_stream_params_state(state);
    return result;
}

VideoStreamParamsState AxtpAdapter::video_stream_params_state(
    const std::string& device_id) const
{
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    auto state = runtime_->video_params_state;
    if (!session_active) {
        state.session_id.clear();
        state.active_stream_id.reset();
    }
    return state;
}

VideoStreamParamsSubscriptionPtr AxtpAdapter::subscribe_video_stream_params(
    const std::string& device_id,
    VideoStreamParamsObserver observer)
{
    if (!observer) {
        return {};
    }
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }
    if (!session_active) {
        return {};
    }
    auto slot = std::make_shared<VideoStreamParamsObserverSlot>(std::move(observer));
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->video_params_observers.push_back(slot);
    }
    return std::make_unique<VideoStreamParamsSubscriptionState>(std::move(slot));
}

void AxtpAdapter::notify_video_stream_params_state(VideoStreamParamsState state)
{
    std::vector<std::shared_ptr<VideoStreamParamsObserverSlot>> observers;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        auto& weak_observers = runtime_->video_params_observers;
        for (auto it = weak_observers.begin(); it != weak_observers.end();) {
            if (auto observer = it->lock()) {
                observers.push_back(std::move(observer));
                ++it;
            } else {
                it = weak_observers.erase(it);
            }
        }
    }
    for (const auto& observer : observers) {
        observer->publish(state);
    }
}

void AxtpAdapter::drop_pending_media_frames_for_device(const std::string& device_id)
{
    std::lock_guard<std::mutex> lock(pending_media_mutex_);
    std::queue<std::pair<std::string, MediaFrame>> retained;
    while (!pending_media_frames_.empty()) {
        auto entry = std::move(pending_media_frames_.front());
        pending_media_frames_.pop();
        if (entry.first != device_id) {
            retained.push(std::move(entry));
        }
    }
    pending_media_frames_.swap(retained);
}

void AxtpAdapter::bind_media_delivery_session(const std::string& device_id,
                                              const std::string& session_id)
{
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        media_delivery_sessions_[device_id] = session_id;
    }
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    runtime_->video_params_state.session_id = session_id;
}

void AxtpAdapter::unbind_media_delivery_session(const std::string& device_id,
                                                const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
    const auto binding = media_delivery_sessions_.find(device_id);
    if (binding != media_delivery_sessions_.end() && binding->second == session_id) {
        media_delivery_sessions_.erase(binding);
    }
    std::lock_guard<std::mutex> video_lock(runtime_->video_params_mutex);
    if (runtime_->video_params_state.session_id == session_id) {
        runtime_->video_params_state.session_id.clear();
    }
}

void AxtpAdapter::clear_video_stream_params_session(bool preserve_logical_session)
{
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    const auto logical_session_id = preserve_logical_session
        ? runtime_->video_params_state.session_id
        : std::string{};
    runtime_->session_video_frame_rate.reset();
    runtime_->video_reconfigure.reset();
    runtime_->active_video_open_params = nlohmann::json::object();
    runtime_->active_audio_open_params = nlohmann::json::object();
    runtime_->suppress_video_auto_open = false;
    runtime_->video_frame_rates.clear();
    runtime_->video_supports_active_reconfigure = true;
    runtime_->video_params_state = {};
    runtime_->video_params_state.session_id = logical_session_id;
    runtime_->health_probe_method.clear();
    runtime_->health_probe_params = nlohmann::json::object();
}

void AxtpAdapter::clear_media_streams(MediaStreamEventReason reason)
{
    std::vector<MediaStreamEvent> closed_events;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        closed_events.reserve(active_media_streams_.size());
        for (const auto& entry : active_media_streams_) {
            closed_events.push_back(
                {MediaStreamEventKind::Closed, entry.second.descriptor, reason});
        }
        active_media_streams_.clear();
        pending_video_source_recovery_close_.reset();
        pending_audio_source_recovery_close_.reset();
        source_recovery_cycle_active_ = false;
        source_recovery_close_sent_ = false;
        source_recovery_reopen_mask_ = 0;
    }
    enqueue_media_stream_events(std::move(closed_events));
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.active_video_stream_id = 0;
    diagnostics_.active_audio_stream_id = 0;
    diagnostics_.active_media_streams = 0;
    next_media_configure_attempt_ = {};
    media_configure_attempts_ = 0;
    video_source_terminal_ = false;
    audio_source_terminal_ = false;
    video_source_recovery_pending_ = false;
    audio_source_recovery_pending_ = false;
    next_video_source_recovery_attempt_ = {};
    next_audio_source_recovery_attempt_ = {};
    std::lock_guard<std::mutex> source_event_lock(pending_media_source_state_mutex_);
    std::queue<MediaSourceStateEvent> empty_source_events;
    pending_media_source_state_events_.swap(empty_source_events);
}

void AxtpAdapter::enqueue_media_stream_events(std::vector<MediaStreamEvent> events)
{
    if (events.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(pending_media_event_mutex_);
    for (auto& event : events) {
        pending_media_stream_events_.push(std::move(event));
    }
}

void AxtpAdapter::enqueue_media_source_state_event(MediaSourceStateEvent event)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_media_source_event_id = event.event_id;
        diagnostics_.last_media_source_event_name = event.event_name;
        diagnostics_.last_media_source_event_source = event.source;
        diagnostics_.last_media_source_event_state = event.state;
        diagnostics_.last_media_source_event_reason = event.reason;
        diagnostics_.last_media_source_event_active_stream_id = event.active_stream_id;
        diagnostics_.last_media_source_event_has_active_stream_id = event.has_active_stream_id;
        diagnostics_.last_event = "media-source-event name=" + event.event_name +
            " id=0x" + hex4(static_cast<std::uint16_t>(event.event_id)) +
            " source=" + (event.source.empty() ? "<absent>" : event.source) +
            " state=" + (event.state.empty() ? "<absent>" : event.state) +
            " reason=" + (event.reason.empty() ? "<absent>" : event.reason) +
            " activeStreamId=" + (event.has_active_stream_id
                ? std::to_string(event.active_stream_id)
                : "<absent>");
        if (!event.valid) {
            diagnostics_.last_error = "invalid media source state event payload";
        }
    }
    std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
    pending_media_source_state_events_.push(std::move(event));
}

void AxtpAdapter::process_pending_media_source_state_events(const std::string& device_id)
{
    for (;;) {
        std::queue<MediaSourceStateEvent> events;
        {
            std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
            events.swap(pending_media_source_state_events_);
        }
        if (events.empty()) {
            return;
        }
        while (!events.empty()) {
            process_media_source_state_event(device_id, events.front());
            events.pop();
        }
    }
}

void AxtpAdapter::process_media_source_state_event(
    const std::string& device_id,
    const MediaSourceStateEvent& event)
{
    if (!event.valid || event.kind == MediaKind::Unknown) {
        return;
    }
    if (event.kind == MediaKind::Video || event.kind == MediaKind::Audio) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (runtime_->video_reconfigure.has_value()) {
            return;
        }
    }

    const auto configured_source = event.kind == MediaKind::Video
        ? config_.video_source
        : config_.audio_source;
    const auto& event_source = event.source.empty() ? configured_source : event.source;
    if (!configured_source.empty() && event_source != configured_source) {
        return;
    }
    const auto source_matches = [&](const MediaStreamDescriptor& descriptor) {
        // A source-state event may omit source. Prefer the configured source
        // in that case; if neither side names one, the event is still a valid
        // kind-scoped lifecycle fact.
        return event_source.empty() || descriptor.source == event_source;
    };

    if (is_terminal_source_state(event.state, event.reason)) {
        std::optional<MediaStreamDescriptor> closed_descriptor;
        std::optional<VideoStreamParamsState> video_params_update;
        std::uint32_t active_stream_count = 0;
        bool active_stream_id_is_foreign = false;
        {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            auto match = active_media_streams_.end();
            if (event.has_active_stream_id && event.active_stream_id != 0) {
                const auto active = active_media_streams_.find(event.active_stream_id);
                if (active != active_media_streams_.end() &&
                    active->second.descriptor.kind == event.kind &&
                    source_matches(active->second.descriptor)) {
                    match = active;
                } else {
                    // A non-zero ID is an exact identity hint. Never fall back
                    // to kind/source when it identifies another, stale, or
                    // otherwise unknown stream.
                    active_stream_id_is_foreign = true;
                }
            } else {
                match = std::find_if(
                    active_media_streams_.begin(),
                    active_media_streams_.end(),
                    [&](const auto& entry) {
                        return entry.second.descriptor.kind == event.kind &&
                            source_matches(entry.second.descriptor);
                    });
            }
            if (match != active_media_streams_.end()) {
                closed_descriptor = match->second.descriptor;
                active_media_streams_.erase(match);
            }
            active_stream_count = static_cast<std::uint32_t>(active_media_streams_.size());
        }
        if (active_stream_id_is_foreign) {
            return;
        }
        // Start one paired recovery cycle for the first terminal source event.
        // Capture the descriptor which just closed as well as the other active
        // leg.  The terminal event is removed from active_media_streams_ above,
        // so taking the snapshot only from the map would lose the video leg.
        if (event.kind == MediaKind::Video) {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            if (!source_recovery_cycle_active_ &&
                (closed_descriptor.has_value() || !active_media_streams_.empty())) {
                source_recovery_cycle_active_ = true;
                source_recovery_close_sent_ = false;
                source_recovery_reopen_mask_ = 0;
            }
            if (source_recovery_cycle_active_ && !source_recovery_close_sent_) {
                const auto capture = [&](const MediaStreamDescriptor& descriptor) {
                    if (descriptor.kind == MediaKind::Video &&
                        !pending_video_source_recovery_close_.has_value()) {
                        pending_video_source_recovery_close_ = descriptor;
                        source_recovery_reopen_mask_ |= 0x01;
                    } else if (descriptor.kind == MediaKind::Audio &&
                               !pending_audio_source_recovery_close_.has_value()) {
                        pending_audio_source_recovery_close_ = descriptor;
                        source_recovery_reopen_mask_ |= 0x02;
                    }
                };
                if (closed_descriptor.has_value()) {
                    capture(*closed_descriptor);
                }
                for (const auto& entry : active_media_streams_) {
                    capture(entry.second.descriptor);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (event.kind == MediaKind::Video) {
                video_source_terminal_ = true;
                video_source_recovery_pending_ = false;
                next_video_source_recovery_attempt_ = {};
                if (closed_descriptor.has_value()) {
                    diagnostics_.active_video_stream_id = 0;
                }
            } else {
                audio_source_terminal_ = true;
                audio_source_recovery_pending_ = false;
                next_audio_source_recovery_attempt_ = {};
                if (closed_descriptor.has_value()) {
                    diagnostics_.active_audio_stream_id = 0;
                }
            }
            diagnostics_.active_media_streams = active_stream_count;
        }
        if (closed_descriptor.has_value()) {
            enqueue_media_stream_events({
                {MediaStreamEventKind::Closed,
                 std::move(*closed_descriptor),
                 MediaStreamEventReason::SourceRecovery},
            });
        }
        if (event.kind == MediaKind::Video) {
            {
                std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                auto& current = runtime_->video_params_state;
                current.state = VideoStreamParamsStateKind::Idle;
                current.phase = VideoStreamParamsPhase::Idle;
                current.effective_frame_rate.reset();
                current.active_stream_id.reset();
                current.previous_stream_id.reset();
                current.reconfigure_id.clear();
                current.rollback_applied = false;
                current.last_error.reset();
                current.changed_fields.clear();
                video_params_update = current;
            }
            notify_video_stream_params_state(std::move(*video_params_update));
        }
        return;
    }

    if (!is_streamable_source_state(event.state)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (event.kind == MediaKind::Video) {
            video_source_terminal_ = false;
        } else {
            audio_source_terminal_ = false;
        }
    }

    // A source-level stop is different from a normal per-kind retry: NA20 can
    // report the source as available again while NT10 is still carrying the
    // old encoder state.  Reset both receiver-pull legs before the first
    // replacement open so the encoder emits a fresh IDR.  This is deliberately
    // one-shot per terminal/recovery cycle and does not alter the retry timer.
    std::vector<MediaStreamDescriptor> source_recovery_closes;
    std::vector<MediaStreamEvent> source_recovery_closed_events;
    std::uint32_t source_recovery_active_stream_count = 0;
    bool source_recovery_video_closed = false;
    bool source_recovery_audio_closed = false;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        if (event.kind == MediaKind::Video &&
            source_recovery_cycle_active_ && !source_recovery_close_sent_) {
            if (pending_video_source_recovery_close_.has_value()) {
                source_recovery_closes.push_back(
                    std::move(*pending_video_source_recovery_close_));
                pending_video_source_recovery_close_.reset();
            }
            if (pending_audio_source_recovery_close_.has_value()) {
                source_recovery_closes.push_back(
                    std::move(*pending_audio_source_recovery_close_));
                pending_audio_source_recovery_close_.reset();
            }
            source_recovery_close_sent_ = true;

            // The explicit paired close is authoritative even when the device
            // does not emit a terminal event for the second leg.  Remove that
            // leg now so stale payloads cannot reach the renderer while the
            // source is being reopened, and publish its lifecycle Closed before
            // the replacement Opened event.
            for (const auto& descriptor : source_recovery_closes) {
                source_recovery_video_closed |= descriptor.kind == MediaKind::Video;
                source_recovery_audio_closed |= descriptor.kind == MediaKind::Audio;
                const auto active = active_media_streams_.find(descriptor.key.stream_id);
                if (active != active_media_streams_.end() &&
                    active->second.descriptor.key.generation == descriptor.key.generation) {
                    source_recovery_closed_events.push_back(
                        {MediaStreamEventKind::Closed,
                         active->second.descriptor,
                         MediaStreamEventReason::SourceRecovery});
                    active_media_streams_.erase(active);
                }
            }
            source_recovery_active_stream_count =
                static_cast<std::uint32_t>(active_media_streams_.size());
        }
    }
    if (!source_recovery_closes.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.active_media_streams = source_recovery_active_stream_count;
        if (source_recovery_active_stream_count == 0) {
            diagnostics_.active_video_stream_id = 0;
            diagnostics_.active_audio_stream_id = 0;
        }
    }
    enqueue_media_stream_events(std::move(source_recovery_closed_events));
    if (!source_recovery_closes.empty() && runtime_->client != nullptr) {
        axtp::sdk::CallOptions options;
        options.timeout = std::chrono::milliseconds(500);
        for (const auto& descriptor : source_recovery_closes) {
            const std::string method_prefix = std::string(media_kind_name(descriptor.kind));
            const nlohmann::json params{
                {"streamId", descriptor.key.stream_id},
                {"peerRole", "transmitter"},
                {"reason", "sourceRecovery"},
            };
            (void)runtime_->client->callJson(
                method_prefix + ".closeStream", params.dump(), options);
        }
        // A close response may have queued a terminal source event. Drain it
        // before opening the replacement so a late old-generation event cannot
        // close the newly opened same-ID stream.
        process_pending_media_source_state_events(device_id);
    }
    bool already_active = false;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto active = std::find_if(
            active_media_streams_.begin(),
            active_media_streams_.end(),
            [&](const auto& entry) {
                return entry.second.descriptor.kind == event.kind;
            });
        already_active = active != active_media_streams_.end();
    }
    if (already_active) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (event.kind == MediaKind::Video) {
            video_source_recovery_pending_ = false;
            next_video_source_recovery_attempt_ = {};
        } else {
            audio_source_recovery_pending_ = false;
            next_audio_source_recovery_attempt_ = {};
        }
        return;
    }
    const auto recovery_attempt = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool pending = event.kind == MediaKind::Video
            ? video_source_recovery_pending_
            : audio_source_recovery_pending_;
        const auto next_attempt = event.kind == MediaKind::Video
            ? next_video_source_recovery_attempt_
            : next_audio_source_recovery_attempt_;
        if (pending && recovery_attempt < next_attempt) {
            return;
        }
    }
    const auto schedule_recovery_retry = [this](MediaKind kind) {
        const auto next_attempt = std::chrono::steady_clock::now() +
            kMediaConfigureRetryInterval;
        std::lock_guard<std::mutex> lock(mutex_);
        if (kind == MediaKind::Video) {
            video_source_recovery_pending_ = true;
            next_video_source_recovery_attempt_ = next_attempt;
        } else {
            audio_source_recovery_pending_ = true;
            next_audio_source_recovery_attempt_ = next_attempt;
        }
    };

    // A paired source-recovery close must be followed by paired opens whenever
    // the other source is still streamable.  This keeps NT10's encoder reset
    // symmetric and avoids leaving an otherwise healthy audio leg pointing at
    // a receiver-pull stream which we explicitly closed.
    if (!source_recovery_closes.empty()) {
        const auto is_active_kind = [this](MediaKind kind) {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            return std::find_if(
                active_media_streams_.begin(),
                active_media_streams_.end(),
                [kind](const auto& entry) {
                    return entry.second.descriptor.kind == kind;
                }) != active_media_streams_.end();
        };
        bool current_opened = is_active_kind(event.kind);
        if (!current_opened) {
            current_opened = configure_media_stream_kind(
                device_id,
                event.kind,
                false,
                false,
                MediaStreamEventReason::SourceRecovery);
        }
        const auto other_kind = event.kind == MediaKind::Video
            ? MediaKind::Audio
            : MediaKind::Video;
        const bool other_was_closed = other_kind == MediaKind::Video
            ? source_recovery_video_closed
            : source_recovery_audio_closed;
        bool other_terminal = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            other_terminal = other_kind == MediaKind::Video
                ? video_source_terminal_
                : audio_source_terminal_;
        }
        bool other_opened = is_active_kind(other_kind);
        if (other_was_closed && !other_terminal) {
            if (!other_opened) {
                other_opened = configure_media_stream_kind(
                    device_id,
                    other_kind,
                    false,
                    false,
                    MediaStreamEventReason::SourceRecovery);
            }
        }
        if (!current_opened) {
            schedule_recovery_retry(event.kind);
        }
        if (other_was_closed && !other_terminal && !other_opened) {
            schedule_recovery_retry(other_kind);
        }
        if (current_opened || other_opened) {
            return;
        }
        return;
    }

    if (configure_media_stream_kind(
            device_id,
            event.kind,
            false,
            false,
            MediaStreamEventReason::SourceRecovery)) {
        return;
    }
    schedule_recovery_retry(event.kind);
}

void AxtpAdapter::reset_session_for_device(const std::string& device_id)
{
    recovery_generation_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        recovery_requested_ = false;
        recovery_device_id_.clear();
        recovery_reason_.clear();
    }
    recovery_cv_.notify_all();
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        media_delivery_sessions_.erase(device_id);
    }
    std::thread stopped_pump;
    {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (active_device_id_ != device_id) {
                drop_pending_media_frames_for_device(device_id);
                return;
            }
            stopped_pump = request_stop_session_pump_locked();
        }
        if (stopped_pump.joinable()) {
            stopped_pump.join();
        }
        {
            std::lock_guard<std::mutex> client_lock(client_mutex_);
            if (runtime_->client != nullptr) {
                runtime_->client->close();
                runtime_->client.reset();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            runtime_->active_transport = nullptr;
            active_device_id_.clear();
            diagnostics_.open = false;
        }
    }
    clear_media_streams();
    clear_video_stream_params_session();
    drop_pending_media_frames_for_device(device_id);
}

bool AxtpAdapter::open_session(const std::string& device_id, std::string& error)
{
    return open_session_status(device_id, error) == ControlStatus::Ok;
}

ControlStatus AxtpAdapter::open_session_status(const std::string& device_id, std::string& error)
{
    std::lock_guard<std::mutex> session_lock(session_mutex_);
    ControlStatus status = ControlStatus::Unavailable;
    (void)ensure_session_locked(device_id, error, status, true,
                                MediaStreamEventReason::InitialOpen);
    return status;
}

std::thread AxtpAdapter::request_stop_session_pump_locked()
{
    stop_session_pump_.store(true);
    if (!session_pump_.joinable()) {
        return {};
    }
    return std::move(session_pump_);
}

bool AxtpAdapter::has_media_delivery_session(const std::string& device_id) const
{
    std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
    const auto it = media_delivery_sessions_.find(device_id);
    return it != media_delivery_sessions_.end() && !it->second.empty();
}

void AxtpAdapter::set_session_health(SessionHealthState state,
                                     std::uint32_t probe_failures,
                                     std::string reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    session_health_ = state;
    health_probe_failures_ = probe_failures;
    if (!reason.empty()) {
        last_session_recovery_reason_ = std::move(reason);
    }
    diagnostics_.session_health = session_health_;
    diagnostics_.health_probe_failures = health_probe_failures_;
    diagnostics_.session_recoveries = session_recoveries_;
    diagnostics_.last_session_recovery_reason = last_session_recovery_reason_;
}

bool AxtpAdapter::session_health_probe_due(std::chrono::steady_clock::time_point now)
{
    if (!config_.enable_session_health_probe ||
        runtime_->health_probe_method.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_device_id_.empty() || !diagnostics_.open ||
        session_health_ == SessionHealthState::Recovering) {
        return false;
    }
    const auto interval = std::chrono::milliseconds(
        std::max<std::uint32_t>(1, config_.session_health_probe_interval_ms));
    if (next_health_probe_.time_since_epoch().count() != 0 &&
        now < next_health_probe_) {
        return false;
    }
    if (last_transport_activity_.time_since_epoch().count() != 0 &&
        now - last_transport_activity_ < interval) {
        next_health_probe_ = last_transport_activity_ + interval;
        return false;
    }
    next_health_probe_ = now + interval;
    return true;
}

bool AxtpAdapter::run_session_health_probe(const std::string& device_id)
{
    if (runtime_->client == nullptr ||
        runtime_->health_probe_method.empty() ||
        !has_media_delivery_session(device_id)) {
        return true;
    }

    axtp::sdk::CallOptions options;
    options.timeout = std::chrono::milliseconds(
        std::max<std::uint32_t>(1, config_.session_health_probe_timeout_ms));
    (void)runtime_->client->callJson(
        runtime_->health_probe_method,
        runtime_->health_probe_params.dump(),
        options);
    const auto error = runtime_->client->lastError();
    process_pending_media_source_state_events(device_id);
    if (error.ok()) {
        return true;
    }

    switch (error.code) {
    case axtp::ErrorCode::Unavailable:
    case axtp::ErrorCode::Timeout:
    case axtp::ErrorCode::RpcResponseTimeout:
    case axtp::ErrorCode::TransportReadFailed:
    case axtp::ErrorCode::TransportWriteFailed:
    case axtp::ErrorCode::TransportDisconnected:
    case axtp::ErrorCode::ControlOpenRequired:
    case axtp::ErrorCode::ControlSessionInvalid:
    case axtp::ErrorCode::ControlSessionExpired:
    case axtp::ErrorCode::ControlHeartbeatTimeout:
        return false;
    default:
        // A typed response such as NotSupported or InvalidArgument proves
        // that the device and its session are alive; do not reconnect for a
        // business-level failure.
        return true;
    }
}

void AxtpAdapter::request_session_recovery(const std::string& device_id,
                                           std::string reason)
{
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        if (stop_recovery_worker_ || recovery_requested_) {
            return;
        }
        recovery_generation_.fetch_add(1);
        recovery_requested_ = true;
        recovery_device_id_ = device_id;
        recovery_reason_ = std::move(reason);
        recovery_attempt_ = 0;
    }
    set_session_health(
        SessionHealthState::Recovering,
        health_probe_failures_,
        "health-probe-timeout");
    recovery_cv_.notify_one();
}

bool AxtpAdapter::recover_session_once(const std::string& device_id,
                                       std::string& error,
                                       std::uint64_t recovery_generation)
{
    if (!has_media_delivery_session(device_id) ||
        recovery_generation_.load() != recovery_generation) {
        error = "media session was released during recovery";
        return false;
    }

    std::thread stopped_pump;
    {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!active_device_id_.empty() && active_device_id_ != device_id) {
                error = "AXTP session device changed during recovery";
                return false;
            }
            stopped_pump = request_stop_session_pump_locked();
        }
        if (stopped_pump.joinable()) {
            stopped_pump.join();
        }
        {
            std::lock_guard<std::mutex> client_lock(client_mutex_);
            if (runtime_->client != nullptr) {
                runtime_->client->close();
                runtime_->client.reset();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            runtime_->active_transport = nullptr;
            active_device_id_.clear();
            diagnostics_.open = false;
        }
    }

    // The old physical session is gone; publish its terminal lifecycle before
    // the replacement open and never send closeStream to the dead device.
    clear_media_streams(MediaStreamEventReason::SessionRecovery);
    drain_pending_media_callbacks();
    clear_video_stream_params_session(true);

    if (!has_media_delivery_session(device_id) ||
        recovery_generation_.load() != recovery_generation) {
        error = "media session was released during recovery";
        return false;
    }

    std::lock_guard<std::mutex> session_lock(session_mutex_);
    ControlStatus status = ControlStatus::Unavailable;
    if (!ensure_session_locked(
            device_id,
            error,
            status,
            true,
            MediaStreamEventReason::SessionRecovery)) {
        if (error.empty()) {
            error = "AXTP session recovery failed: " + error_name(
                runtime_->client != nullptr
                    ? runtime_->client->lastError().code
                    : axtp::ErrorCode::Unavailable);
        }
        return false;
    }
    drain_pending_media_callbacks();
    return true;
}

void AxtpAdapter::run_session_recovery_worker()
{
    for (;;) {
        std::string device_id;
        std::string reason;
        std::uint64_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(recovery_mutex_);
            recovery_cv_.wait(lock, [this]() {
                return stop_recovery_worker_ || recovery_requested_;
            });
            if (stop_recovery_worker_) {
                return;
            }
            recovery_requested_ = false;
            device_id = recovery_device_id_;
            reason = recovery_reason_;
            generation = recovery_generation_.load();
            recovery_attempt_ = 0;
        }

        for (;;) {
            if (!has_media_delivery_session(device_id) ||
                recovery_generation_.load() != generation) {
                break;
            }
            std::string error;
            if (recover_session_once(device_id, error, generation)) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++session_recoveries_;
                    last_session_recovery_reason_ = reason;
                    session_health_ = SessionHealthState::Healthy;
                    health_probe_failures_ = 0;
                    diagnostics_.session_health = session_health_;
                    diagnostics_.health_probe_failures = 0;
                    diagnostics_.session_recoveries = session_recoveries_;
                    diagnostics_.last_session_recovery_reason =
                        last_session_recovery_reason_;
                    diagnostics_.last_event = "session-recovered";
                }
                break;
            }

            const auto attempt = ++recovery_attempt_;
            const auto initial = std::max<std::uint32_t>(
                1, config_.session_recovery_backoff_initial_ms);
            const auto maximum = std::max(
                initial,
                config_.session_recovery_backoff_max_ms);
            const auto backoff = std::min<std::uint32_t>(
                maximum,
                initial << std::min<std::uint32_t>(attempt - 1, 8));
            set_session_health(SessionHealthState::Failed, health_probe_failures_, error);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                diagnostics_.last_event = "session-recovery-retry-" +
                    std::to_string(attempt);
            }
            std::unique_lock<std::mutex> lock(recovery_mutex_);
            if (recovery_cv_.wait_for(lock, std::chrono::milliseconds(backoff), [this, generation]() {
                    return stop_recovery_worker_ ||
                        recovery_generation_.load() != generation;
                })) {
                break;
            }
        }
    }
}

bool AxtpAdapter::ensure_session_locked(const std::string& device_id,
                                        std::string& error,
                                        ControlStatus& status,
                                        bool configure_media,
                                        MediaStreamEventReason open_reason)
{
    status = ControlStatus::Unavailable;
    std::thread stopped_pump;
    {
        std::lock_guard<std::mutex> client_lock(client_mutex_);
        bool session_ready = false;
        bool media_ready = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            session_ready = runtime_->client != nullptr && runtime_->client->isConnected() && runtime_->client->isAppReady() &&
                active_device_id_ == device_id;
            media_ready = diagnostics_.active_media_streams != 0 ||
                !config_.enable_media ||
                (!config_.enable_video && !config_.enable_audio);
            if (!active_device_id_.empty() && active_device_id_ != device_id) {
                error = "AXTP session busy for active device " + active_device_id_;
                status = ControlStatus::Busy;
                return false;
            }
        }
        if (session_ready) {
            if (configure_media && !media_ready && runtime_->client != nullptr) {
                configure_media_streams(device_id, open_reason);
            }
            status = ControlStatus::Ok;
            return true;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_pump = request_stop_session_pump_locked();
        }
    }
    if (stopped_pump.joinable()) {
        stopped_pump.join();
    }
    {
        std::lock_guard<std::mutex> client_lock(client_mutex_);
        if (runtime_->client != nullptr) {
            runtime_->client->close();
            runtime_->client.reset();
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (runtime_->active_transport != nullptr || !active_device_id_.empty()) {
            runtime_->active_transport = nullptr;
            active_device_id_.clear();
            diagnostics_.open = false;
        }
    }
    clear_media_streams();
    clear_video_stream_params_session();
    if (config_.selector.kind != TransportKind::Hid) {
        error = "AXTP adapter is configured for a non-HID selector";
        return false;
    }

    auto hid_options = detail::hid_options_from_selector(config_.selector);
    hid_options.reportTrace = [this](const axent::transport::HidReportTrace& trace) {
        record_transport_trace(
            trace_event_name(trace.kind),
            trace.kind == axent::transport::HidReportTraceKind::AcceptedReport,
            trace.kind == axent::transport::HidReportTraceKind::WriteReport,
            trace.kind == axent::transport::HidReportTraceKind::ReadError,
            trace.kind == axent::transport::HidReportTraceKind::WriteError,
            trace.kind == axent::transport::HidReportTraceKind::DroppedReportId,
            trace.message);
    };
    auto transport = runtime_->factory ? runtime_->factory->create(hid_options) : nullptr;
    if (transport == nullptr) {
        error = "AXTP HID transport target is unavailable";
        return false;
    }

    axtp::sdk::ClientOptions client_options;
    client_options.autoOpen = true;
    client_options.autoIdentify = false;
    auto client = std::make_unique<axtp::sdk::AxtpClient>(client_options);
    auto register_source_state_event = [this, &client](
                                           axtp::EventId event_id,
                                           MediaKind kind,
                                           const char* event_name) {
        const auto wire_id = static_cast<std::uint32_t>(event_id);
        client->registerEventHandler(
            wire_id,
            [this, wire_id, kind, event_name](const axtp::RpcPayload& payload) {
                const auto parsed = parse_media_source_state_event(
                    payload, kind, wire_id, event_name);
                MediaSourceStateEvent event;
                event.event_id = parsed.event_id;
                event.event_name = parsed.event_name;
                event.kind = parsed.kind;
                event.source = parsed.source;
                event.state = parsed.state;
                event.reason = parsed.reason;
                event.active_stream_id = parsed.active_stream_id;
                event.has_active_stream_id = parsed.has_active_stream_id;
                event.valid = parsed.valid;
                enqueue_media_source_state_event(std::move(event));
            });
    };
    register_source_state_event(
        axtp::EventId::VideoStreamSourceStateChanged,
        MediaKind::Video,
        "video.streamSourceStateChanged");
    register_source_state_event(
        axtp::EventId::AudioStreamSourceStateChanged,
        MediaKind::Audio,
        "audio.streamSourceStateChanged");
    client->setStreamHandler(
        [this, device_id](const axtp::BrokerContext&, const axtp::StreamPayload& stream) {
            handle_stream_payload(
                device_id, stream.streamId, stream.seqId, stream.cursor, stream.data);
        });
    auto* active_transport = transport.get();
    client->attachTransport(std::move(transport));

    axtp::sdk::AppReadyOptions ready_options;
    ready_options.timeout = std::chrono::milliseconds(5000);
    std::string last_ready_event;
    ready_options.trace = [&last_ready_event](const axtp::sdk::AppReadyTraceEvent& event) {
        last_ready_event = "app-ready:" + event.stage + ":" + event.action;
    };
    const auto ready = client->ensureAppReady(ready_options);
    std::unique_lock<std::mutex> client_lock(client_mutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        runtime_->client = std::move(client);
        runtime_->active_transport = active_transport;
        if (!last_ready_event.empty()) {
            diagnostics_.last_event = last_ready_event;
        }
        refresh_diagnostics_locked();
    }
    if (!ready.ok) {
        error = "AXTP app-ready failed at " + ready.stage + ": " + error_name(ready.statusCode);
        {
            runtime_->client->close();
            runtime_->client.reset();
        }
        client_lock.unlock();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            runtime_->active_transport = nullptr;
            active_device_id_.clear();
            diagnostics_.open = false;
        }
        return false;
    }

    std::string logical_session_id;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end()) {
            logical_session_id = binding->second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->session_video_frame_rate = config_.video_frame_rate;
        runtime_->video_params_state = {};
        runtime_->video_params_state.session_id = std::move(logical_session_id);
        runtime_->video_params_state.source = config_.video_source;
        runtime_->video_params_state.desired_frame_rate = config_.video_frame_rate;
        runtime_->video_params_state.state = VideoStreamParamsStateKind::Idle;
        runtime_->video_params_state.phase = VideoStreamParamsPhase::Idle;
        runtime_->suppress_video_auto_open = false;
    }

    if (configure_media) {
        std::lock_guard<std::mutex> lock(mutex_);
        next_media_configure_attempt_ = std::chrono::steady_clock::now() + kMediaConfigureRetryInterval;
        media_configure_attempts_ = 0;
    }
    if (configure_media) {
        configure_media_streams(device_id, open_reason);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_device_id_ = device_id;
        diagnostics_.open = true;
        if (diagnostics_.active_media_streams == 0 &&
            (diagnostics_.last_event.empty() || diagnostics_.last_event.rfind("app-ready", 0) == 0)) {
            diagnostics_.last_event = "app-ready";
        }
        session_health_ = SessionHealthState::Healthy;
        health_probe_failures_ = 0;
        last_transport_activity_ = std::chrono::steady_clock::now();
        next_health_probe_ = last_transport_activity_ +
            std::chrono::milliseconds(config_.session_health_probe_interval_ms);
        refresh_diagnostics_locked();
        stop_session_pump_.store(false);
        session_pump_ = std::thread([this, device_id]() {
            while (!stop_session_pump_.load()) {
                bool request_recovery = false;
                std::string recovery_reason;
                {
                    std::lock_guard<std::mutex> client_lock(client_mutex_);
                    if (runtime_->client == nullptr) {
                        break;
                    }
                    bool retry_media_configure = false;
                    std::uint32_t retry_attempt = 0;
                    std::string retry_device_id;
                    const auto now = std::chrono::steady_clock::now();
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (media_configure_retry_due_locked(now)) {
                            retry_media_configure = true;
                            retry_attempt = ++media_configure_attempts_;
                            next_media_configure_attempt_ = now + kMediaConfigureRetryInterval;
                            diagnostics_.last_event =
                                "media-open-retry-" + std::to_string(retry_attempt);
                            retry_device_id = active_device_id_;
                        }
                    }
                    if (retry_media_configure) {
                        configure_media_streams(retry_device_id);
                    }
                    runtime_->client->poll();
                    // Source transitions are reconciled after the complete
                    // runtime poll. AXTP receiver-pull requires a successful
                    // replacement openStream response before the device emits
                    // frames for the new generation; any frame already in the
                    // same pre-open poll therefore belongs to the old one.
                    process_pending_media_source_state_events(device_id);
                    advance_video_reconfigure(device_id);
                    retry_pending_media_source_recoveries(device_id, now);
                    if (session_health_probe_due(now)) {
                        if (run_session_health_probe(device_id)) {
                            set_session_health(SessionHealthState::Healthy, 0);
                        } else {
                            std::uint32_t failures = 0;
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                failures = ++health_probe_failures_;
                                session_health_ = failures >=
                                        std::max<std::uint32_t>(
                                            1, config_.session_health_failure_threshold)
                                    ? SessionHealthState::Recovering
                                    : SessionHealthState::Suspect;
                                diagnostics_.session_health = session_health_;
                                diagnostics_.health_probe_failures = failures;
                                diagnostics_.last_event =
                                    "session-health-probe-failed-" + std::to_string(failures);
                            }
                            if (failures >= std::max<std::uint32_t>(
                                    1, config_.session_health_failure_threshold)) {
                                request_recovery = true;
                                recovery_reason = "health-probe-timeout";
                            }
                        }
                    }
                }
                if (request_recovery) {
                    request_session_recovery(device_id, std::move(recovery_reason));
                }
                drain_pending_media_callbacks();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    status = ControlStatus::Ok;
    return true;
}

bool AxtpAdapter::media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const
{
    if (!config_.enable_media ||
        (!config_.enable_video && !config_.enable_audio) ||
        diagnostics_.active_media_streams != 0) {
        return false;
    }
    const bool video_can_open = config_.enable_video && !video_source_terminal_ &&
        !video_source_recovery_pending_;
    const bool audio_can_open = config_.enable_audio && !audio_source_terminal_ &&
        !audio_source_recovery_pending_;
    if (!video_can_open && !audio_can_open) {
        return false;
    }
    return next_media_configure_attempt_.time_since_epoch().count() == 0 ||
        now >= next_media_configure_attempt_;
}

void AxtpAdapter::configure_media_streams(
    const std::string& device_id,
    MediaStreamEventReason open_reason)
{
    bool configure_video = false;
    bool configure_audio = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configure_video = config_.enable_video && !video_source_recovery_pending_;
        configure_audio = config_.enable_audio && !audio_source_recovery_pending_;
    }
    if (configure_video) {
        configure_media_stream_kind(
            device_id, MediaKind::Video, true, false, open_reason);
    }
    if (configure_audio) {
        configure_media_stream_kind(
            device_id, MediaKind::Audio, true, false, open_reason);
    }
}

void AxtpAdapter::retry_pending_media_source_recoveries(
    const std::string& device_id,
    std::chrono::steady_clock::time_point now)
{
    // Retry only the missing kind whose event-driven recovery failed. The
    // other kind remains open, and this scheduling is independent of HID's
    // transport-level read-error backoff.
    const auto retry_kind = [&](MediaKind kind) {
        bool retry = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& pending = kind == MediaKind::Video
                ? video_source_recovery_pending_
                : audio_source_recovery_pending_;
            const auto terminal = kind == MediaKind::Video
                ? video_source_terminal_
                : audio_source_terminal_;
            auto& next_attempt = kind == MediaKind::Video
                ? next_video_source_recovery_attempt_
                : next_audio_source_recovery_attempt_;
            if (pending && !terminal && now >= next_attempt) {
                retry = true;
                next_attempt = now + kMediaConfigureRetryInterval;
            }
        }
        if (!retry) {
            return;
        }
        configure_media_stream_kind(
            device_id, kind, false, false, MediaStreamEventReason::SourceRecovery);
        // configure_media_stream_kind() uses public runtime callJson and may
        // itself dispatch source events. Preserve the same lifecycle fence as
        // AxtpAdapter::call() before any callbacks are drained or another kind
        // is considered for retry.
        process_pending_media_source_state_events(device_id);
    };
    retry_kind(MediaKind::Video);
    retry_kind(MediaKind::Audio);
}

void AxtpAdapter::advance_video_reconfigure(const std::string& device_id)
{
    VideoReconfigureOperation operation;
    VideoStreamParamsState state;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!runtime_->video_reconfigure.has_value() || runtime_->client == nullptr) {
            return;
        }
        operation = *runtime_->video_reconfigure;
        state = runtime_->video_params_state;
    }

    auto publish_state = [this](VideoStreamParamsState next) {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            runtime_->video_params_state = next;
        }
        notify_video_stream_params_state(std::move(next));
    };

    auto finish_failed = [&](std::uint32_t code,
                             std::string message,
                             bool preserve_previous_stream) {
        VideoStreamParamsState failed;
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            runtime_->session_video_frame_rate = operation.previous_frame_rate;
            runtime_->video_reconfigure.reset();
            runtime_->suppress_video_auto_open = !preserve_previous_stream;
            auto& current = runtime_->video_params_state;
            current.desired_frame_rate = operation.previous_frame_rate;
            current.state = VideoStreamParamsStateKind::Failed;
            current.phase = VideoStreamParamsPhase::Failed;
            current.rollback_applied = false;
            current.last_error = VideoStreamParamsError{code, std::move(message)};
            if (!preserve_previous_stream) {
                current.active_stream_id.reset();
                current.effective_frame_rate.reset();
            }
            failed = current;
        }
        notify_video_stream_params_state(std::move(failed));
    };

    auto mark_previous_closed = [&](const MediaStreamDescriptor& descriptor) {
        bool removed = false;
        std::uint32_t active_count = 0;
        {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            const auto it = active_media_streams_.find(descriptor.key.stream_id);
            if (it != active_media_streams_.end() &&
                it->second.descriptor.kind == descriptor.kind &&
                it->second.descriptor.key.generation == descriptor.key.generation) {
                active_media_streams_.erase(it);
                removed = true;
            }
            active_count = static_cast<std::uint32_t>(active_media_streams_.size());
        }
        if (removed) {
            enqueue_media_stream_events({
                {MediaStreamEventKind::Closed, descriptor},
            });
            std::lock_guard<std::mutex> lock(mutex_);
            if (descriptor.kind == MediaKind::Video) {
                diagnostics_.active_video_stream_id = 0;
            } else {
                diagnostics_.active_audio_stream_id = 0;
            }
            diagnostics_.active_media_streams = active_count;
        }
    };

    const auto advance_close = [&](MediaKind kind,
                                   const std::optional<MediaStreamDescriptor>& descriptor,
                                   bool& close_sent,
                                   bool& close_terminal,
                                   std::uint32_t& failure_code,
                                   std::string& failure_message) {
        if (!descriptor.has_value() || close_terminal) {
            close_terminal = true;
            return true;
        }

        nlohmann::json close_result = nlohmann::json::object();
        const auto method_prefix = std::string(media_kind_name(kind));
        if (!close_sent) {
            axtp::sdk::CallOptions options;
            options.timeout = std::chrono::milliseconds(5000);
            const nlohmann::json params{
                {"streamId", descriptor->key.stream_id},
                {"peerRole", "transmitter"},
                {"reason", "encodingReconfigure"},
            };
            const auto text = runtime_->client->callJson(
                method_prefix + ".closeStream", params.dump(), options);
            const auto error = runtime_->client->lastError();
            if (!error.ok()) {
                failure_code = static_cast<std::uint32_t>(error.code);
                failure_message = error.message.empty()
                    ? method_prefix + ".closeStream failed"
                    : error.message;
                return false;
            }
            close_result = parse_json_object(text).value_or(nlohmann::json::object());
            close_sent = true;
        } else {
            if (std::chrono::steady_clock::now() >= operation.close_deadline) {
                failure_code = kStatusMediaStreamStopFailed;
                failure_message = method_prefix +
                    " stream did not reach a terminal state";
                return false;
            }
            axtp::sdk::CallOptions options;
            options.timeout = std::chrono::milliseconds(500);
            const nlohmann::json params{
                {"streamId", descriptor->key.stream_id},
            };
            const auto text = runtime_->client->callJson(
                method_prefix + ".getStreamState", params.dump(), options);
            if (runtime_->client->lastError().ok()) {
                close_result = parse_json_object(text).value_or(nlohmann::json::object());
            }
        }

        const auto close_state = ascii_lower(json_string_or(close_result, "state"));
        if (close_state == "failed") {
            failure_code = kStatusMediaStreamStopFailed;
            failure_message = method_prefix + " stream close entered failed state";
            return false;
        }
        if (close_state == "closed" || close_result.value("alreadyClosed", false)) {
            close_terminal = true;
            mark_previous_closed(*descriptor);
        }
        return true;
    };

    // NA20 only applies NT10 encoder changes after both receiver-pull legs
    // stop. Send both close requests before opening either replacement, then
    // treat each stream generation as an independent lifecycle boundary.
    const bool has_close_targets = operation.previous_video_descriptor.has_value() ||
        operation.previous_audio_descriptor.has_value();
    if (has_close_targets &&
        (!operation.video_close_terminal || !operation.audio_close_terminal)) {
        if (!operation.video_close_sent && !operation.audio_close_sent) {
            operation.close_deadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(5);
        }
        std::uint32_t close_failure_code = 0;
        std::string close_failure_message;
        if (!advance_close(
                MediaKind::Video,
                operation.previous_video_descriptor,
                operation.video_close_sent,
                operation.video_close_terminal,
                close_failure_code,
                close_failure_message)) {
            finish_failed(close_failure_code, std::move(close_failure_message), true);
            return;
        }
        if (!advance_close(
                MediaKind::Audio,
                operation.previous_audio_descriptor,
                operation.audio_close_sent,
                operation.audio_close_terminal,
                close_failure_code,
                close_failure_message)) {
            finish_failed(close_failure_code, std::move(close_failure_message), false);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value()) {
                *runtime_->video_reconfigure = operation;
            }
        }
        if (!operation.video_close_terminal || !operation.audio_close_terminal) {
            return;
        }
        state.active_stream_id.reset();
        state.phase = VideoStreamParamsPhase::Opening;
        publish_state(state);
    }

    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!runtime_->video_reconfigure.has_value()) {
            return;
        }
        runtime_->video_params_state.phase = VideoStreamParamsPhase::Opening;
        runtime_->video_params_state.state = VideoStreamParamsStateKind::Pending;
        state = runtime_->video_params_state;
    }

    // A device may legally reuse the numeric stream id after close. The
    // generation assigned by configure_media_stream_kind() is the lifecycle
    // boundary, so stale frames/events remain isolated without rejecting the
    // replacement or making rollback impossible on NA20.
    if (!operation.video_opened) {
        const bool opened = configure_media_stream_kind(
            device_id,
            MediaKind::Video,
            false,
            true,
            MediaStreamEventReason::ParameterReconfigure);
        if (opened) {
            operation.video_opened = true;
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value()) {
                *runtime_->video_reconfigure = operation;
            }
        } else {
            const auto open_error = runtime_->client->lastError();
            std::string last_adapter_event;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_adapter_event = diagnostics_.last_event;
            }
            const auto error_code = open_error.ok()
                ? (last_adapter_event.find("source-waiting") != std::string::npos ||
                   last_adapter_event.find("capabilities-unavailable") != std::string::npos
                       ? kStatusMediaSourceUnavailable
                       : kStatusMediaStreamStartFailed)
                : static_cast<std::uint32_t>(open_error.code);
            const auto error_message = open_error.message.empty()
                ? std::string("video.openStream failed")
                : open_error.message;

            if (!operation.rollback &&
                (operation.previous_video_descriptor.has_value() ||
                 !operation.previous_video_open_params.empty())) {
                VideoStreamParamsState rollback_state;
                {
                    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                    if (!runtime_->video_reconfigure.has_value()) {
                        return;
                    }
                    runtime_->session_video_frame_rate = operation.previous_frame_rate;
                    operation.rollback = true;
                    operation.video_opened = false;
                    operation.audio_opened = false;
                    *runtime_->video_reconfigure = operation;
                    auto& current = runtime_->video_params_state;
                    current.desired_frame_rate = operation.previous_frame_rate;
                    current.phase = VideoStreamParamsPhase::Opening;
                    current.last_error = VideoStreamParamsError{error_code, error_message};
                    rollback_state = current;
                }
                notify_video_stream_params_state(std::move(rollback_state));
                return;
            }

            finish_failed(error_code, error_message, false);
            return;
        }
    }

    const bool should_reopen_audio = operation.previous_audio_descriptor.has_value() ||
        !operation.previous_audio_open_params.empty();
    if (should_reopen_audio && !operation.audio_opened) {
        if (!configure_media_stream_kind(
                device_id,
                MediaKind::Audio,
                false,
                true,
                MediaStreamEventReason::ParameterReconfigure)) {
            const auto open_error = runtime_->client->lastError();
            const auto error_code = open_error.ok()
                ? kStatusMediaStreamStartFailed
                : static_cast<std::uint32_t>(open_error.code);
            const auto error_message = open_error.message.empty()
                ? std::string("audio.openStream failed")
                : open_error.message;

            if (!operation.rollback) {
                std::optional<MediaStreamDescriptor> replacement_video;
                {
                    std::lock_guard<std::mutex> lock(media_stream_mutex_);
                    const auto active = std::find_if(
                        active_media_streams_.begin(), active_media_streams_.end(),
                        [](const auto& entry) {
                            return entry.second.descriptor.kind == MediaKind::Video;
                        });
                    if (active != active_media_streams_.end()) {
                        replacement_video = active->second.descriptor;
                    }
                }
                VideoStreamParamsState rollback_state;
                {
                    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                    if (!runtime_->video_reconfigure.has_value()) {
                        return;
                    }
                    runtime_->session_video_frame_rate = operation.previous_frame_rate;
                    operation.rollback = true;
                    operation.previous_video_descriptor = replacement_video;
                    operation.previous_audio_descriptor.reset();
                    operation.video_close_sent = false;
                    operation.audio_close_sent = false;
                    operation.video_close_terminal = !replacement_video.has_value();
                    operation.audio_close_terminal = true;
                    operation.video_opened = false;
                    operation.audio_opened = false;
                    *runtime_->video_reconfigure = operation;
                    auto& current = runtime_->video_params_state;
                    current.desired_frame_rate = operation.previous_frame_rate;
                    current.phase = replacement_video.has_value()
                        ? VideoStreamParamsPhase::Closing
                        : VideoStreamParamsPhase::Opening;
                    current.last_error = VideoStreamParamsError{error_code, error_message};
                    rollback_state = current;
                }
                notify_video_stream_params_state(std::move(rollback_state));
                return;
            }

            finish_failed(error_code, error_message, false);
            return;
        }
        operation.audio_opened = true;
    }

    if (operation.video_opened && (!should_reopen_audio || operation.audio_opened)) {
        VideoStreamParamsState completed;
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            auto& current = runtime_->video_params_state;
            current.state = operation.rollback
                ? VideoStreamParamsStateKind::RolledBack
                : VideoStreamParamsStateKind::Applied;
            current.phase = operation.rollback
                ? VideoStreamParamsPhase::RolledBack
                : VideoStreamParamsPhase::Streaming;
            current.rollback_applied = operation.rollback;
            current.last_error.reset();
            runtime_->video_reconfigure.reset();
            runtime_->suppress_video_auto_open = false;
            completed = current;
        }
        notify_video_stream_params_state(std::move(completed));
        return;
    }
}

bool AxtpAdapter::configure_media_stream_kind(
    const std::string& device_id,
    MediaKind kind,
    bool update_retry_state,
    bool from_video_reconfigure,
    MediaStreamEventReason open_reason)
{
    if (!config_.enable_media || runtime_->client == nullptr ||
        (kind != MediaKind::Video && kind != MediaKind::Audio)) {
        return false;
    }
    std::optional<VideoStreamParamsState> video_params_update;
    if (!from_video_reconfigure) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (runtime_->video_reconfigure.has_value() ||
            (kind == MediaKind::Video && runtime_->suppress_video_auto_open)) {
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((kind == MediaKind::Video && video_source_terminal_) ||
            (kind == MediaKind::Audio && audio_source_terminal_)) {
            return false;
        }
    }

    auto call_json = [this](const std::string& method, const nlohmann::json& params)
        -> std::optional<nlohmann::json> {
        axtp::sdk::CallOptions options;
        options.timeout = std::chrono::milliseconds(5000);
        const auto text = runtime_->client->callJson(method, params.dump(), options);
        if (!runtime_->client->lastError().ok()) {
            return std::nullopt;
        }
        return parse_json_object(text);
    };

    const bool is_video = kind == MediaKind::Video;
    const std::string source = is_video ? config_.video_source : config_.audio_source;
    // Snapshot the frame-rate selected for this particular open before the
    // capabilities RPC pumps any more inbound events.  A replacement open
    // must use the reconfigure operation's immutable target (or its previous
    // value during rollback), rather than re-reading ambient session state
    // after close/source lifecycle events have been dispatched.
    std::optional<std::uint32_t> video_open_frame_rate;
    if (is_video) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (from_video_reconfigure && runtime_->video_reconfigure.has_value()) {
            const auto& operation = *runtime_->video_reconfigure;
            video_open_frame_rate = operation.rollback
                ? operation.previous_frame_rate
                : operation.requested_frame_rate;
        } else {
            video_open_frame_rate = runtime_->session_video_frame_rate;
        }
    }
    const nlohmann::json source_params{{"source", source}};
    auto capabilities = call_json(capabilities_method_name(kind), source_params);
    if (!capabilities.has_value()) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_event =
            std::string(media_kind_name(kind)) + "-capabilities-unavailable";
        return false;
    }
    // A successful capabilities response is a safe, read-only liveness probe
    // for this device/session.  Keep the method that actually worked instead
    // of inventing a transport-specific heartbeat request.
    runtime_->health_probe_method = capabilities_method_name(kind);
    runtime_->health_probe_params = source_params;
    if (!capabilities_are_streamable(*capabilities, source)) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_event = std::string(media_kind_name(kind)) + "-source-waiting";
        return false;
    }
    if (is_video) {
        std::vector<std::uint32_t> frame_rates;
        bool supports_reconfigure = true;
        if (capabilities->contains("sources") && (*capabilities)["sources"].is_array()) {
            for (const auto& entry : (*capabilities)["sources"]) {
                if (!source_matches(entry, source)) {
                    continue;
                }
                if (entry.contains("frameRates") && entry["frameRates"].is_array()) {
                    for (const auto& value : entry["frameRates"]) {
                        const auto frame_rate = json_u32_or(
                            nlohmann::json{{"value", value}}, "value", 0);
                        if (frame_rate != 0) {
                            frame_rates.push_back(frame_rate);
                        }
                    }
                }
                if (entry.contains("supportsReconfigure") &&
                    entry["supportsReconfigure"].is_boolean()) {
                    supports_reconfigure = entry["supportsReconfigure"].get<bool>();
                }
                break;
            }
        }
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->video_frame_rates = std::move(frame_rates);
        runtime_->video_supports_active_reconfigure = supports_reconfigure;
    }

    nlohmann::json open_params;
    if (is_video) {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value() &&
                runtime_->video_reconfigure->previous_video_open_params.is_object() &&
                !runtime_->video_reconfigure->previous_video_open_params.empty()) {
                open_params = runtime_->video_reconfigure->previous_video_open_params;
                open_params.erase("frameRate");
                open_params.erase("streamId");
                open_params.erase("state");
            } else {
                open_params = nlohmann::json{
                    {"source", source},
                    {"peerRole", "transmitter"},
                    {"codec", "h264"},
                    {"streamProfile", "media.video"},
                    {"cursorUnit", "timestampUs"},
                };
            }
            if (video_open_frame_rate.has_value()) {
                open_params["frameRate"] = *video_open_frame_rate;
            }
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value() &&
                runtime_->video_reconfigure->previous_audio_open_params.is_object() &&
                !runtime_->video_reconfigure->previous_audio_open_params.empty()) {
                open_params = runtime_->video_reconfigure->previous_audio_open_params;
                open_params.erase("streamId");
                open_params.erase("state");
            }
        }
        if (open_params.empty()) {
            open_params = nlohmann::json{
                {"source", source},
                {"peerRole", "transmitter"},
                {"codec", "aac"},
                {"transportFormat", "adts"},
                {"sampleRate", config_.audio_sample_rate == 0 ? 48000 : config_.audio_sample_rate},
                {"channels", choose_audio_channels(*capabilities, source, config_.audio_channels)},
                {"streamProfile", "media.audio"},
                {"cursorUnit", "timestampUs"},
            };
        }
    }

    const auto response = call_json(open_stream_method_name(kind), open_params);
    if (!response.has_value()) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_event = std::string(media_kind_name(kind)) + "-open-failed";
        return false;
    }

    const auto stream_id = json_u32_or(*response, "streamId", 0);
    if (stream_id == 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_event = std::string(media_kind_name(kind)) + "-open-no-stream-id";
        return false;
    }

    MediaStreamDescriptor descriptor;
    // The adapter owns a device-scoped stream generation. AxentHost binds
    // the final lease session id when publishing/replaying this descriptor.
    descriptor.key.session_id.clear();
    descriptor.key.stream_id = stream_id;
    descriptor.device_id = device_id;
    descriptor.kind = kind;
    descriptor.codec = codec_for_open_result(kind, *response);
    descriptor.source = json_string_or(*response, "source", source);
    descriptor.transport_format = json_string_or(
        *response, "transportFormat", json_string_or(open_params, "transportFormat"));
    descriptor.stream_profile = json_string_or(
        *response, "streamProfile", json_string_or(open_params, "streamProfile"));
    descriptor.cursor_unit = json_string_or(
        *response, "cursorUnit", json_string_or(open_params, "cursorUnit", "timestampUs"));
    descriptor.sample_rate = json_u32_or(
        *response, "sampleRate", json_u32_or(open_params, "sampleRate", 0));
    descriptor.channels = json_u32_or(
        *response, "channels", json_u32_or(open_params, "channels", 0));
    descriptor.width = json_u32_or(
        *response, "width", json_u32_or(open_params, "width", 0));
    descriptor.height = json_u32_or(
        *response, "height", json_u32_or(open_params, "height", 0));
    descriptor.frame_rate = json_u32_or(
        *response, "frameRate", json_u32_or(open_params, "frameRate", 0));
    std::optional<MediaStreamDescriptor> replaced_descriptor;
    std::uint32_t active_stream_count = 0;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        auto& generation = media_stream_generations_[stream_id];
        ++generation;
        if (generation == 0) {
            generation = 1;
        }
        descriptor.key.generation = generation;
        const auto existing = active_media_streams_.find(stream_id);
        if (existing != active_media_streams_.end()) {
            replaced_descriptor = existing->second.descriptor;
        }
        active_media_streams_[stream_id] = ActiveMediaStream{descriptor};
        active_stream_count = static_cast<std::uint32_t>(active_media_streams_.size());
        if (source_recovery_cycle_active_) {
            const auto kind_bit = kind == MediaKind::Video ? std::uint8_t{0x01}
                                                            : std::uint8_t{0x02};
            source_recovery_reopen_mask_ &= static_cast<std::uint8_t>(~kind_bit);
            if (source_recovery_reopen_mask_ == 0) {
                source_recovery_cycle_active_ = false;
                source_recovery_close_sent_ = false;
                pending_video_source_recovery_close_.reset();
                pending_audio_source_recovery_close_.reset();
            }
        }
    }
    std::vector<MediaStreamEvent> lifecycle_events;
    if (replaced_descriptor.has_value()) {
        lifecycle_events.push_back(
            {MediaStreamEventKind::Closed,
             std::move(*replaced_descriptor),
             open_reason});
    }
    lifecycle_events.push_back(
        {MediaStreamEventKind::Opened, descriptor, open_reason});
    enqueue_media_stream_events(std::move(lifecycle_events));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (update_retry_state) {
            diagnostics_.last_event = std::string(media_kind_name(kind)) + "-stream-open";
        }
        if (kind == MediaKind::Video) {
            diagnostics_.active_video_stream_id = stream_id;
        } else {
            diagnostics_.active_audio_stream_id = stream_id;
        }
        diagnostics_.active_media_streams = active_stream_count;
        if (update_retry_state) {
            media_configure_attempts_ = 0;
            next_media_configure_attempt_ = {};
        }
        if (kind == MediaKind::Video) {
            video_source_recovery_pending_ = false;
            next_video_source_recovery_attempt_ = {};
        } else {
            audio_source_recovery_pending_ = false;
            next_audio_source_recovery_attempt_ = {};
        }
    }
    if (kind == MediaKind::Video) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->active_video_open_params = open_params;
        for (const char* key : {"source", "peerRole", "codec", "streamProfile",
                                "cursorUnit", "syncGroupId", "castSessionId"}) {
            if (response->contains(key)) {
                runtime_->active_video_open_params[key] = (*response)[key];
            }
        }
        runtime_->video_params_state.source = descriptor.source;
        runtime_->video_params_state.desired_frame_rate =
            runtime_->session_video_frame_rate;
        runtime_->video_params_state.effective_frame_rate = descriptor.frame_rate == 0
            ? runtime_->session_video_frame_rate
            : std::optional<std::uint32_t>(descriptor.frame_rate);
        runtime_->video_params_state.stream_profile = descriptor.stream_profile;
        runtime_->video_params_state.active_stream_id = stream_id;
        if (!runtime_->video_reconfigure.has_value()) {
            runtime_->video_params_state.state = VideoStreamParamsStateKind::Applied;
            runtime_->video_params_state.phase = VideoStreamParamsPhase::Streaming;
            runtime_->video_params_state.rollback_applied = false;
            runtime_->video_params_state.last_error.reset();
            video_params_update = runtime_->video_params_state;
        }
    } else {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->active_audio_open_params = open_params;
        for (const char* key : {"source", "peerRole", "codec", "transportFormat",
                                "sampleRate", "channels", "streamProfile",
                                "cursorUnit", "syncGroupId", "castSessionId"}) {
            if (response->contains(key)) {
                runtime_->active_audio_open_params[key] = (*response)[key];
            }
        }
    }
    if (video_params_update.has_value()) {
        notify_video_stream_params_state(std::move(*video_params_update));
    }
    return true;
}

MediaFrame AxtpAdapter::frame_from_stream(const std::string& device_id,
                                          std::uint32_t stream_id,
                                          std::uint32_t sequence_id,
                                          std::uint64_t cursor,
                                          std::vector<std::uint8_t> data) const
{
    MediaFrame frame;
    frame.device_id = device_id;
    frame.stream_id = stream_id;
    frame.sequence_id = sequence_id;
    frame.cursor = cursor;
    frame.timestamp_us = cursor;
    frame.payload = std::move(data);
    frame.flags = MediaFrameFlag::EndOfFrame;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end()) {
            frame.session_id = binding->second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto it = active_media_streams_.find(stream_id);
        if (it != active_media_streams_.end()) {
            frame.generation = it->second.descriptor.key.generation;
            frame.kind = it->second.descriptor.kind;
            frame.codec = it->second.descriptor.codec;
            return frame;
        }
        const auto known_generation = media_stream_generations_.find(stream_id);
        if (known_generation != media_stream_generations_.end()) {
            // Preserve the last known generation for a configured stream that
            // has already closed. is_current_media_frame() will reject it
            // because there is no matching active descriptor. Truly unknown
            // legacy streams keep generation zero and retain compatibility.
            frame.generation = known_generation->second;
            return frame;
        }
    }
    if ((stream_id & 0xF000U) == 0x1000U) {
        frame.kind = MediaKind::Video;
        frame.codec = MediaCodec::H264;
    } else if ((stream_id & 0xF000U) == 0x2000U) {
        frame.kind = MediaKind::Audio;
        frame.codec = MediaCodec::Aac;
    } else {
        frame.kind = MediaKind::Unknown;
        frame.codec = MediaCodec::Opaque;
    }
    return frame;
}

void AxtpAdapter::handle_stream_payload(const std::string& device_id,
                                        std::uint32_t stream_id,
                                        std::uint32_t sequence_id,
                                        std::uint64_t cursor,
                                        std::vector<std::uint8_t> data)
{
    auto frame = frame_from_stream(
        device_id, stream_id, sequence_id, cursor, std::move(data));
    std::lock_guard<std::mutex> lock(pending_media_mutex_);
    pending_media_frames_.emplace(device_id, std::move(frame));
}

bool AxtpAdapter::is_current_media_frame(const MediaFrame& frame) const
{
    if (frame.generation == 0) {
        return true;
    }
    std::lock_guard<std::mutex> lock(media_stream_mutex_);
    const auto active = active_media_streams_.find(frame.stream_id);
    return active != active_media_streams_.end() &&
        active->second.descriptor.device_id == frame.device_id &&
        active->second.descriptor.key.generation == frame.generation;
}

void AxtpAdapter::drain_pending_media_callbacks()
{
    std::lock_guard<std::recursive_mutex> dispatch_lock(
        media_callback_dispatch_mutex_);
    if (draining_media_callbacks_) {
        return;
    }
    draining_media_callbacks_ = true;
    struct DrainStateGuard {
        bool& active;
        ~DrainStateGuard() { active = false; }
    } drain_state_guard{draining_media_callbacks_};

    for (;;) {
        std::queue<MediaStreamEvent> events;
        std::queue<std::pair<std::string, MediaFrame>> frames;
        {
            std::lock_guard<std::mutex> lock(pending_media_event_mutex_);
            events.swap(pending_media_stream_events_);
        }
        {
            std::lock_guard<std::mutex> lock(pending_media_mutex_);
            frames.swap(pending_media_frames_);
        }
        if (events.empty() && frames.empty()) {
            return;
        }

        MediaFrameCallback callback;
        MediaStreamEventCallback event_callback;
        {
            std::lock_guard<std::mutex> lock(media_callback_mutex_);
            callback = media_frame_callback_;
            event_callback = media_stream_event_callback_;
        }
        while (!events.empty()) {
            auto event = std::move(events.front());
            events.pop();
            if (event_callback) {
                try {
                    event_callback(std::move(event));
                } catch (...) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    diagnostics_.last_error = "media stream event callback threw";
                }
            }
        }
        while (!frames.empty()) {
            auto entry = std::move(frames.front());
            frames.pop();
            if (!callback || !is_current_media_frame(entry.second)) {
                continue;
            }
            try {
                callback(entry.first, std::move(entry.second));
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                diagnostics_.last_error = "media frame callback threw";
            }
        }
    }
}

void AxtpAdapter::refresh_diagnostics_locked()
{
    diagnostics_.session_health = session_health_;
    diagnostics_.health_probe_failures = health_probe_failures_;
    diagnostics_.session_recoveries = session_recoveries_;
    diagnostics_.last_session_recovery_reason = last_session_recovery_reason_;
#if AXENT_HAS_AXTP_HID_TRANSPORT
    if (runtime_->client == nullptr || !runtime_->client->isConnected()) {
        diagnostics_.open = false;
        return;
    }
    const auto* transport = dynamic_cast<const axent::transport::HidTransport*>(runtime_->active_transport);
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
    diagnostics_.open = runtime_->client != nullptr && runtime_->client->isConnected() && runtime_->client->isAppReady();
#endif
}

void testing::AxtpAdapterTestSeam::disconnect_session(AxtpAdapter& adapter)
{
    std::lock_guard<std::mutex> client_lock(adapter.client_mutex_);
    if (adapter.runtime_->client != nullptr) {
        adapter.runtime_->client->close();
    }
}

void testing::AxtpAdapterTestSeam::bind_media_delivery_session(
    AxtpAdapter& adapter,
    const std::string& device_id,
    const std::string& session_id)
{
    adapter.bind_media_delivery_session(device_id, session_id);
}

void testing::AxtpAdapterTestSeam::release_session(
    AxtpAdapter& adapter,
    const std::string& device_id)
{
    adapter.reset_session_for_device(device_id);
}

void testing::AxtpAdapterTestSeam::stop_session_pump(AxtpAdapter& adapter)
{
    std::thread pump;
    {
        std::lock_guard<std::mutex> lock(adapter.mutex_);
        pump = adapter.request_stop_session_pump_locked();
    }
    if (pump.joinable()) {
        pump.join();
    }
}

void testing::AxtpAdapterTestSeam::enqueue_stream_payload(
    AxtpAdapter& adapter,
    const std::string& device_id,
    std::uint32_t stream_id,
    std::uint32_t sequence_id,
    std::uint64_t cursor,
    std::vector<std::uint8_t> data)
{
    adapter.handle_stream_payload(
        device_id, stream_id, sequence_id, cursor, std::move(data));
}

void testing::AxtpAdapterTestSeam::reopen_media_streams(
    AxtpAdapter& adapter,
    const std::string& device_id)
{
    std::lock_guard<std::mutex> session_lock(adapter.session_mutex_);
    std::lock_guard<std::mutex> client_lock(adapter.client_mutex_);
    adapter.configure_media_streams(device_id);
}

void testing::AxtpAdapterTestSeam::drain_media_callbacks(AxtpAdapter& adapter)
{
    adapter.drain_pending_media_callbacks();
}

bool testing::AxtpAdapterTestSeam::is_current_media_frame(
    const AxtpAdapter& adapter,
    const MediaFrame& frame)
{
    return adapter.is_current_media_frame(frame);
}

} // namespace axent
