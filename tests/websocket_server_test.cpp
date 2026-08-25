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

class BlockingAdapter final : public axent::Adapter {
public:
    axent::AdapterMetadata metadata() const override
    {
        return {"blocking", "Blocking multi-endpoint adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        return {
            make_device("blocking-a", "endpoint/controlled-a"),
            make_device("blocking-b", "endpoint/controlled-b"),
        };
    }

    axent::ControlResult call(const std::string& device_id,
                              const std::string& method,
                              const nlohmann::json&) override
    {
        if (device_id == "blocking-a" && method == "control.block") {
            std::unique_lock<std::mutex> lock(mutex_);
            blocked_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this]() { return released_; });
        }
        return {axent::ControlStatus::Ok,
                {{"device", device_id}, {"method", method}}};
    }

    axent::ControlResult start_firmware_update(
        const std::string&, const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

    bool wait_until_blocked(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_until(
            lock, Clock::now() + timeout, [this]() { return blocked_; });
    }

    void release()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        released_ = true;
        condition_.notify_all();
    }

private:
    static axent::DeviceSnapshot make_device(std::string id,
                                             std::string endpoint_id)
    {
        axent::DeviceSnapshot device;
        device.id = std::move(id);
        device.endpoint_id = std::move(endpoint_id);
        device.adapter = "blocking";
        device.connection.online = true;
        return device;
    }

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_ = false;
    bool released_ = false;
};

nlohmann::json response_with_id(const std::vector<std::string>& messages,
                                int id)
{
    for (const auto& message : messages) {
        const auto parsed = nlohmann::json::parse(message);
        if (parsed.value("id", -1) == id) {
            return parsed;
        }
    }
    return {};
}

} // namespace

int main()
{
    axent::MockAdapter adapter;
    BlockingAdapter blocking_adapter;
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }
    for (const auto& device : blocking_adapter.discover()) {
        devices.upsert(device);
    }

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(adapter);
    broker.register_adapter(blocking_adapter);
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
        R"({"jsonrpc":"2.0","id":1,"src":"controller:nearcast","dst":"endpoint/mock-primary","method":"status.get","params":{}})");
    requester.send_text(
        R"({"jsonrpc":"2.0","id":2,"method":"devices.list","params":{}})");

    require(requester.wait_for_messages(2, std::chrono::seconds(3)), "requesting client should receive responses");
    require(!passive.wait_for_any_message(std::chrono::milliseconds(250)),
            "passive client should not receive another client's response");

    const auto requester_messages = requester.messages();
    require(requester_messages.size() == 2, "requesting client should receive exactly two responses");
    const auto response = response_with_id(requester_messages, 1);
    require(!response.empty(), "status response should be present");
    require(response.at("jsonrpc") == "2.0", "response should be JSON-RPC 2.0");
    require(response.at("id") == 1, "response should preserve the numeric JSON-RPC id");
    require(response.at("src") == "endpoint/mock-primary",
            "response source should be the controlled device endpoint");
    require(response.at("dst") == "controller:nearcast",
            "response destination should be the requesting endpoint");
    require(response.at("result").at("health") == "ok", "response should contain the status result");
    const auto second_response = response_with_id(requester_messages, 2);
    require(!second_response.empty(), "device-list response should be present");
    require(second_response.at("jsonrpc") == "2.0", "second response should be JSON-RPC 2.0");
    require(second_response.at("id") == 2, "second response should preserve the numeric JSON-RPC id");
    require(second_response.at("result").at("devices").is_array(), "second response should contain device list");
    require(second_response.at("result").at("devices").at(0).at("endpointId") ==
                "endpoint/mock-primary",
            "device list should advertise the stable endpoint used for dst routing");
    require(passive.is_open(), "passive client should remain connected");

    // One endpoint is intentionally blocked. A legacy physical selector for
    // that same device must share its FIFO lane, while a different endpoint
    // can be dispatched by another worker before the blocked route releases.
    TestClient parallel(server.local_port());
    parallel.start();
    require(parallel.wait_until_open(std::chrono::seconds(3)),
            "parallel requester should connect");
    parallel.send_text(
        R"({"jsonrpc":"2.0","id":10,"src":"controller:nearcast","dst":"endpoint/controlled-a","method":"control.block","params":{}})");
    if (!blocking_adapter.wait_until_blocked(std::chrono::seconds(3))) {
        blocking_adapter.release();
        require(false, "first endpoint request should enter its blocking adapter call");
    }
    parallel.send_text(
        R"({"jsonrpc":"2.0","id":11,"method":"status.get","params":{"deviceId":"blocking-a"}})");
    parallel.send_text(
        R"({"jsonrpc":"2.0","id":12,"src":"controller:nearcast","dst":"endpoint/controlled-b","method":"status.get","params":{}})");
    if (!parallel.wait_for_messages(1, std::chrono::seconds(3))) {
        blocking_adapter.release();
        require(false, "unblocked endpoint should respond while another endpoint is busy");
    }
    const auto parallel_before_release = parallel.messages();
    const auto first_parallel = nlohmann::json::parse(parallel_before_release.front());
    if (first_parallel.at("id") != 12 ||
        parallel.wait_for_messages(2, std::chrono::milliseconds(200))) {
        blocking_adapter.release();
        require(false,
                "different dst should run in parallel and same dst must remain ordered");
    }
    require(first_parallel.at("src") == "endpoint/controlled-b" &&
                first_parallel.at("dst") == "controller:nearcast",
            "parallel endpoint response must reverse its routing envelope");
    blocking_adapter.release();
    require(parallel.wait_for_messages(3, std::chrono::seconds(3)),
            "blocked endpoint requests should finish after release");
    const auto parallel_messages = parallel.messages();
    require(nlohmann::json::parse(parallel_messages[1]).at("id") == 10 &&
                nlohmann::json::parse(parallel_messages[2]).at("id") == 11,
            "endpoint and legacy aliases for one physical device must share FIFO order");
    const auto addressed_blocking_response =
        nlohmann::json::parse(parallel_messages[1]);
    require(addressed_blocking_response.at("src") == "endpoint/controlled-a" &&
                addressed_blocking_response.at("dst") == "controller:nearcast",
            "blocked endpoint response must reverse its routing envelope");
    const auto legacy_alias_response =
        nlohmann::json::parse(parallel_messages[2]);
    require(legacy_alias_response.at("result").at("device") == "blocking-a" &&
                !legacy_alias_response.contains("src") &&
                !legacy_alias_response.contains("dst"),
            "legacy alias response should retain legacy envelope semantics");

    server.stop();

    // Saturate one route while its active request is blocked. Validation is
    // part of the wire contract and must still win over the queue-full fast
    // path for a malformed half-routing envelope.
    BlockingAdapter saturation_adapter;
    axent::DeviceManager saturation_devices;
    for (const auto& device : saturation_adapter.discover()) {
        saturation_devices.upsert(device);
    }
    axent::RouteManager saturation_routes(saturation_devices);
    axent::Middleware saturation_middleware(logger);
    axent::FlowControl saturation_flow;
    axent::Broker saturation_broker(
        saturation_routes, saturation_middleware, saturation_flow);
    saturation_broker.register_adapter(saturation_adapter);
    axent::ControlPlane saturation_control_plane(saturation_broker);
    axent::WebSocketServer saturation_server;
    require(saturation_server.start(
                saturation_control_plane, "127.0.0.1", 0),
            "queue saturation server should start");
    TestClient saturation_client(saturation_server.local_port());
    saturation_client.start();
    require(saturation_client.wait_until_open(std::chrono::seconds(3)),
            "queue saturation client should connect");
    saturation_client.send_text(
        R"({"jsonrpc":"2.0","id":1000,"src":"controller:nearcast","dst":"endpoint/controlled-a","method":"control.block","params":{}})");
    if (!saturation_adapter.wait_until_blocked(std::chrono::seconds(3))) {
        saturation_adapter.release();
        require(false, "queue saturation request should block its route");
    }
    for (int id = 1001; id <= 1256; ++id) {
        saturation_client.send_text(
            nlohmann::json{
                {"jsonrpc", "2.0"},
                {"id", id},
                {"src", "controller:nearcast"},
                {"dst", "endpoint/controlled-a"},
                {"method", "status.get"},
                {"params", nlohmann::json::object()},
            }.dump());
    }
    saturation_client.send_text(
        R"({"jsonrpc":"2.0","id":2000,"src":"controller:nearcast","method":"status.get","params":{}})");
    if (!saturation_client.wait_for_messages(1, std::chrono::seconds(3))) {
        saturation_adapter.release();
        require(false, "queue-full malformed request should receive a response");
    }
    const auto malformed_queue_full =
        response_with_id(saturation_client.messages(), 2000);
    if (malformed_queue_full.empty() ||
        malformed_queue_full.at("error").at("code") != -32602) {
        saturation_adapter.release();
        require(false,
                "routing validation must take precedence over queue saturation");
    }
    saturation_adapter.release();
    saturation_server.stop();
    return 0;
}
