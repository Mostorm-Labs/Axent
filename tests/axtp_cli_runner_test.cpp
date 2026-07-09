#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "axtp_core.hpp"
#include "axent/cli/axtp_cli.hpp"

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

CommandResult run(std::vector<std::string> args)
{
    std::ostringstream output;
    std::ostringstream error;
    CommandResult result;
    result.code = axent::run_axtp_cli(args, "axent", output, error);
    result.output = output.str();
    result.error = error.str();
    return result;
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
    const auto mock = run({"ping"});
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
    const auto result = run({"-c", "audio.getAlgorithmConfig", "-o", "json"});
    require(result.code == 0, "mock dynamic call should succeed");
    require(result.output.find(R"("method":"audio.getAlgorithmConfig")") != std::string::npos,
            "mock call should include method");
    require(result.output.find("noiseSuppression") != std::string::npos,
            "mock call should include configured result");
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
    return 0;
}
