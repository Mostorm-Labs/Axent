#pragma once

#include <string>

#include "axent/logging/logger.hpp"

namespace axent {

enum class CliParseStatus {
    Ok,
    Help,
    Version,
    Error,
};

struct CommonCliOptions {
    bool help = false;
    bool version = false;
    bool log_enabled = false;
    bool json_output = false;
    std::string log_dir = "logs";
    std::string log_file = "axent";
    LogLevel log_level = LogLevel::Info;
};

struct AxentCliOptions {
    CommonCliOptions common;
    std::string command = "help";
    bool offline = false;
};

struct AxentdCliOptions {
    CommonCliOptions common;
    bool foreground = false;
    std::string bind_host = "0.0.0.0";
    int port = 6060;
    bool enable_mock_adapter = true;
};

template <typename T>
struct CliParseResult {
    CliParseStatus status = CliParseStatus::Ok;
    T options{};
    std::string message;
};

CliParseResult<AxentCliOptions> parse_axent_cli(int argc, char** argv);
CliParseResult<AxentdCliOptions> parse_axentd_cli(int argc, char** argv);

std::string axent_usage();
std::string axentd_usage();

} // namespace axent
