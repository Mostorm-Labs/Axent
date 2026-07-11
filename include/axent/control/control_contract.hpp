#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace axent {

class AxtpControlEndpoint;

namespace control {

struct ControlRoute {
    std::optional<std::uint32_t> protocol_id;
    std::string name;
    bool allow_private = false;
};

struct ControlStatus {
    std::uint32_t code = 0;

    constexpr bool ok() const noexcept
    {
        return code == 0;
    }

    static constexpr ControlStatus from_protocol_code(std::uint32_t code) noexcept
    {
        return {code};
    }

    static constexpr ControlStatus success() noexcept { return {0x0000}; }
    static constexpr ControlStatus invalid_state() noexcept { return {0x0004}; }
    static constexpr ControlStatus busy() noexcept { return {0x0005}; }
    static constexpr ControlStatus timeout() noexcept { return {0x0006}; }
    static constexpr ControlStatus invalid_argument() noexcept { return {0x000A}; }
    static constexpr ControlStatus not_found() noexcept { return {0x000C}; }
    static constexpr ControlStatus internal_error() noexcept { return {0x000E}; }
    static constexpr ControlStatus unavailable() noexcept { return {0x000F}; }
    static constexpr ControlStatus not_supported() noexcept { return {0x0003}; }
};

constexpr bool operator==(ControlStatus lhs, ControlStatus rhs) noexcept
{
    return lhs.code == rhs.code;
}

constexpr bool operator!=(ControlStatus lhs, ControlStatus rhs) noexcept
{
    return !(lhs == rhs);
}

struct ControlRequest {
    std::uint32_t request_id = 0;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
};

struct ControlResult {
    ControlStatus status = ControlStatus::success();
    nlohmann::json body = nlohmann::json::object();
};

using ControlHandler = std::function<ControlResult(const ControlRequest&)>;
using ControlTask = std::function<void()>;

class RegistrationToken final {
public:
    RegistrationToken() = default;
    ~RegistrationToken();

    RegistrationToken(const RegistrationToken&) = delete;
    RegistrationToken& operator=(const RegistrationToken&) = delete;
    RegistrationToken(RegistrationToken&& other) noexcept;
    RegistrationToken& operator=(RegistrationToken&& other) noexcept;

    explicit operator bool() const noexcept;
    void reset() noexcept;

private:
    friend class ::axent::AxtpControlEndpoint;

    explicit RegistrationToken(std::function<void()> reset);

    std::function<void()> reset_;
};

} // namespace control
} // namespace axent
