#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "axent/control/control_plane.hpp"

namespace axent {

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    bool start(ControlPlane& control_plane, const std::string& bind_host, std::uint16_t port);
    void stop();
    std::uint16_t local_port() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
