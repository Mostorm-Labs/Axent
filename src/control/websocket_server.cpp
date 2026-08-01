#include "axent/control/websocket_server.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

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
        std::string route_key;
    };

    static constexpr std::size_t kMaxPendingRequests = 256;
    static constexpr std::size_t kWorkerCount = 4;

    std::unique_ptr<ix::WebSocketServer> server;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::deque<ControlTask> pending_requests;
    std::unordered_set<std::string> active_routes;
    std::vector<std::thread> workers;
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
        workers.reserve(kWorkerCount);
        for (std::size_t index = 0; index < kWorkerCount; ++index) {
            workers.emplace_back([this, &control_plane]() {
                run_worker(control_plane);
            });
        }
    }

    void stop_worker()
    {
        std::vector<std::thread> stopped_workers;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stopping_worker = true;
            pending_requests.clear();
            stopped_workers = std::move(workers);
        }
        queue_cv.notify_all();
        for (auto& stopped_worker : stopped_workers) {
            if (stopped_worker.joinable() &&
                stopped_worker.get_id() != std::this_thread::get_id()) {
                stopped_worker.join();
            } else if (stopped_worker.joinable()) {
                stopped_worker.detach();
            }
        }
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            active_routes.clear();
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

    void release_route(const std::string& route_key)
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            active_routes.erase(route_key);
        }
        queue_cv.notify_all();
    }

    bool enqueue(nlohmann::json request,
                 std::shared_ptr<ix::WebSocket> web_socket,
                 const ControlPlane& control_plane)
    {
        if (!web_socket) {
            return false;
        }
        std::string route_key;
        try {
            route_key = control_plane.route_key(request);
        } catch (...) {
            // Keep malformed requests in a deterministic lane; the worker
            // will turn them into a protocol error without poisoning a route.
            route_key = "control-plane";
        }
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stopping_worker || pending_requests.size() >= kMaxPendingRequests) {
                return false;
            }
            pending_requests.push_back(
                {std::move(request), std::move(web_socket), std::move(route_key)});
        }
        queue_cv.notify_all();
        return true;
    }

    std::string queue_full_response(const nlohmann::json& request)
    {
        try {
            const auto decoded = decode_control_message(request);
            const ControlResult result{
                ControlStatus::Unavailable,
                {{"error", "control request queue full"}},
            };
            return encode_control_response(decoded, result).dump();
        } catch (...) {
            return nlohmann::json{
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {{"code", -32600}, {"message", "invalid request"}}},
            }.dump();
        }
    }

    void run_worker(ControlPlane& control_plane)
    {
        for (;;) {
            ControlTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this]() {
                    return stopping_worker ||
                        std::any_of(
                            pending_requests.begin(),
                            pending_requests.end(),
                            [this](const ControlTask& pending) {
                                return active_routes.find(pending.route_key) ==
                                    active_routes.end();
                            });
                });
                if (stopping_worker && pending_requests.empty()) {
                    break;
                }
                const auto runnable = std::find_if(
                    pending_requests.begin(),
                    pending_requests.end(),
                    [this](const ControlTask& pending) {
                        return active_routes.find(pending.route_key) ==
                            active_routes.end();
                    });
                if (runnable == pending_requests.end()) {
                    continue;
                }
                task = std::move(*runnable);
                pending_requests.erase(runnable);
                active_routes.insert(task.route_key);
            }

            struct RouteGuard {
                Impl* owner = nullptr;
                std::string key;
                ~RouteGuard()
                {
                    if (owner != nullptr) {
                        owner->release_route(key);
                    }
                }
            } route_guard{this, task.route_key};

            std::string response;
            try {
                response = control_plane.handle_text(task.request).dump();
            } catch (const std::exception& error) {
                try {
                    const auto decoded = decode_control_message(task.request);
                    response = encode_control_response(
                        decoded,
                        {ControlStatus::InternalError, {{"error", error.what()}}})
                                   .dump();
                } catch (...) {
                    response = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"id", nullptr},
                        {"error", {{"code", -32603}, {"message", "internal error"}}},
                    }.dump();
                }
            } catch (...) {
                try {
                    const auto decoded = decode_control_message(task.request);
                    response = encode_control_response(
                        decoded,
                        {ControlStatus::InternalError,
                         {{"error", "unknown control-plane error"}}})
                                   .dump();
                } catch (...) {
                    response = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"id", nullptr},
                        {"error", {{"code", -32603}, {"message", "internal error"}}},
                    }.dump();
                }
            }
            try {
                if (task.web_socket &&
                    task.web_socket->getReadyState() == ix::ReadyState::Open) {
                    (void)task.web_socket->sendText(response);
                }
            } catch (...) {
                // A disconnected peer must not strand active_routes and stop
                // FIFO progress for later requests on the same endpoint.
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
        if (!impl_->enqueue(request, client, control_plane)) {
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
