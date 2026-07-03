#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

enum class LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Trace,
};

enum class LogCategory {
    General,
    Daemon,
    Control,
    Transport,
    Adapter,
    Media,
    Diagnostics,
    Audit,
};

struct LogConfig {
    LogLevel minimum_level = LogLevel::Info;
    bool console_enabled = false;
    bool file_enabled = false;
    std::string directory = "logs";
    std::string file_prefix = "axent";
    std::size_t memory_limit = 1024;
    std::size_t max_segment_bytes = 10 * 1024 * 1024;
    std::size_t retained_segments = 5;
};

struct LogRecord {
    LogLevel level = LogLevel::Info;
    LogCategory category = LogCategory::General;
    std::string channel;
    std::string message;
    nlohmann::json fields;
    std::chrono::system_clock::time_point timestamp;
};

const char* log_level_name(LogLevel level);
const char* log_category_name(LogCategory category);
bool parse_log_level(const std::string& text, LogLevel& level);

class Logger {
public:
    Logger();
    explicit Logger(LogConfig config);

    void configure(LogConfig config);
    LogConfig config() const;
    bool enabled(LogLevel level, LogCategory category = LogCategory::General) const;

    void write(LogLevel level,
               LogCategory category,
               const std::string& message,
               nlohmann::json fields = nlohmann::json::object());
    void checkpoint(const std::string& message, nlohmann::json fields = nlohmann::json::object());

    void core(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void audit(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void adapter(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void flush();

    std::vector<LogRecord> records() const;
    std::size_t dropped_count() const;
    std::string file_path() const;

private:
    void write_record(LogLevel level,
                      LogCategory category,
                      std::string channel,
                      const std::string& message,
                      nlohmann::json fields,
                      bool force = false);
    void remember(LogRecord record);
    void emit(const LogRecord& record);
    void ensure_file();
    void rotate_if_needed(std::size_t next_line_bytes);
    void rotate();
    std::filesystem::path segment_path(std::size_t index) const;
    void note_sink_drop();

    mutable std::mutex mutex_;
    LogConfig config_;
    std::vector<LogRecord> records_;
    std::size_t dropped_count_ = 0;
    std::filesystem::path active_file_path_;
    std::ofstream file_;
    std::size_t file_bytes_ = 0;
};

} // namespace axent
