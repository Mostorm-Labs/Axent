#include "axent/cli/options.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <string>

namespace axent {
namespace {

bool is_option(const std::string& token)
{
    return token.size() > 1 && token[0] == '-';
}

std::string missing_value_message(const std::string& option)
{
    return "missing value for " + option;
}

bool require_value(int argc, char** argv, int& index, const std::string& option, std::string& value, std::string& message)
{
    if (index + 1 >= argc || is_option(argv[index + 1])) {
        message = missing_value_message(option);
        return false;
    }
    value = argv[++index];
    return true;
}

bool parse_common_option(int argc,
                         char** argv,
                         int& index,
                         CommonCliOptions& options,
                         CliParseStatus& status,
                         std::string& message,
                         bool& handled)
{
    handled = true;
    const std::string token = argv[index];

    if (token == "--help" || token == "-h") {
        options.help = true;
        status = CliParseStatus::Help;
        return true;
    }
    if (token == "--version") {
        options.version = true;
        status = CliParseStatus::Version;
        return true;
    }
    if (token == "--log") {
        options.log_enabled = true;
        return true;
    }
    if (token == "--log-dir") {
        return require_value(argc, argv, index, token, options.log_dir, message);
    }
    if (token == "--log-file") {
        return require_value(argc, argv, index, token, options.log_file, message);
    }
    if (token == "--log-level") {
        std::string value;
        if (!require_value(argc, argv, index, token, value, message)) {
            return false;
        }
        if (!parse_log_level(value, options.log_level)) {
            message = "invalid value for --log-level: " + value;
            return false;
        }
        return true;
    }
    if (token == "--debug") {
        options.log_level = LogLevel::Debug;
        return true;
    }
    if (token == "--json") {
        options.json_output = true;
        return true;
    }

    handled = false;
    return true;
}

bool parse_port_value(const std::string& text, int& port)
{
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' || parsed < 1 || parsed > 65535 || parsed > INT_MAX) {
        return false;
    }

    port = static_cast<int>(parsed);
    return true;
}

bool parse_u32_value(const std::string& text, std::uint32_t max_value, std::uint32_t& value)
{
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 0);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' || parsed > max_value) {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

template <typename Assign>
bool parse_sized_option(int argc,
                        char** argv,
                        int& index,
                        const std::string& option,
                        std::uint32_t max_value,
                        Assign assign,
                        std::string& message)
{
    std::string value_text;
    if (!require_value(argc, argv, index, option, value_text, message)) {
        return false;
    }
    std::uint32_t value = 0;
    if (!parse_u32_value(value_text, max_value, value)) {
        message = "invalid value for " + option + ": " + value_text;
        return false;
    }
    assign(value);
    return true;
}

bool is_axent_command(const std::string& token)
{
    return token == "status" || token == "list" || token == "reload" || token == "diagnostics" ||
           token == "axtp";
}

template <typename T>
CliParseResult<T> error_result(T options, std::string message)
{
    CliParseResult<T> result;
    result.status = CliParseStatus::Error;
    result.options = std::move(options);
    result.message = std::move(message);
    return result;
}

} // namespace

CliParseResult<AxentCliOptions> parse_axent_cli(int argc, char** argv)
{
    CliParseResult<AxentCliOptions> result;
    bool saw_command = false;

    for (int i = 1; i < argc; ++i) {
        CliParseStatus common_status = CliParseStatus::Ok;
        bool handled = false;
        if (!parse_common_option(argc, argv, i, result.options.common, common_status, result.message, handled)) {
            return error_result(result.options, result.message);
        }
        if (common_status == CliParseStatus::Help || common_status == CliParseStatus::Version) {
            result.status = common_status;
            return result;
        }
        if (handled) {
            continue;
        }

        const std::string token = argv[i];
        if (token == "--offline") {
            if (!saw_command || result.options.command != "status") {
                return error_result(result.options, "unknown option or command: " + token);
            }
            result.options.offline = true;
            continue;
        }

        if (is_option(token)) {
            return error_result(result.options, "unknown option or command: " + token);
        }

        if (saw_command || !is_axent_command(token)) {
            return error_result(result.options, "unknown option or command: " + token);
        }
        result.options.command = token;
        saw_command = true;

        if (token == "axtp") {
            for (++i; i < argc; ++i) {
                result.options.command_args.emplace_back(argv[i]);
            }
            break;
        }
    }

    if (!saw_command) {
        result.status = CliParseStatus::Help;
        result.options.common.help = true;
    }

    return result;
}

CliParseResult<AxentdCliOptions> parse_axentd_cli(int argc, char** argv)
{
    CliParseResult<AxentdCliOptions> result;
    result.options.axtp = AxtpAdapter::na20_defaults();

    for (int i = 1; i < argc; ++i) {
        CliParseStatus common_status = CliParseStatus::Ok;
        bool handled = false;
        if (!parse_common_option(argc, argv, i, result.options.common, common_status, result.message, handled)) {
            return error_result(result.options, result.message);
        }
        if (common_status == CliParseStatus::Help || common_status == CliParseStatus::Version) {
            result.status = common_status;
            return result;
        }
        if (handled) {
            continue;
        }

        const std::string token = argv[i];
        if (token == "--foreground") {
            result.options.foreground = true;
            continue;
        }
        if (token == "--bind") {
            if (!require_value(argc, argv, i, token, result.options.bind_host, result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--port") {
            std::string value;
            if (!require_value(argc, argv, i, token, value, result.message)) {
                return error_result(result.options, result.message);
            }
            if (!parse_port_value(value, result.options.port)) {
                return error_result(result.options, "invalid value for --port: " + value);
            }
            continue;
        }
        if (token == "--no-mock-adapter") {
            result.options.enable_mock_adapter = false;
            continue;
        }
        if (token == "--axtp-real") {
            result.options.enable_axtp_adapter = true;
            continue;
        }
        if (token == "--no-axtp-real") {
            result.options.enable_axtp_adapter = false;
            continue;
        }
        if (token == "--hid-path") {
            if (!require_value(argc, argv, i, token, result.options.axtp.selector.path, result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-serial") {
            if (!require_value(argc, argv, i, token, result.options.axtp.selector.serial_number, result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-vid") {
            if (!parse_sized_option(argc, argv, i, token, 0xffffU,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.vendor_id = static_cast<std::uint16_t>(value);
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-pid") {
            if (!parse_sized_option(argc, argv, i, token, 0xffffU,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.product_id = static_cast<std::uint16_t>(value);
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-usage-page") {
            if (!parse_sized_option(argc, argv, i, token, 0xffffU,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.usage_page = static_cast<std::uint16_t>(value);
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-usage") {
            if (!parse_sized_option(argc, argv, i, token, 0xffffU,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.usage = static_cast<std::uint16_t>(value);
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-report-id") {
            if (!parse_sized_option(argc, argv, i, token, 0xffU,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.report_id = static_cast<std::uint8_t>(value);
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-input-report-size") {
            if (!parse_sized_option(argc, argv, i, token, 1024U * 1024U,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.input_report_size = value;
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-output-report-size") {
            if (!parse_sized_option(argc, argv, i, token, 1024U * 1024U,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.output_report_size = value;
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-read-buffer-size") {
            if (!parse_sized_option(argc, argv, i, token, 1024U * 1024U,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.read_buffer_size = value;
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }
        if (token == "--hid-max-reports-per-poll") {
            if (!parse_sized_option(argc, argv, i, token, 4096U,
                                    [&](std::uint32_t value) {
                                        result.options.axtp.selector.max_reports_per_poll = value;
                                    },
                                    result.message)) {
                return error_result(result.options, result.message);
            }
            continue;
        }

        return error_result(result.options, "unknown option: " + token);
    }

    return result;
}

std::string axent_usage()
{
    std::ostringstream out;
    out << "Usage: axent [options] <command> [command-options]\n"
        << "\n"
        << "Commands:\n"
        << "  status [--offline]\n"
        << "  list\n"
        << "  reload\n"
        << "  diagnostics\n"
        << "  axtp [axtp-options] <axtp-command>\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help\n"
        << "  --version\n"
        << "  --log\n"
        << "  --log-dir <path>\n"
        << "  --log-file <name-or-prefix>\n"
        << "  --log-level <error|warn|warning|info|debug|trace>\n"
        << "  --debug\n"
        << "  --json\n";
    out << "\nCommon options may appear before or after the command.\n";
    return out.str();
}

std::string axentd_usage()
{
    std::ostringstream out;
    out << "Usage: axentd [options]\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help\n"
        << "  --version\n"
        << "  --foreground\n"
        << "  --bind <host>\n"
        << "  --port <1..65535>\n"
        << "  --no-mock-adapter\n"
        << "  --axtp-real\n"
        << "  --no-axtp-real\n"
        << "  --hid-path <path>\n"
        << "  --hid-serial <serial>\n"
        << "  --hid-vid <value>\n"
        << "  --hid-pid <value>\n"
        << "  --hid-usage-page <value>\n"
        << "  --hid-usage <value>\n"
        << "  --hid-report-id <value>\n"
        << "  --hid-input-report-size <bytes>\n"
        << "  --hid-output-report-size <bytes>\n"
        << "  --hid-read-buffer-size <bytes>\n"
        << "  --hid-max-reports-per-poll <count>\n"
        << "  --log\n"
        << "  --log-dir <path>\n"
        << "  --log-file <name-or-prefix>\n"
        << "  --log-level <error|warn|warning|info|debug|trace>\n"
        << "  --debug\n"
        << "  --json\n";
    return out.str();
}

} // namespace axent
