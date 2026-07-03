#pragma once

namespace axent {

struct FlowControlSnapshot {
    bool paused = false;
    int dropped = 0;
};

class FlowControl {
public:
    void pause();
    void resume();
    void record_drop();
    FlowControlSnapshot snapshot() const;

private:
    FlowControlSnapshot snapshot_;
};

} // namespace axent
