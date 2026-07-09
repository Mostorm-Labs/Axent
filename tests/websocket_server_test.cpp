#include <condition_variable>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <nlohmann/json.hpp>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/control/websocket_server.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/logging/logger.hpp"

namespace {

using Clock = std::chrono::steady_clock;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TestClient {
public:
    explicit TestClient(std::uint16_t port)
    {
        web_socket_.setUrl("ws://127.0.0.1:" + std::to_string(port));
        web_socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!message) {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            if (message->type == ix::WebSocketMessageType::Open) {
                open_ = true;
                condition_.notify_all();
                return;
            }
            if (message->type == ix::WebSocketMessageType::Message && !message->binary) {
                messages_.push_back(message->str);
                condition_.notify_all();
                return;
            }
            if (message->type == ix::WebSocketMessageType::Close ||
                message->type == ix::WebSocketMessageType::Error) {
                closed_ = true;
                condition_.notify_all();
            }
        });
    }

    ~TestClient()
    {
        web_socket_.stop();
    }

    void start()
    {
        web_socket_.start();
    }

    void send_text(const std::string& text)
    {
        (void)web_socket_.sendText(text);
    }

    bool wait_until_open(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_until(lock, Clock::now() + timeout, [this] { return open_ || closed_; }) && open_;
    }

    bool wait_for_messages(std::size_t count, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_until(lock, Clock::now() + timeout, [this, count] {
            return messages_.size() >= count || closed_;
        }) && messages_.size() >= count;
    }

    bool wait_for_any_message(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_until(lock, Clock::now() + timeout, [this] {
            return !messages_.empty() || closed_;
        }) && !messages_.empty();
    }

    bool is_open() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return open_ && !closed_;
    }

    std::vector<std::string> messages() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

private:
    ix::WebSocket web_socket_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool open_ = false;
    bool closed_ = false;
    std::vector<std::string> messages_;
};

} // namespace

int main()
{
    axent::MockAdapter adapter;
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(adapter);
    axent::ControlPlane control_plane(broker);

    axent::WebSocketServer server;
    require(server.start(control_plane, "127.0.0.1", 0), "websocket server should start on a free port");
    require(server.local_port() != 0, "websocket server should publish the selected free port");

    TestClient requester(server.local_port());
    TestClient passive(server.local_port());
    requester.start();
    passive.start();
    require(requester.wait_until_open(std::chrono::seconds(3)), "requesting client should connect");
    require(passive.wait_until_open(std::chrono::seconds(3)), "passive client should connect");

    requester.send_text(
        R"({"jsonrpc":"2.0","id":1,"method":"status.get","params":{"deviceId":"mock-device-001"}})");
    requester.send_text(
        R"({"jsonrpc":"2.0","id":2,"method":"devices.list","params":{}})");

    require(requester.wait_for_messages(2, std::chrono::seconds(3)), "requesting client should receive responses");
    require(!passive.wait_for_any_message(std::chrono::milliseconds(250)),
            "passive client should not receive another client's response");

    const auto requester_messages = requester.messages();
    require(requester_messages.size() == 2, "requesting client should receive exactly two responses");
    const auto response = nlohmann::json::parse(requester_messages[0]);
    require(response.at("jsonrpc") == "2.0", "response should be JSON-RPC 2.0");
    require(response.at("id") == 1, "response should preserve the numeric JSON-RPC id");
    require(response.at("result").at("health") == "ok", "response should contain the status result");
    const auto second_response = nlohmann::json::parse(requester_messages[1]);
    require(second_response.at("jsonrpc") == "2.0", "second response should be JSON-RPC 2.0");
    require(second_response.at("id") == 2, "second response should preserve the numeric JSON-RPC id");
    require(second_response.at("result").at("devices").is_array(), "second response should contain device list");
    require(passive.is_open(), "passive client should remain connected");

    server.stop();
    return 0;
}
