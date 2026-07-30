#include "axent/core/control_operation.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "control_operation_internal.hpp"

namespace axent {

namespace {

ControlResult cancelled_result()
{
    return {ControlStatus::Unavailable, {{"error", "control operation cancelled"}}};
}

} // namespace

struct ControlOperation::State {
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::optional<ControlResult> result;
    ControlOperationSource::CancelHandler cancel_handler;
    std::vector<ControlOperationSource::CompletionHandler> completion_handlers;
    bool cancellation_requested = false;
};

ControlOperation::ControlOperation(std::shared_ptr<State> state)
    : state_(std::move(state))
{
}

ControlOperation::~ControlOperation() = default;

bool ControlOperation::ready() const noexcept
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->result.has_value();
}

std::optional<ControlResult> ControlOperation::try_result() const
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->result;
}

ControlResult ControlOperation::wait()
{
    std::unique_lock<std::mutex> lock(state_->mutex);
    state_->cv.wait(lock, [this]() { return state_->result.has_value(); });
    return *state_->result;
}

std::optional<ControlResult> ControlOperation::wait_for(
    std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (!state_->cv.wait_for(
            lock, timeout, [this]() { return state_->result.has_value(); })) {
        return std::nullopt;
    }
    return state_->result;
}

void ControlOperation::cancel() noexcept
{
    ControlOperationSource::CancelHandler cancel_handler;
    std::vector<ControlOperationSource::CompletionHandler> completion_handlers;
    ControlResult result;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->result.has_value()) {
            return;
        }
        state_->cancellation_requested = true;
        state_->result = cancelled_result();
        result = *state_->result;
        cancel_handler = state_->cancel_handler;
        completion_handlers.swap(state_->completion_handlers);
    }
    state_->cv.notify_all();
    if (cancel_handler) {
        try {
            cancel_handler();
        } catch (...) {
        }
    }
    for (auto& handler : completion_handlers) {
        if (handler) {
            try {
                handler(result);
            } catch (...) {
            }
        }
    }
}

ControlOperationSource::ControlOperationSource(CancelHandler cancel_handler)
    : state_(std::make_shared<ControlOperation::State>())
    , operation_(std::shared_ptr<ControlOperation>(
          new ControlOperation(state_)))
{
    state_->cancel_handler = std::move(cancel_handler);
}

ControlOperationPtr ControlOperationSource::operation() const
{
    return operation_;
}

bool ControlOperationSource::complete(ControlResult result) const
{
    std::vector<CompletionHandler> completion_handlers;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->result.has_value()) {
            return false;
        }
        state_->result = std::move(result);
        result = *state_->result;
        completion_handlers.swap(state_->completion_handlers);
    }
    state_->cv.notify_all();
    for (auto& handler : completion_handlers) {
        if (handler) {
            try {
                handler(result);
            } catch (...) {
            }
        }
    }
    return true;
}

void ControlOperationSource::on_complete(CompletionHandler handler) const
{
    std::optional<ControlResult> completed;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->result.has_value()) {
            completed = state_->result;
        } else {
            state_->completion_handlers.push_back(std::move(handler));
            return;
        }
    }
    if (handler) {
        handler(*completed);
    }
}

void ControlOperationSource::set_cancel_handler(CancelHandler handler) const
{
    bool already_cancelled = false;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->cancel_handler = std::move(handler);
        already_cancelled = state_->cancellation_requested;
        if (!already_cancelled) {
            return;
        }
        handler = state_->cancel_handler;
    }
    if (handler) {
        handler();
    }
}

bool ControlOperationSource::cancellation_requested() const noexcept
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->cancellation_requested;
}

void ControlOperationSource::observe(
    const ControlOperationPtr& operation,
    CompletionHandler handler)
{
    if (!operation) {
        return;
    }
    ControlOperationSource source;
    source.state_ = operation->state_;
    source.operation_ = operation;
    source.on_complete(std::move(handler));
}

ControlOperationPtr make_completed_control_operation(ControlResult result)
{
    ControlOperationSource source;
    source.complete(std::move(result));
    return source.operation();
}

} // namespace axent
