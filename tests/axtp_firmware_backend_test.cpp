#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "axtp_firmware_backend.hpp"
#include "core/protocol/wire/inbound_processor.hpp"
#include "core/runtime/testing/mock_transport.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

axtp::Bytes json_bytes(const nlohmann::json& value)
{
    const auto text = value.dump();
    return {text.begin(), text.end()};
}

struct StreamSink final : axtp::IPayloadSink {
    void onControl(axtp::ControlPayload) override {}
    void onRpc(axtp::RpcPayload) override {}
    void onStream(axtp::StreamPayload value) override
    {
        streams.push_back(std::move(value));
    }

    std::vector<axtp::StreamPayload> streams;
};

axtp::StreamPayload decode_stream(const axtp::Bytes& bytes)
{
    StreamSink sink;
    axtp::InboundProcessor inbound(sink);
    inbound.onBytes(bytes.data(), bytes.size());
    require(sink.streams.size() == 1, "one stream payload should be encoded");
    return sink.streams.front();
}

void test_transaction_encoding()
{
    axtp::sdk::AxtpClient client;
    auto transport = std::make_unique<axtp::MockTransport>();
    auto* transport_ptr = transport.get();
    client.attachTransport(std::move(transport));

    axtp::RpcPayload captured_begin;
    axtp::RpcPayload captured_finish;
    client.registerMethod(
        static_cast<std::uint16_t>(axtp::MethodId::FirmwareBeginUpdate),
        [&](const axtp::RpcPayload& request) {
            captured_begin = request;
            return json_bytes({
                {"updateSessionId", "session-1"},
                {"state", "receiving"},
                {"streams",
                 nlohmann::json::array(
                     {{{"fileId", "application"}, {"streamId", 0x1001}}})},
                {"chunkSize", 2},
            });
        });
    client.registerMethod(
        static_cast<std::uint16_t>(axtp::MethodId::FirmwareFinishUpdate),
        [&](const axtp::RpcPayload& request) {
            captured_finish = request;
            return json_bytes({
                {"updateSessionId", "session-1"},
                {"accepted", true},
                {"state", "verifying"},
            });
        });

    axent::firmware::detail::AxtpFirmwareBackend backend(client);
    axent::firmware::FirmwareBeginRequest begin;
    begin.file_id = "application";
    begin.target = "main";
    begin.package_id = "pkg-1";
    begin.version = "1.2.3";
    begin.md5 = "00112233445566778899aabbccddeeff";
    begin.size = 5;
    begin.preferred_chunk_size = 4;
    begin.sid = "sid-1";
    const auto opened = backend.begin(begin);
    require(opened.status.ok && opened.update_session_id == "session-1" &&
                opened.stream_id == 0x1001 && opened.chunk_size == 2,
            "begin response should map to typed Axent fields");
    require(captured_begin.meta.jsonMethodOrEventName == "firmware.beginUpdate" &&
                captured_begin.meta.jsonSid == "sid-1",
            "begin should preserve generated method and SID");
    const auto begin_json = nlohmann::json::parse(
        std::string(captured_begin.body.begin(), captured_begin.body.end()));
    const auto& manifest = begin_json.at("manifest");
    const auto& file = manifest.at("files").front();
    require(manifest.at("packageId") == "pkg-1" &&
                manifest.at("version") == "1.2.3" &&
                file.at("fileId") == "application" &&
                file.at("target") == "main" &&
                file.at("size") == 5 &&
                file.at("md5") == "00112233445566778899aabbccddeeff",
            "begin manifest should preserve all fields");

    axent::firmware::FirmwareChunkRequest chunk;
    chunk.update_session_id = opened.update_session_id;
    chunk.stream_id = opened.stream_id;
    chunk.sequence_id = 7;
    chunk.cursor = 14;
    chunk.data = {0xA0, 0xA1};
    require(backend.send_chunk(chunk).ok,
            "attached AXTP backend should send a chunk");
    const auto outgoing = transport_ptr->tryPopOutgoing();
    require(outgoing.has_value(), "stream bytes should reach the transport");
    const auto stream = decode_stream(*outgoing);
    require(stream.streamId == 0x1001 && stream.seqId == 7 &&
                stream.cursor == 14 &&
                stream.data == axtp::Bytes({0xA0, 0xA1}),
            "stream encoding should preserve id, sequence, cursor, and bytes");

    axent::firmware::FirmwareFinishRequest finish;
    finish.update_session_id = opened.update_session_id;
    finish.sid = "sid-1";
    const auto finished = backend.finish(finish);
    require(finished.status.ok && finished.accepted &&
                finished.state == "verifying",
            "finish response should map to typed Axent fields");
    const auto finish_json = nlohmann::json::parse(
        std::string(captured_finish.body.begin(), captured_finish.body.end()));
    require(captured_finish.meta.jsonMethodOrEventName == "firmware.finishUpdate" &&
                captured_finish.meta.jsonSid == "sid-1" &&
                finish_json.at("updateSessionId") == "session-1",
            "finish should preserve method, SID, and update session");
}

void test_malformed_and_send_failure()
{
    axtp::sdk::AxtpClient malformed_client;
    auto transport = std::make_unique<axtp::MockTransport>();
    malformed_client.attachTransport(std::move(transport));
    malformed_client.registerMethod(
        static_cast<std::uint16_t>(axtp::MethodId::FirmwareBeginUpdate),
        [](const axtp::RpcPayload&) {
            return json_bytes({
                {"updateSessionId", "session-1"},
                {"streams",
                 nlohmann::json::array(
                     {{{"fileId", "other"}, {"streamId", 1}}})},
            });
        });
    axent::firmware::detail::AxtpFirmwareBackend malformed(malformed_client);
    axent::firmware::FirmwareBeginRequest begin;
    begin.file_id = "application";
    const auto malformed_result = malformed.begin(begin);
    require(!malformed_result.status.ok &&
                malformed_result.status.protocol_status ==
                    static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
            "missing matching binding should be RpcPayloadInvalid");

    axtp::sdk::AxtpClient detached_client;
    axent::firmware::detail::AxtpFirmwareBackend detached(detached_client);
    axent::firmware::FirmwareChunkRequest chunk;
    chunk.data = {0x01};
    const auto send = detached.send_chunk(chunk);
    require(!send.ok &&
                send.protocol_status ==
                    static_cast<std::uint16_t>(axtp::ErrorCode::Unavailable),
            "detached transport should expose stream send failure");

    axtp::sdk::AxtpClient observed_client;
    observed_client.attachTransport(std::make_unique<axtp::MockTransport>());
    std::uint64_t observations = 0;
    axent::firmware::detail::AxtpFirmwareBackend observed(
        observed_client,
        [&observations]() {
            return observations++;
        });
    const auto observed_send = observed.send_chunk(chunk);
    require(!observed_send.ok &&
                observed_send.protocol_status ==
                    static_cast<std::uint16_t>(axtp::ErrorCode::Unavailable),
            "transport write-error counters should fail the active chunk");
}

} // namespace

int main()
{
    test_transaction_encoding();
    test_malformed_and_send_failure();
    return 0;
}
