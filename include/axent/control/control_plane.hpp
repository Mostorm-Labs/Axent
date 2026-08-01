#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "axent/control/protocol_codecs.hpp"
#include "axent/core/broker.hpp"

namespace axent {

class ControlPlane {
public:
    explicit ControlPlane(Broker& broker);

    nlohmann::json handle_text(const nlohmann::json& message);
    // Internal WebSocket scheduling identity. It canonicalizes endpoint and
    // legacy selectors through the Broker without exposing physical IDs on
    // the wire.
    std::string route_key(const nlohmann::json& message) const;

private:
    Broker& broker_;
};

} // namespace axent
