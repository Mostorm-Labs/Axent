#include "axent/core/middleware.hpp"

namespace axent {

Middleware::Middleware(Logger& logger)
    : logger_(logger)
{
}

void Middleware::before_dispatch(const ControlCommand& command)
{
    logger_.audit("control.request", {
        {"requestId", command.request_id},
        {"method", command.method},
        {"deviceId", command.device_id},
        {"source", protocol_source_name(command.source)}
    });
}

void Middleware::after_dispatch(const ControlCommand& command, const ControlResult& result)
{
    logger_.audit("control.response", {
        {"requestId", command.request_id},
        {"method", command.method},
        {"deviceId", command.device_id},
        {"source", protocol_source_name(command.source)},
        {"status", control_status_name(result.status)}
    });
}

} // namespace axent
