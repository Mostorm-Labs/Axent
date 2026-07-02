#include "axent/firmware/firmware_task.hpp"

#include <utility>

namespace axent {

FirmwareTask::FirmwareTask(std::string task_id, std::string device_id, std::string file_path)
    : task_id_(std::move(task_id)), device_id_(std::move(device_id)), file_path_(std::move(file_path))
{
}

const std::string& FirmwareTask::task_id() const
{
    return task_id_;
}

const std::string& FirmwareTask::device_id() const
{
    return device_id_;
}

const std::string& FirmwareTask::file_path() const
{
    return file_path_;
}

const FirmwareProgress& FirmwareTask::progress() const
{
    return progress_;
}

std::string FirmwareTask::state_name() const
{
    return state_;
}

void FirmwareTask::mark_validating(std::string target_version)
{
    state_ = "validating";
    progress_.stage = "validating";
    progress_.target_version = std::move(target_version);
}

void FirmwareTask::mark_transferring(std::uint64_t transferred, std::uint64_t total)
{
    state_ = "transferring";
    progress_.stage = "transferring";
    progress_.transferred_bytes = transferred;
    progress_.total_bytes = total;
    progress_.percent = total == 0 ? 0 : static_cast<int>((transferred * 100U) / total);
}

void FirmwareTask::mark_succeeded()
{
    state_ = "succeeded";
    progress_.stage = "succeeded";
    progress_.percent = 100;
}

void FirmwareTask::mark_failed(std::string error)
{
    state_ = "failed";
    progress_.stage = "failed";
    progress_.last_error = std::move(error);
    progress_.recoverable = false;
}

void FirmwareTask::mark_recoverable(std::string error)
{
    state_ = "recoverable";
    progress_.stage = "recoverable";
    progress_.last_error = std::move(error);
    progress_.recoverable = true;
}

} // namespace axent
