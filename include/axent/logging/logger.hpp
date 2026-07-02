#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

struct LogRecord {
    std::string channel;
    std::string message;
    nlohmann::json fields;
};

class Logger {
public:
    void core(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void audit(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void adapter(const std::string& message, nlohmann::json fields = nlohmann::json::object());

    const std::vector<LogRecord>& records() const;

private:
    std::vector<LogRecord> records_;
};

} // namespace axent
