#pragma once

#include <map>
#include <string>

#include "axent/core/adapter.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"

namespace axent {

class Broker {
public:
    Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control);

    void register_adapter(Adapter* adapter);
    ControlResult dispatch(const ControlCommand& command);

private:
    RouteManager& routes_;
    Middleware& middleware_;
    FlowControl& flow_control_;
    std::map<std::string, Adapter*> adapters_;
};

} // namespace axent
