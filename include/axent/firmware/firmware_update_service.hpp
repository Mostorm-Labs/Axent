#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace axent::firmware {

enum class FirmwareUpdateStage {
    Validating,
    AcquiringMaintenance,
    Beginning,
    Transferring,
    Finishing,
    Completed,
};

enum class FirmwareUpdateCode {
    Success,
    Busy,
    InvalidArgument,
    FileReadFailed,
    BeginFailed,
    TransferFailed,
    FinishFailed,
    Rejected,
    InternalError,
};

struct FirmwareUpdateRequest {
    std::string device_id;
    std::filesystem::path file_path;
    std::string file_id = "firmware";
    std::string target;
    std::string package_id;
    std::string version;
    std::uint32_t preferred_chunk_size = 1024;
    std::chrono::milliseconds timeout{5000};
    std::string sid;
};

struct FirmwareUpdateProgress {
    FirmwareUpdateStage stage = FirmwareUpdateStage::Validating;
    std::uint64_t bytes_transferred = 0;
    std::uint64_t total_bytes = 0;
    std::uint32_t chunks_transferred = 0;
};

using FirmwareProgressObserver = std::function<void(const FirmwareUpdateProgress&)>;

struct FirmwareBackendStatus {
    bool ok = false;
    std::uint16_t protocol_status = 0;
    std::string message;

    static FirmwareBackendStatus success();
    static FirmwareBackendStatus failure(std::uint16_t protocol_status,
                                         std::string message = {});
};

struct FirmwareBeginRequest {
    std::string file_id;
    std::string target;
    std::string package_id;
    std::string version;
    std::string md5;
    std::uint64_t size = 0;
    std::uint32_t preferred_chunk_size = 1024;
    std::chrono::milliseconds timeout{5000};
    std::string sid;
};

struct FirmwareBeginResult {
    FirmwareBackendStatus status;
    std::string update_session_id;
    std::string state;
    std::uint32_t stream_id = 0;
    std::uint32_t chunk_size = 0;
};

struct FirmwareChunkRequest {
    std::string update_session_id;
    std::uint32_t stream_id = 0;
    std::uint32_t sequence_id = 0;
    std::uint64_t cursor = 0;
    std::vector<std::uint8_t> data;
};

struct FirmwareFinishRequest {
    std::string update_session_id;
    std::chrono::milliseconds timeout{5000};
    std::string sid;
};

struct FirmwareFinishResult {
    FirmwareBackendStatus status;
    std::string update_session_id;
    bool accepted = false;
    std::string state;
};

class IFirmwareUpdateBackend {
public:
    virtual ~IFirmwareUpdateBackend() = default;

    virtual FirmwareBeginResult begin(const FirmwareBeginRequest& request) = 0;
    virtual FirmwareBackendStatus send_chunk(const FirmwareChunkRequest& request) = 0;
    virtual FirmwareFinishResult finish(const FirmwareFinishRequest& request) = 0;
};

class MaintenanceLease {
public:
    MaintenanceLease();
    ~MaintenanceLease();

    MaintenanceLease(const MaintenanceLease&) = delete;
    MaintenanceLease& operator=(const MaintenanceLease&) = delete;
    MaintenanceLease(MaintenanceLease&&) noexcept;
    MaintenanceLease& operator=(MaintenanceLease&&) noexcept;

    explicit operator bool() const noexcept;
    const std::string& device_id() const noexcept;
    void reset() noexcept;

private:
    friend class MaintenanceLeaseProvider;

    struct State;
    explicit MaintenanceLease(std::unique_ptr<State> state);

    std::unique_ptr<State> state_;
};

class MaintenanceLeaseProvider {
public:
    virtual ~MaintenanceLeaseProvider() = default;

    virtual MaintenanceLease try_acquire_maintenance(const std::string& device_id,
                                                      std::string& reason) = 0;

protected:
    static MaintenanceLease grant_maintenance(std::string device_id,
                                               std::function<void()> release);
};

struct FirmwareUpdateResult {
    FirmwareUpdateCode code = FirmwareUpdateCode::InternalError;
    std::string message;
    std::uint16_t protocol_status = 0;
    std::string failed_method;
    std::string file_id;
    std::string md5;
    std::string update_session_id;
    std::uint32_t stream_id = 0;
    std::uint32_t chunk_size = 0;
    std::uint32_t chunks = 0;
    std::uint64_t bytes = 0;
    FirmwareBeginResult begin;
    FirmwareFinishResult finish;

    bool ok() const noexcept;
};

class FirmwareUpdateService {
public:
    FirmwareUpdateService(IFirmwareUpdateBackend& backend,
                          MaintenanceLeaseProvider& maintenance);

    FirmwareUpdateResult run(const FirmwareUpdateRequest& request,
                             FirmwareProgressObserver observer = {});

private:
    IFirmwareUpdateBackend& backend_;
    MaintenanceLeaseProvider& maintenance_;
};

} // namespace axent::firmware
