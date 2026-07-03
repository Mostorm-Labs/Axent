#pragma once

#include "axent/core/types.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

class Middleware {
public:
    explicit Middleware(Logger& logger);

    void before_dispatch(const ControlCommand& command);
    void after_dispatch(const ControlCommand& command, const ControlResult& result);

private:
    Logger& logger_;
};

} // namespace axent
