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

void test_level_parsing()
{
    axent::LogLevel level = axent::LogLevel::Info;

    require(axent::parse_log_level("error", level) && level == axent::LogLevel::Error, "error level should parse");
    require(axent::parse_log_level("warn", level) && level == axent::LogLevel::Warn, "warn level should parse");
    require(axent::parse_log_level("warning", level) && level == axent::LogLevel::Warn, "warning should parse as warn");
    require(axent::parse_log_level("info", level) && level == axent::LogLevel::Info, "info level should parse");
    require(axent::parse_log_level("debug", level) && level == axent::LogLevel::Debug, "debug level should parse");
    require(axent::parse_log_level("trace", level) && level == axent::LogLevel::Trace, "trace level should parse");
    require(!axent::parse_log_level("verbose", level), "unknown level should not parse");
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

void test_checkpoint_is_not_filtered()
{
    auto dir = make_temp_dir("checkpoint-filter");
    axent::LogConfig config;
    config.minimum_level = axent::LogLevel::Error;
    config.file_enabled = true;
    config.directory = dir.string();
    config.file_prefix = "checkpoint";

    axent::Logger logger(config);
    logger.checkpoint("forced-checkpoint");

    const auto text = read_file(logger.file_path());
    require(contains(text, "Checkpoint: forced-checkpoint"), "checkpoint should bypass level filtering");
}

void test_compatibility_channels()
{
    axent::Logger logger;

    logger.core("core.line");
    logger.audit("audit.line");
    logger.adapter("adapter.line");

    require(logger.records().size() == 3, "compatibility helpers should retain records");
    require(logger.records()[1].channel == "audit", "audit channel should remain compatible");
    require(logger.records()[2].channel == "adapter", "adapter channel should remain compatible");
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
    test_level_parsing();
    test_level_filtering_and_checkpoint();
    test_checkpoint_is_not_filtered();
    test_compatibility_channels();
    test_rotation_and_retention();
    test_bounded_memory_records_keep_mandatory_lines();
    return 0;
}
