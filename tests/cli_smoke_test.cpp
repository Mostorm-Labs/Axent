#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    if (argc != 2 && argc != 3) {
        return fail("usage: cli_smoke_test <axent-path> [axtpctl-path]");
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

    if (argc != 3) {
        std::filesystem::remove_all(log_dir);
        return 0;
    }

    const std::string axtpctl = shell_quote(argv[2]);
    const auto direct_help = run_command(axtpctl + " --help");
    if (direct_help.code != 0 ||
        direct_help.output.find("Usage: axtpctl ") == std::string::npos) {
        return fail("axtpctl --help did not use the direct invocation name");
    }
    const auto alias_help = run_command(axent + " axtp --help");
    if (alias_help.code != 0 ||
        alias_help.output.find("Usage: axent axtp ") == std::string::npos) {
        return fail("axent axtp --help did not use the compatibility invocation name");
    }

    const auto axtp_ping = run_command(axent + " axtp -t mock ping");
    if (axtp_ping.code != 0) {
        return fail("axent axtp ping exited with non-zero status");
    }
    if (axtp_ping.output.find("\"ok\":true") == std::string::npos) {
        return fail("axent axtp ping did not report ok");
    }
    const auto direct_ping = run_command(axtpctl + " -t mock ping");
    if (direct_ping.code != 0 || direct_ping.output != axtp_ping.output) {
        return fail("axtpctl ping did not match axent axtp ping");
    }

    const auto axtp_methods = run_command(axent + " axtp list-methods");
    if (axtp_methods.code != 0) {
        return fail("axent axtp list-methods exited with non-zero status");
    }
    if (axtp_methods.output.find("audio.getAlgorithmConfig") == std::string::npos) {
        return fail("axent axtp list-methods did not include generated methods");
    }
    const auto direct_methods = run_command(axtpctl + " list-methods");
    if (direct_methods.code != 0 || direct_methods.output != axtp_methods.output) {
        return fail("axtpctl list-methods did not match axent axtp list-methods");
    }

    const auto axtp_call =
        run_command(axent + " axtp -t mock -c audio.getAlgorithmConfig -o json");
    if (axtp_call.code != 0) {
        return fail("axent axtp mock call exited with non-zero status");
    }
    if (axtp_call.output.find("noiseSuppression") == std::string::npos) {
        return fail("axent axtp mock call did not include mock result");
    }
    if (axtp_call.output.find("\"op\":8") == std::string::npos ||
        axtp_call.output.find("\"status\":{\"code\":0,\"ok\":true}") ==
            std::string::npos) {
        return fail("axent axtp mock call did not use the AXTP response envelope");
    }
    const auto direct_call =
        run_command(axtpctl + " -t mock -c audio.getAlgorithmConfig -o json");
    if (direct_call.code != 0 || direct_call.output != axtp_call.output) {
        return fail("axtpctl mock call did not match axent axtp mock call");
    }

    const auto firmware_dir =
        std::filesystem::temp_directory_path() / "axent-axtpctl-smoke-firmware";
    const auto firmware_path = firmware_dir / "firmware.bin";
    std::filesystem::remove_all(firmware_dir);
    std::filesystem::create_directories(firmware_dir);
    {
        std::ofstream firmware(firmware_path, std::ios::binary);
        firmware << "123456789";
        if (!firmware) {
            return fail("failed to create firmware smoke fixture");
        }
    }
    const std::string firmware_args =
        " -t mock -o json firmware update --file " + shell_quote(firmware_path.string()) +
        " --file-id firmware --chunk-size 4";
    const auto alias_firmware = run_command(axent + " axtp" + firmware_args);
    const auto direct_firmware = run_command(axtpctl + firmware_args);
    if (alias_firmware.code != 0 || direct_firmware.code != 0 ||
        direct_firmware.output != alias_firmware.output) {
        return fail("axtpctl firmware update did not match axent axtp firmware update");
    }
    if (direct_firmware.output.find("25f9e794323b453885f5181f1b624d0b") == std::string::npos) {
        return fail("firmware update did not report the expected MD5");
    }

    std::filesystem::remove_all(log_dir);
    std::filesystem::remove_all(firmware_dir);
    return 0;
}
