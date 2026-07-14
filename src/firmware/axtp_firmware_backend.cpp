#include "axtp_firmware_backend.hpp"

#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace axent::firmware::detail {
namespace {

nlohmann::json parse_payload(const axtp::Bytes& bytes)
{
    if (bytes.empty()) {
        return nullptr;
    }
    try {
        return nlohmann::json::parse(std::string(bytes.begin(), bytes.end()));
    } catch (...) {
        return std::string(bytes.begin(), bytes.end());
    }
}

std::optional<std::string> string_field(const nlohmann::json& object, const char* field)
{
    if (!object.is_object() || !object.contains(field) || !object[field].is_string()) {
        return std::nullopt;
    }
    return object[field].get<std::string>();
}

std::optional<std::uint32_t> uint32_field(const nlohmann::json& object, const char* field)
{
    if (!object.is_object() || !object.contains(field)) {
        return std::nullopt;
    }
    const auto& value = object[field];
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (number <= std::numeric_limits<std::uint32_t>::max()) {
            return static_cast<std::uint32_t>(number);
        }
        return std::nullopt;
    }
    if (value.is_number_integer()) {
        const auto number = value.get<std::int64_t>();
        if (number >= 0 &&
            static_cast<std::uint64_t>(number) <= std::numeric_limits<std::uint32_t>::max()) {
            return static_cast<std::uint32_t>(number);
        }
    }
    return std::nullopt;
}

axtp::RpcPayload json_request(std::uint16_t method_id,
                              std::string method_name,
                              const nlohmann::json& params,
                              const std::string& sid)
{
    const auto body = params.dump();
    axtp::RpcPayload request;
    request.encoding = axtp::RpcEncoding::Json;
    request.op = axtp::RpcOp::Request;
    request.methodOrEventId = method_id;
    request.bodyEncoding = axtp::RpcBodyEncoding::None;
    request.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
    request.meta.jsonMethodOrEventName = std::move(method_name);
    request.meta.jsonSid = sid;
    request.body.assign(body.begin(), body.end());
    return request;
}

FirmwareBackendStatus response_status(const axtp::RpcPayload& response,
                                      const char* fallback)
{
    if (response.statusCode == axtp::ErrorCode::Success) {
        return FirmwareBackendStatus::success();
    }
    return FirmwareBackendStatus::failure(
        static_cast<std::uint16_t>(response.statusCode), fallback);
}

} // namespace

AxtpFirmwareBackend::AxtpFirmwareBackend(
    axtp::sdk::AxtpClient& client,
    SendErrorCount send_error_count)
    : client_(client)
    , send_error_count_(std::move(send_error_count))
{
}

FirmwareBeginResult AxtpFirmwareBackend::begin(const FirmwareBeginRequest& request)
{
    begin_payload_ = nullptr;
    auto file = nlohmann::json::object();
    file["fileId"] = request.file_id;
    if (!request.target.empty()) {
        file["target"] = request.target;
    }
    file["size"] = request.size;
    file["md5"] = request.md5;

    auto manifest = nlohmann::json::object();
    if (!request.package_id.empty()) {
        manifest["packageId"] = request.package_id;
    }
    if (!request.version.empty()) {
        manifest["version"] = request.version;
    }
    manifest["files"] = nlohmann::json::array({file});
    const auto params = nlohmann::json{{"manifest", manifest}};

    axtp::sdk::CallOptions options;
    options.timeout = request.timeout;
    options.encoding = axtp::RpcEncoding::Json;
    const auto response = client_.callRaw(
        json_request(static_cast<std::uint16_t>(axtp::MethodId::FirmwareBeginUpdate),
                     "firmware.beginUpdate", params, request.sid),
        options);

    FirmwareBeginResult result;
    result.status = response_status(response, "firmware begin request failed");
    begin_payload_ = parse_payload(response.body);
    if (!result.status.ok) {
        return result;
    }
    if (!begin_payload_.is_object()) {
        result.status = FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
            "firmware begin response is not an object");
        return result;
    }
    const auto session = string_field(begin_payload_, "updateSessionId");
    if (!session.has_value()) {
        result.status = FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
            "firmware begin response has no update session");
        return result;
    }
    if (!begin_payload_.contains("streams") || !begin_payload_["streams"].is_array()) {
        result.status = FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
            "firmware begin response has no stream bindings");
        return result;
    }
    result.update_session_id = *session;
    result.state = string_field(begin_payload_, "state").value_or(std::string{});
    result.chunk_size = uint32_field(begin_payload_, "chunkSize").value_or(0);
    for (const auto& binding : begin_payload_["streams"]) {
        if (string_field(binding, "fileId").value_or(std::string{}) != request.file_id) {
            continue;
        }
        const auto stream_id = uint32_field(binding, "streamId");
        if (stream_id.has_value()) {
            result.stream_id = *stream_id;
            return result;
        }
    }
    result.status = FirmwareBackendStatus::failure(
        static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
        "firmware begin response has no matching stream binding");
    return result;
}

FirmwareBackendStatus AxtpFirmwareBackend::send_chunk(const FirmwareChunkRequest& request)
{
    const auto write_errors_before = send_error_count_ ? send_error_count_() : 0;
    axtp::StreamPayload stream;
    stream.streamId = request.stream_id;
    stream.seqId = request.sequence_id;
    stream.cursor = request.cursor;
    stream.data = request.data;
    client_.sendStream(std::move(stream));
    const auto& error = client_.lastError();
    if (!error.ok()) {
        return FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(error.code), error.message);
    }
    if (send_error_count_ && send_error_count_() > write_errors_before) {
        return FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(axtp::ErrorCode::Unavailable),
            "transport failed to send firmware stream data");
    }
    return FirmwareBackendStatus::success();
}

FirmwareFinishResult AxtpFirmwareBackend::finish(const FirmwareFinishRequest& request)
{
    finish_payload_ = nullptr;
    const auto params = nlohmann::json{{"updateSessionId", request.update_session_id}};
    axtp::sdk::CallOptions options;
    options.timeout = request.timeout;
    options.encoding = axtp::RpcEncoding::Json;
    const auto response = client_.callRaw(
        json_request(static_cast<std::uint16_t>(axtp::MethodId::FirmwareFinishUpdate),
                     "firmware.finishUpdate", params, request.sid),
        options);

    FirmwareFinishResult result;
    result.status = response_status(response, "firmware finish request failed");
    finish_payload_ = parse_payload(response.body);
    if (!result.status.ok) {
        return result;
    }
    if (!finish_payload_.is_object()) {
        result.status = FirmwareBackendStatus::failure(
            static_cast<std::uint16_t>(axtp::ErrorCode::RpcPayloadInvalid),
            "firmware finish response is not an object");
        return result;
    }
    result.update_session_id = string_field(finish_payload_, "updateSessionId")
                                   .value_or(request.update_session_id);
    result.accepted = finish_payload_.value("accepted", false);
    result.state = string_field(finish_payload_, "state").value_or(std::string{});
    return result;
}

const nlohmann::json& AxtpFirmwareBackend::begin_payload() const noexcept
{
    return begin_payload_;
}

const nlohmann::json& AxtpFirmwareBackend::finish_payload() const noexcept
{
    return finish_payload_;
}

} // namespace axent::firmware::detail
