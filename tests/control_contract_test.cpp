#include <cstdint>

#include "axent/control/control_contract.hpp"
#include "core/protocol/generated/axtp_ids_generated.h"

namespace {

using axent::control::ControlStatus;

constexpr std::uint32_t code(axtp::ErrorCode value)
{
    return static_cast<std::uint32_t>(value);
}

static_assert(ControlStatus::success().code == code(axtp::ErrorCode::Success));
static_assert(ControlStatus::success().code == 0x0000);
static_assert(ControlStatus::not_supported().code == code(axtp::ErrorCode::NotSupported));
static_assert(ControlStatus::not_supported().code == 0x0003);
static_assert(ControlStatus::invalid_state().code == code(axtp::ErrorCode::InvalidState));
static_assert(ControlStatus::busy().code == code(axtp::ErrorCode::Busy));
static_assert(ControlStatus::timeout().code == code(axtp::ErrorCode::Timeout));
static_assert(ControlStatus::invalid_argument().code == code(axtp::ErrorCode::InvalidArgument));
static_assert(ControlStatus::invalid_argument().code == 0x000A);
static_assert(ControlStatus::not_found().code == code(axtp::ErrorCode::NotFound));
static_assert(ControlStatus::internal_error().code == code(axtp::ErrorCode::InternalError));
static_assert(ControlStatus::unavailable().code == code(axtp::ErrorCode::Unavailable));
static_assert(ControlStatus::unavailable().code == 0x000F);
static_assert(ControlStatus::stream_not_open().code == code(axtp::ErrorCode::StreamNotOpen));
static_assert(ControlStatus::stream_not_open().code == 0x0506);

} // namespace

int main()
{
    return 0;
}
