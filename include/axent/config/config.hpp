#pragma once

#include <string>

namespace axent {

struct ServerConfig {
    std::string bind_host = "0.0.0.0";
    int port = 6060;
    std::string authentication = "none";
};

struct LoggingConfig {
    std::string level = "info";
    std::string directory = "logs";
    bool audit = true;
};

struct AxentConfig {
    ServerConfig server;
    LoggingConfig logging;

    static AxentConfig dev_trial_defaults();
};

} // namespace axent
