#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "core/runtime/transport/transport.hpp"
#include "tcp/native_tcp_transport.hpp"

namespace {

class CapturingSink final : public axtp::IByteSink {
public:
    void onBytes(const axtp::Byte* data, std::size_t size) override
    {
        bytes.insert(bytes.end(), data, data + size);
    }

    axtp::Bytes bytes;
};

template <typename Predicate>
bool poll_until(axent::transport::TcpServerTransport& server,
                axent::transport::TcpClientTransport& client,
                Predicate&& predicate)
{
    for (int attempt = 0; attempt < 200; ++attempt) {
        server.poll();
        client.poll();
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
    CapturingSink server_sink;
    axent::transport::TcpServerTransport server(0);
    server.bind(server_sink);
    server.open();

    const auto profile = server.profile();
    assert(profile.kind == axtp::TransportKind::Tcp);
    assert(profile.wireMode == axtp::AxtpWireMode::FramedBinary);
    assert(profile.defaultRpcEncoding == axtp::jsonBinaryRpcEncoding());
    assert(!profile.messageOriented);
    assert(profile.preferredFrameSize == 4096);
    assert(server.isOpen());
    assert(server.localPort() != 0);

    CapturingSink client_sink;
    axent::transport::TcpClientTransport client(
        "127.0.0.1", server.localPort(), std::chrono::milliseconds(500));
    client.bind(client_sink);
    client.open();
    assert(client.isOpen());
    assert(client.localPort() != 0);

    const axtp::Bytes request{0x01, 0x02, 0x03, 0x04, 0x05};
    client.sendBytes(request.data(), 2);
    client.sendBytes(request.data() + 2, request.size() - 2);
    assert(poll_until(server, client, [&] { return server_sink.bytes.size() == request.size(); }));
    assert(server_sink.bytes == request);
    assert(server.hasConnection());

    const axtp::Bytes response{0xA0, 0xA1, 0xA2};
    server.sendBytes(response.data(), response.size());
    assert(poll_until(server, client, [&] { return client_sink.bytes.size() == response.size(); }));
    assert(client_sink.bytes == response);

    client.close();
    assert(poll_until(server, client, [&] { return !server.hasConnection(); }));
    assert(server.isOpen());

    server_sink.bytes.clear();
    CapturingSink second_client_sink;
    axent::transport::TcpClientTransport second_client(
        "127.0.0.1", server.localPort(), std::chrono::milliseconds(500));
    second_client.bind(second_client_sink);
    second_client.open();
    assert(second_client.isOpen());
    const axtp::Bytes reopened_request{0xB0, 0xB1};
    second_client.sendBytes(reopened_request.data(), reopened_request.size());
    assert(poll_until(
        server, second_client, [&] { return server_sink.bytes.size() == reopened_request.size(); }));
    assert(server_sink.bytes == reopened_request);
    second_client.close();

    const auto closed_port = server.localPort();
    server.close();
    assert(!server.isOpen());

    CapturingSink failed_client_sink;
    axent::transport::TcpClientTransport failed_client(
        "127.0.0.1", closed_port, std::chrono::milliseconds(50));
    failed_client.bind(failed_client_sink);
    failed_client.open();
    assert(!failed_client.isOpen());
    failed_client.close();
    return 0;
}
