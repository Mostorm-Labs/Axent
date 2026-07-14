#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "axent/firmware/firmware_update_service.hpp"

namespace {

using namespace axent::firmware;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TestMaintenance final : public MaintenanceLeaseProvider {
public:
    MaintenanceLease try_acquire_maintenance(
        const std::string& device_id,
        std::string& reason) override
    {
        ++attempts;
        if (busy) {
            reason = "test maintenance busy";
            return {};
        }
        ++active;
        return grant_maintenance(device_id, [this]() {
            --active;
            ++released;
        });
    }

    bool busy = false;
    int attempts = 0;
    int active = 0;
    int released = 0;
};

class TestBackend final : public IFirmwareUpdateBackend {
public:
    FirmwareBeginResult begin(const FirmwareBeginRequest& request) override
    {
        ++begin_calls;
        begin_request = request;
        if (throw_begin) {
            throw std::runtime_error("begin exception");
        }
        return begin_result;
    }

    FirmwareBackendStatus send_chunk(const FirmwareChunkRequest& request) override
    {
        chunks.push_back(request);
        if (fail_chunk >= 0 &&
            request.sequence_id == static_cast<std::uint32_t>(fail_chunk)) {
            return FirmwareBackendStatus::failure(0x000F, "stream send failed");
        }
        return FirmwareBackendStatus::success();
    }

    FirmwareFinishResult finish(const FirmwareFinishRequest& request) override
    {
        ++finish_calls;
        finish_request = request;
        if (throw_finish) {
            throw std::runtime_error("finish exception");
        }
        return finish_result;
    }

    FirmwareBeginResult begin_result{
        FirmwareBackendStatus::success(), "update-1", "receiving", 0x1001, 4};
    FirmwareFinishResult finish_result{
        FirmwareBackendStatus::success(), "update-1", true, "verifying"};
    FirmwareBeginRequest begin_request;
    FirmwareFinishRequest finish_request;
    std::vector<FirmwareChunkRequest> chunks;
    int begin_calls = 0;
    int finish_calls = 0;
    int fail_chunk = -1;
    bool throw_begin = false;
    bool throw_finish = false;
};

struct FixtureFiles {
    FixtureFiles()
    {
        root = std::filesystem::temp_directory_path() /
               ("axent-firmware-service-" +
                std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        image = root / "firmware.bin";
        std::ofstream output(image, std::ios::binary);
        output << "123456789";
        require(static_cast<bool>(output), "firmware fixture should be writable");
        output.close();
        empty = root / "empty.bin";
        std::ofstream(empty, std::ios::binary).close();
    }

    ~FixtureFiles()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    FirmwareUpdateRequest request() const
    {
        FirmwareUpdateRequest value;
        value.device_id = "device-1";
        value.file_path = image;
        value.file_id = "application";
        value.target = "main";
        value.package_id = "pkg-1";
        value.version = "1.2.3";
        value.preferred_chunk_size = 8;
        value.timeout = std::chrono::milliseconds(123);
        value.sid = "sid-1";
        return value;
    }

    std::filesystem::path root;
    std::filesystem::path image;
    std::filesystem::path empty;
};

void test_success_and_progress(const FixtureFiles& files)
{
    TestBackend backend;
    TestMaintenance maintenance;
    FirmwareUpdateService service(backend, maintenance);
    std::vector<FirmwareUpdateProgress> progress;
    const auto result = service.run(files.request(), [&](const auto& value) {
        progress.push_back(value);
    });

    require(result.ok(), "firmware update should succeed");
    require(result.md5 == "25f9e794323b453885f5181f1b624d0b",
            "firmware service should calculate the RFC MD5 vector");
    require(result.bytes == 9 && result.chunks == 3,
            "server chunk size should split nine bytes into three chunks");
    require(result.chunk_size == 4 && result.stream_id == 0x1001,
            "begin response should select stream and server chunk size");
    require(backend.begin_request.file_id == "application" &&
                backend.begin_request.target == "main" &&
                backend.begin_request.package_id == "pkg-1" &&
                backend.begin_request.version == "1.2.3" &&
                backend.begin_request.size == 9 &&
                backend.begin_request.md5 == result.md5 &&
                backend.begin_request.sid == "sid-1",
            "typed begin request should preserve firmware metadata");
    require(backend.chunks.size() == 3,
            "backend should receive every firmware chunk");
    require(backend.chunks[0].sequence_id == 0 && backend.chunks[0].cursor == 0 &&
                backend.chunks[0].data.size() == 4 &&
                backend.chunks[1].sequence_id == 1 && backend.chunks[1].cursor == 4 &&
                backend.chunks[1].data.size() == 4 &&
                backend.chunks[2].sequence_id == 2 && backend.chunks[2].cursor == 8 &&
                backend.chunks[2].data.size() == 1,
            "chunks should preserve sequence, cursor, and tail size");
    require(backend.finish_calls == 1 &&
                backend.finish_request.update_session_id == "update-1" &&
                backend.finish_request.sid == "sid-1",
            "finish should use the begin session and request SID");
    require(maintenance.active == 0 && maintenance.released == 1,
            "maintenance lease should release after success");
    require(!progress.empty() &&
                progress.front().stage == FirmwareUpdateStage::Validating &&
                progress.back().stage == FirmwareUpdateStage::Completed,
            "progress should span validation through completion");
    std::uint64_t previous_bytes = 0;
    for (const auto& value : progress) {
        require(value.bytes_transferred >= previous_bytes,
                "progress bytes should be monotonic");
        previous_bytes = value.bytes_transferred;
    }
}

void test_validation_and_busy(const FixtureFiles& files)
{
    TestBackend backend;
    TestMaintenance maintenance;
    FirmwareUpdateService service(backend, maintenance);

    auto request = files.request();
    request.file_path.clear();
    require(service.run(request).code == FirmwareUpdateCode::InvalidArgument,
            "missing file path should be invalid");
    request = files.request();
    request.device_id.clear();
    require(service.run(request).code == FirmwareUpdateCode::InvalidArgument,
            "missing device id should be invalid");
    request = files.request();
    request.file_path = files.root / "missing.bin";
    require(service.run(request).code == FirmwareUpdateCode::FileReadFailed,
            "missing file should report read failure");
    request = files.request();
    request.file_path = files.empty;
    require(service.run(request).code == FirmwareUpdateCode::InvalidArgument,
            "empty firmware should be rejected");
    request = files.request();
    request.preferred_chunk_size = 0;
    require(service.run(request).code == FirmwareUpdateCode::InvalidArgument,
            "zero chunk size should be rejected");
    require(backend.begin_calls == 0 && maintenance.attempts == 0,
            "validation failures must not acquire or call the backend");

    maintenance.busy = true;
    request = files.request();
    const auto busy = service.run(request);
    require(busy.code == FirmwareUpdateCode::Busy &&
                busy.message == "test maintenance busy" &&
                backend.begin_calls == 0,
            "Busy should fail fast before begin");
}

void test_failure_paths_release_lease(const FixtureFiles& files)
{
    {
        TestBackend backend;
        TestMaintenance maintenance;
        backend.begin_result.status =
            FirmwareBackendStatus::failure(0x000A, "begin rejected");
        FirmwareUpdateService service(backend, maintenance);
        const auto result = service.run(files.request());
        require(result.code == FirmwareUpdateCode::BeginFailed &&
                    result.protocol_status == 0x000A &&
                    result.failed_method == "firmware.beginUpdate" &&
                    maintenance.released == 1,
                "begin failure should be typed and release maintenance");
    }
    {
        TestBackend backend;
        TestMaintenance maintenance;
        backend.begin_result.update_session_id.clear();
        FirmwareUpdateService service(backend, maintenance);
        const auto malformed = service.run(files.request());
        require(malformed.code == FirmwareUpdateCode::BeginFailed &&
                    malformed.protocol_status == 0x0033 &&
                    maintenance.released == 1,
                "malformed begin response should release maintenance");
    }
    {
        TestBackend backend;
        TestMaintenance maintenance;
        backend.fail_chunk = 1;
        FirmwareUpdateService service(backend, maintenance);
        const auto result = service.run(files.request());
        require(result.code == FirmwareUpdateCode::TransferFailed &&
                    result.chunks == 1 && backend.finish_calls == 0 &&
                    result.failed_method == "firmware.stream" &&
                    maintenance.released == 1,
                "stream failure should stop before finish and release maintenance");
    }
    {
        TestBackend backend;
        TestMaintenance maintenance;
        backend.finish_result.status =
            FirmwareBackendStatus::failure(0x000F, "finish unavailable");
        FirmwareUpdateService service(backend, maintenance);
        const auto result = service.run(files.request());
        require(result.code == FirmwareUpdateCode::FinishFailed &&
                    result.failed_method == "firmware.finishUpdate" &&
                    maintenance.released == 1,
                "finish failure should be typed and release maintenance");
    }
    {
        TestBackend backend;
        TestMaintenance maintenance;
        backend.finish_result.accepted = false;
        FirmwareUpdateService service(backend, maintenance);
        const auto result = service.run(files.request());
        require(result.code == FirmwareUpdateCode::Rejected &&
                    result.protocol_status == 0 &&
                    maintenance.released == 1,
                "accepted=false should be a typed rejection");
    }
}

void test_fallback_and_observer_isolation(const FixtureFiles& files)
{
    TestBackend backend;
    TestMaintenance maintenance;
    backend.begin_result.chunk_size = 0;
    auto request = files.request();
    request.preferred_chunk_size = 5;
    FirmwareUpdateService service(backend, maintenance);
    const auto result = service.run(request, [](const auto&) {
        throw std::runtime_error("observer failure");
    });
    require(result.ok() && result.chunk_size == 5 && result.chunks == 2,
            "zero server chunk should fall back and observer exceptions should be isolated");
}

} // namespace

int main()
{
    FixtureFiles files;
    test_success_and_progress(files);
    test_validation_and_busy(files);
    test_failure_paths_release_lease(files);
    test_fallback_and_observer_isolation(files);
    return 0;
}
