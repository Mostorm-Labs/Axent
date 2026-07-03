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

    void register_adapter(Adapter& adapter);
    void unregister_adapter(const std::string& name);
    ControlResult dispatch(const ControlCommand& command);

private:
    RouteManager& routes_;
    Middleware& middleware_;
    FlowControl& flow_control_;
    // Registered adapters are non-owning and must outlive their broker registration.
    std::map<std::string, Adapter*> adapters_;
};

} // namespace axent
