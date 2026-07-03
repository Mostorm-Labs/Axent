#pragma once

#include <nlohmann/json.hpp>

#include "axent/control/protocol_codecs.hpp"
#include "axent/core/broker.hpp"

namespace axent {

class ControlPlane {
public:
    explicit ControlPlane(Broker& broker);

    nlohmann::json handle_text(const nlohmann::json& message);

private:
    Broker& broker_;
};

} // namespace axent
