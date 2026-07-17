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

void test_axent_axtp_preserves_subcommand_arguments()
{
    std::vector<std::string> args = {
        "axent",
        "axtp",
        "-c",
        "audio.getAlgorithmConfig",
        "--json",
        R"({"value":1})",
        "-o",
        "json"};
    auto pointers = argv(args);

    const auto result = axent::parse_axent_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axent axtp parse should succeed");
    require(result.options.command == "axtp", "axtp command should be selected");
    require(!result.options.common.json_output, "axtp --json should not be parsed as Axent global JSON");
    require(result.options.command_args.size() == args.size() - 2, "axtp args should be preserved");
    require(result.options.command_args[0] == "-c", "axtp first arg mismatch");
    require(result.options.command_args[3] == R"({"value":1})", "axtp JSON body should be preserved");
}

void test_axent_global_json_before_axtp()
{
    std::vector<std::string> args = {"axent", "--json", "axtp", "ping"};
    auto pointers = argv(args);

    const auto result = axent::parse_axent_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axent --json axtp parse should succeed");
    require(result.options.common.json_output, "global --json before axtp should remain global");
    require(result.options.command == "axtp", "axtp command should be selected after global options");
    require(result.options.command_args.size() == 1, "axtp ping should be preserved");
    require(result.options.command_args[0] == "ping", "axtp ping arg mismatch");
}

void test_axentd_options()
{
    std::vector<std::string> args = {
        "axentd",
        "--foreground",
        "--bind",
        "127.0.0.1",
        "--port",
        "6061",
        "--log-level",
        "trace",
        "--no-mock-adapter",
        "--axtp-real",
        "--hid-vid",
        "0x0581",
        "--hid-pid",
        "0x2581",
        "--hid-usage-page",
        "0x0081",
        "--hid-usage",
        "0x0001",
        "--hid-report-id",
        "0x05",
        "--hid-input-report-size",
        "0",
        "--hid-output-report-size",
        "0",
        "--hid-read-buffer-size",
        "8192",
        "--hid-max-reports-per-poll",
        "64",
        "--hid-path",
        "specific-path",
        "--hid-serial",
        "NA20-SERIAL"};
    auto pointers = argv(args);

    const auto result = axent::parse_axentd_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axentd parse should succeed");
    require(result.options.foreground, "--foreground should be enabled");
    require(result.options.bind_host == "127.0.0.1", "--bind should set host");
    require(result.options.port == 6061, "--port should set port");
    require(result.options.common.log_level == axent::LogLevel::Trace, "--log-level trace should parse");
    require(!result.options.enable_mock_adapter, "--no-mock-adapter should disable mock adapter");
    require(result.options.enable_axtp_adapter, "--axtp-real should enable real AXTP adapter");
    require(result.options.axtp.selector.vendor_id == 0x0581, "--hid-vid should set selector VID");
    require(result.options.axtp.selector.product_id == 0x2581, "--hid-pid should set selector PID");
    require(result.options.axtp.selector.usage_page == 0x0081, "--hid-usage-page should set selector usage page");
    require(result.options.axtp.selector.usage == 0x0001, "--hid-usage should set selector usage");
    require(result.options.axtp.selector.report_id == 0x05, "--hid-report-id should set report id");
    require(result.options.axtp.selector.input_report_size == 0, "--hid-input-report-size should keep auto");
    require(result.options.axtp.selector.output_report_size == 0, "--hid-output-report-size should keep auto");
    require(result.options.axtp.selector.read_buffer_size == 8192, "--hid-read-buffer-size should set read buffer");
    require(result.options.axtp.selector.max_reports_per_poll == 64,
            "--hid-max-reports-per-poll should set poll budget");
    require(result.options.axtp.selector.path == "specific-path", "--hid-path should set path");
    require(result.options.axtp.selector.serial_number == "NA20-SERIAL", "--hid-serial should set serial");
}

void test_axentd_real_adapter_defaults_and_precedence()
{
    std::vector<std::string> args = {"axentd", "--axtp-real", "--no-axtp-real"};
    auto pointers = argv(args);

    const auto result = axent::parse_axentd_cli(static_cast<int>(args.size()), pointers.data());

    require(result.status == axent::CliParseStatus::Ok, "axentd default parse should succeed");
    require(!result.options.enable_axtp_adapter, "--no-axtp-real should override earlier --axtp-real");
    require(result.options.axtp.selector.kind == axent::TransportKind::Hid, "default AXTP selector should be HID");
    require(result.options.axtp.selector.vendor_id == 0x0581, "default AXTP VID mismatch");
    require(result.options.axtp.selector.product_id == 0x2582, "default AXTP PID mismatch");
    require(result.options.axtp.selector.usage_page == 0x0081, "default AXTP usage page mismatch");
    require(result.options.axtp.selector.report_id == 0x05, "default AXTP report id mismatch");
    require(result.options.axtp.selector.input_report_size == 0, "default input report size should be auto");
    require(result.options.axtp.selector.output_report_size == 0, "default output report size should be auto");
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
        {{"axentd", "--hid-vid", "--foreground"}, true, "--hid-vid"},
        {{"axentd", "--hid-path", "--foreground"}, true, "--hid-path"},
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
    std::vector<std::string> high_vid = {"axentd", "--hid-vid", "0x10000"};
    auto zero_port_argv = argv(zero_port);
    auto high_port_argv = argv(high_port);
    auto high_vid_argv = argv(high_vid);

    const auto zero_result = axent::parse_axentd_cli(static_cast<int>(zero_port.size()), zero_port_argv.data());
    const auto high_result = axent::parse_axentd_cli(static_cast<int>(high_port.size()), high_port_argv.data());
    const auto high_vid_result = axent::parse_axentd_cli(static_cast<int>(high_vid.size()), high_vid_argv.data());

    require(zero_result.status == axent::CliParseStatus::Error, "port 0 should fail");
    require(zero_result.message.find("--port") != std::string::npos, "port 0 error should name --port");
    require(high_result.status == axent::CliParseStatus::Error, "port above 65535 should fail");
    require(high_result.message.find("--port") != std::string::npos, "high port error should name --port");
    require(high_vid_result.status == axent::CliParseStatus::Error, "VID above 0xffff should fail");
    require(high_vid_result.message.find("--hid-vid") != std::string::npos, "high VID error should name --hid-vid");
}

} // namespace

int main()
{
    test_axent_common_options_and_status_command();
    test_axent_axtp_preserves_subcommand_arguments();
    test_axent_global_json_before_axtp();
    test_axentd_options();
    test_axentd_real_adapter_defaults_and_precedence();
    test_invalid_log_level_reports_error();
    test_missing_port_value_reports_error();
    test_option_value_must_not_be_another_option();
    test_help_and_version_status();
    test_invalid_port_reports_error();
    return 0;
}
