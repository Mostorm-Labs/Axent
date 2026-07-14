#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>

#include "core/runtime/transport/transport.hpp"
#include "websocket/ix_websocket_transport.hpp"

namespace {

class CapturingSink final : public axtp::IByteSink {
public:
    void onBytes(const axtp::Byte* data, std::size_t size) override
    {
        messages.emplace_back(reinterpret_cast<const char*>(data), size);
    }

    std::vector<std::string> messages;
};

class WebSocketProbe final {
public:
    explicit WebSocketProbe(std::uint16_t port)
    {
        socket_.setUrl("ws://127.0.0.1:" + std::to_string(port) + "/");
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!message) {
                return;
            }
            if (message->type == ix::WebSocketMessageType::Open) {
                opened_.store(true);
                cv_.notify_all();
                return;
            }
            if (message->type == ix::WebSocketMessageType::Message) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    messages_.push(message->str);
                }
                cv_.notify_all();
            }
        });
        socket_.start();
        std::unique_lock<std::mutex> lock(mutex_);
        assert(cv_.wait_for(lock, std::chrono::seconds(5), [&] { return opened_.load(); }));
    }

    ~WebSocketProbe()
    {
        socket_.stop();
    }

    WebSocketProbe(const WebSocketProbe&) = delete;
    WebSocketProbe& operator=(const WebSocketProbe&) = delete;

    void send_text(const std::string& text)
    {
        socket_.sendText(text);
    }

    void send_binary(const std::string& bytes)
    {
        socket_.sendBinary(bytes);
    }

    std::string wait_message()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        assert(cv_.wait_for(lock, std::chrono::seconds(5), [&] { return !messages_.empty(); }));
        auto message = std::move(messages_.front());
        messages_.pop();
        return message;
    }

    void stop()
    {
        socket_.stop();
    }

private:
    ix::WebSocket socket_;
    std::atomic<bool> opened_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> messages_;
};

template <typename Predicate>
bool wait_until(Predicate&& predicate)
{
    for (int attempt = 0; attempt < 500; ++attempt) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

} // namespace

int main()
{
    CapturingSink sink;
    axent::transport::WebSocketTransport transport(0);
    transport.bind(sink);
    transport.open();

    const auto profile = transport.profile();
    assert(profile.kind == axtp::TransportKind::WebSocket);
    assert(profile.wireMode == axtp::AxtpWireMode::WebSocketJsonRpc);
    assert(profile.defaultRpcEncoding == axtp::RpcEncoding::Json);
    assert(profile.messageOriented);
    assert(profile.supportsTextMessage);
    assert(!profile.supportsBinaryMessage);
    assert(transport.localPort() != 0);

    WebSocketProbe first(transport.localPort());
    WebSocketProbe second(transport.localPort());
    assert(wait_until([&] {
        return transport.hasConnection() && transport.connectionGeneration() >= 2;
    }));

    first.send_text("first-request");
    assert(wait_until([&] {
        transport.poll();
        return sink.messages.size() == 1;
    }));
    assert(sink.messages.front() == "first-request");

    first.send_binary("ignored-binary");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    transport.poll();
    assert(sink.messages.size() == 1);

    const std::string broadcast = "broadcast-response";
    transport.sendBytes(
        reinterpret_cast<const axtp::Byte*>(broadcast.data()), broadcast.size());
    assert(first.wait_message() == broadcast);
    assert(second.wait_message() == broadcast);

    first.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(transport.hasConnection());

    const std::string queued = "discard-on-close";
    second.send_text(queued);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto delivered_before_close = sink.messages.size();
    const auto generation_before_restart = transport.connectionGeneration();
    transport.close();
    transport.poll();
    assert(transport.localPort() == 0);
    assert(!transport.hasConnection());
    assert(sink.messages.size() == delivered_before_close);

    second.stop();
    transport.open();
    assert(transport.localPort() != 0);
    {
        WebSocketProbe restarted(transport.localPort());
        assert(wait_until([&] {
            return transport.hasConnection() &&
                   transport.connectionGeneration() > generation_before_restart;
        }));
    }
    transport.close();
    transport.close();
    assert(transport.localPort() == 0);
    assert(!transport.hasConnection());
    return 0;
}
