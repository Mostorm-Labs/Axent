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
    if (index + 1 >= argc) {
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

bool is_axent_command(const std::string& token)
{
    return token == "status" || token == "list" || token == "reload" || token == "diagnostics";
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
        << "  --log\n"
        << "  --log-dir <path>\n"
        << "  --log-file <name-or-prefix>\n"
        << "  --log-level <error|warn|warning|info|debug|trace>\n"
        << "  --debug\n"
        << "  --json\n";
    return out.str();
}

} // namespace axent
