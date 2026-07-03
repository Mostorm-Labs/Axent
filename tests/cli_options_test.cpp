#include <stdexcept>
#include <string>
#include <vector>

#include "axent/cli/options.hpp"

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<char*> argv(std::vector<std::string>& args)
{
    std::vector<char*> pointers;
    pointers.reserve(args.size());
    for (auto& arg : args) {
        pointers.push_back(arg.data());
    }
    return pointers;
}

void test_axent_common_options_and_status_command()
{
    std::vector<std::string> args = {"axent", "--log", "--debug", "--log-dir", "logs/dev", "--json", "status", "--offline"};
    auto pointers = argv(args);

    const auto result = axent::parse_axent_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axent status parse should succeed");
    require(result.options.common.log_enabled, "--log should enable logging");
    require(result.options.common.log_level == axent::LogLevel::Debug, "--debug should select debug logging");
    require(result.options.common.log_dir == "logs/dev", "--log-dir should set directory");
    require(result.options.common.json_output, "--json should enable JSON output");
    require(result.options.command == "status", "status command should be selected");
    require(result.options.offline, "--offline should be accepted after status");
}

void test_axentd_options()
{
    std::vector<std::string> args = {
        "axentd", "--foreground", "--bind", "127.0.0.1", "--port", "6061", "--log-level", "trace", "--no-mock-adapter"};
    auto pointers = argv(args);

    const auto result = axent::parse_axentd_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axentd parse should succeed");
    require(result.options.foreground, "--foreground should be enabled");
    require(result.options.bind_host == "127.0.0.1", "--bind should set host");
    require(result.options.port == 6061, "--port should set port");
    require(result.options.common.log_level == axent::LogLevel::Trace, "--log-level trace should parse");
    require(!result.options.enable_mock_adapter, "--no-mock-adapter should disable mock adapter");
}

void test_invalid_log_level_reports_error()
{
    std::vector<std::string> args = {"axent", "--log-level", "verbose", "status"};
    auto pointers = argv(args);

    const auto result = axent::parse_axent_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Error, "invalid log level should fail");
    require(result.message.find("--log-level") != std::string::npos, "invalid log level error should name --log-level");
    require(result.message.find("verbose") != std::string::npos, "invalid log level error should include value");
}

void test_missing_port_value_reports_error()
{
    std::vector<std::string> args = {"axentd", "--port"};
    auto pointers = argv(args);

    const auto result = axent::parse_axentd_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Error, "missing port value should fail");
    require(result.message.find("--port") != std::string::npos, "missing port value error should name --port");
}

void test_option_value_must_not_be_another_option()
{
    struct Case {
        std::vector<std::string> args;
        bool daemon = false;
        std::string option;
    };

    Case cases[] = {
        {{"axent", "--log-dir", "--json", "status"}, false, "--log-dir"},
        {{"axent", "--log-file", "--debug", "status"}, false, "--log-file"},
        {{"axent", "--log-level", "--json", "status"}, false, "--log-level"},
        {{"axentd", "--bind", "--foreground"}, true, "--bind"},
        {{"axentd", "--port", "--foreground"}, true, "--port"},
    };

    for (auto& test_case : cases) {
        auto pointers = argv(test_case.args);
        if (test_case.daemon) {
            const auto result = axent::parse_axentd_cli(static_cast<int>(test_case.args.size()), pointers.data());
            require(result.status == axent::CliParseStatus::Error,
                    test_case.option + " should reject another option as its value");
            require(result.message.find(test_case.option) != std::string::npos,
                    test_case.option + " error should name option");
        } else {
            const auto result = axent::parse_axent_cli(static_cast<int>(test_case.args.size()), pointers.data());
            require(result.status == axent::CliParseStatus::Error,
                    test_case.option + " should reject another option as its value");
            require(result.message.find(test_case.option) != std::string::npos,
                    test_case.option + " error should name option");
        }
    }
}

void test_help_and_version_status()
{
    std::vector<std::string> axent_help = {"axent", "--help"};
    std::vector<std::string> axentd_help = {"axentd", "-h"};
    std::vector<std::string> axent_version = {"axent", "--version"};
    std::vector<std::string> axentd_version = {"axentd", "--version"};
    std::vector<std::string> axent_no_command = {"axent"};

    auto axent_help_argv = argv(axent_help);
    auto axentd_help_argv = argv(axentd_help);
    auto axent_version_argv = argv(axent_version);
    auto axentd_version_argv = argv(axentd_version);
    auto axent_no_command_argv = argv(axent_no_command);

    require(axent::parse_axent_cli(static_cast<int>(axent_help.size()), axent_help_argv.data()).status == axent::CliParseStatus::Help,
            "axent --help should return Help");
    require(axent::parse_axentd_cli(static_cast<int>(axentd_help.size()), axentd_help_argv.data()).status == axent::CliParseStatus::Help,
            "axentd -h should return Help");
    require(axent::parse_axent_cli(static_cast<int>(axent_version.size()), axent_version_argv.data()).status == axent::CliParseStatus::Version,
            "axent --version should return Version");
    require(axent::parse_axentd_cli(static_cast<int>(axentd_version.size()), axentd_version_argv.data()).status == axent::CliParseStatus::Version,
            "axentd --version should return Version");
    require(axent::parse_axent_cli(static_cast<int>(axent_no_command.size()), axent_no_command_argv.data()).status == axent::CliParseStatus::Help,
            "axent with no command should return Help");
}

void test_invalid_port_reports_error()
{
    std::vector<std::string> zero_port = {"axentd", "--port", "0"};
    std::vector<std::string> high_port = {"axentd", "--port", "70000"};
    auto zero_port_argv = argv(zero_port);
    auto high_port_argv = argv(high_port);

    const auto zero_result = axent::parse_axentd_cli(static_cast<int>(zero_port.size()), zero_port_argv.data());
    const auto high_result = axent::parse_axentd_cli(static_cast<int>(high_port.size()), high_port_argv.data());

    require(zero_result.status == axent::CliParseStatus::Error, "port 0 should fail");
    require(zero_result.message.find("--port") != std::string::npos, "port 0 error should name --port");
    require(high_result.status == axent::CliParseStatus::Error, "port above 65535 should fail");
    require(high_result.message.find("--port") != std::string::npos, "high port error should name --port");
}

} // namespace

int main()
{
    test_axent_common_options_and_status_command();
    test_axentd_options();
    test_invalid_log_level_reports_error();
    test_missing_port_value_reports_error();
    test_option_value_must_not_be_another_option();
    test_help_and_version_status();
    test_invalid_port_reports_error();
    return 0;
}
