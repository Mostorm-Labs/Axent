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
    if (decoded.command.source == ProtocolSource::JsonRpc &&
        message.is_object() &&
        (message.contains("src") || message.contains("dst"))) {
        const bool has_valid_envelope =
            message.contains("src") && message.at("src").is_string() &&
            !message.at("src").get<std::string>().empty() &&
            message.contains("dst") && message.at("dst").is_string() &&
            !message.at("dst").get<std::string>().empty();
        if (!has_valid_envelope) {
            return encode_control_response(
                decoded,
                {ControlStatus::InvalidArgument,
                 {{"error", "JSON-RPC src and dst must be non-empty strings"}}});
        }
    }
    const auto result = broker_.dispatch(decoded.command);
    return encode_control_response(decoded, result);
}

} // namespace axent
