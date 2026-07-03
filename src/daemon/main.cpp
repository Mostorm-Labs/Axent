#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "axent/config/config.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/control/websocket_server.hpp"
#include "axent/host/axent_host.hpp"
#include "axent/version.hpp"

namespace {

std::atomic<bool> running{true};

void stop_signal(int)
{
    running.store(false);
}

} // namespace

int main(int argc, char** argv)
{
    bool foreground = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--foreground") {
            foreground = true;
        }
    }

    std::signal(SIGINT, stop_signal);
    std::signal(SIGTERM, stop_signal);

    const axent::AxentConfig config = axent::AxentConfig::dev_trial_defaults();
    axent::AxentHost host;
    axent::AxentHostOptions host_options;
    host_options.enable_mock_adapter = true;
    if (!host.start(host_options)) {
        std::cerr << "failed to start axent host\n";
        return 2;
    }

    axent::ControlPlane control_plane(host.broker());
    axent::WebSocketServer server;
    if (!server.start(control_plane, config.server.bind_host, static_cast<std::uint16_t>(config.server.port))) {
        std::cerr << "failed to start axentd websocket server\n";
        host.stop();
        return 2;
    }

    std::cout << axent::product_name() << ' ' << axent::version()
              << " axentd listening on " << config.server.bind_host
              << ':' << server.local_port() << '\n';

    if (!foreground) {
        std::cout << "service mode scaffold active\n";
    }

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    server.stop();
    host.stop();
    std::cout << "axentd stopped\n";
    return 0;
}
