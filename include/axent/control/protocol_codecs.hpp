#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "axent/core/types.hpp"

namespace axent {

struct DecodedControlMessage {
    ControlCommand command;
    std::string wire_method;
    nlohmann::json json_rpc_id = nullptr;
    nlohmann::json original;
};

DecodedControlMessage decode_control_message(const nlohmann::json& message);
nlohmann::json encode_control_response(const DecodedControlMessage& decoded, const ControlResult& result);

} // namespace axent
