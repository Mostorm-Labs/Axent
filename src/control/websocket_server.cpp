#include "axent/control/websocket_server.hpp"

#include <mutex>
#include <string>

#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

namespace axent {

struct WebSocketServer::Impl {
    std::unique_ptr<ix::WebSocketServer> server;
    std::mutex request_mutex;
    std::uint16_t local_port = 0;
    bool net_initialized = false;

    void stop()
    {
        if (server) {
            server->stop();
            server.reset();
        }
        local_port = 0;
        if (net_initialized) {
            ix::uninitNetSystem();
            net_initialized = false;
        }
    }
};

WebSocketServer::WebSocketServer()
    : impl_(std::make_unique<Impl>())
{
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(ControlPlane& control_plane, const std::string& bind_host, std::uint16_t port)
{
    stop();
    impl_->net_initialized = ix::initNetSystem();
    const auto listen_port = port == 0 ? ix::getFreePort() : static_cast<int>(port);
    if (listen_port <= 0) {
        stop();
        return false;
    }

    impl_->server = std::make_unique<ix::WebSocketServer>(listen_port, bind_host);
    impl_->server->disablePerMessageDeflate();
    impl_->server->setOnClientMessageCallback(
        [this, &control_plane](std::shared_ptr<ix::ConnectionState>,
                               ix::WebSocket& web_socket,
                               const ix::WebSocketMessagePtr& message) {
        if (!message || message->type != ix::WebSocketMessageType::Message || message->binary) {
            return;
        }

        const auto request = nlohmann::json::parse(message->str, nullptr, false);
        if (request.is_discarded()) {
            return;
        }

        std::string response;
        {
            std::lock_guard<std::mutex> lock(impl_->request_mutex);
            response = control_plane.handle_text(request).dump();
        }
        (void)web_socket.sendText(response);
    });

    if (!impl_->server->listenAndStart()) {
        stop();
        return false;
    }

    impl_->local_port = static_cast<std::uint16_t>(listen_port);
    return true;
}

void WebSocketServer::stop()
{
    impl_->stop();
}

std::uint16_t WebSocketServer::local_port() const
{
    return impl_->local_port;
}

} // namespace axent
