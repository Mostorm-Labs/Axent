#include "axent/control/control_contract.hpp"

#include <utility>

namespace axent::control {

RegistrationToken::RegistrationToken(std::function<void()> reset)
    : reset_(std::move(reset))
{
}

RegistrationToken::~RegistrationToken()
{
    reset();
}

RegistrationToken::RegistrationToken(RegistrationToken&& other) noexcept
    : reset_(std::move(other.reset_))
{
    other.reset_ = {};
}

RegistrationToken& RegistrationToken::operator=(RegistrationToken&& other) noexcept
{
    if (this != &other) {
        reset();
        reset_ = std::move(other.reset_);
        other.reset_ = {};
    }
    return *this;
}

RegistrationToken::operator bool() const noexcept
{
    return static_cast<bool>(reset_);
}

void RegistrationToken::reset() noexcept
{
    if (!reset_) {
        return;
    }
    auto reset = std::move(reset_);
    reset_ = {};
    try {
        reset();
    } catch (...) {
    }
}

} // namespace axent::control
