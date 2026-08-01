#include "axent/core/middleware.hpp"

#include <utility>

namespace axent {

Middleware::Middleware(Logger& logger)
    : logger_(logger)
{
}

void Middleware::before_dispatch(const ControlCommand& command)
{
    nlohmann::json fields = {
        {"requestId", command.request_id},
        {"method", command.method},
        {"deviceId", command.device_id},
        {"source", protocol_source_name(command.source)}
    };
    if (!command.src.empty()) {
        fields["src"] = command.src;
    }
    if (!command.dst.empty()) {
        fields["dst"] = command.dst;
    }
    logger_.audit("control.request", std::move(fields));
}

void Middleware::after_dispatch(const ControlCommand& command, const ControlResult& result)
{
    nlohmann::json fields = {
        {"requestId", command.request_id},
        {"method", command.method},
        {"deviceId", command.device_id},
        {"source", protocol_source_name(command.source)},
        {"status", control_status_name(result.status)}
    };
    if (!command.src.empty()) {
        fields["src"] = command.src;
    }
    if (!command.dst.empty()) {
        fields["dst"] = command.dst;
    }
    logger_.audit("control.response", std::move(fields));
}

} // namespace axent
