#include "axent/logging/logger.hpp"

#include <utility>

namespace axent {

void Logger::core(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"core", message, std::move(fields)});
}

void Logger::audit(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"audit", message, std::move(fields)});
}

void Logger::adapter(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"adapter", message, std::move(fields)});
}

const std::vector<LogRecord>& Logger::records() const
{
    return records_;
}

} // namespace axent
