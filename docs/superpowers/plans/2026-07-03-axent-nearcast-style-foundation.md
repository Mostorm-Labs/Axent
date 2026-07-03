# Axent NearCast-Style Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Axent-native logging and command-line foundations that match the core capabilities NearCast already has, without copying NearCast's Windows-specific implementation.

**Architecture:** `libaxent` owns shared logging and CLI parser modules. `axentd` and `axent` both consume those modules, so daemon mode and local management mode share option semantics, logging setup, and tests.

**Tech Stack:** C++17, CMake, nlohmann/json, existing `libaxent`, standard library filesystem/streams/mutexes. No new third-party dependencies.

---

## File Structure

- Modify: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/logging/logger.hpp`
  - Add `LogLevel`, `LogCategory`, `LogConfig`, richer `LogRecord`, and `Logger` APIs.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/logging/logger.cpp`
  - Implement level filtering, memory retention, console/file sinks, checkpoints, flush, and rotation.
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/cli/options.hpp`
  - Declare shared parser result types and parser functions for `axent` and `axentd`.
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/cli/options.cpp`
  - Implement common option parsing, level parsing, usage strings, and command-specific parsing.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/cli/main.cpp`
  - Replace ad hoc parsing with the shared parser and optional logging setup.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/daemon/main.cpp`
  - Replace ad hoc parsing with the shared parser, logging setup, option overrides, and checkpoints.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/diagnostics/diagnostics.cpp`
  - Include logging diagnostics in bundles while preserving existing audit behavior and redaction.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
  - Add parser source and new tests.
- Create: `/Users/qing/Desktop/sources/gitee/Axent/tests/logging_foundation_test.cpp`
  - Cover level filtering, checkpoint flush, rotation, and bounded memory record drops.
- Create: `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_options_test.cpp`
  - Cover parser behavior for common, CLI, and daemon options.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/tests/diagnostics_config_test.cpp`
  - Add expectations for logging diagnostics without weakening existing redaction checks.
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/README.md`
  - Document the new logging and CLI options.

Do not touch or stage `/Users/qing/Desktop/sources/gitee/Axent/agent/`.

## Task 1: Logging Foundation API And Tests

**Files:**
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/logging/logger.hpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/logging/logger.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/tests/logging_foundation_test.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`

- [ ] **Step 1: Write the failing logging test**

Create `/Users/qing/Desktop/sources/gitee/Axent/tests/logging_foundation_test.cpp`:

```cpp
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "axent/logging/logger.hpp"

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path make_temp_dir(const std::string& name)
{
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("axent-" + name + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text;
}

bool contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

void test_level_filtering_and_checkpoint()
{
    auto dir = make_temp_dir("log-filter");
    axent::LogConfig config;
    config.minimum_level = axent::LogLevel::Info;
    config.file_enabled = true;
    config.directory = dir.string();
    config.file_prefix = "filter";

    axent::Logger logger(config);
    logger.write(axent::LogLevel::Debug, axent::LogCategory::Transport, "hidden debug line");
    logger.write(axent::LogLevel::Info, axent::LogCategory::Transport, "visible info line");
    logger.checkpoint("checkpoint-test");
    logger.flush();

    const auto text = read_file(logger.file_path());
    require(!contains(text, "hidden debug line"), "debug line should be filtered at info level");
    require(contains(text, "[INFO] [transport] visible info line"), "info line should be written");
    require(contains(text, "Checkpoint: checkpoint-test"), "checkpoint should be written and flushed");
}

void test_rotation_and_retention()
{
    auto dir = make_temp_dir("log-rotate");
    axent::LogConfig config;
    config.minimum_level = axent::LogLevel::Info;
    config.file_enabled = true;
    config.directory = dir.string();
    config.file_prefix = "rotate";
    config.max_segment_bytes = 220;
    config.retained_segments = 3;

    axent::Logger logger(config);
    for (int i = 0; i < 20; ++i) {
        logger.write(axent::LogLevel::Info,
                     axent::LogCategory::Daemon,
                     "rotation line index=" + std::to_string(i) + " payload=abcdefghijklmnopqrstuvwxyz");
    }
    logger.flush();

    std::size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().filename().string().find("rotate") == 0) {
            ++files;
        }
    }
    require(files >= 2, "rotation should create more than one log segment");
    require(files <= 4, "rotation should retain active file plus configured segment count");
}

void test_bounded_memory_records_keep_mandatory_lines()
{
    axent::LogConfig config;
    config.minimum_level = axent::LogLevel::Trace;
    config.memory_limit = 3;
    axent::Logger logger(config);

    logger.write(axent::LogLevel::Debug, axent::LogCategory::General, "debug drop candidate 1");
    logger.write(axent::LogLevel::Debug, axent::LogCategory::General, "debug drop candidate 2");
    logger.write(axent::LogLevel::Info, axent::LogCategory::General, "info drop candidate");
    logger.write(axent::LogLevel::Warn, axent::LogCategory::General, "mandatory warn line");

    require(logger.dropped_count() > 0, "bounded memory records should report dropped lines");
    bool saw_warn = false;
    for (const auto& record : logger.records()) {
        if (record.message == "mandatory warn line") {
            saw_warn = true;
        }
    }
    require(saw_warn, "warn line should remain in bounded records");
}

} // namespace

int main()
{
    test_level_filtering_and_checkpoint();
    test_rotation_and_retention();
    test_bounded_memory_records_keep_mandatory_lines();
    return 0;
}
```

- [ ] **Step 2: Register the failing test**

Modify `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
    add_executable(logging_foundation_test tests/logging_foundation_test.cpp)
    target_link_libraries(logging_foundation_test PRIVATE axent::libaxent)
    axent_enable_warnings(logging_foundation_test)
    add_test(NAME logging_foundation_test COMMAND logging_foundation_test)
```

- [ ] **Step 3: Run the test and verify it fails**

Run:

```bash
cmake -S /Users/qing/Desktop/sources/gitee/Axent -B /Users/qing/Desktop/sources/gitee/Axent/build -DBUILD_TESTING=ON
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target logging_foundation_test
```

Expected: build fails because `LogConfig`, `LogLevel`, `LogCategory`, `write`, `checkpoint`, `flush`, `file_path`, and `dropped_count` do not exist yet.

- [ ] **Step 4: Implement the logging header**

Replace `/Users/qing/Desktop/sources/gitee/Axent/include/axent/logging/logger.hpp` with:

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

enum class LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
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
    std::uint64_t max_segment_bytes = 10ull * 1024ull * 1024ull;
    std::size_t retained_segments = 5;
};

struct LogRecord {
    std::string channel;
    std::string message;
    nlohmann::json fields;
    LogLevel level = LogLevel::Info;
    LogCategory category = LogCategory::General;
    std::chrono::system_clock::time_point timestamp;
};

const char* log_level_name(LogLevel level);
const char* log_category_name(LogCategory category);
bool parse_log_level(const std::string& text, LogLevel& level);

class Logger {
public:
    Logger();
    explicit Logger(LogConfig config);
    ~Logger();

    void configure(LogConfig config);
    const LogConfig& config() const;

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
    const std::vector<LogRecord>& records() const;
    std::uint64_t dropped_count() const;
    std::string file_path() const;

private:
    void open_file_locked();
    void rotate_if_needed_locked(std::size_t next_line_bytes);
    void remember_record_locked(LogRecord record);
    void write_record(LogLevel level,
                      LogCategory category,
                      std::string channel,
                      const std::string& message,
                      nlohmann::json fields,
                      bool force_flush);
    void write_line_locked(const std::string& line);

    LogConfig config_;
    mutable std::mutex mutex_;
    std::vector<LogRecord> records_;
    std::uint64_t dropped_count_ = 0;
    std::filesystem::path file_path_;
    std::ofstream file_;
    std::uint64_t current_file_bytes_ = 0;
};

} // namespace axent
```

- [ ] **Step 5: Implement the logging source**

Replace `/Users/qing/Desktop/sources/gitee/Axent/src/logging/logger.cpp` with an implementation that:

```cpp
#include "axent/logging/logger.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace axent {
namespace {

bool is_mandatory(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Warn;
}

std::string iso_timestamp(std::chrono::system_clock::time_point time)
{
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(time);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time - seconds).count();
    const std::time_t raw = std::chrono::system_clock::to_time_t(time);
    std::tm tm {};
#ifdef _WIN32
    gmtime_s(&tm, &raw);
#else
    gmtime_r(&raw, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << millis << 'Z';
    return out.str();
}

std::string fields_to_text(const nlohmann::json& fields)
{
    if (!fields.is_object() || fields.empty()) {
        return "";
    }
    std::string text;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        text += ' ';
        text += it.key();
        text += '=';
        text += it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
    }
    return text;
}

std::string channel_for(LogCategory category)
{
    return log_category_name(category);
}

std::string line_for(const LogRecord& record)
{
    return iso_timestamp(record.timestamp) + " [" + log_level_name(record.level) + "] [" +
        log_category_name(record.category) + "] " + record.message + fields_to_text(record.fields) + "\n";
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
    return "INFO";
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
    if (text == "error") {
        level = LogLevel::Error;
        return true;
    }
    if (text == "warn" || text == "warning") {
        level = LogLevel::Warn;
        return true;
    }
    if (text == "info") {
        level = LogLevel::Info;
        return true;
    }
    if (text == "debug") {
        level = LogLevel::Debug;
        return true;
    }
    if (text == "trace") {
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

Logger::~Logger()
{
    flush();
}

void Logger::configure(LogConfig config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    flush();
    config_ = std::move(config);
    file_path_.clear();
    current_file_bytes_ = 0;
    if (config_.file_enabled) {
        open_file_locked();
    }
}

const LogConfig& Logger::config() const
{
    return config_;
}

bool Logger::enabled(LogLevel level, LogCategory) const
{
    return static_cast<int>(level) <= static_cast<int>(config_.minimum_level);
}

void Logger::write(LogLevel level, LogCategory category, const std::string& message, nlohmann::json fields)
{
    write_record(level, category, channel_for(category), message, std::move(fields), false);
}

void Logger::checkpoint(const std::string& message, nlohmann::json fields)
{
    fields["checkpoint"] = true;
    write_record(LogLevel::Info, LogCategory::General, "core", "Checkpoint: " + message, std::move(fields), true);
}

void Logger::core(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::General, "core", message, std::move(fields), false);
}

void Logger::audit(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::Audit, "audit", message, std::move(fields), false);
}

void Logger::adapter(const std::string& message, nlohmann::json fields)
{
    write_record(LogLevel::Info, LogCategory::Adapter, "adapter", message, std::move(fields), false);
}

void Logger::write_record(LogLevel level,
                          LogCategory category,
                          std::string channel,
                          const std::string& message,
                          nlohmann::json fields,
                          bool force_flush)
{
    if (!enabled(level, category)) {
        return;
    }

    LogRecord record;
    record.channel = std::move(channel);
    record.message = message;
    record.fields = std::move(fields);
    record.level = level;
    record.category = category;
    record.timestamp = std::chrono::system_clock::now();

    const auto line = line_for(record);

    std::lock_guard<std::mutex> lock(mutex_);
    remember_record_locked(record);
    write_line_locked(line);
    if (force_flush && file_.is_open()) {
        file_.flush();
    }
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

const std::vector<LogRecord>& Logger::records() const
{
    return records_;
}

std::uint64_t Logger::dropped_count() const
{
    return dropped_count_;
}

std::string Logger::file_path() const
{
    return file_path_.string();
}

void Logger::open_file_locked()
{
    std::filesystem::create_directories(config_.directory);
    file_path_ = std::filesystem::path(config_.directory) / (config_.file_prefix + ".log");
    file_.open(file_path_, std::ios::out | std::ios::app | std::ios::binary);
    current_file_bytes_ = std::filesystem::exists(file_path_) ? std::filesystem::file_size(file_path_) : 0;
}

void Logger::rotate_if_needed_locked(std::size_t next_line_bytes)
{
    if (!file_.is_open() || config_.max_segment_bytes == 0 ||
        current_file_bytes_ + next_line_bytes <= config_.max_segment_bytes) {
        return;
    }
    file_.close();
    for (std::size_t i = config_.retained_segments; i > 0; --i) {
        auto from = std::filesystem::path(config_.directory) /
            (config_.file_prefix + "." + std::to_string(i) + ".log");
        auto to = std::filesystem::path(config_.directory) /
            (config_.file_prefix + "." + std::to_string(i + 1) + ".log");
        if (std::filesystem::exists(to)) {
            std::filesystem::remove(to);
        }
        if (std::filesystem::exists(from)) {
            std::filesystem::rename(from, to);
        }
    }
    if (std::filesystem::exists(file_path_)) {
        std::filesystem::rename(file_path_,
                                std::filesystem::path(config_.directory) / (config_.file_prefix + ".1.log"));
    }
    open_file_locked();
}

void Logger::remember_record_locked(LogRecord record)
{
    if (config_.memory_limit == 0) {
        ++dropped_count_;
        return;
    }
    if (records_.size() >= config_.memory_limit) {
        ++dropped_count_;
        auto drop = std::find_if(records_.begin(), records_.end(), [](const LogRecord& candidate) {
            return !is_mandatory(candidate.level);
        });
        if (drop == records_.end()) {
            drop = records_.begin();
        }
        records_.erase(drop);
    }
    records_.push_back(std::move(record));
}

void Logger::write_line_locked(const std::string& line)
{
    if (config_.console_enabled) {
        std::cerr << line;
    }
    if (config_.file_enabled) {
        if (!file_.is_open()) {
            open_file_locked();
        }
        rotate_if_needed_locked(line.size());
        file_ << line;
        current_file_bytes_ += line.size();
    }
}

} // namespace axent
```

- [ ] **Step 6: Run the logging test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target logging_foundation_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R logging_foundation_test --output-on-failure
```

Expected: `logging_foundation_test` passes.

- [ ] **Step 7: Commit**

Run:

```bash
git add /Users/qing/Desktop/sources/gitee/Axent/include/axent/logging/logger.hpp \
        /Users/qing/Desktop/sources/gitee/Axent/src/logging/logger.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/tests/logging_foundation_test.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt
git commit -m "feat: add axent logging foundation"
```

## Task 2: CLI Parser Library

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/cli/options.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/cli/options.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_options_test.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`

- [ ] **Step 1: Write the failing parser test**

Create `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_options_test.cpp`:

```cpp
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

std::vector<char*> argv_from(std::vector<std::string>& args)
{
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    return argv;
}

void test_axent_common_options()
{
    std::vector<std::string> args {"axent", "--log", "--debug", "--log-dir", "logs/dev", "--json", "status", "--offline"};
    auto argv = argv_from(args);
    const auto result = axent::parse_axent_cli(static_cast<int>(argv.size()), argv.data());

    require(result.status == axent::CliParseStatus::Ok, "axent parser should accept common options");
    require(result.options.common.log_enabled, "--log should enable file logging");
    require(result.options.common.log_level == axent::LogLevel::Debug, "--debug should set debug level");
    require(result.options.common.log_dir == "logs/dev", "--log-dir should be captured");
    require(result.options.common.json_output, "--json should be captured");
    require(result.options.command == "status", "status command should be captured");
    require(result.options.offline, "--offline should be captured for status");
}

void test_axentd_options()
{
    std::vector<std::string> args {
        "axentd", "--foreground", "--bind", "127.0.0.1", "--port", "6061",
        "--log-level", "trace", "--no-mock-adapter"
    };
    auto argv = argv_from(args);
    const auto result = axent::parse_axentd_cli(static_cast<int>(argv.size()), argv.data());

    require(result.status == axent::CliParseStatus::Ok, "axentd parser should accept daemon options");
    require(result.options.foreground, "--foreground should be captured");
    require(result.options.bind_host == "127.0.0.1", "--bind should be captured");
    require(result.options.port == 6061, "--port should be captured");
    require(result.options.common.log_level == axent::LogLevel::Trace, "--log-level trace should be captured");
    require(!result.options.enable_mock_adapter, "--no-mock-adapter should disable mock adapter");
}

void test_invalid_options()
{
    std::vector<std::string> bad_level {"axent", "--log-level", "chatty", "status"};
    auto bad_level_argv = argv_from(bad_level);
    auto bad_level_result = axent::parse_axent_cli(static_cast<int>(bad_level_argv.size()), bad_level_argv.data());
    require(bad_level_result.status == axent::CliParseStatus::Error, "invalid log level should fail");
    require(bad_level_result.message.find("log level") != std::string::npos, "invalid level error should be clear");

    std::vector<std::string> missing_value {"axentd", "--port"};
    auto missing_argv = argv_from(missing_value);
    auto missing_result = axent::parse_axentd_cli(static_cast<int>(missing_argv.size()), missing_argv.data());
    require(missing_result.status == axent::CliParseStatus::Error, "missing port value should fail");
    require(missing_result.message.find("--port") != std::string::npos, "missing value error should name option");
}

void test_help_and_version()
{
    std::vector<std::string> help {"axent", "--help"};
    auto help_argv = argv_from(help);
    auto help_result = axent::parse_axent_cli(static_cast<int>(help_argv.size()), help_argv.data());
    require(help_result.status == axent::CliParseStatus::Help, "--help should produce help status");

    std::vector<std::string> version {"axentd", "--version"};
    auto version_argv = argv_from(version);
    auto version_result = axent::parse_axentd_cli(static_cast<int>(version_argv.size()), version_argv.data());
    require(version_result.status == axent::CliParseStatus::Version, "--version should produce version status");
}

} // namespace

int main()
{
    test_axent_common_options();
    test_axentd_options();
    test_invalid_options();
    test_help_and_version();
    return 0;
}
```

- [ ] **Step 2: Register parser source and test**

Modify `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`:

```cmake
add_library(libaxent
    src/version.cpp
    src/cli/options.cpp
    src/control/protocol_codecs.cpp
    src/control/control_plane.cpp
    src/control/websocket_server.cpp
    src/core/json.cpp
    src/core/device_manager.cpp
    src/core/session_manager.cpp
    src/core/capability_registry.cpp
    src/core/adapter_registry.cpp
    src/core/route_manager.cpp
    src/core/middleware.cpp
    src/core/flow_control.cpp
    src/core/broker.cpp
    src/config/config.cpp
    src/diagnostics/diagnostics.cpp
    src/firmware/firmware_task.cpp
    src/logging/logger.cpp
    src/adapters/axdp_adapter.cpp
    src/adapters/axtp_adapter.cpp
    src/adapters/mock_adapter.cpp
    src/adapters/tea_adapter.cpp
    src/host/axent_host.cpp
    src/media/media_relay.cpp
    src/cli/options.cpp
)
```

Inside `if(BUILD_TESTING)`:

```cmake
    add_executable(cli_options_test tests/cli_options_test.cpp)
    target_link_libraries(cli_options_test PRIVATE axent::libaxent)
    axent_enable_warnings(cli_options_test)
    add_test(NAME cli_options_test COMMAND cli_options_test)
```

- [ ] **Step 3: Run the parser test and verify it fails**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target cli_options_test
```

Expected: build fails because `axent/cli/options.hpp` and parser functions do not exist.

- [ ] **Step 4: Implement parser header**

Create `/Users/qing/Desktop/sources/gitee/Axent/include/axent/cli/options.hpp`:

```cpp
#pragma once

#include <cstdint>
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
    T options {};
    std::string message;
};

CliParseResult<AxentCliOptions> parse_axent_cli(int argc, char** argv);
CliParseResult<AxentdCliOptions> parse_axentd_cli(int argc, char** argv);

std::string axent_usage();
std::string axentd_usage();

} // namespace axent
```

- [ ] **Step 5: Implement parser source**

Create `/Users/qing/Desktop/sources/gitee/Axent/src/cli/options.cpp` with these behaviors:

```cpp
#include "axent/cli/options.hpp"

#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace axent {
namespace {

bool is_common_option(const std::string& arg)
{
    return arg == "--help" || arg == "-h" || arg == "--version" || arg == "--log" ||
        arg == "--log-dir" || arg == "--log-file" || arg == "--log-level" ||
        arg == "--debug" || arg == "--json";
}

bool take_value(int& index, int argc, char** argv, const std::string& option, std::string& value, std::string& error)
{
    if (index + 1 >= argc) {
        error = "missing value for " + option;
        return false;
    }
    ++index;
    value = argv[index];
    return true;
}

bool parse_common(int& index,
                  int argc,
                  char** argv,
                  CommonCliOptions& options,
                  CliParseStatus& status,
                  std::string& message)
{
    const std::string arg = argv[index];
    if (arg == "--help" || arg == "-h") {
        options.help = true;
        status = CliParseStatus::Help;
        return true;
    }
    if (arg == "--version") {
        options.version = true;
        status = CliParseStatus::Version;
        return true;
    }
    if (arg == "--log") {
        options.log_enabled = true;
        return true;
    }
    if (arg == "--debug") {
        options.log_level = LogLevel::Debug;
        return true;
    }
    if (arg == "--json") {
        options.json_output = true;
        return true;
    }
    if (arg == "--log-dir") {
        return take_value(index, argc, argv, arg, options.log_dir, message);
    }
    if (arg == "--log-file") {
        return take_value(index, argc, argv, arg, options.log_file, message);
    }
    if (arg == "--log-level") {
        std::string value;
        if (!take_value(index, argc, argv, arg, value, message)) {
            return false;
        }
        if (!parse_log_level(value, options.log_level)) {
            message = "invalid log level: " + value;
            return false;
        }
        return true;
    }
    return false;
}

bool parse_port(const std::string& value, int& port)
{
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed <= 0 || parsed > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    port = static_cast<int>(parsed);
    return true;
}

} // namespace

std::string axent_usage()
{
    return "usage: axent [--version] [--log] [--debug] [--log-level <level>] "
           "[--log-dir <path>] [--json] status|list|reload|diagnostics\n";
}

std::string axentd_usage()
{
    return "usage: axentd [--foreground] [--bind <host>] [--port <port>] "
           "[--log] [--debug] [--log-level <level>] [--log-dir <path>] [--no-mock-adapter]\n";
}

CliParseResult<AxentCliOptions> parse_axent_cli(int argc, char** argv)
{
    CliParseResult<AxentCliOptions> result;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (is_common_option(arg)) {
            CliParseStatus status = result.status;
            if (!parse_common(i, argc, argv, result.options.common, status, result.message)) {
                result.status = CliParseStatus::Error;
                return result;
            }
            result.status = status;
            if (status == CliParseStatus::Help || status == CliParseStatus::Version) {
                return result;
            }
            continue;
        }
        if (arg == "status" || arg == "list" || arg == "reload" || arg == "diagnostics") {
            result.options.command = arg;
            continue;
        }
        if (arg == "--offline" && result.options.command == "status") {
            result.options.offline = true;
            continue;
        }
        result.status = CliParseStatus::Error;
        result.message = "unknown option or command: " + arg;
        return result;
    }
    if (result.options.command == "help") {
        result.status = CliParseStatus::Help;
    }
    return result;
}

CliParseResult<AxentdCliOptions> parse_axentd_cli(int argc, char** argv)
{
    CliParseResult<AxentdCliOptions> result;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (is_common_option(arg)) {
            CliParseStatus status = result.status;
            if (!parse_common(i, argc, argv, result.options.common, status, result.message)) {
                result.status = CliParseStatus::Error;
                return result;
            }
            result.status = status;
            if (status == CliParseStatus::Help || status == CliParseStatus::Version) {
                return result;
            }
            continue;
        }
        if (arg == "--foreground") {
            result.options.foreground = true;
            continue;
        }
        if (arg == "--no-mock-adapter") {
            result.options.enable_mock_adapter = false;
            continue;
        }
        if (arg == "--bind") {
            if (!take_value(i, argc, argv, arg, result.options.bind_host, result.message)) {
                result.status = CliParseStatus::Error;
                return result;
            }
            continue;
        }
        if (arg == "--port") {
            std::string value;
            if (!take_value(i, argc, argv, arg, value, result.message) || !parse_port(value, result.options.port)) {
                result.status = CliParseStatus::Error;
                result.message = result.message.empty() ? "invalid value for --port: " + value : result.message;
                return result;
            }
            continue;
        }
        result.status = CliParseStatus::Error;
        result.message = "unknown option: " + arg;
        return result;
    }
    return result;
}

} // namespace axent
```

- [ ] **Step 6: Run parser tests**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target cli_options_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R cli_options_test --output-on-failure
```

Expected: `cli_options_test` passes.

- [ ] **Step 7: Commit**

Run:

```bash
git add /Users/qing/Desktop/sources/gitee/Axent/include/axent/cli/options.hpp \
        /Users/qing/Desktop/sources/gitee/Axent/src/cli/options.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/tests/cli_options_test.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt
git commit -m "feat: add shared axent cli parser"
```

## Task 3: Wire Parser And Logging Into `axent` And `axentd`

**Files:**
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/cli/main.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/daemon/main.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_smoke_test.cpp`

- [ ] **Step 1: Extend smoke expectations**

Modify `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_smoke_test.cpp` to also run:

```cpp
const std::string version_command = std::string(argv[1]) + " --version";
```

Read its output the same way as the status command and assert it contains `Axent`.

- [ ] **Step 2: Wire `axent` CLI**

Replace `/Users/qing/Desktop/sources/gitee/Axent/src/cli/main.cpp` with code that:

```cpp
#include <iostream>

#include "axent/cli/options.hpp"
#include "axent/logging/logger.hpp"
#include "axent/version.hpp"

namespace {

axent::Logger make_logger(const axent::CommonCliOptions& options)
{
    axent::LogConfig config;
    config.minimum_level = options.log_level;
    config.file_enabled = options.log_enabled;
    config.console_enabled = false;
    config.directory = options.log_dir;
    config.file_prefix = options.log_file;
    return axent::Logger(config);
}

void print_version()
{
    std::cout << axent::product_name() << ' ' << axent::version() << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    const auto parsed = axent::parse_axent_cli(argc, argv);
    if (parsed.status == axent::CliParseStatus::Error) {
        std::cerr << parsed.message << '\n' << axent::axent_usage();
        return 1;
    }
    if (parsed.status == axent::CliParseStatus::Help) {
        std::cout << axent::product_name() << ' ' << axent::version() << '\n'
                  << axent::axent_usage();
        return 0;
    }
    if (parsed.status == axent::CliParseStatus::Version) {
        print_version();
        return 0;
    }

    auto logger = make_logger(parsed.options.common);
    logger.core("axent.cli.start", {{"command", parsed.options.command}});

    if (parsed.options.command == "status") {
        std::cout << "axentd status: " << (parsed.options.offline ? "offline" : "unknown") << '\n';
        return 0;
    }
    if (parsed.options.command == "list") {
        std::cout << "axent list: connect to axentd websocket for live devices\n";
        return 0;
    }
    if (parsed.options.command == "reload") {
        std::cout << "axent reload: requested\n";
        return 0;
    }
    if (parsed.options.command == "diagnostics") {
        std::cout << "axent diagnostics: requested\n";
        return 0;
    }

    std::cerr << axent::axent_usage();
    return 1;
}
```

- [ ] **Step 3: Wire `axentd` CLI and checkpoints**

Modify `/Users/qing/Desktop/sources/gitee/Axent/src/daemon/main.cpp` to:

- parse with `parse_axentd_cli()`
- handle help/version/error before starting the host
- create `Logger` from parsed common options
- override config server bind/port from parsed daemon options
- set `host_options.enable_mock_adapter` from parsed daemon options
- emit `checkpoint()` around host/server lifecycle
- log startup errors before returning

The main should preserve signal handling and foreground behavior.

- [ ] **Step 4: Build and run smoke tests**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target axent axentd cli_smoke_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R cli_smoke_test --output-on-failure
/Users/qing/Desktop/sources/gitee/Axent/build/axent --version
/Users/qing/Desktop/sources/gitee/Axent/build/axent status --offline --log --debug --log-dir /tmp/axent-cli-log
```

Expected: smoke test passes, version prints product/version, status remains offline.

- [ ] **Step 5: Commit**

Run:

```bash
git add /Users/qing/Desktop/sources/gitee/Axent/src/cli/main.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/src/daemon/main.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/tests/cli_smoke_test.cpp
git commit -m "feat: wire axent logging and cli options"
```

## Task 4: Diagnostics And Documentation

**Files:**
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/src/diagnostics/diagnostics.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/tests/diagnostics_config_test.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/README.md`

- [ ] **Step 1: Extend diagnostics test**

In `/Users/qing/Desktop/sources/gitee/Axent/tests/diagnostics_config_test.cpp`, after constructing the logger and before collecting diagnostics, configure file logging in a temp directory and assert:

```cpp
require(bundle.at("logging").at("minimumLevel") == "INFO", "diagnostics should expose logging level");
require(bundle.at("logging").contains("droppedCount"), "diagnostics should expose dropped log count");
```

Keep existing audit and redaction assertions.

- [ ] **Step 2: Update diagnostics implementation**

In `/Users/qing/Desktop/sources/gitee/Axent/src/diagnostics/diagnostics.cpp`, add a `logging` object to the returned bundle:

```cpp
{"logging", {
    {"minimumLevel", log_level_name(logger_.config().minimum_level)},
    {"filePath", logger_.file_path()},
    {"droppedCount", logger_.dropped_count()}
}}
```

Do not include raw sensitive fields outside the existing `auditLog` handling.

- [ ] **Step 3: Document usage**

Add a short section to `/Users/qing/Desktop/sources/gitee/Axent/README.md`:

```markdown
## Logging And CLI

`axent` and `axentd` share Axent-native logging options:

    build/axent --version
    build/axent status --offline --log --debug
    build/axentd --foreground --bind 127.0.0.1 --port 6060 --log --log-level debug

`--log` enables file logging, `--debug` maps to `--log-level debug`, and
`--log-dir` selects the log directory. The logger supports level filtering,
structured in-memory diagnostics, checkpoints, bounded memory records, and
rotating file segments without adding a third-party logging dependency.
```

- [ ] **Step 4: Run diagnostics test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target diagnostics_config_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R diagnostics_config_test --output-on-failure
```

Expected: diagnostics test passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add /Users/qing/Desktop/sources/gitee/Axent/src/diagnostics/diagnostics.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/tests/diagnostics_config_test.cpp \
        /Users/qing/Desktop/sources/gitee/Axent/README.md
git commit -m "docs: expose axent logging foundation"
```

## Task 5: Full Verification And Cleanup

**Files:**
- Review all touched files.

- [ ] **Step 1: Configure and build**

Run:

```bash
cmake -S /Users/qing/Desktop/sources/gitee/Axent -B /Users/qing/Desktop/sources/gitee/Axent/build -DBUILD_TESTING=ON
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build
```

Expected: full build succeeds.

- [ ] **Step 2: Run all tests**

Run:

```bash
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build --output-on-failure
```

Expected: all tests pass, including dependency boundary, logging foundation, CLI parser, CLI smoke, diagnostics, and WebSocket tests.

- [ ] **Step 3: Run command smoke checks**

Run:

```bash
/Users/qing/Desktop/sources/gitee/Axent/build/axent --version
/Users/qing/Desktop/sources/gitee/Axent/build/axent status --offline
/Users/qing/Desktop/sources/gitee/Axent/build/axent status --offline --log --debug --log-dir /tmp/axent-cli-log
/Users/qing/Desktop/sources/gitee/Axent/build/axentd --version
```

Expected: commands exit successfully and do not require a running daemon.

- [ ] **Step 4: Check git hygiene**

Run:

```bash
git diff --check
git status --short
```

Expected: `git diff --check` passes. `git status --short` may still show `?? agent/`; do not stage it.

- [ ] **Step 5: Confirm there are no unstaged feature changes**

Run:

```bash
git status --short
```

Expected: only `?? agent/` is allowed to remain. If any feature file is still
modified, inspect it, run the relevant test, stage that specific file, and make
a focused commit with a message that describes the exact fix. Do not push or
create a PR unless explicitly requested.
