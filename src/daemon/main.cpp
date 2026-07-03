#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <thread>

#include "axent/cli/options.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/control/websocket_server.hpp"
#include "axent/host/axent_host.hpp"
#include "axent/logging/logger.hpp"
#include "axent/version.hpp"

namespace {

std::atomic<bool> running{true};

void stop_signal(int)
{
    running.store(false);
}

void print_product_version(std::ostream& out)
{
    out << axent::product_name() << ' ' << axent::version() << '\n';
}

axent::LogConfig make_log_config(const axent::AxentdCliOptions& options)
{
    axent::LogConfig config;
    config.minimum_level = options.common.log_level;
    config.console_enabled = options.foreground;
    config.file_enabled = options.common.log_enabled;
    config.directory = options.common.log_dir;
    config.file_prefix = options.common.log_file;
    return config;
}

} // namespace

int main(int argc, char** argv)
{
    const auto parsed = axent::parse_axentd_cli(argc, argv);

    if (parsed.status == axent::CliParseStatus::Error) {
        std::cerr << parsed.message << "\n" << axent::axentd_usage();
        return 1;
    }
    if (parsed.status == axent::CliParseStatus::Help) {
        print_product_version(std::cout);
        std::cout << axent::axentd_usage();
        return 0;
    }
    if (parsed.status == axent::CliParseStatus::Version) {
        print_product_version(std::cout);
        return 0;
    }

    axent::Logger logger(make_log_config(parsed.options));
    logger.checkpoint("process start");
    logger.checkpoint("options parsed",
                      {{"foreground", parsed.options.foreground},
                       {"bind_host", parsed.options.bind_host},
                       {"port", parsed.options.port},
                       {"mock_adapter", parsed.options.enable_mock_adapter},
                       {"log_enabled", parsed.options.common.log_enabled}});

    std::signal(SIGINT, stop_signal);
    std::signal(SIGTERM, stop_signal);

    axent::AxentHost host;
    axent::AxentHostOptions host_options;
    host_options.enable_mock_adapter = parsed.options.enable_mock_adapter;
    logger.checkpoint("host starting", {{"mock_adapter", host_options.enable_mock_adapter}});
    if (!host.start(host_options)) {
        logger.write(axent::LogLevel::Error, axent::LogCategory::Daemon, "failed to start axent host");
        std::cerr << "failed to start axent host\n";
        return 2;
    }
    logger.checkpoint("host started");

    axent::ControlPlane control_plane(host.broker());
    axent::WebSocketServer server;
    logger.checkpoint("websocket starting", {{"bind_host", parsed.options.bind_host}, {"port", parsed.options.port}});
    if (!server.start(control_plane, parsed.options.bind_host, static_cast<std::uint16_t>(parsed.options.port))) {
        logger.write(axent::LogLevel::Error,
                     axent::LogCategory::Daemon,
                     "failed to start axentd websocket server",
                     {{"bind_host", parsed.options.bind_host}, {"port", parsed.options.port}});
        std::cerr << "failed to start axentd websocket server\n";
        host.stop();
        logger.checkpoint("host stopped");
        return 2;
    }
    logger.checkpoint("websocket listening", {{"bind_host", parsed.options.bind_host}, {"port", server.local_port()}});

    std::cout << axent::product_name() << ' ' << axent::version()
              << " axentd listening on " << parsed.options.bind_host
              << ':' << server.local_port() << '\n';

    if (!parsed.options.foreground) {
        std::cout << "service mode scaffold active\n";
    }

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    logger.checkpoint("shutdown requested");
    server.stop();
    logger.checkpoint("server stopped");
    host.stop();
    logger.checkpoint("host stopped");
    std::cout << "axentd stopped\n";
    return 0;
}
