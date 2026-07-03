#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#define AXENT_POPEN _popen
#define AXENT_PCLOSE _pclose
#else
#define AXENT_POPEN popen
#define AXENT_PCLOSE pclose
#endif

namespace {

struct CommandResult {
    int code = 0;
    std::string output;
};

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

std::string shell_quote(const std::string& value)
{
#ifdef _WIN32
    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += '"';
    return quoted;
#else
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
#endif
}

CommandResult run_command(const std::string& command)
{
    std::array<char, 256> buffer{};
    CommandResult result;
    FILE* pipe = AXENT_POPEN(command.c_str(), "r");
    if (pipe == nullptr) {
        result.code = -1;
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

    result.code = AXENT_PCLOSE(pipe);
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        return fail("usage: cli_smoke_test <axent-path>");
    }

    const std::string axent = shell_quote(argv[1]);

    const auto version = run_command(axent + " --version");
    if (version.code != 0) {
        return fail("axent --version exited with non-zero status");
    }
    if (version.output.find("Axent") == std::string::npos) {
        return fail("version output did not mention Axent");
    }

    const std::filesystem::path log_dir = std::filesystem::temp_directory_path() / "axent-cli-smoke-log";
    std::filesystem::remove_all(log_dir);

    const auto status = run_command(axent + " status --offline --log --debug --log-dir " + shell_quote(log_dir.string()));
    if (status.code != 0) {
        return fail("axent status command exited with non-zero status");
    }
    if (status.output.find("axentd") == std::string::npos) {
        return fail("status output did not mention axentd");
    }
    if (status.output.find("offline") == std::string::npos) {
        return fail("status output did not mention offline");
    }
    if (!std::filesystem::exists(log_dir / "axent.log")) {
        return fail("status command did not create axent.log");
    }

    std::filesystem::remove_all(log_dir);
    return 0;
}
