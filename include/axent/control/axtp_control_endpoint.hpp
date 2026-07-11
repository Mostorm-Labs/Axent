#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "axent/control/control_contract.hpp"

namespace axent {

struct AxtpControlEndpointOptions {
    std::string bind_address = "127.0.0.1";
    std::uint16_t port = 0;
};

class AxtpControlEndpoint final {
public:
    AxtpControlEndpoint();
    ~AxtpControlEndpoint();

    AxtpControlEndpoint(const AxtpControlEndpoint&) = delete;
    AxtpControlEndpoint& operator=(const AxtpControlEndpoint&) = delete;
    AxtpControlEndpoint(AxtpControlEndpoint&&) = delete;
    AxtpControlEndpoint& operator=(AxtpControlEndpoint&&) = delete;

    control::RegistrationToken register_handler(control::ControlRoute route,
                                                  control::ControlHandler handler);

    control::ControlStatus start(AxtpControlEndpointOptions options = {});
    control::ControlStatus stop();
    bool running() const noexcept;
    std::uint16_t local_port() const noexcept;

    control::ControlStatus post(control::ControlTask task);

    control::ControlStatus publish_event(control::ControlRoute route,
                                         nlohmann::json data = nlohmann::json::object());

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
