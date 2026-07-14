#include "axent/firmware/firmware_update_service.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <fstream>
#include <limits>
#include <utility>

#include "md5.hpp"

namespace axent::firmware {

struct MaintenanceLease::State {
    std::string device_id;
    std::function<void()> release;
    bool released = false;

    ~State()
    {
        if (!released && release) {
            try {
                release();
            } catch (...) {
            }
        }
    }
};

FirmwareBackendStatus FirmwareBackendStatus::success()
{
    return FirmwareBackendStatus{true, 0, {}};
}

FirmwareBackendStatus FirmwareBackendStatus::failure(std::uint16_t protocol_status,
                                                     std::string message)
{
    return FirmwareBackendStatus{false, protocol_status, std::move(message)};
}

MaintenanceLease::MaintenanceLease() = default;

MaintenanceLease::MaintenanceLease(std::unique_ptr<State> state)
    : state_(std::move(state))
{
}

MaintenanceLease::~MaintenanceLease() = default;

MaintenanceLease::MaintenanceLease(MaintenanceLease&& other) noexcept
    : state_(std::move(other.state_))
{
}

MaintenanceLease& MaintenanceLease::operator=(MaintenanceLease&& other) noexcept
{
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
    }
    return *this;
}

MaintenanceLease::operator bool() const noexcept
{
    return state_ != nullptr && !state_->released;
}

const std::string& MaintenanceLease::device_id() const noexcept
{
    static const std::string empty;
    return state_ ? state_->device_id : empty;
}

void MaintenanceLease::reset() noexcept
{
    if (!state_) {
        return;
    }
    if (!state_->released && state_->release) {
        try {
            state_->release();
        } catch (...) {
        }
    }
    state_->released = true;
    state_.reset();
}

MaintenanceLease MaintenanceLeaseProvider::grant_maintenance(
    std::string device_id,
    std::function<void()> release)
{
    auto state = std::make_unique<MaintenanceLease::State>();
    state->device_id = std::move(device_id);
    state->release = std::move(release);
    return MaintenanceLease(std::move(state));
}

bool FirmwareUpdateResult::ok() const noexcept
{
    return code == FirmwareUpdateCode::Success;
}

FirmwareUpdateService::FirmwareUpdateService(IFirmwareUpdateBackend& backend,
                                             MaintenanceLeaseProvider& maintenance)
    : backend_(backend)
    , maintenance_(maintenance)
{
}

namespace {

constexpr std::uint16_t k_unavailable_status = 0x000F;
constexpr std::uint16_t k_rpc_payload_invalid_status = 0x0033;

void notify(const FirmwareProgressObserver& observer, FirmwareUpdateProgress progress)
{
    if (!observer) {
        return;
    }
    try {
        observer(progress);
    } catch (...) {
        // Progress observers are diagnostic hooks and must not change update semantics.
    }
}

FirmwareUpdateResult invalid_result(FirmwareUpdateCode code,
                                    std::string message,
                                    std::string file_id)
{
    FirmwareUpdateResult result;
    result.code = code;
    result.message = std::move(message);
    result.file_id = std::move(file_id);
    return result;
}

std::string status_message(const FirmwareBackendStatus& status, const char* fallback)
{
    return status.message.empty() ? std::string(fallback) : status.message;
}

} // namespace

FirmwareUpdateResult FirmwareUpdateService::run(const FirmwareUpdateRequest& request,
                                                FirmwareProgressObserver observer)
{
    FirmwareUpdateResult result;
    result.file_id = request.file_id;
    notify(observer, {FirmwareUpdateStage::Validating, 0, 0, 0});

    if (request.file_path.empty()) {
        return invalid_result(FirmwareUpdateCode::InvalidArgument,
                              "firmware file path is required",
                              request.file_id);
    }
    if (request.device_id.empty()) {
        return invalid_result(FirmwareUpdateCode::InvalidArgument,
                              "firmware device id is required",
                              request.file_id);
    }
    if (request.file_id.empty()) {
        return invalid_result(FirmwareUpdateCode::InvalidArgument,
                              "firmware file id is required",
                              request.file_id);
    }
    if (request.preferred_chunk_size == 0) {
        return invalid_result(FirmwareUpdateCode::InvalidArgument,
                              "firmware chunk size must be greater than zero",
                              request.file_id);
    }
    if (request.timeout.count() < 0) {
        return invalid_result(FirmwareUpdateCode::InvalidArgument,
                              "firmware timeout must not be negative",
                              request.file_id);
    }

    std::ifstream input(request.file_path, std::ios::binary | std::ios::ate);
    if (!input) {
        return invalid_result(FirmwareUpdateCode::FileReadFailed,
                              "failed to read firmware file",
                              request.file_id);
    }
    const auto end = input.tellg();
    if (end <= 0) {
        return invalid_result(end == 0 ? FirmwareUpdateCode::InvalidArgument
                                       : FirmwareUpdateCode::FileReadFailed,
                              end == 0 ? "firmware file must not be empty"
                                       : "failed to determine firmware file size",
                              request.file_id);
    }
    const auto size = static_cast<std::uint64_t>(end);
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return invalid_result(FirmwareUpdateCode::FileReadFailed,
                              "firmware file is too large",
                              request.file_id);
    }
    if (size > static_cast<std::uint64_t>(
                   std::numeric_limits<std::streamsize>::max())) {
        return invalid_result(FirmwareUpdateCode::FileReadFailed,
                              "firmware file is too large to read",
                              request.file_id);
    }
    std::vector<std::uint8_t> image(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()));
    if (!input) {
        return invalid_result(FirmwareUpdateCode::FileReadFailed,
                              "failed to read firmware file",
                              request.file_id);
    }

    result.bytes = image.size();
    result.md5 = detail::md5_hex(image.data(), image.size());
    notify(observer,
           {FirmwareUpdateStage::Validating, 0, result.bytes, 0});

    std::string lease_reason;
    notify(observer,
           {FirmwareUpdateStage::AcquiringMaintenance, 0, result.bytes, 0});
    auto lease = maintenance_.try_acquire_maintenance(request.device_id, lease_reason);
    if (!lease) {
        result.code = FirmwareUpdateCode::Busy;
        result.message = lease_reason.empty() ? "maintenance lease busy" : std::move(lease_reason);
        return result;
    }

    FirmwareBeginRequest begin_request;
    begin_request.file_id = request.file_id;
    begin_request.target = request.target;
    begin_request.package_id = request.package_id;
    begin_request.version = request.version;
    begin_request.md5 = result.md5;
    begin_request.size = result.bytes;
    begin_request.preferred_chunk_size = request.preferred_chunk_size;
    begin_request.timeout = request.timeout;
    begin_request.sid = request.sid;

    notify(observer,
           {FirmwareUpdateStage::Beginning, 0, result.bytes, 0});
    try {
        result.begin = backend_.begin(begin_request);
    } catch (const std::exception& exception) {
        result.code = FirmwareUpdateCode::BeginFailed;
        result.protocol_status = k_unavailable_status;
        result.message = exception.what();
        result.failed_method = "firmware.beginUpdate";
        return result;
    } catch (...) {
        result.code = FirmwareUpdateCode::BeginFailed;
        result.protocol_status = k_unavailable_status;
        result.message = "firmware begin failed";
        result.failed_method = "firmware.beginUpdate";
        return result;
    }
    result.protocol_status = result.begin.status.protocol_status;
    if (!result.begin.status.ok) {
        result.code = FirmwareUpdateCode::BeginFailed;
        result.message = status_message(result.begin.status, "firmware begin failed");
        result.failed_method = "firmware.beginUpdate";
        return result;
    }
    if (result.begin.update_session_id.empty()) {
        result.code = FirmwareUpdateCode::BeginFailed;
        result.protocol_status = k_rpc_payload_invalid_status;
        result.message = "firmware begin response has no update session";
        result.failed_method = "firmware.beginUpdate";
        return result;
    }

    result.update_session_id = result.begin.update_session_id;
    result.stream_id = result.begin.stream_id;
    result.chunk_size = result.begin.chunk_size == 0
                            ? request.preferred_chunk_size
                            : result.begin.chunk_size;
    if (result.chunk_size == 0) {
        result.code = FirmwareUpdateCode::BeginFailed;
        result.protocol_status = k_rpc_payload_invalid_status;
        result.message = "firmware begin response has invalid chunk size";
        result.failed_method = "firmware.beginUpdate";
        return result;
    }

    for (std::size_t offset = 0; offset < image.size(); offset += result.chunk_size) {
        const auto count = std::min<std::size_t>(
            result.chunk_size, image.size() - offset);
        FirmwareChunkRequest chunk;
        chunk.update_session_id = result.update_session_id;
        chunk.stream_id = result.stream_id;
        chunk.sequence_id = result.chunks;
        chunk.cursor = offset;
        chunk.data.assign(image.begin() + static_cast<std::ptrdiff_t>(offset),
                          image.begin() + static_cast<std::ptrdiff_t>(offset + count));
        FirmwareBackendStatus chunk_status;
        try {
            chunk_status = backend_.send_chunk(chunk);
        } catch (const std::exception& exception) {
            result.code = FirmwareUpdateCode::TransferFailed;
            result.protocol_status = k_unavailable_status;
            result.message = exception.what();
            result.failed_method = "firmware.stream";
            return result;
        } catch (...) {
            result.code = FirmwareUpdateCode::TransferFailed;
            result.protocol_status = k_unavailable_status;
            result.message = "firmware stream failed";
            result.failed_method = "firmware.stream";
            return result;
        }
        if (!chunk_status.ok) {
            result.protocol_status = chunk_status.protocol_status;
            result.code = FirmwareUpdateCode::TransferFailed;
            result.message = status_message(chunk_status, "firmware stream failed");
            result.failed_method = "firmware.stream";
            return result;
        }
        ++result.chunks;
        notify(observer,
               {FirmwareUpdateStage::Transferring,
                result.bytes == 0 ? 0 : std::min<std::uint64_t>(
                                          result.bytes, offset + count),
                result.bytes,
                result.chunks});
    }

    FirmwareFinishRequest finish_request;
    finish_request.update_session_id = result.update_session_id;
    finish_request.timeout = request.timeout;
    finish_request.sid = request.sid;
    notify(observer,
           {FirmwareUpdateStage::Finishing, result.bytes, result.bytes, result.chunks});
    try {
        result.finish = backend_.finish(finish_request);
    } catch (const std::exception& exception) {
        result.code = FirmwareUpdateCode::FinishFailed;
        result.protocol_status = k_unavailable_status;
        result.message = exception.what();
        result.failed_method = "firmware.finishUpdate";
        return result;
    } catch (...) {
        result.code = FirmwareUpdateCode::FinishFailed;
        result.protocol_status = k_unavailable_status;
        result.message = "firmware finish failed";
        result.failed_method = "firmware.finishUpdate";
        return result;
    }
    result.protocol_status = result.finish.status.protocol_status;
    if (!result.finish.status.ok) {
        result.code = FirmwareUpdateCode::FinishFailed;
        result.message = status_message(result.finish.status, "firmware finish failed");
        result.failed_method = "firmware.finishUpdate";
        return result;
    }
    if (!result.finish.accepted) {
        result.code = FirmwareUpdateCode::Rejected;
        result.message = "firmware update rejected";
        result.failed_method = "firmware.finishUpdate";
        return result;
    }

    result.code = FirmwareUpdateCode::Success;
    result.message.clear();
    notify(observer,
           {FirmwareUpdateStage::Completed, result.bytes, result.bytes, result.chunks});
    return result;
}

} // namespace axent::firmware
