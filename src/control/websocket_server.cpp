#include "axent/control/websocket_server.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "core/runtime/transport/transport.hpp"
#include "transports/websocket/ix/websocket_transport.hpp"

namespace axent {
namespace {

class ControlPlaneSink final : public axtp::IByteSink {
public:
    ControlPlaneSink(ControlPlane& control_plane, axtp::WebSocketTransport& transport)
        : control_plane_(control_plane)
        , transport_(transport)
    {
    }

    void onBytes(const axtp::Byte* data, std::size_t size) override
    {
        const std::string text(reinterpret_cast<const char*>(data), size);
        const auto request = nlohmann::json::parse(text, nullptr, false);
        if (request.is_discarded()) {
            return;
        }

        const auto response = control_plane_.handle_text(request).dump();
        transport_.sendBytes(reinterpret_cast<const axtp::Byte*>(response.data()), response.size());
    }

private:
    ControlPlane& control_plane_;
    axtp::WebSocketTransport& transport_;
};

} // namespace

struct WebSocketServer::Impl {
    std::unique_ptr<axtp::WebSocketTransport> transport;
    std::unique_ptr<ControlPlaneSink> sink;
    std::thread poll_thread;
    std::atomic<bool> running{false};

    void stop()
    {
        running.store(false);
        if (poll_thread.joinable()) {
            poll_thread.join();
        }
        if (transport) {
            transport->close();
        }
        sink.reset();
        transport.reset();
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
    impl_->transport = std::make_unique<axtp::WebSocketTransport>(port, bind_host.c_str());
    impl_->sink = std::make_unique<ControlPlaneSink>(control_plane, *impl_->transport);
    impl_->transport->bind(*impl_->sink);
    impl_->transport->open();
    if (impl_->transport->localPort() == 0) {
        stop();
        return false;
    }

    impl_->running.store(true);
    impl_->poll_thread = std::thread([this]() {
        while (impl_->running.load()) {
            impl_->transport->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    return true;
}

void WebSocketServer::stop()
{
    impl_->stop();
}

std::uint16_t WebSocketServer::local_port() const
{
    return impl_->transport ? impl_->transport->localPort() : 0;
}

} // namespace axent
