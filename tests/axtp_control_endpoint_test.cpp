#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <nlohmann/json.hpp>

#include "axent/control/axtp_control_endpoint.hpp"

namespace {

using namespace std::chrono_literals;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class WebSocketProbe final {
public:
    explicit WebSocketProbe(std::uint16_t port)
    {
        socket_.setUrl("ws://127.0.0.1:" + std::to_string(port) + "/");
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!message || message->type != ix::WebSocketMessageType::Message) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                messages_.push(message->str);
            }
            ready_.notify_one();
        });
        socket_.start();
    }

    ~WebSocketProbe()
    {
        socket_.stop();
    }

    nlohmann::json next()
    {
        auto message = next_for(5s);
        if (!message.has_value()) {
            throw std::runtime_error("timed out waiting for AXTP WebSocket message");
        }
        return std::move(*message);
    }

    std::optional<nlohmann::json> next_for(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!ready_.wait_for(lock, timeout, [this]() { return !messages_.empty(); })) {
            return std::nullopt;
        }
        auto message = std::move(messages_.front());
        messages_.pop();
        return nlohmann::json::parse(message);
    }

    void send(nlohmann::json message)
    {
        socket_.sendText(message.dump());
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::string> messages_;
    ix::WebSocket socket_;
};

axent::control::ControlRoute generated(std::uint32_t id, std::string name)
{
    return {id, std::move(name), false};
}

axent::control::ControlRoute private_route(std::uint32_t id, std::string name)
{
    return {id, std::move(name), true};
}

void expect_invalid_registration(axent::AxtpControlEndpoint& endpoint,
                                 axent::control::ControlRoute route)
{
    bool threw = false;
    try {
        auto ignored = endpoint.register_handler(
            std::move(route),
            [](const axent::control::ControlRequest&) {
                return axent::control::ControlResult{};
            });
        (void)ignored;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "invalid control route registration should fail fast");
}

nlohmann::json request_message(const std::string& sid,
                               std::uint32_t id,
                               std::string method,
                               nlohmann::json params = nlohmann::json::object())
{
    return {{"sid", sid},
            {"op", 7},
            {"d", {{"id", id}, {"method", std::move(method)}, {"params", std::move(params)}}}};
}

std::uint32_t response_code(const nlohmann::json& response)
{
    return response.at("d").at("status").at("code").get<std::uint32_t>();
}

} // namespace

int main()
{
    using axent::control::ControlResult;
    using axent::control::ControlStatus;

    require(ControlStatus::success().ok(), "success status should be ok");
    require(!ControlStatus::busy().ok(), "busy status should not be ok");
    require(ControlStatus::from_protocol_code(0x0555).code == 0x0555,
            "unknown protocol status code must be preserved");

    axent::AxtpControlEndpoint endpoint;
    std::atomic<int> handler_calls{0};
    std::mutex worker_id_mutex;
    std::thread::id worker_id;
    auto observe_worker = [&]() {
        std::lock_guard<std::mutex> lock(worker_id_mutex);
        if (worker_id == std::thread::id{}) {
            worker_id = std::this_thread::get_id();
        } else {
            require(worker_id == std::this_thread::get_id(),
                    "handlers and posted work must use one endpoint worker");
        }
    };

    auto status_token = endpoint.register_handler(
        generated(0x1612, "cast.getStatus"),
        [&](const axent::control::ControlRequest& request) {
            observe_worker();
            ++handler_calls;
            if (request.params.value("throwInvalid", false)) {
                throw std::invalid_argument("bad request");
            }
            if (request.params.contains("statusCode")) {
                return ControlResult{
                    ControlStatus::from_protocol_code(request.params.at("statusCode").get<std::uint32_t>()),
                    nlohmann::json::object(),
                };
            }
            if (request.params.value("publish", false)) {
                require(endpoint.publish_event(
                            generated(0x1601, "cast.sessionIncoming"),
                            {{"fromHandler", true}}) == ControlStatus::success(),
                        "handler should enqueue an event without synchronous send");
            }
            return ControlResult{
                ControlStatus::success(),
                {{"requestId", request.request_id}, {"method", request.method}},
            };
        });
    auto private_token = endpoint.register_handler(
        private_route(0x7F23, "cast.subscribeEvents"),
        [](const axent::control::ControlRequest&) {
            return ControlResult{ControlStatus::success(), {{"accepted", true}}};
        });

    std::mutex blocking_mutex;
    std::condition_variable blocking_cv;
    bool handler_entered = false;
    bool release_handler = false;
    auto blocking_token = endpoint.register_handler(
        generated(0x1601, "cast.getSession"),
        [&](const axent::control::ControlRequest&) {
            observe_worker();
            std::unique_lock<std::mutex> lock(blocking_mutex);
            handler_entered = true;
            blocking_cv.notify_all();
            blocking_cv.wait(lock, [&]() { return release_handler; });
            return ControlResult{ControlStatus::success(), {{"released", true}}};
        });

    auto self_stop_token = endpoint.register_handler(
        generated(0x1602, "cast.stopSession"),
        [&](const axent::control::ControlRequest&) {
            const auto stopped = endpoint.stop();
            return ControlResult{ControlStatus::success(), {{"stopStatus", stopped.code}}};
        });

    auto removable = endpoint.register_handler(
        generated(0x1605, "cast.getAudio"),
        [](const axent::control::ControlRequest&) { return ControlResult{}; });
    removable.reset();
    auto replacement = endpoint.register_handler(
        generated(0x1605, "cast.getAudio"),
        [](const axent::control::ControlRequest&) {
            return ControlResult{ControlStatus::success(), {{"replacement", true}}};
        });

    expect_invalid_registration(endpoint, generated(0x1601, "cast.getStatus"));
    expect_invalid_registration(endpoint, generated(0x1612, "cast.unknown"));
    expect_invalid_registration(endpoint, private_route(0x1612, "cast.getStatus"));
    expect_invalid_registration(endpoint, {0x7F24, "cast.unknown", false});
    expect_invalid_registration(endpoint, private_route(0x7F25, "cast.subscribeEvents"));

    require(endpoint.start() == ControlStatus::success(),
            "endpoint should start on an ephemeral port");
    require(endpoint.running(), "endpoint should report running");
    require(endpoint.local_port() != 0, "endpoint should publish its bound port");

    bool sealed = false;
    try {
        auto ignored = endpoint.register_handler(
            generated(0x1603, "cast.getAirPlayName"),
            [](const axent::control::ControlRequest&) { return ControlResult{}; });
        (void)ignored;
    } catch (const std::logic_error&) {
        sealed = true;
    }
    require(sealed, "endpoint registry must be sealed after start");

    auto first = std::make_unique<WebSocketProbe>(endpoint.local_port());
    auto hello = first->next();
    require(hello.at("sid") == "", "hello sid must stay empty");
    require(hello.at("op") == 0, "endpoint must preserve AXTP Hello op");
    require(hello.at("d").contains("axtpVersion"), "hello must advertise AXTP version");

    first->send(request_message("", 1, "cast.getStatus"));
    auto response = first->next();
    require(response_code(response) == 0x0024,
            "request before Identify must preserve ControlOpenRequired");

    first->send({{"sid", ""},
                 {"op", 2},
                 {"d", {{"randomSeed", 0x12345678},
                         {"resumeSid", "legacy-session"},
                         {"eventMasks", "000000"}}}});
    auto identified = first->next();
    require(identified.at("op") == 3, "endpoint must preserve AXTP Identified op");
    std::string sid = identified.at("sid").get<std::string>();
    require(sid == "legacy-session", "resumeSid behavior must stay compatible");

    first->send({{"sid", sid}, {"op", 4}, {"d", {{"eventMasks", "cast.*"}}}});
    auto reidentified = first->next();
    require(reidentified.at("op") == 3 && reidentified.at("sid") == sid,
            "Reidentify must acknowledge the existing SID exactly once");
    require(!first->next_for(100ms).has_value(),
            "Reidentify must not enqueue a duplicate Identified frame");

    first->send({{"sid", ""},
                 {"op", 2},
                 {"d", {{"randomSeed", 7}, {"resumeSid", "replacement-session"}}}});
    auto duplicate_identified = first->next();
    require(duplicate_identified.at("op") == 3 && duplicate_identified.at("sid") == sid,
            "duplicate Identify must keep the active SID");
    require(!first->next_for(100ms).has_value(),
            "duplicate Identify must produce only one acknowledgement");

    first->send(request_message("wrong-session", 2, "cast.getStatus"));
    response = first->next();
    require(response_code(response) == 0x0033, "wrong SID must preserve RpcPayloadInvalid");

    first->send({{"sid", sid}, {"op", 9}, {"d", {{"id", 3}, {"requests", nlohmann::json::array()}}}});
    response = first->next();
    require(response.at("op") == 10, "batch error must use RequestBatchResponse");
    require(response_code(response) == 0x003F, "batch must preserve RpcBatchUnsupported");

    first->send(request_message(sid, 4, "cast.unknown"));
    response = first->next();
    require(response_code(response) == 0x0036, "unknown method must preserve RpcMethodNotFound");

    auto request = [&](std::uint32_t id, std::string method, nlohmann::json params) {
        first->send(request_message(sid, id, std::move(method), std::move(params)));
        return first->next();
    };

    response = request(101, "cast.getStatus", {{"includeSensitive", false}});
    require(response.at("sid") == sid, "response sid must match the identified session");
    require(response.at("op") == 8, "endpoint must preserve AXTP response op");
    require(response.at("d").at("id") == 101, "response id mismatch");
    require(response_code(response) == 0, "successful status mismatch");
    require(response.at("d").at("result").at("method") == "cast.getStatus",
            "handler result mismatch");

    response = request(102, "cast.getStatus", {{"throwInvalid", true}});
    require(response_code(response) == ControlStatus::invalid_argument().code,
            "invalid_argument exception mapping mismatch");

    const std::vector<std::uint32_t> status_codes = {
        ControlStatus::not_supported().code,
        ControlStatus::busy().code,
        ControlStatus::timeout().code,
        ControlStatus::unavailable().code,
        ControlStatus::internal_error().code,
        ControlStatus::stream_not_open().code,
        0x0033,
        0x0555,
    };
    std::uint32_t request_id = 200;
    for (const auto code : status_codes) {
        response = request(request_id++, "cast.getStatus", {{"statusCode", code}});
        require(response_code(response) == code,
                "protocol error status must be preserved without remapping");
    }

    response = request(301, "cast.subscribeEvents", nlohmann::json::object());
    require(response_code(response) == 0, "explicit private method should dispatch");
    require(response.at("d").at("result").at("accepted") == true,
            "private method result mismatch");

    response = request(302, "cast.getAudio", nlohmann::json::object());
    require(response.at("d").at("result").at("replacement") == true,
            "route reset before start should permit a replacement registration");

    response = request(303, "cast.getStatus", {{"publish", true}});
    require(response.at("op") == 8 && response.at("d").at("id") == 303,
            "handler response must be sent before its queued event");
    auto event = first->next();
    require(event.at("op") == 6, "queued handler event must follow the response");
    require(event.at("d").at("event") == "cast.sessionIncoming",
            "queued handler event name mismatch");
    require(event.at("d").at("data").at("fromHandler") == true,
            "queued handler event data mismatch");

    std::atomic<bool> posted{false};
    require(endpoint.post([&]() {
                observe_worker();
                posted.store(true);
            }) == ControlStatus::success(),
            "endpoint should accept product-neutral worker tasks");
    const auto post_deadline = std::chrono::steady_clock::now() + 2s;
    while (!posted.load() && std::chrono::steady_clock::now() < post_deadline) {
        std::this_thread::sleep_for(2ms);
    }
    require(posted.load(), "posted task should run on the endpoint worker");

    response = request(304, "cast.stopSession", nlohmann::json::object());
    require(response.at("d").at("result").at("stopStatus") ==
                ControlStatus::invalid_state().code,
            "stop called from a handler must fail fast instead of self-joining");
    require(endpoint.running(), "rejected self-stop must leave endpoint running");

    require(endpoint.publish_event(generated(0x1602, "cast.sessionStateChanged"), nullptr) ==
                ControlStatus::success(),
            "event with null data should enqueue");
    event = first->next();
    require(event.at("d").contains("data") && event.at("d").at("data").is_null(),
            "event null data must remain null on the wire");

    auto second = std::make_unique<WebSocketProbe>(endpoint.local_port());
    const auto second_hello = second->next();
    require(second_hello.at("op") == 0,
            "new connection must receive a targeted Hello");
    require(!first->next_for(250ms).has_value(),
            "existing clients must not receive a new client's Hello");
    second->send({{"sid", ""}, {"op", 2}, {"d", {{"resumeSid", "shared-session"}}}});
    const auto second_identified = second->next();
    require(second_identified.at("sid") == sid,
            "additional clients must join the active shared SID without replacing it");
    require(!first->next_for(250ms).has_value(),
            "existing clients must not receive another client's Identified response");

    second->send(request_message(sid, 350, "cast.getStatus"));
    const auto second_response = second->next();
    require(second_response.at("d").at("id") == 350 &&
                response_code(second_response) == 0,
            "second client request must receive its own response");
    require(!first->next_for(250ms).has_value(),
            "request responses must not leak to other clients");

    first->send(request_message(sid, 351, "cast.getStatus"));
    const auto first_response = first->next();
    require(first_response.at("d").at("id") == 351 &&
                response_code(first_response) == 0,
            "first client must remain usable after another client identifies");
    require(!second->next_for(250ms).has_value(),
            "first client responses must not leak to the second client");

    require(endpoint.publish_event(
                generated(0x1601, "cast.sessionIncoming"), {{"broadcast", true}}) ==
                ControlStatus::success(),
            "broadcast event should enqueue");
    const auto first_event = first->next();
    const auto second_event = second->next();
    require(first_event.at("sid") == sid && second_event.at("sid") == sid,
            "event must preserve current multi-client broadcast/shared SID behavior");
    second.reset();

    const auto calls_before_reset = handler_calls.load();
    status_token.reset();
    response = request(401, "cast.getStatus", nlohmann::json::object());
    require(response_code(response) == ControlStatus::unavailable().code,
            "inactive sealed registration should return unavailable");
    require(handler_calls.load() == calls_before_reset,
            "handler must not be called after registration token reset");

    first->send(request_message(sid, 500, "cast.getSession", {{"block", true}}));
    {
        std::unique_lock<std::mutex> lock(blocking_mutex);
        require(blocking_cv.wait_for(lock, 2s, [&]() { return handler_entered; }),
                "blocking handler should enter before stop");
    }
    std::atomic<bool> stop_finished{false};
    ControlStatus stop_status = ControlStatus::internal_error();
    std::thread stopper([&]() {
        stop_status = endpoint.stop();
        stop_finished.store(true);
    });
    std::this_thread::sleep_for(50ms);
    require(!stop_finished.load(), "stop must wait for an in-flight handler");
    require(endpoint.start() == ControlStatus::invalid_state(),
            "start must fail fast while another thread is stopping the endpoint");
    std::atomic<bool> follower_stop_finished{false};
    ControlStatus follower_stop_status = ControlStatus::internal_error();
    std::thread follower_stopper([&]() {
        follower_stop_status = endpoint.stop();
        follower_stop_finished.store(true);
    });
    std::this_thread::sleep_for(20ms);
    require(!follower_stop_finished.load(),
            "concurrent stop should wait for the active stop operation");
    require(endpoint.post([]() {}) == ControlStatus::unavailable(),
            "stop must reject newly posted work");
    require(endpoint.publish_event(generated(0x1601, "cast.sessionIncoming"), {}) ==
                ControlStatus::unavailable(),
            "stop must reject newly published events");
    {
        std::lock_guard<std::mutex> lock(blocking_mutex);
        release_handler = true;
    }
    blocking_cv.notify_all();
    stopper.join();
    follower_stopper.join();
    require(stop_status == ControlStatus::success() && stop_finished.load(),
            "external stop should join its worker successfully");
    require(follower_stop_status == ControlStatus::success() && follower_stop_finished.load(),
            "concurrent stop should complete idempotently after the shared worker joins");
    require(!endpoint.running(), "endpoint should report stopped");
    require(endpoint.local_port() == 0, "stopped endpoint must clear its local port");
    require(endpoint.stop() == ControlStatus::success(), "stop should be idempotent");

    first.reset();
    require(endpoint.start() == ControlStatus::success(),
            "endpoint should restart with its sealed static registry");
    auto reconnected = std::make_unique<WebSocketProbe>(endpoint.local_port());
    hello = reconnected->next();
    require(hello.at("op") == 0, "reconnected client should receive Hello");
    reconnected->send({{"sid", ""}, {"op", 2}, {"d", {{"randomSeed", 7}}}});
    identified = reconnected->next();
    require(identified.at("op") == 3 && identified.at("sid").is_string(),
            "reconnected client should complete Identify");
    reconnected.reset();
    require(endpoint.stop() == ControlStatus::success(), "restarted endpoint should stop cleanly");

    axent::control::RegistrationToken survivor;
    {
        axent::AxtpControlEndpoint short_lived;
        survivor = short_lived.register_handler(
            generated(0x1601, "cast.getSession"),
            [](const axent::control::ControlRequest&) { return ControlResult{}; });
    }
    survivor.reset();
    require(!survivor, "token reset after endpoint destruction must be safe");

    (void)private_token;
    (void)blocking_token;
    (void)self_stop_token;
    (void)replacement;
    return 0;
}
