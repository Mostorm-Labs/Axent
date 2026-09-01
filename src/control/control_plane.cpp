#include "axent/control/control_plane.hpp"

namespace axent {

ControlPlane::ControlPlane(Broker& broker)
    : broker_(broker)
{
}

std::string ControlPlane::route_key(const nlohmann::json& message) const
{
    return broker_.route_key(decode_control_message(message).command);
}

nlohmann::json ControlPlane::handle_text(const nlohmann::json& message)
{
    const auto decoded = decode_control_message(message);
    if (decoded.validation_error.has_value()) {
        return encode_control_response(
            decoded,
            {ControlStatus::InvalidArgument,
             {{"error", *decoded.validation_error}}});
    }
    const auto result = broker_.dispatch(decoded.command);
    return encode_control_response(decoded, result);
}

} // namespace axent
