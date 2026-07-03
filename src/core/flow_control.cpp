#include "axent/core/flow_control.hpp"

namespace axent {

void FlowControl::pause()
{
    snapshot_.paused = true;
}

void FlowControl::resume()
{
    snapshot_.paused = false;
}

void FlowControl::record_drop()
{
    ++snapshot_.dropped;
}

FlowControlSnapshot FlowControl::snapshot() const
{
    return snapshot_;
}

} // namespace axent
