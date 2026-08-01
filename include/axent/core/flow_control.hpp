#pragma once

#include <mutex>

namespace axent {

struct FlowControlSnapshot {
    bool paused = false;
    int dropped = 0;
};

class FlowControl {
public:
    FlowControl() = default;
    FlowControl(const FlowControl& other);
    FlowControl& operator=(const FlowControl& other);
    FlowControl(FlowControl&& other) noexcept;
    FlowControl& operator=(FlowControl&& other) noexcept;

    void pause();
    void resume();
    void record_drop();
    FlowControlSnapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    FlowControlSnapshot snapshot_;
};

} // namespace axent
