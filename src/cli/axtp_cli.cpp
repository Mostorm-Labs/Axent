#include "axent/cli/axtp_cli.hpp"

#include <ostream>

#if !defined(AXENT_HAS_AXTP_DIRECT_CLI)

namespace axent {

int run_axtp_cli(const std::vector<std::string>&,
                 const std::string&,
                 std::ostream&,
                 std::ostream& err)
{
    err << "AXTP direct CLI unavailable in this build\n";
    return 4;
}

} // namespace axent

#else

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "axtp_core.hpp"
#include "axtp_sdk.hpp"
#include "json_rpc/method_registry_json.hpp"

#include "core/runtime/testing/mock_transport.hpp"
#include "transports/hidapi/hid_transport.hpp"
#include "transports/tcp/native/tcp_transport.hpp"
#include "transports/websocket/ix/websocket_transport.hpp"

namespace axent {
namespace {

constexpr std::uint32_t k_default_hid_vendor_id = 0x0581;
constexpr std::uint32_t k_default_hid_product_id = 0x2581;
constexpr std::uint32_t k_default_hid_usage_page = 0x81;

enum class OutputFormat {
    Pretty,
    Json,
    Hex,
    File,
};

struct CliOptions {
    std::string transport = "mock";
    std::string executable_path;
    std::string endpoint;
    std::string host = "127.0.0.1";
    std::string path;
    std::string serial_number;
    std::string wire = "websocket-json-rpc";
    std::string encoding = "json";
    std::string registry_file;
    std::string output = "pretty";
    std::string sid = "00000000";
    std::optional<std::string> shortcut_method;
    std::optional<std::string> json;
    std::optional<std::string> json_file;
    std::optional<std::uint32_t> random_seed;
    std::optional<std::uint32_t> port;
    std::optional<std::uint32_t> vid = k_default_hid_vendor_id;
    std::optional<std::uint32_t> pid = k_default_hid_product_id;
    std::optional<std::uint32_t> usage_page = k_default_hid_usage_page;
    std::optional<std::uint32_t> usage;
    std::uint32_t timeout_ms = 5000;
    std::uint32_t report_id = 0x05;
    std::uint32_t input_report_size = 255;
    std::uint32_t read_buffer_size = 4096;
    std::uint32_t output_report_size = 255;
    std::uint32_t max_reports_per_poll = 16;
    bool no_app_ready = false;
    bool log_enabled = false;
    bool log_body = false;
    bool verbose = false;
    std::vector<std::string> command;
};

struct HidOpenOptions {
    std::optional<std::uint32_t> vendor_id;
    std::optional<std::uint32_t> product_id;
    std::optional<std::uint32_t> usage_page;
    std::optional<std::uint32_t> usage;
    std::string device_path;
    std::string serial_number;
    std::uint32_t report_id = 0x05;
    std::uint32_t input_report_size = 255;
    std::uint32_t read_buffer_size = 4096;
    std::uint32_t output_report_size = 255;
    std::uint32_t max_reports_per_poll = 16;
    bool use_read_thread = true;
    std::uint32_t read_thread_timeout_ms = 1000;
    std::function<void(const axtp::HidReportTrace&)> report_trace;
};

struct TransportOpenOptions {
    std::string kind = "mock";
    std::string host = "127.0.0.1";
    std::optional<std::uint32_t> port;
    HidOpenOptions hid;
};

struct TransportBundle {
    std::unique_ptr<axtp::ITransport> transport;
    axtp::HidTransport* hid_transport = nullptr;
};

bool is_option(const std::string& text)
{
    return !text.empty() && text[0] == '-';
}

void print_usage(std::ostream& out)
{
    out << "Usage: axent axtp [options] <command>\n"
        << "       axent axtp -c <method> [--json JSON|--json-file FILE]\n"
        << "\n"
        << "Options:\n"
        << "  -c, --command <method>       Call an AXTP method by name\n"
        << "  -j, --json <json>            JSON params for the method call\n"
        << "  -f, --json-file <path>       Read JSON params from file\n"
        << "  -t, --transport <kind>       Select transport: hid, tcp, websocket, mock\n"
        << "  -o, --output <format>        Output format: pretty, json, hex, file\n"
        << "      --host <host>            TCP or WebSocket bind host\n"
        << "      --port <port>            TCP or WebSocket port\n"
        << "      --vid, --hid-vid <hex>   HID vendor id, default 0x0581\n"
        << "      --pid, --hid-pid <hex>   HID product id, default 0x2581\n"
        << "      --usage-page <hex|dec>   HID usage page filter, default 0x81\n"
        << "      --usage <hex|dec>        HID usage filter\n"
        << "      --path, --hid-path <path> HID path from list-hid\n"
        << "      --serial, --hid-serial <value> HID serial value for VID/PID open\n"
        << "      --endpoint <value>       Transport endpoint value\n"
        << "      --wire <mode>            Wire mode: framed-binary, websocket-json-rpc\n"
        << "      --encoding <format>      RPC encoding: json, tlv, raw\n"
        << "      --registry-file <file>   Load an additional method registry JSON file\n"
        << "      --timeout <ms>           RPC timeout in milliseconds\n"
        << "      --report-id, --hid-report-id <id> HID report id, default 0x05\n"
        << "      --input-report-size, --hid-input-report-size <n> HIDAPI input buffer bytes incl report id\n"
        << "      --read-buffer-size, --hid-read-buffer-size <n> HIDAPI read buffer bytes\n"
        << "      --output-report-size, --hid-output-report-size <n> HIDAPI output buffer bytes incl report id\n"
        << "      --max-reports-per-poll, --hid-max-reports-per-poll <n> HID read limit per poll\n"
        << "      --no-app-ready           Skip app-ready before a HID call\n"
        << "      --random-seed <hex|dec>  Override Identify randomSeed for app-ready\n"
        << "      --sid <value>            JSON envelope sid when --no-app-ready is used\n"
        << "      --log                    Write a local AXTP CLI log beside the executable\n"
        << "      --no-log                 Disable local file logging\n"
        << "      --log-body               Include request/response bodies in local log\n"
        << "      --verbose                Enable verbose diagnostics\n"
        << "  -h, --help                   Show this help\n"
        << "\n"
        << "Commands:\n"
        << "  call [method] [--method-id ID] [--json JSON|--json-file FILE|--tlv-hex HEX|--raw-hex HEX]\n"
        << "  capability methods\n"
        << "  list-methods\n"
        << "  handshake\n"
        << "  list-hid [--vid VID --pid PID --usage-page PAGE --usage USAGE]\n"
        << "  read-hid [--path PATH] [--timeout MS]\n"
        << "  ping\n"
        << "  inspect frame --hex HEX\n";
}

std::optional<std::string> option_value(const std::vector<std::string>& args,
                                        const std::string& name)
{
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) {
            return args[i + 1];
        }
    }
    return std::nullopt;
}

std::optional<std::string> first_option_value(const std::vector<std::string>& args,
                                              const std::vector<std::string>& names)
{
    for (const auto& name : names) {
        if (const auto value = option_value(args, name)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> read_text_file(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<axtp::Bytes> read_binary_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return axtp::Bytes(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool write_binary_file(const std::string& path, const axtp::Bytes& bytes)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

std::optional<std::uint32_t> parse_uint32(std::string_view text)
{
    if (text.empty()) {
        return std::nullopt;
    }
    std::size_t offset = 0;
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        offset = 2;
        base = 16;
    }
    if (offset >= text.size()) {
        return std::nullopt;
    }
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(std::string(text.substr(offset)), &consumed, base);
        if (consumed == text.size() - offset &&
            value <= std::numeric_limits<std::uint32_t>::max()) {
            return static_cast<std::uint32_t>(value);
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    }
    return -1;
}

std::optional<axtp::Bytes> parse_hex(std::string text)
{
    std::string compact;
    compact.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto ch = text[i];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
            continue;
        }
        if (ch == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            ++i;
            continue;
        }
        compact.push_back(ch);
    }
    if (compact.size() % 2 != 0) {
        return std::nullopt;
    }

    axtp::Bytes bytes;
    bytes.reserve(compact.size() / 2);
    for (std::size_t i = 0; i < compact.size(); i += 2) {
        const auto hi = hex_nibble(compact[i]);
        const auto lo = hex_nibble(compact[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<axtp::Byte>((hi << 4) | lo));
    }
    return bytes;
}

std::string to_hex(const axtp::Bytes& bytes)
{
    static constexpr char k_digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        out.push_back(k_digits[(byte >> 4) & 0x0F]);
        out.push_back(k_digits[byte & 0x0F]);
    }
    return out;
}

std::string to_hex_byte(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << (value & 0xFFU);
    return out.str();
}

std::string to_hex_id(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
        << value;
    return out.str();
}

std::string to_hex_u32(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
        << value;
    return out.str();
}

std::string error_name(axtp::ErrorCode code)
{
    const auto* descriptor = axtp::RegistryLookup::errorByCode(code);
    return descriptor != nullptr ? descriptor->name : "UNKNOWN_ERROR";
}

OutputFormat parse_output_format(const std::string& value)
{
    if (value == "json") {
        return OutputFormat::Json;
    }
    if (value == "hex") {
        return OutputFormat::Hex;
    }
    if (value == "file") {
        return OutputFormat::File;
    }
    return OutputFormat::Pretty;
}

std::tm local_time(std::time_t value)
{
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::string format_time(const char* pattern)
{
    const auto now = std::time(nullptr);
    const auto tm = local_time(now);
    std::ostringstream out;
    out << std::put_time(&tm, pattern);
    return out.str();
}

std::uint32_t process_id()
{
#if defined(_WIN32)
    return static_cast<std::uint32_t>(_getpid());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

class LocalLogger {
public:
    LocalLogger(std::filesystem::path executable_path,
                std::string stem,
                bool enabled,
                bool include_body)
    {
        open(std::move(executable_path), std::move(stem), enabled, include_body);
    }

    void open(std::filesystem::path executable_path,
              std::string stem,
              bool enabled,
              bool include_body)
    {
        include_body_ = include_body;
        file_stream_.reset();
        path_.clear();
        if (!enabled) {
            return;
        }

        std::error_code ec;
        if (executable_path.empty()) {
            executable_path = std::filesystem::current_path(ec) / stem;
        }
        if (executable_path.is_relative()) {
            executable_path = std::filesystem::absolute(executable_path, ec);
        }

        const auto dir = executable_path.parent_path() / (stem + "-logs");
        std::filesystem::create_directories(dir, ec);
        std::ostringstream name;
        name << stem << "-" << format_time("%Y%m%d-%H%M%S") << "-" << process_id() << ".log";
        path_ = dir / name.str();

        auto stream = std::make_unique<std::ofstream>(path_, std::ios::out | std::ios::app);
        if (*stream) {
            file_stream_ = std::move(stream);
            write("log opened");
        } else {
            path_.clear();
        }
    }

    void write(std::string_view line)
    {
        if (!file_stream_) {
            return;
        }
        *file_stream_ << format_time("%Y-%m-%d %H:%M:%S") << " " << line << "\n";
        file_stream_->flush();
    }

    bool include_body() const
    {
        return include_body_;
    }

private:
    bool include_body_ = false;
    std::filesystem::path path_;
    std::unique_ptr<std::ofstream> file_stream_;
};

bool parse_axtp_options(const std::vector<std::string>& args, CliOptions& options, std::ostream& err)
{
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string arg = args[i];
        auto require_value = [&](const std::string& name) -> std::optional<std::string> {
            if (i + 1 >= args.size()) {
                err << "missing value for " << name << "\n";
                return std::nullopt;
            }
            return args[++i];
        };

        if (arg == "--help" || arg == "-h") {
            options.command = {"help"};
            return true;
        }
        if (arg == "--verbose") {
            options.verbose = true;
            continue;
        }
        if (arg == "--log") {
            options.log_enabled = true;
            continue;
        }
        if (arg == "--no-log") {
            options.log_enabled = false;
            options.log_body = false;
            continue;
        }
        if (arg == "--log-body") {
            options.log_enabled = true;
            options.log_body = true;
            continue;
        }
        if (arg == "--no-app-ready") {
            options.no_app_ready = true;
            continue;
        }
        if (arg == "-c" || arg == "--command") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            options.shortcut_method = *value;
            continue;
        }
        if (arg == "-j" || arg == "--json") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            options.json = *value;
            continue;
        }
        if (arg == "-f" || arg == "--json-file") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            options.json_file = *value;
            continue;
        }
        if (arg == "-t" || arg == "--transport") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            options.transport = *value;
            continue;
        }
        if (arg == "-o" || arg == "--output") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            options.output = *value;
            continue;
        }
        if (arg == "--endpoint" || arg == "--wire" || arg == "--registry-file" ||
            arg == "--encoding" || arg == "--timeout" || arg == "--host" || arg == "--port" ||
            arg == "--vid" || arg == "--hid-vid" || arg == "--pid" || arg == "--hid-pid" ||
            arg == "--path" || arg == "--hid-path" || arg == "--serial" || arg == "--hid-serial" ||
            arg == "--usage-page" || arg == "--usagepage" ||
            arg == "--hid-usage-page" || arg == "--hid-usagepage" || arg == "--usage" ||
            arg == "--hid-usage" || arg == "--sid" || arg == "--random-seed" ||
            arg == "--report-id" || arg == "--hid-report-id" ||
            arg == "--input-report-size" || arg == "--hid-input-report-size" ||
            arg == "--read-buffer-size" || arg == "--hid-read-buffer-size" ||
            arg == "--output-report-size" || arg == "--hid-output-report-size" ||
            arg == "--max-reports-per-poll" || arg == "--hid-max-reports-per-poll") {
            const auto value = require_value(arg);
            if (!value.has_value()) {
                return false;
            }
            if (arg == "--endpoint") {
                options.endpoint = *value;
            } else if (arg == "--wire") {
                options.wire = *value;
            } else if (arg == "--encoding") {
                options.encoding = *value;
            } else if (arg == "--registry-file") {
                options.registry_file = *value;
            } else if (arg == "--timeout") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value()) {
                    err << "invalid --timeout\n";
                    return false;
                }
                options.timeout_ms = *parsed;
            } else if (arg == "--host") {
                options.host = *value;
            } else if (arg == "--port") {
                options.port = parse_uint32(*value);
                if (!options.port.has_value() || *options.port > 65535U) {
                    err << "invalid --port\n";
                    return false;
                }
            } else if (arg == "--vid" || arg == "--hid-vid") {
                options.vid = parse_uint32(*value);
                if (!options.vid.has_value() || *options.vid > 0xFFFFU) {
                    err << "invalid --vid\n";
                    return false;
                }
            } else if (arg == "--pid" || arg == "--hid-pid") {
                options.pid = parse_uint32(*value);
                if (!options.pid.has_value() || *options.pid > 0xFFFFU) {
                    err << "invalid --pid\n";
                    return false;
                }
            } else if (arg == "--usage-page" || arg == "--usagepage" ||
                       arg == "--hid-usage-page" || arg == "--hid-usagepage") {
                options.usage_page = parse_uint32(*value);
                if (!options.usage_page.has_value() || *options.usage_page > 0xFFFFU) {
                    err << "invalid --usage-page\n";
                    return false;
                }
            } else if (arg == "--usage" || arg == "--hid-usage") {
                options.usage = parse_uint32(*value);
                if (!options.usage.has_value() || *options.usage > 0xFFFFU) {
                    err << "invalid --usage\n";
                    return false;
                }
            } else if (arg == "--path" || arg == "--hid-path") {
                options.path = *value;
            } else if (arg == "--serial" || arg == "--hid-serial") {
                options.serial_number = *value;
            } else if (arg == "--sid") {
                options.sid = *value;
            } else if (arg == "--random-seed") {
                options.random_seed = parse_uint32(*value);
                if (!options.random_seed.has_value()) {
                    err << "invalid --random-seed\n";
                    return false;
                }
            } else if (arg == "--report-id" || arg == "--hid-report-id") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value() || *parsed > 0xFFU) {
                    err << "invalid --report-id\n";
                    return false;
                }
                options.report_id = *parsed;
            } else if (arg == "--input-report-size" || arg == "--hid-input-report-size") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value() || *parsed == 0U) {
                    err << "invalid --input-report-size\n";
                    return false;
                }
                options.input_report_size = *parsed;
            } else if (arg == "--read-buffer-size" || arg == "--hid-read-buffer-size") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value() || *parsed == 0U) {
                    err << "invalid --read-buffer-size\n";
                    return false;
                }
                options.read_buffer_size = *parsed;
            } else if (arg == "--output-report-size" || arg == "--hid-output-report-size") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value() || *parsed == 0U) {
                    err << "invalid --output-report-size\n";
                    return false;
                }
                options.output_report_size = *parsed;
            } else if (arg == "--max-reports-per-poll" || arg == "--hid-max-reports-per-poll") {
                const auto parsed = parse_uint32(*value);
                if (!parsed.has_value() || *parsed == 0U) {
                    err << "invalid --max-reports-per-poll\n";
                    return false;
                }
                options.max_reports_per_poll = *parsed;
            }
            continue;
        }

        options.command.push_back(arg);
    }

    if (options.shortcut_method.has_value()) {
        if (options.shortcut_method->empty()) {
            err << "method name must not be empty\n";
            return false;
        }
        if (!options.command.empty()) {
            err << "-c/--command cannot be combined with an explicit command\n";
            return false;
        }
        options.command = {"call", *options.shortcut_method};
    }
    return true;
}

std::uint16_t read_u16_be(const axtp::Bytes& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] << 8) |
           static_cast<std::uint16_t>(bytes[offset + 1]);
}

std::string payload_type_name(std::uint8_t type)
{
    if (type == static_cast<std::uint8_t>(axtp::PayloadType::Control)) {
        return "control";
    }
    if (type == static_cast<std::uint8_t>(axtp::PayloadType::Rpc)) {
        return "rpc";
    }
    if (type == static_cast<std::uint8_t>(axtp::PayloadType::Stream)) {
        return "stream";
    }
    return "unknown";
}

int inspect_frame(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    const auto hex = option_value(args, "--hex");
    if (!hex.has_value()) {
        err << "inspect frame requires --hex\n";
        return 2;
    }
    const auto bytes = parse_hex(*hex);
    if (!bytes.has_value()) {
        err << "invalid hex\n";
        return 2;
    }
    if (bytes->size() < axtp::kStandardFrameHeaderSize) {
        err << "frame too short\n";
        return 2;
    }

    auto object = nlohmann::json::object();
    object["magic"] =
        ((*bytes)[0] == axtp::kAxtpStandardMagic0 && (*bytes)[1] == axtp::kAxtpStandardMagic1)
            ? "AX"
            : "invalid";
    object["version"] = (*bytes)[2];
    object["payloadType"] = payload_type_name((*bytes)[3]);
    const auto payload_length = read_u16_be(*bytes, 4);
    object["payloadLength"] = payload_length;
    object["sourceId"] = (*bytes)[6];
    object["destinationId"] = (*bytes)[7];
    object["messageId"] = read_u16_be(*bytes, 8);
    object["frameIndex"] = (*bytes)[10];
    object["frameCount"] = (*bytes)[11];

    const auto total = static_cast<std::size_t>(axtp::kStandardFrameHeaderSize) + payload_length +
                       axtp::kStandardFrameCrcSize;
    object["complete"] = bytes->size() >= total;
    if (bytes->size() >= total) {
        const auto expected = read_u16_be(*bytes, total - axtp::kStandardFrameCrcSize);
        const auto actual =
            axtp::crc16CcittFalse(bytes->data(), total - axtp::kStandardFrameCrcSize);
        object["crcExpected"] = expected;
        object["crcActual"] = actual;
        object["crcOk"] = expected == actual;
    }

    out << object.dump() << "\n";
    return 0;
}

nlohmann::json parse_json_value_or_string(const axtp::Bytes& bytes)
{
    if (bytes.empty()) {
        return nullptr;
    }
    const std::string text(bytes.begin(), bytes.end());
    try {
        return nlohmann::json::parse(text);
    } catch (const std::exception&) {
        return text;
    }
}

bool validate_json(std::string_view text, std::ostream& err)
{
    try {
        const auto parsed = nlohmann::json::parse(text);
        (void)parsed;
        return true;
    } catch (const std::exception& ex) {
        err << "invalid JSON params: " << ex.what() << "\n";
        return false;
    }
}

void print_json_object(const nlohmann::json& object, OutputFormat, std::ostream& out)
{
    out << object.dump() << "\n";
}

void install_mock_handlers(axtp::sdk::AxtpClient& client)
{
    client.registerMethod(static_cast<std::uint32_t>(axtp::MethodId::AudioGetAlgorithmConfig),
                          [](const axtp::RpcPayload&) {
                              const std::string body =
                                  R"({"noiseSuppression":{"enabled":true,"level":3},"echoCancellation":{"enabled":true}})";
                              return axtp::Bytes(body.begin(), body.end());
                          });
    client.registerMethod(static_cast<std::uint32_t>(axtp::MethodId::AudioSetAlgorithmConfig),
                          [](const axtp::RpcPayload&) { return axtp::Bytes{}; });
    client.registerMethod(static_cast<std::uint32_t>(axtp::MethodId::AudioGetAlgorithmCapabilities),
                          [](const axtp::RpcPayload&) {
                              const std::string body =
                                  R"({"algorithms":{"noiseSuppression":{"level":{"min":0,"max":5}}}})";
                              return axtp::Bytes(body.begin(), body.end());
                          });
    client.registerMethod(static_cast<std::uint32_t>(axtp::MethodId::AudioResetAlgorithmConfig),
                          [](const axtp::RpcPayload&) { return axtp::Bytes{}; });
}

bool has_hid_target(const HidOpenOptions& options)
{
    return !options.device_path.empty() ||
           (options.vendor_id.has_value() && options.product_id.has_value());
}

axtp::HidTransportOptions make_hid_transport_options(const HidOpenOptions& options)
{
    axtp::HidTransportOptions hid;
    hid.vendorId = static_cast<std::uint16_t>(options.vendor_id.value_or(0));
    hid.productId = static_cast<std::uint16_t>(options.product_id.value_or(0));
    hid.usagePage = static_cast<std::uint16_t>(options.usage_page.value_or(0));
    hid.usage = static_cast<std::uint16_t>(options.usage.value_or(0));
    hid.devicePath = options.device_path;
    hid.serialNumber = options.serial_number;
    hid.reportId = static_cast<std::uint8_t>(options.report_id);
    hid.inputReportSize = static_cast<std::size_t>(options.input_report_size);
    hid.readBufferSize = static_cast<std::size_t>(options.read_buffer_size);
    hid.outputReportSize = static_cast<std::size_t>(options.output_report_size);
    hid.maxReportsPerPoll = static_cast<std::size_t>(options.max_reports_per_poll);
    hid.useReadThread = options.use_read_thread;
    hid.readThreadTimeoutMs = options.read_thread_timeout_ms;
    hid.reportTrace = options.report_trace;
    return hid;
}

bool matches_hid_device_filters(const axtp::HidDeviceInfo& device, const HidOpenOptions& options)
{
    if (options.usage_page.value_or(0) != 0 && device.usagePage != *options.usage_page) {
        return false;
    }
    if (options.usage.value_or(0) != 0 && device.usage != *options.usage) {
        return false;
    }
    return true;
}

std::vector<axtp::HidDeviceInfo> list_hid_devices(const HidOpenOptions& options)
{
    const auto all_devices =
        axtp::enumerateHidDevices(static_cast<std::uint16_t>(options.vendor_id.value_or(0)),
                                  static_cast<std::uint16_t>(options.product_id.value_or(0)));
    std::vector<axtp::HidDeviceInfo> devices;
    for (const auto& device : all_devices) {
        if (matches_hid_device_filters(device, options)) {
            devices.push_back(device);
        }
    }
    return devices;
}

void print_hid_devices(const HidOpenOptions& options, OutputFormat format, std::ostream& out)
{
    const auto devices = list_hid_devices(options);
    if (format == OutputFormat::Json) {
        auto json = nlohmann::json::array();
        for (const auto& device : devices) {
            auto item = nlohmann::json::object();
            item["path"] = device.path;
            item["vendorId"] = device.vendorId;
            item["productId"] = device.productId;
            item["releaseNumber"] = device.releaseNumber;
            item["serialNumber"] = device.serialNumber;
            item["manufacturer"] = device.manufacturer;
            item["product"] = device.product;
            item["usagePage"] = device.usagePage;
            item["usage"] = device.usage;
            item["interfaceNumber"] = device.interfaceNumber;
            item["busType"] = device.busType;
            json.push_back(std::move(item));
        }
        out << json.dump() << "\n";
        return;
    }

    for (const auto& device : devices) {
        out << "path=" << device.path
            << " vid=" << to_hex_id(device.vendorId)
            << " pid=" << to_hex_id(device.productId)
            << " serial=" << (device.serialNumber.empty() ? "<none>" : device.serialNumber)
            << " manufacturer=" << (device.manufacturer.empty() ? "<none>" : device.manufacturer)
            << " product=" << (device.product.empty() ? "<none>" : device.product)
            << " usagePage=" << to_hex_id(device.usagePage)
            << " usage=" << to_hex_id(device.usage)
            << " interface=" << device.interfaceNumber
            << " bus=" << (device.busType.empty() ? "<none>" : device.busType)
            << "\n";
    }
}

TransportBundle make_transport(const TransportOpenOptions& options)
{
    TransportBundle bundle;
    if (options.kind == "mock") {
        bundle.transport = std::make_unique<axtp::MockTransport>();
        return bundle;
    }
    if (options.kind == "tcp") {
        bundle.transport = std::make_unique<axtp::TcpTransport>(
            static_cast<std::uint16_t>(options.port.value_or(0)), options.host.c_str());
        return bundle;
    }
    if (options.kind == "websocket" || options.kind == "ws") {
        bundle.transport = std::make_unique<axtp::WebSocketTransport>(
            static_cast<std::uint16_t>(options.port.value_or(0)), options.host.c_str());
        return bundle;
    }
    if (options.kind == "hid" || options.kind == "hidapi") {
        auto transport = std::make_unique<axtp::HidTransport>(make_hid_transport_options(options.hid));
        bundle.hid_transport = transport.get();
        bundle.transport = std::move(transport);
        return bundle;
    }
    return bundle;
}

HidOpenOptions hid_options_from_cli(const CliOptions& options,
                                    std::function<void(const axtp::HidReportTrace&)> trace = {})
{
    HidOpenOptions hid;
    hid.vendor_id = options.vid;
    hid.product_id = options.pid;
    hid.usage_page = options.usage_page;
    hid.usage = options.usage;
    hid.device_path = options.path;
    hid.serial_number = options.serial_number;
    hid.report_id = options.report_id;
    hid.input_report_size = options.input_report_size;
    hid.read_buffer_size = options.read_buffer_size;
    hid.output_report_size = options.output_report_size;
    hid.max_reports_per_poll = options.max_reports_per_poll;
    hid.use_read_thread = true;
    hid.read_thread_timeout_ms = 1000;
    hid.report_trace = std::move(trace);
    return hid;
}

TransportOpenOptions transport_options_from_cli(
    const CliOptions& options,
    std::function<void(const axtp::HidReportTrace&)> trace = {})
{
    TransportOpenOptions transport;
    transport.kind = options.transport;
    transport.host = options.host;
    transport.port = options.port;
    transport.hid = hid_options_from_cli(options, std::move(trace));
    return transport;
}

bool is_hid_transport(const CliOptions& options)
{
    return options.transport == "hid" || options.transport == "hidapi";
}

bool attach_transport(const CliOptions& options,
                      axtp::sdk::AxtpClient& client,
                      std::function<void(const axtp::HidReportTrace&)> trace,
                      std::ostream& err)
{
    auto bundle = make_transport(transport_options_from_cli(options, std::move(trace)));
    if (!bundle.transport) {
        err << "unsupported transport: " << options.transport << "\n";
        return false;
    }
    client.attachTransport(std::move(bundle.transport));
    return client.isConnected();
}

std::string hid_trace_kind_name(axtp::HidReportTraceKind kind)
{
    switch (kind) {
    case axtp::HidReportTraceKind::ReadReport:
        return "read-report";
    case axtp::HidReportTraceKind::ReadTimeout:
        return "read-timeout";
    case axtp::HidReportTraceKind::ReadError:
        return "read-error";
    case axtp::HidReportTraceKind::WriteFrame:
        return "write-frame";
    case axtp::HidReportTraceKind::WriteReport:
        return "write-report";
    case axtp::HidReportTraceKind::WriteError:
        return "write-error";
    case axtp::HidReportTraceKind::AcceptedReport:
        return "accepted-report";
    case axtp::HidReportTraceKind::DroppedReportId:
        return "dropped-report-id";
    }
    return "unknown";
}

std::string hid_trace_line(const axtp::HidReportTrace& trace, bool include_body)
{
    std::ostringstream out;
    out << "hid " << hid_trace_kind_name(trace.kind)
        << " size=" << trace.size
        << " reportId=" << to_hex_byte(trace.reportId);
    if (trace.expectedReportId != 0) {
        out << " expectedReportId=" << to_hex_byte(trace.expectedReportId);
    }
    if (trace.timeoutMs != 0) {
        out << " timeoutMs=" << trace.timeoutMs;
    }
    if (!trace.message.empty()) {
        out << " message=" << trace.message;
    }
    if (include_body && trace.data != nullptr && trace.size > 0) {
        axtp::Bytes body(trace.data, trace.data + trace.size);
        out << " data=" << to_hex(body);
    }
    return out.str();
}

void print_hid_trace(const axtp::HidReportTrace& trace,
                     OutputFormat format,
                     bool include_body,
                     std::ostream& out)
{
    const bool has_bytes = trace.data != nullptr && trace.size > 0;
    if (format == OutputFormat::Json) {
        auto object = nlohmann::json::object();
        object["kind"] = hid_trace_kind_name(trace.kind);
        object["size"] = trace.size;
        object["reportId"] = trace.reportId;
        object["expectedReportId"] = trace.expectedReportId;
        object["timeoutMs"] = trace.timeoutMs;
        object["message"] = trace.message;
        if (has_bytes && include_body) {
            axtp::Bytes body(trace.data, trace.data + trace.size);
            object["hex"] = to_hex(body);
        }
        out << object.dump() << "\n";
        return;
    }
    if (trace.kind == axtp::HidReportTraceKind::ReadReport ||
        trace.kind == axtp::HidReportTraceKind::AcceptedReport ||
        trace.kind == axtp::HidReportTraceKind::DroppedReportId ||
        trace.kind == axtp::HidReportTraceKind::ReadError ||
        trace.kind == axtp::HidReportTraceKind::WriteError) {
        out << hid_trace_line(trace, include_body) << "\n";
    }
}

axtp::RpcEncoding encoding_from_name(const std::string& name)
{
    if (name == "tlv" || name == "raw") {
        return axtp::jsonBinaryRpcEncoding();
    }
    return axtp::RpcEncoding::Json;
}

bool build_call_body(const CliOptions& options,
                     const std::vector<std::string>& args,
                     axtp::RpcEncoding& encoding,
                     axtp::Bytes& body,
                     std::ostream& err)
{
    const auto command_json = first_option_value(args, {"--json", "-j", "--params"});
    const auto command_json_file = first_option_value(args, {"--json-file", "-f", "--params-file"});
    const bool has_json = options.json.has_value() || command_json.has_value();
    const bool has_json_file = options.json_file.has_value() || command_json_file.has_value();
    if (has_json && has_json_file) {
        err << "--json and --json-file cannot be used together\n";
        return false;
    }

    encoding = encoding_from_name(options.encoding);
    body.assign({'{', '}'});

    if (has_json) {
        const auto text = options.json.value_or(*command_json);
        if (!validate_json(text, err)) {
            return false;
        }
        body.assign(text.begin(), text.end());
        encoding = axtp::RpcEncoding::Json;
        return true;
    }
    if (has_json_file) {
        const auto path = options.json_file.value_or(*command_json_file);
        const auto contents = read_text_file(path);
        if (!contents.has_value()) {
            err << "failed to read JSON params file: " << path << "\n";
            return false;
        }
        if (!validate_json(*contents, err)) {
            return false;
        }
        body.assign(contents->begin(), contents->end());
        encoding = axtp::RpcEncoding::Json;
        return true;
    }
    if (const auto hex = option_value(args, "--tlv-hex")) {
        auto parsed = parse_hex(*hex);
        if (!parsed.has_value()) {
            err << "invalid --tlv-hex\n";
            return false;
        }
        body = std::move(*parsed);
        encoding = axtp::jsonBinaryRpcEncoding();
        return true;
    }
    if (const auto file = option_value(args, "--tlv-file")) {
        const auto contents = read_binary_file(*file);
        if (!contents.has_value()) {
            err << "failed to read tlv file: " << *file << "\n";
            return false;
        }
        body = *contents;
        encoding = axtp::jsonBinaryRpcEncoding();
        return true;
    }
    if (const auto hex = option_value(args, "--raw-hex")) {
        auto parsed = parse_hex(*hex);
        if (!parsed.has_value()) {
            err << "invalid --raw-hex\n";
            return false;
        }
        body = std::move(*parsed);
        encoding = axtp::jsonBinaryRpcEncoding();
        return true;
    }
    if (const auto file = option_value(args, "--raw-file")) {
        const auto contents = read_binary_file(*file);
        if (!contents.has_value()) {
            err << "failed to read raw file: " << *file << "\n";
            return false;
        }
        body = *contents;
        encoding = axtp::jsonBinaryRpcEncoding();
        return true;
    }
    return true;
}

void print_app_ready_trace(const axtp::sdk::AppReadyTraceEvent& event,
                           bool include_body,
                           bool verbose,
                           std::ostream& err)
{
    if (!verbose) {
        return;
    }
    err << "APP_READY " << event.stage << ":" << event.action
        << " status=" << error_name(event.statusCode);
    if (event.controlId != 0) {
        err << " controlId=" << event.controlId;
    }
    if (!event.sid.empty()) {
        err << " sid=" << event.sid;
    }
    if (event.hasRandomSeed) {
        err << " randomSeed=" << to_hex_u32(event.randomSeed);
    }
    if (!event.detail.empty()) {
        err << " detail=" << event.detail;
    }
    if (include_body && !event.bodyText.empty()) {
        err << " body=" << event.bodyText;
    }
    err << "\n";
}

std::string app_ready_trace_line(const axtp::sdk::AppReadyTraceEvent& event, bool include_body)
{
    std::ostringstream out;
    out << "app-ready stage=" << event.stage
        << " action=" << event.action
        << " status=" << error_name(event.statusCode);
    if (event.controlId != 0) {
        out << " controlId=" << event.controlId;
    }
    if (!event.sid.empty()) {
        out << " sid=" << event.sid;
    }
    if (event.hasRandomSeed) {
        out << " randomSeed=" << to_hex_u32(event.randomSeed);
    }
    if (!event.detail.empty()) {
        out << " detail=" << event.detail;
    }
    if (include_body && !event.bodyText.empty()) {
        out << " body=" << event.bodyText;
    }
    return out.str();
}

int print_app_ready_result(const axtp::sdk::AppReadyResult& result,
                           std::chrono::milliseconds elapsed,
                           OutputFormat format,
                           std::ostream& out,
                           std::ostream& err)
{
    if (format == OutputFormat::Json) {
        auto output = nlohmann::json::object();
        output["ok"] = result.ok;
        output["stage"] = result.stage;
        output["statusCode"] = static_cast<std::uint16_t>(result.statusCode);
        output["status"] = error_name(result.statusCode);
        output["sid"] = result.sid;
        if (result.hasRandomSeed) {
            output["randomSeed"] = result.randomSeed;
            output["randomSeedHex"] = to_hex_u32(result.randomSeed);
        }
        output["elapsedMs"] = elapsed.count();
        out << output.dump() << "\n";
        return result.ok ? 0 : 4;
    }
    if (!result.ok) {
        err << "AXTP app-ready failed at " << result.stage << ": "
            << error_name(result.statusCode) << " ("
            << static_cast<std::uint16_t>(result.statusCode) << ")\n";
        return 4;
    }
    out << "APP_READY sid=" << result.sid;
    if (result.hasRandomSeed) {
        out << " randomSeed=" << to_hex_u32(result.randomSeed);
    }
    out << " elapsedMs=" << elapsed.count() << "\n";
    return 0;
}

LocalLogger make_logger(const CliOptions& options)
{
    return LocalLogger(options.executable_path, "axent-axtp", options.log_enabled, options.log_body);
}

int call_method(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    std::optional<std::string> method_name;
    if (options.command.size() >= 2 && !is_option(options.command[1])) {
        method_name = options.command[1];
    }
    if (method_name.has_value() && method_name->empty()) {
        err << "method name must not be empty\n";
        return 2;
    }

    std::optional<std::uint32_t> method_id;
    if (const auto raw_id = option_value(options.command, "--method-id")) {
        method_id = parse_uint32(*raw_id);
        if (!method_id.has_value()) {
            err << "invalid --method-id\n";
            return 2;
        }
    }

    axtp::RpcEncoding encoding = axtp::RpcEncoding::Json;
    axtp::Bytes body;
    if (!build_call_body(options, options.command, encoding, body, err)) {
        return 2;
    }

    auto logger = make_logger(options);
    std::mutex trace_mutex;
    auto hid_trace = [&logger, &options, &trace_mutex, &out](const axtp::HidReportTrace& trace) {
        logger.write(hid_trace_line(trace, logger.include_body()));
        if (options.verbose) {
            std::lock_guard<std::mutex> lock(trace_mutex);
            print_hid_trace(trace, parse_output_format(options.output), logger.include_body(), out);
        }
    };

    axtp::sdk::ClientOptions client_options;
    client_options.autoIdentify = !options.no_app_ready;
    axtp::sdk::AxtpClient client(client_options);
    if (!options.registry_file.empty()) {
        auto registry = axtp::MethodRegistryJson::fromFile(options.registry_file);
        for (const auto& entry : registry.entries()) {
            client.registry().addMethod(entry.id, entry.name);
        }
    }

    if (!method_id.has_value() && method_name.has_value()) {
        method_id = client.registry().findMethodId(*method_name);
    }
    if (!method_name.has_value() && !method_id.has_value()) {
        err << "call requires a method name or --method-id\n";
        return 2;
    }
    if (!method_id.has_value()) {
        err << "Unknown method: " << *method_name
            << "\nRun `axent axtp list-methods` to view available methods.\n";
        return 3;
    }

    if (!attach_transport(options, client, hid_trace, err)) {
        err << "failed to connect transport: " << options.transport << "\n";
        return 4;
    }
    if (options.transport == "mock") {
        client.registerMethod(*method_id,
                              [](const axtp::RpcPayload& request) { return request.body; });
        install_mock_handlers(client);
    }

    if (is_hid_transport(options) && !options.no_app_ready) {
        axtp::sdk::AppReadyOptions app_options;
        app_options.timeout = std::chrono::milliseconds(options.timeout_ms);
        app_options.randomSeed = options.random_seed;
        app_options.trace = [&logger, &options, &err](const axtp::sdk::AppReadyTraceEvent& event) {
            logger.write(app_ready_trace_line(event, logger.include_body()));
            print_app_ready_trace(event, logger.include_body(), options.verbose, err);
        };
        const auto ready = client.ensureAppReady(app_options);
        if (!ready.ok) {
            return print_app_ready_result(
                ready, std::chrono::milliseconds(0), parse_output_format(options.output), out, err);
        }
    }

    axtp::RpcPayload request;
    request.encoding = encoding;
    request.op = axtp::RpcOp::Request;
    request.methodOrEventId = *method_id;
    request.bodyEncoding = axtp::bodyEncodingForRpcEncoding(encoding);
    request.meta.sourceProtocol = encoding == axtp::RpcEncoding::Json ? axtp::SourceProtocol::JsonRpc
                                                                      : axtp::SourceProtocol::AxtpV1;
    if (method_name.has_value()) {
        request.meta.jsonMethodOrEventName = *method_name;
    }
    if (options.no_app_ready && encoding == axtp::RpcEncoding::Json) {
        request.meta.jsonSid = options.sid;
    }
    request.body = std::move(body);

    axtp::sdk::CallOptions call_options;
    call_options.timeout = std::chrono::milliseconds(options.timeout_ms);
    call_options.encoding = request.encoding;
    auto response = client.callRaw(std::move(request), call_options);

    const auto output_mode =
        first_option_value(options.command, {"--output", "-o"}).value_or(options.output);
    const auto output_format = parse_output_format(output_mode);
    if (output_format == OutputFormat::Hex) {
        out << to_hex(response.body) << "\n";
        return response.statusCode == axtp::ErrorCode::Success ? 0 : 4;
    }
    if (output_format == OutputFormat::File) {
        const auto path = option_value(options.command, "--output-file");
        if (!path.has_value() || !write_binary_file(*path, response.body)) {
            err << "failed to write output file\n";
            return 2;
        }
        return response.statusCode == axtp::ErrorCode::Success ? 0 : 4;
    }

    auto output = nlohmann::json::object();
    output["ok"] = response.statusCode == axtp::ErrorCode::Success;
    if (method_name.has_value()) {
        output["method"] = *method_name;
    }
    output["methodId"] = *method_id;
    output["requestId"] = response.requestId;
    if (response.statusCode == axtp::ErrorCode::Success) {
        if (!response.body.empty()) {
            if (response.encoding == axtp::RpcEncoding::Json) {
                output["result"] = parse_json_value_or_string(response.body);
            } else {
                output["resultHex"] = to_hex(response.body);
            }
        }
    } else {
        auto error = nlohmann::json::object();
        error["code"] = error_name(response.statusCode);
        error["numericCode"] = static_cast<std::uint16_t>(response.statusCode);
        error["message"] = error_name(response.statusCode);
        output["error"] = std::move(error);
    }
    print_json_object(output, output_format, out);
    return response.statusCode == axtp::ErrorCode::Success ? 0 : 4;
}

int list_hid_devices_command(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    if (!is_hid_transport(options)) {
        err << "list-hid requires -t hid\n";
        return 2;
    }
    print_hid_devices(hid_options_from_cli(options), parse_output_format(options.output), out);
    return 0;
}

class CountingByteSink final : public axtp::IByteSink {
public:
    void onBytes(const axtp::Byte*, std::size_t size) override
    {
        chunks.fetch_add(1);
        bytes.fetch_add(static_cast<std::uint64_t>(size));
    }

    std::atomic<std::uint64_t> chunks{0};
    std::atomic<std::uint64_t> bytes{0};
};

int read_hid_command(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    if (!is_hid_transport(options)) {
        err << "read-hid requires -t hid\n";
        return 2;
    }

    auto hid_open = hid_options_from_cli(options);
    if (!has_hid_target(hid_open)) {
        err << "read-hid requires --path/--hid-path or both --vid and --pid\n";
        return 2;
    }

    auto logger = make_logger(options);
    std::mutex trace_mutex;
    const auto format = parse_output_format(options.output);
    hid_open.report_trace = [&logger, &trace_mutex, format, &out](const axtp::HidReportTrace& trace) {
        logger.write(hid_trace_line(trace, logger.include_body()));
        std::lock_guard<std::mutex> lock(trace_mutex);
        print_hid_trace(trace, format, true, out);
    };

    auto transport = std::make_unique<axtp::HidTransport>(make_hid_transport_options(hid_open));
    CountingByteSink sink;
    transport->bind(sink);
    transport->open();
    if (!transport->isOpen()) {
        err << "failed to open HID device";
        if (!options.path.empty()) {
            err << " path=" << options.path;
        }
        if (options.vid.has_value() || options.pid.has_value()) {
            err << " vid=" << to_hex_id(options.vid.value_or(0))
                << " pid=" << to_hex_id(options.pid.value_or(0));
        }
        if (options.usage_page.has_value()) {
            err << " usagePage=" << to_hex_id(*options.usage_page);
        }
        if (options.usage.has_value()) {
            err << " usage=" << to_hex_id(*options.usage);
        }
        err << "\n";
        return 4;
    }

    err << "read-hid opened; sending is disabled; ";
    if (options.timeout_ms == 0) {
        err << "reading until interrupted\n";
    } else {
        err << "reading for " << options.timeout_ms << " ms\n";
    }

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(options.timeout_ms);
    while (options.timeout_ms == 0 || std::chrono::steady_clock::now() < deadline) {
        transport->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto stats = transport->stats();
    transport->close();

    std::ostringstream summary;
    summary << "hid read stats readReports=" << stats.readReports
            << " readBytes=" << stats.readBytes
            << " acceptedReports=" << stats.acceptedReports
            << " droppedReportId=" << stats.droppedReportId
            << " readErrors=" << stats.readErrors
            << " queuedReports=" << stats.queuedReports
            << " sinkChunks=" << sink.chunks.load()
            << " sinkBytes=" << sink.bytes.load();
    logger.write(summary.str());
    err << summary.str() << "\n";
    return stats.readErrors == 0 ? 0 : 4;
}

int handshake_command(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    auto logger = make_logger(options);
    std::mutex trace_mutex;
    auto hid_trace = [&logger, &options, &trace_mutex, &out](const axtp::HidReportTrace& trace) {
        logger.write(hid_trace_line(trace, logger.include_body()));
        if (options.verbose) {
            std::lock_guard<std::mutex> lock(trace_mutex);
            print_hid_trace(trace, parse_output_format(options.output), logger.include_body(), out);
        }
    };

    axtp::sdk::ClientOptions client_options;
    client_options.autoIdentify = false;
    axtp::sdk::AxtpClient client(client_options);
    if (!attach_transport(options, client, hid_trace, err)) {
        err << "failed to connect transport: " << options.transport << "\n";
        return 4;
    }

    axtp::sdk::AppReadyOptions app_options;
    app_options.timeout = std::chrono::milliseconds(options.timeout_ms);
    app_options.randomSeed = options.random_seed;
    app_options.trace = [&logger, &options, &err](const axtp::sdk::AppReadyTraceEvent& event) {
        logger.write(app_ready_trace_line(event, logger.include_body()));
        print_app_ready_trace(event, logger.include_body(), options.verbose, err);
    };

    const auto started = std::chrono::steady_clock::now();
    const auto result = client.ensureAppReady(app_options);
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                              started);
    client.close();
    return print_app_ready_result(result, elapsed, parse_output_format(options.output), out, err);
}

int print_capability_methods(std::ostream& out)
{
    auto methods = nlohmann::json::array();
    for (const auto& method : axtp::kMethodRegistry) {
        auto item = nlohmann::json::object();
        item["id"] = method.id;
        item["name"] = method.name;
        item["domain"] = method.domain;
        item["requestSchema"] = method.request_schema;
        item["responseSchema"] = method.response_schema;
        methods.push_back(std::move(item));
    }
    out << methods.dump() << "\n";
    return 0;
}

int ping(const CliOptions& options, std::ostream& out)
{
    auto output = nlohmann::json::object();
    output["ok"] = options.transport == "mock";
    output["transport"] = options.transport;
    output["wire"] = options.wire;
    if (options.transport != "mock") {
        output["message"] = "real transport ping is not implemented in P0";
    }
    out << output.dump() << "\n";
    return options.transport == "mock" ? 0 : 4;
}

int run_command(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    if (options.command.empty() || options.command[0] == "help") {
        print_usage(out);
        return 0;
    }
    if (options.command[0] == "call") {
        return call_method(options, out, err);
    }
    if (options.command[0] == "list-methods") {
        return print_capability_methods(out);
    }
    if (options.command[0] == "list-hid") {
        return list_hid_devices_command(options, out, err);
    }
    if (options.command[0] == "read-hid") {
        return read_hid_command(options, out, err);
    }
    if (options.command[0] == "handshake") {
        return handshake_command(options, out, err);
    }
    if (options.command[0] == "capability" && options.command.size() >= 2 &&
        options.command[1] == "methods") {
        return print_capability_methods(out);
    }
    if (options.command[0] == "ping") {
        return ping(options, out);
    }
    if (options.command[0] == "inspect" && options.command.size() >= 2 &&
        options.command[1] == "frame") {
        return inspect_frame(options.command, out, err);
    }

    err << "unknown command\n";
    print_usage(err);
    return 2;
}

} // namespace

int run_axtp_cli(const std::vector<std::string>& args,
                 const std::string& executable_path,
                 std::ostream& out,
                 std::ostream& err)
{
    try {
        CliOptions options;
        options.executable_path = executable_path;
        if (!parse_axtp_options(args, options, err)) {
            return 2;
        }
        return run_command(options, out, err);
    } catch (const std::exception& ex) {
        err << "axent axtp: " << ex.what() << "\n";
        return 1;
    }
}

} // namespace axent

#endif
