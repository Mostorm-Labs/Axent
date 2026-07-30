#pragma once

#include <functional>

#include "axent/core/control_operation.hpp"

namespace axent {

class ControlOperationSource final {
public:
    using CancelHandler = std::function<void()>;
    using CompletionHandler = std::function<void(const ControlResult&)>;

    explicit ControlOperationSource(CancelHandler cancel_handler = {});

    ControlOperationPtr operation() const;
    bool complete(ControlResult result) const;
    void on_complete(CompletionHandler handler) const;
    void set_cancel_handler(CancelHandler handler) const;
    bool cancellation_requested() const noexcept;
    static void observe(const ControlOperationPtr& operation,
                        CompletionHandler handler);

private:
    std::shared_ptr<ControlOperation::State> state_;
    ControlOperationPtr operation_;
};

} // namespace axent
