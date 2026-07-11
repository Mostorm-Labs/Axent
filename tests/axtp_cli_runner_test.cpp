#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <streambuf>
#include <vector>

#include "axtp_core.hpp"
#include "axent/tooling/axtp_cli.hpp"
#include <nlohmann/json.hpp>

namespace {

struct CommandResult {
    int code = 0;
    std::string output;
    std::string error;
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

CommandResult run_with(axent::AxtpCliInvocation invocation, std::vector<std::string> args)
{
    std::ostringstream output;
    std::ostringstream error;
    CommandResult result;
    result.code = axent::run_axtp_cli(args, invocation, output, error);
    result.output = output.str();
    result.error = error.str();
    return result;
}

CommandResult run(std::vector<std::string> args)
{
    return run_with({"axtpctl", "axtpctl", "axtpctl"}, std::move(args));
}

bool directory_has_file(const std::filesystem::path& directory)
{
    if (!std::filesystem::is_directory(directory)) {
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            return true;
        }
    }
    return false;
}

std::string to_hex(const axtp::Bytes& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        result.push_back(digits[(byte >> 4) & 0x0f]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

std::string valid_empty_rpc_frame_hex()
{
    axtp::Bytes frame{
        axtp::kAxtpStandardMagic0,
        axtp::kAxtpStandardMagic1,
        axtp::kAxtpVersion1,
        static_cast<axtp::Byte>(axtp::PayloadType::Rpc),
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x01,
    };
    const auto crc = axtp::crc16CcittFalse(frame.data(), frame.size());
    frame.push_back(static_cast<axtp::Byte>((crc >> 8) & 0xff));
    frame.push_back(static_cast<axtp::Byte>(crc & 0xff));
    return to_hex(frame);
}

void test_ping_mock_and_real_transport_status()
{
    const auto defaults = run({"ping"});
    require(defaults.code == 4, "default ping should select the real HID transport");
    require(defaults.output.find(R"("transport":"hid")") != std::string::npos,
            "default transport should be HID");

    const auto mock = run({"-t", "mock", "ping"});
    require(mock.code == 0, "mock ping should succeed");
    require(mock.output.find(R"("ok":true)") != std::string::npos, "mock ping should report ok");

    const auto hid = run({"-t", "hid", "ping"});
    require(hid.code == 4, "real transport ping should be P0-unimplemented");
    require(hid.output.find("real transport ping is not implemented in P0") != std::string::npos,
            "real transport ping should explain status");
}

void test_list_methods()
{
    const auto result = run({"list-methods"});
    require(result.code == 0, "list-methods should succeed");
    require(result.output.find("audio.getAlgorithmConfig") != std::string::npos,
            "list-methods should include generated methods");
}

void test_mock_call_json_output()
{
    const auto result =
        run({"-t", "mock", "-c", "audio.getAlgorithmConfig", "-o", "json"});
    require(result.code == 0, "mock dynamic call should succeed");
    const auto response = nlohmann::json::parse(result.output);
    require(response.size() == 3 && response.contains("sid") && response.contains("op") &&
                response.contains("d"),
            "JSON call should use the AXTP sid/op/d envelope");
    require(response.at("sid").is_string(), "AXTP response sid should be a string");
    require(response.at("op") == static_cast<std::uint8_t>(axtp::RpcOp::RequestResponse),
            "AXTP response should use RequestResponse op");
    const auto& data = response.at("d");
    require(data.at("id") == 1, "AXTP response should preserve the request id");
    require(data.at("status").at("ok") == true,
            "AXTP response status should report success");
    require(data.at("status").at("code") == 0,
            "AXTP success response should use status code zero");
    require(data.at("result").contains("noiseSuppression"),
            "AXTP response should contain the configured result");
    require(!response.contains("method") && !response.contains("methodId") &&
                !response.contains("requestId") && !response.contains("ok"),
            "JSON call should not emit the retired tooling summary fields");
}

void test_inspect_frame_success_and_failures()
{
    const auto valid = run({"inspect", "frame", "--hex", valid_empty_rpc_frame_hex()});
    require(valid.code == 0, "inspect frame should parse a valid frame");
    require(valid.output.find(R"("crcOk":true)") != std::string::npos,
            "inspect frame should report crcOk");

    const auto invalid_hex = run({"inspect", "frame", "--hex", "xyz"});
    require(invalid_hex.code == 2, "inspect frame should reject invalid hex");
    require(invalid_hex.error.find("invalid hex") != std::string::npos,
            "inspect invalid hex error should be clear");

    const auto missing_hex = run({"inspect", "frame"});
    require(missing_hex.code == 2, "inspect frame should require --hex");
    require(missing_hex.error.find("--hex") != std::string::npos,
            "inspect missing --hex error should name option");
}

void test_invalid_option_value()
{
    const auto result = run({"--timeout", "wat", "ping"});
    require(result.code == 2, "invalid timeout should fail");
    require(result.error.find("--timeout") != std::string::npos,
            "invalid timeout error should name option");
}

void test_hid_alias_options_are_accepted()
{
    const auto result = run({
        "--hid-vid",
        "0x0581",
        "--hid-pid",
        "0x2581",
        "--hid-serial",
        "NA20-SERIAL",
        "--hid-report-id",
        "0x05",
        "--hid-input-report-size",
        "255",
        "--hid-output-report-size",
        "255",
        "--hid-read-buffer-size",
        "4096",
        "--hid-max-reports-per-poll",
        "16",
        "-t",
        "mock",
        "ping",
    });
    require(result.code == 0, "--hid-* aliases should be parsed before command dispatch");
    require(result.output.find(R"("ok":true)") != std::string::npos,
            "--hid-* alias parse should still dispatch ping");
}

void test_json_body_file_mutex()
{
    const auto result =
        run({"call", "audio.setAlgorithmConfig", "--json", "{}", "--json-file", "missing.json"});
    require(result.code == 2, "json and json-file together should fail");
    require(result.error.find("--json and --json-file cannot be used together") != std::string::npos,
            "json/json-file mutex error should be clear");
}

void test_invocation_names_and_log_stems()
{
    const auto direct_help = run_with({"axtpctl", "axtpctl", "axtpctl"}, {"--help"});
    require(direct_help.code == 0, "axtpctl help should succeed");
    require(direct_help.output.find("Usage: axtpctl ") != std::string::npos,
            "axtpctl help should use the direct invocation name");
    require(direct_help.output.find("default hid") != std::string::npos,
            "help should document the HID default");
    require(direct_help.output.find("default 0x82") != std::string::npos,
            "help should document the NA20 usage default");
    require(direct_help.output.find("Select one device by HID serial") != std::string::npos,
            "help should document serial as an explicit device selector");
    require(direct_help.output.find("default 1234567890tsyatern") == std::string::npos,
            "help must not expose a device-specific default serial");
    require(direct_help.output.find("default 5000") != std::string::npos,
            "help should document the timeout default");

    const auto alias_help =
        run_with({"axent axtp", "axent", "axent-axtp"}, {"--help"});
    require(alias_help.code == 0, "axent axtp help should succeed");
    require(alias_help.output.find("Usage: axent axtp ") != std::string::npos,
            "alias help should use the compatibility invocation name");

    const auto root = std::filesystem::temp_directory_path() / "axent-axtp-cli-runner-logs";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto direct_call = run_with(
        {"axtpctl", (root / "axtpctl.exe").string(), "axtpctl"},
        {"--log", "-t", "mock", "-c", "audio.getAlgorithmConfig", "-o", "json"});
    require(direct_call.code == 0, "logged axtpctl call should succeed");
    require(directory_has_file(root / "axtpctl-logs"),
            "axtpctl should write to axtpctl-logs");

    const auto alias_call = run_with(
        {"axent axtp", (root / "axent.exe").string(), "axent-axtp"},
        {"--log", "-t", "mock", "-c", "audio.getAlgorithmConfig", "-o", "json"});
    require(alias_call.code == 0, "logged axent axtp call should succeed");
    require(directory_has_file(root / "axent-axtp-logs"),
            "axent axtp should preserve its legacy log directory");

    std::filesystem::remove_all(root);
}

class FailingStreamBuffer final : public std::streambuf {
protected:
    int_type overflow(int_type) override
    {
        return traits_type::eof();
    }
};

void test_exception_prefix_uses_invocation_name()
{
    FailingStreamBuffer buffer;
    std::ostream output(&buffer);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    std::ostringstream error;
    const auto code = axent::run_axtp_cli(
        {"--help"}, {"axtpctl", "axtpctl", "axtpctl"}, output, error);
    require(code == 1, "output stream failure should map to the unexpected-error exit code");
    require(error.str().find("axtpctl:") != std::string::npos,
            "unexpected errors should use the active invocation prefix");
}

void test_firmware_update()
{
#if defined(AXENT_HAS_AXTP_FIRMWARE_PROFILE)
    const auto root = std::filesystem::temp_directory_path() / "axent-axtp-cli-firmware";
    const auto firmware_path = root / "firmware.bin";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream firmware(firmware_path, std::ios::binary);
        firmware << "123456789";
        require(static_cast<bool>(firmware), "firmware fixture should be writable");
    }

    const auto success = run({
        "-t", "mock", "-o", "json", "firmware", "update",
        "--file", firmware_path.string(),
        "--file-id", "application",
        "--target", "main",
        "--package-id", "pkg-1",
        "--version", "1.2.3",
        "--chunk-size", "4",
    });
    require(success.code == 0, "mock firmware update should succeed");
    require(success.output.find(R"("ok":true)") != std::string::npos,
            "firmware output should report success");
    require(success.output.find(R"("md5":"25f9e794323b453885f5181f1b624d0b")") !=
                std::string::npos,
            "firmware output should contain the RFC MD5 vector");
    require(success.output.find(R"("chunks":3)") != std::string::npos,
            "nine bytes should be sent in three mock chunks");
    require(success.output.find(R"("fileId":"application")") != std::string::npos,
            "firmware output should preserve file-id");

    const auto missing_subcommand = run({"firmware"});
    require(missing_subcommand.code == 2, "firmware should require the update subcommand");

    const auto missing_file = run({"firmware", "update"});
    require(missing_file.code == 2, "firmware update should require --file");

    const auto unreadable =
        run({"firmware", "update", "--file", (root / "missing.bin").string()});
    require(unreadable.code == 2, "firmware update should reject an unreadable file");

    const auto invalid_chunk = run(
        {"firmware", "update", "--file", firmware_path.string(), "--chunk-size", "0"});
    require(invalid_chunk.code == 2, "firmware update should reject chunk size zero");

    const auto websocket = run(
        {"-t", "websocket", "firmware", "update", "--file", firmware_path.string()});
    require(websocket.code == 2, "firmware update should reject WebSocket transport");
    require(websocket.error.find("framed-binary") != std::string::npos,
            "WebSocket rejection should explain the transport requirement");

    std::filesystem::remove_all(root);
#else
    const auto unavailable = run({"firmware", "update"});
    require(unavailable.code == 4, "firmware update should be unavailable without its profile");
    require(unavailable.error.find("unavailable") != std::string::npos,
            "missing firmware profile should be reported explicitly");
#endif
}

} // namespace

int main()
{
    test_ping_mock_and_real_transport_status();
    test_list_methods();
    test_mock_call_json_output();
    test_inspect_frame_success_and_failures();
    test_invalid_option_value();
    test_hid_alias_options_are_accepted();
    test_json_body_file_mutex();
    test_invocation_names_and_log_stems();
    test_exception_prefix_uses_invocation_name();
    test_firmware_update();
    return 0;
}
