#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "axent/core/types.hpp"

namespace axent {

struct ControlCallOptions {
    std::chrono::milliseconds timeout{5000};
    // The first public dispatch boundary converts timeout into an absolute
    // deadline.  Keeping the deadline with the operation prevents time spent
    // in middleware, session arbitration, or an earlier FIFO entry from
    // silently extending the caller's timeout budget.
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

class ControlOperation final {
public:
    ~ControlOperation();

    ControlOperation(const ControlOperation&) = delete;
    ControlOperation& operator=(const ControlOperation&) = delete;
    ControlOperation(ControlOperation&&) = delete;
    ControlOperation& operator=(ControlOperation&&) = delete;

    bool ready() const noexcept;
    std::optional<ControlResult> try_result() const;
    ControlResult wait();
    std::optional<ControlResult> wait_for(std::chrono::milliseconds timeout);
    void cancel() noexcept;

private:
    struct State;
    explicit ControlOperation(std::shared_ptr<State> state);

    std::shared_ptr<State> state_;

    friend class ControlOperationSource;
};

using ControlOperationPtr = std::shared_ptr<ControlOperation>;

// Adapter implementations that complete synchronously can use this helper
// without exposing producer-side operation state to product hosts.
ControlOperationPtr make_completed_control_operation(ControlResult result);

} // namespace axent
