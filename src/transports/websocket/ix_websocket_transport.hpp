#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include "core/runtime/transport/transport.hpp"

namespace axent::transport {

using axtp::AxtpWireMode;
using axtp::Byte;
using axtp::IByteSink;
using axtp::ITransport;
using axtp::RpcEncoding;
using axtp::TransportKind;
using axtp::TransportProfile;

class WebSocketTransport : public ITransport {
public:
    explicit WebSocketTransport(std::uint16_t port, const char* address = "127.0.0.1")
        : _port(port)
        , _address(address != nullptr ? address : "127.0.0.1") {}

    ~WebSocketTransport() override {
        close();
    }

    void bind(IByteSink& sink) override {
        _sink = &sink;
    }

    void open() override {
        close();
        _netInitialized = ix::initNetSystem();
        const auto listenPort = _port == 0 ? ix::getFreePort() : static_cast<int>(_port);
        if (listenPort <= 0) {
            return;
        }
        _localPort = static_cast<std::uint16_t>(listenPort);
        _server = std::make_unique<ix::WebSocketServer>(listenPort, _address);
        _server->disablePerMessageDeflate();
        _server->setOnClientMessageCallback(
            [this](std::shared_ptr<ix::ConnectionState> connectionState,
                   ix::WebSocket& webSocket,
                   const ix::WebSocketMessagePtr& message) {
                if (!message) {
                    return;
                }
                if (message->type == ix::WebSocketMessageType::Open) {
                    auto client = findClient(webSocket);
                    {
                        std::lock_guard<std::mutex> lock(_clientsMutex);
                        const bool firstClient = _clients.empty();
                        const auto id = connectionId(connectionState);
                        _clients[id] = std::move(client);
                        _pendingHelloClients.push(id);
                        if (firstClient) {
                            // The runtime adapter owns one shared AXTP session.  Only the
                            // empty-to-connected transition starts a new session generation;
                            // extra WebSocket peers join the active session.
                            _helloText.clear();
                            _connectionGeneration.fetch_add(1, std::memory_order_relaxed);
                        }
                        _hasConnection.store(true);
                    }
                    return;
                }
                if (message->type == ix::WebSocketMessageType::Close ||
                    message->type == ix::WebSocketMessageType::Error) {
                    {
                        std::lock_guard<std::mutex> lock(_clientsMutex);
                        _clients.erase(connectionId(connectionState));
                        _hasConnection.store(!_clients.empty());
                    }
                    return;
                }
                if (message->type != ix::WebSocketMessageType::Message || message->binary) {
                    return;
                }
                std::lock_guard<std::mutex> lock(_rxMutex);
                _rxMessages.push(
                    ReceivedMessage{connectionId(connectionState), message->str});
            });
        const bool started = _server->listenAndStart();
        _open.store(started);
        if (!started) {
            _server.reset();
            _localPort = 0;
        }
    }

    void close() override {
        if (_server) {
            _server->stop();
            _server.reset();
        }
        _localPort = 0;
        _hasConnection.store(false);
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.clear();
            std::queue<std::string> empty;
            _pendingHelloClients.swap(empty);
            _helloText.clear();
        }
        {
            std::lock_guard<std::mutex> lock(_rxMutex);
            std::queue<ReceivedMessage> empty;
            _rxMessages.swap(empty);
        }
        _open.store(false);
        if (_netInitialized) {
            ix::uninitNetSystem();
            _netInitialized = false;
        }
    }

    void poll() override {
        if (!_open.load() || _sink == nullptr) {
            return;
        }
        if (_polling.exchange(true)) {
            return;
        }

        struct PollGuard {
            explicit PollGuard(std::atomic<bool>& polling)
                : polling_(polling) {}
            ~PollGuard() { polling_.store(false); }
            std::atomic<bool>& polling_;
        } guard(_polling);

        sendCachedHelloToPendingClients();

        while (true) {
            ReceivedMessage message;
            {
                std::lock_guard<std::mutex> lock(_rxMutex);
                if (_rxMessages.empty()) {
                    _dispatchConnectionId.clear();
                    return;
                }
                message = std::move(_rxMessages.front());
                _rxMessages.pop();
            }
            _dispatchConnectionId = message.connectionId;
            struct DispatchGuard {
                explicit DispatchGuard(std::string& connectionId)
                    : connectionId_(connectionId) {}
                ~DispatchGuard() { connectionId_.clear(); }
                std::string& connectionId_;
            } dispatchGuard(_dispatchConnectionId);
            _sink->onBytes(
                reinterpret_cast<const Byte*>(message.text.data()), message.text.size());
        }
    }

    void sendBytes(const Byte* data, std::size_t size) override {
        if (!_server || data == nullptr || size == 0) {
            return;
        }
        const std::string text(reinterpret_cast<const char*>(data), size);

        if (isHello(text)) {
            std::vector<std::shared_ptr<ix::WebSocket>> clients;
            {
                std::lock_guard<std::mutex> lock(_clientsMutex);
                _helloText = text;
                clients = takePendingHelloClientsLocked();
            }
            if (!clients.empty()) {
                sendText(clients, text);
                return;
            }
        }

        if (!_dispatchConnectionId.empty()) {
            auto client = clientFor(_dispatchConnectionId);
            if (client) {
                (void)client->sendText(text);
            }
            return;
        }

        sendText(activeClients(), text);
    }

    TransportProfile profile() const override {
        TransportProfile profile;
        profile.kind = TransportKind::WebSocket;
        profile.wireMode = AxtpWireMode::WebSocketJsonRpc;
        profile.defaultRpcEncoding = RpcEncoding::Json;
        profile.messageOriented = true;
        profile.supportsTextMessage = true;
        profile.supportsBinaryMessage = false;
        return profile;
    }

    std::uint16_t localPort() const {
        return _localPort;
    }

    bool hasConnection() const {
        return _server != nullptr && _hasConnection.load();
    }

    std::uint64_t connectionGeneration() const {
        return _connectionGeneration.load(std::memory_order_relaxed);
    }

private:
    struct ReceivedMessage {
        std::string connectionId;
        std::string text;
    };

    static std::string connectionId(const std::shared_ptr<ix::ConnectionState>& connectionState) {
        return connectionState ? connectionState->getId() : std::string();
    }

    std::shared_ptr<ix::WebSocket> findClient(ix::WebSocket& webSocket) const {
        if (!_server) {
            return {};
        }
        for (const auto& client : _server->getClients()) {
            if (client && client.get() == &webSocket) {
                return client;
            }
        }
        return {};
    }

    std::shared_ptr<ix::WebSocket> clientFor(const std::string& id) const {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        const auto client = _clients.find(id);
        return client == _clients.end() ? std::shared_ptr<ix::WebSocket>{} : client->second;
    }

    std::vector<std::shared_ptr<ix::WebSocket>> activeClients() const {
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        std::lock_guard<std::mutex> lock(_clientsMutex);
        clients.reserve(_clients.size());
        for (const auto& [id, client] : _clients) {
            (void)id;
            if (client) {
                clients.push_back(client);
            }
        }
        return clients;
    }

    std::vector<std::shared_ptr<ix::WebSocket>> takePendingHelloClientsLocked() {
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        while (!_pendingHelloClients.empty()) {
            const auto id = std::move(_pendingHelloClients.front());
            _pendingHelloClients.pop();
            const auto client = _clients.find(id);
            if (client != _clients.end() && client->second) {
                clients.push_back(client->second);
            }
        }
        return clients;
    }

    void sendCachedHelloToPendingClients() {
        std::string hello;
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            if (_helloText.empty() || _pendingHelloClients.empty()) {
                return;
            }
            hello = _helloText;
            clients = takePendingHelloClientsLocked();
        }
        sendText(clients, hello);
    }

    static void sendText(const std::vector<std::shared_ptr<ix::WebSocket>>& clients,
                         const std::string& text) {
        for (const auto& client : clients) {
            if (client) {
                (void)client->sendText(text);
            }
        }
    }

    static bool isHello(const std::string& text) {
        try {
            const auto message = nlohmann::json::parse(text);
            return message.is_object() && message.value("op", -1) == 0;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::uint16_t _port = 0;
    std::uint16_t _localPort = 0;
    std::string _address;
    std::unique_ptr<ix::WebSocketServer> _server;
    IByteSink* _sink = nullptr;
    std::atomic<bool> _open{false};
    std::atomic<bool> _hasConnection{false};
    std::atomic<bool> _polling{false};
    std::atomic<std::uint64_t> _connectionGeneration{0};
    bool _netInitialized = false;
    mutable std::mutex _clientsMutex;
    std::map<std::string, std::shared_ptr<ix::WebSocket>> _clients;
    // Additional peers receive the cached Hello directly, without making the
    // shared runtime adapter reset the active SID.
    std::queue<std::string> _pendingHelloClients;
    std::string _helloText;
    mutable std::mutex _rxMutex;
    std::queue<ReceivedMessage> _rxMessages;
    std::string _dispatchConnectionId;
};

}  // namespace axent::transport
