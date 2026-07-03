#include "axent/logging/logger.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <utility>

namespace axent {
namespace {

int level_rank(LogLevel level)
{
    switch (level) {
    case LogLevel::Error:
        return 0;
    case LogLevel::Warn:
        return 1;
    case LogLevel::Info:
        return 2;
    case LogLevel::Debug:
        return 3;
    case LogLevel::Trace:
        return 4;
    }
    return 4;
}

bool is_mandatory(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Warn;
}

std::string lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string format_line(const LogRecord& record)
{
    std::ostringstream out;
    out << '[' << log_level_name(record.level) << "] [" << log_category_name(record.category) << "] " << record.message;
    if (!record.fields.empty()) {
        out << ' ' << record.fields.dump();
    }
    out << '\n';
    return out.str();
}

} // namespace

const char* log_level_name(LogLevel level)
{
    switch (level) {
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Trace:
        return "TRACE";
    }
    return "TRACE";
}

const char* log_category_name(LogCategory category)
{
    switch (category) {
    case LogCategory::General:
        return "general";
    case LogCategory::Daemon:
        return "daemon";
    case LogCategory::Control:
        return "control";
    case LogCategory::Transport:
        return "transport";
    case LogCategory::Adapter:
        return "adapter";
    case LogCategory::Media:
        return "media";
    case LogCategory::Diagnostics:
        return "diagnostics";
    case LogCategory::Audit:
        return "audit";
    }
    return "general";
}

bool parse_log_level(const std::string& text, LogLevel& level)
{
    const auto normalized = lower_copy(text);
    if (normalized == "error") {
        level = LogLevel::Error;
        return true;
    }
    if (normalized == "warn" || normalized == "warning") {
        level = LogLevel::Warn;
        return true;
    }
    if (normalized == "info") {
        level = LogLevel::Info;
        return true;
    }
    if (normalized == "debug") {
        level = LogLevel::Debug;
        return true;
    }
    if (normalized == "trace") {
        level = LogLevel::Trace;
        return true;
    }
    return false;
}

Logger::Logger() = default;

Logger::Logger(LogConfig config)
{
    configure(std::move(config));
}

void Logger::configure(LogConfig config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(config);
    try {
        file_.close();
    } catch (const std::exception&) {
        note_sink_drop();
    }
    file_bytes_ = 0;
    active_file_path_.clear();

    if (config_.file_enabled) {
        try {
            active_file_path_ = std::filesystem::path(config_.directory) / (config_.file_prefix + ".log");
            if (std::filesystem::exists(active_file_path_)) {
                file_bytes_ = static_cast<std::size_t>(std::filesystem::file_size(active_file_path_));
            }
        } catch (const std::exception&) {
            note_sink_drop();
            active_file_path_.clear();
            file_bytes_ = 0;
        }
    }

    while (records_.size() > config_.memory_limit) {
        records_.erase(records_.begin());
        ++dropped_count_;
    }
}

LogConfig Logger::config() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool Logger::enabled(LogLevel level, LogCategory) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return level_rank(level) <= level_rank(config_.minimum_level);
}

void Logger::write(LogLevel level, LogCategory category, const std::string& message, nlohmann::json fields)
{
    write_record(level, category, log_category_name(category), message, std::move(fields));
}

void Logger::checkpoint(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::General, "core", "Checkpoint: " + message, std::move(fields), true);
    flush();
}

void Logger::core(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::General, "core", message, std::move(fields));
}

void Logger::audit(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::Audit, "audit", message, std::move(fields));
}

void Logger::adapter(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::Adapter, "adapter", message, std::move(fields));
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        if (file_.is_open()) {
            file_.flush();
        }
        std::clog.flush();
    } catch (const std::exception&) {
        note_sink_drop();
    }
}

std::vector<LogRecord> Logger::records() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return records_;
}

std::size_t Logger::dropped_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_count_;
}

std::string Logger::file_path() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_file_path_.string();
}

void Logger::write_record(LogLevel level,
                          LogCategory category,
                          std::string channel,
                          const std::string& message,
                          nlohmann::json fields,
                          bool force)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!force && level_rank(level) > level_rank(config_.minimum_level)) {
        return;
    }

    LogRecord record;
    record.level = level;
    record.category = category;
    record.channel = std::move(channel);
    record.message = message;
    record.fields = std::move(fields);
    record.timestamp = std::chrono::system_clock::now();

    remember(record);
    emit(record);
}

void Logger::remember(LogRecord record)
{
    if (config_.memory_limit == 0) {
        ++dropped_count_;
        return;
    }

    if (records_.size() < config_.memory_limit) {
        records_.push_back(std::move(record));
        return;
    }

    const auto removable = std::find_if(records_.begin(), records_.end(), [](const LogRecord& existing) {
        return !is_mandatory(existing.level);
    });

    if (removable != records_.end()) {
        records_.erase(removable);
        records_.push_back(std::move(record));
        ++dropped_count_;
        return;
    }

    if (is_mandatory(record.level)) {
        records_.erase(records_.begin());
        records_.push_back(std::move(record));
    }
    ++dropped_count_;
}

void Logger::emit(const LogRecord& record)
{
    try {
        const auto line = format_line(record);
        if (config_.console_enabled) {
            std::clog << line;
        }
        if (!config_.file_enabled) {
            return;
        }

        ensure_file();
        rotate_if_needed(line.size());
        ensure_file();
        if (!file_.is_open()) {
            note_sink_drop();
            return;
        }
        file_ << line;
        if (!file_) {
            note_sink_drop();
            file_.close();
            return;
        }
        file_bytes_ += line.size();
    } catch (const std::exception&) {
        note_sink_drop();
        if (file_.is_open()) {
            file_.close();
        }
    }
}

void Logger::ensure_file()
{
    if (!config_.file_enabled || file_.is_open()) {
        return;
    }
    if (active_file_path_.empty()) {
        active_file_path_ = std::filesystem::path(config_.directory) / (config_.file_prefix + ".log");
    }
    const auto parent = active_file_path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    file_.open(active_file_path_, std::ios::out | std::ios::app);
    if (std::filesystem::exists(active_file_path_)) {
        file_bytes_ = static_cast<std::size_t>(std::filesystem::file_size(active_file_path_));
    }
}

void Logger::rotate_if_needed(std::size_t next_line_bytes)
{
    if (config_.max_segment_bytes == 0 || file_bytes_ == 0 || file_bytes_ + next_line_bytes <= config_.max_segment_bytes) {
        return;
    }
    rotate();
}

void Logger::rotate()
{
    if (!config_.file_enabled || active_file_path_.empty()) {
        return;
    }

    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }

    if (config_.retained_segments == 0) {
        std::filesystem::remove(active_file_path_);
        file_bytes_ = 0;
        return;
    }

    std::filesystem::remove(segment_path(config_.retained_segments));
    for (std::size_t index = config_.retained_segments; index > 1; --index) {
        const auto from = segment_path(index - 1);
        if (std::filesystem::exists(from)) {
            std::filesystem::rename(from, segment_path(index));
        }
    }
    if (std::filesystem::exists(active_file_path_)) {
        std::filesystem::rename(active_file_path_, segment_path(1));
    }
    file_bytes_ = 0;
}

std::filesystem::path Logger::segment_path(std::size_t index) const
{
    return active_file_path_.string() + "." + std::to_string(index);
}

void Logger::note_sink_drop()
{
    ++dropped_count_;
}

} // namespace axent
