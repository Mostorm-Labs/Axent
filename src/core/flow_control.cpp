#include "axent/core/flow_control.hpp"

namespace axent {

FlowControl::FlowControl(const FlowControl& other)
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    snapshot_ = other.snapshot_;
}

FlowControl& FlowControl::operator=(const FlowControl& other)
{
    if (this == &other) {
        return *this;
    }
    std::scoped_lock lock(mutex_, other.mutex_);
    snapshot_ = other.snapshot_;
    return *this;
}

FlowControl::FlowControl(FlowControl&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    snapshot_ = other.snapshot_;
}

FlowControl& FlowControl::operator=(FlowControl&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    std::scoped_lock lock(mutex_, other.mutex_);
    snapshot_ = other.snapshot_;
    return *this;
}

void FlowControl::pause()
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.paused = true;
}

void FlowControl::resume()
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.paused = false;
}

void FlowControl::record_drop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++snapshot_.dropped;
}

FlowControlSnapshot FlowControl::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

} // namespace axent
