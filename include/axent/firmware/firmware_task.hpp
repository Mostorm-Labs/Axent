#pragma once

#include <cstdint>
#include <string>

namespace axent {

struct FirmwareProgress {
    std::string stage = "queued";
    std::uint64_t transferred_bytes = 0;
    std::uint64_t total_bytes = 0;
    int percent = 0;
    std::string target_version;
    std::string last_error;
    bool recoverable = false;
};

class FirmwareTask {
public:
    FirmwareTask(std::string task_id, std::string device_id, std::string file_path);

    const std::string& task_id() const;
    const std::string& device_id() const;
    const std::string& file_path() const;
    const FirmwareProgress& progress() const;
    std::string state_name() const;

    void mark_validating(std::string target_version);
    void mark_transferring(std::uint64_t transferred, std::uint64_t total);
    void mark_succeeded();
    void mark_failed(std::string error);
    void mark_recoverable(std::string error);

private:
    std::string task_id_;
    std::string device_id_;
    std::string file_path_;
    std::string state_ = "queued";
    FirmwareProgress progress_;
};

} // namespace axent
