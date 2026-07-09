#include <iostream>
#include <string>

#include "axent/cli/axtp_cli.hpp"
#include "axent/cli/options.hpp"
#include "axent/logging/logger.hpp"
#include "axent/version.hpp"

namespace {

void print_product_version(std::ostream& out)
{
    out << axent::product_name() << ' ' << axent::version() << '\n';
}

axent::LogConfig make_log_config(const axent::CommonCliOptions& options)
{
    axent::LogConfig config;
    config.minimum_level = options.log_level;
    config.console_enabled = false;
    config.file_enabled = options.log_enabled;
    config.directory = options.log_dir;
    config.file_prefix = options.log_file;
    return config;
}

} // namespace

int main(int argc, char** argv)
{
    const auto parsed = axent::parse_axent_cli(argc, argv);

    if (parsed.status == axent::CliParseStatus::Error) {
        std::cerr << parsed.message << "\n" << axent::axent_usage();
        return 1;
    }
    if (parsed.status == axent::CliParseStatus::Help) {
        print_product_version(std::cout);
        std::cout << axent::axent_usage();
        return 0;
    }
    if (parsed.status == axent::CliParseStatus::Version) {
        print_product_version(std::cout);
        return 0;
    }

    axent::Logger logger(make_log_config(parsed.options.common));
    logger.core("axent startup", {{"command", parsed.options.command}});

    if (parsed.options.command == "status") {
        std::cout << "axentd status: " << (parsed.options.offline ? "offline" : "unknown") << '\n';
        return 0;
    }
    if (parsed.options.command == "list") {
        std::cout << "axent list: connect to axentd websocket for live devices\n";
        return 0;
    }
    if (parsed.options.command == "reload") {
        std::cout << "axent reload: requested\n";
        return 0;
    }
    if (parsed.options.command == "diagnostics") {
        std::cout << "axent diagnostics: requested\n";
        return 0;
    }
    if (parsed.options.command == "axtp") {
        const std::string executable_path = argc > 0 && argv[0] != nullptr ? argv[0] : "axent";
        return axent::run_axtp_cli(parsed.options.command_args, executable_path, std::cout, std::cerr);
    }

    std::cerr << "unknown option or command: " << parsed.options.command << "\n" << axent::axent_usage();
    return 1;
}
