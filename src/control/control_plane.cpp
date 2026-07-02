#include "axent/control/control_plane.hpp"

namespace axent {

ControlPlane::ControlPlane(Broker& broker)
    : broker_(broker)
{
}

nlohmann::json ControlPlane::handle_text(const nlohmann::json& message)
{
    const auto decoded = decode_control_message(message);
    const auto result = broker_.dispatch(decoded.command);
    return encode_control_response(decoded, result);
}

} // namespace axent
