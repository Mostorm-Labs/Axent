#include "axent/control/websocket_server.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

namespace axent {

struct WebSocketServer::Impl {
    struct ControlTask {
        nlohmann::json request;
        std::shared_ptr<ix::WebSocket> web_socket;
    };

    static constexpr std::size_t kMaxPendingRequests = 256;

    std::unique_ptr<ix::WebSocketServer> server;
    std::mutex dispatch_mutex;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::deque<ControlTask> pending_requests;
    std::thread worker;
    std::uint16_t local_port = 0;
    bool net_initialized = false;
    bool stopping_worker = false;

    void stop()
    {
        if (server) {
            server->stop();
            server.reset();
        }
        stop_worker();
        local_port = 0;
        if (net_initialized) {
            ix::uninitNetSystem();
            net_initialized = false;
        }
    }

    void start_worker(ControlPlane& control_plane)
    {
        stop_worker();
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stopping_worker = false;
        }
        worker = std::thread([this, &control_plane]() {
            run_worker(control_plane);
        });
    }

    void stop_worker()
    {
        std::thread stopped_worker;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stopping_worker = true;
            pending_requests.clear();
            stopped_worker = std::move(worker);
        }
        queue_cv.notify_all();
        if (stopped_worker.joinable() && stopped_worker.get_id() != std::this_thread::get_id()) {
            stopped_worker.join();
        } else if (stopped_worker.joinable()) {
            stopped_worker.detach();
        }
    }

    std::shared_ptr<ix::WebSocket> shared_client_for(ix::WebSocket& web_socket)
    {
        if (!server) {
            return nullptr;
        }
        for (const auto& client : server->getClients()) {
            if (client.get() == &web_socket) {
                return client;
            }
        }
        return nullptr;
    }

    bool enqueue(nlohmann::json request, std::shared_ptr<ix::WebSocket> web_socket)
    {
        if (!web_socket) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stopping_worker || pending_requests.size() >= kMaxPendingRequests) {
                return false;
            }
            pending_requests.push_back({std::move(request), std::move(web_socket)});
        }
        queue_cv.notify_one();
        return true;
    }

    std::string queue_full_response(const nlohmann::json& request)
    {
        const auto decoded = decode_control_message(request);
        const ControlResult result{
            ControlStatus::Unavailable,
            {{"error", "control request queue full"}},
        };
        return encode_control_response(decoded, result).dump();
    }

    void run_worker(ControlPlane& control_plane)
    {
        for (;;) {
            ControlTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this]() {
                    return stopping_worker || !pending_requests.empty();
                });
                if (stopping_worker && pending_requests.empty()) {
                    break;
                }
                task = std::move(pending_requests.front());
                pending_requests.pop_front();
            }

            std::string response;
            {
                std::lock_guard<std::mutex> lock(dispatch_mutex);
                response = control_plane.handle_text(task.request).dump();
            }
            if (task.web_socket && task.web_socket->getReadyState() == ix::ReadyState::Open) {
                (void)task.web_socket->sendText(response);
            }
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
    impl_->start_worker(control_plane);
    impl_->server->setOnClientMessageCallback(
        [this, &control_plane](std::shared_ptr<ix::ConnectionState>,
                               ix::WebSocket& web_socket,
                               const ix::WebSocketMessagePtr& message) {
        (void)control_plane;
        if (!message || message->type != ix::WebSocketMessageType::Message || message->binary) {
            return;
        }

        const auto request = nlohmann::json::parse(message->str, nullptr, false);
        if (request.is_discarded()) {
            return;
        }

        auto client = impl_->shared_client_for(web_socket);
        if (!impl_->enqueue(request, client)) {
            const std::string response = impl_->queue_full_response(request);
            if (client && client->getReadyState() == ix::ReadyState::Open) {
                (void)client->sendText(response);
            }
        }
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
