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
#ifdef _WIN32
    const std::string popen_command = "call " + command;
#else
    const std::string& popen_command = command;
#endif
    FILE* pipe = AXENT_POPEN(popen_command.c_str(), "r");
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

    const auto axtp_ping = run_command(axent + " axtp ping");
    if (axtp_ping.code != 0) {
        return fail("axent axtp ping exited with non-zero status");
    }
    if (axtp_ping.output.find("\"ok\":true") == std::string::npos) {
        return fail("axent axtp ping did not report ok");
    }

    const auto axtp_methods = run_command(axent + " axtp list-methods");
    if (axtp_methods.code != 0) {
        return fail("axent axtp list-methods exited with non-zero status");
    }
    if (axtp_methods.output.find("audio.getAlgorithmConfig") == std::string::npos) {
        return fail("axent axtp list-methods did not include generated methods");
    }

    const auto axtp_call = run_command(axent + " axtp -c audio.getAlgorithmConfig -o json");
    if (axtp_call.code != 0) {
        return fail("axent axtp mock call exited with non-zero status");
    }
    if (axtp_call.output.find("noiseSuppression") == std::string::npos) {
        return fail("axent axtp mock call did not include mock result");
    }

    std::filesystem::remove_all(log_dir);
    return 0;
}
