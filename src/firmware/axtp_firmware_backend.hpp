#pragma once

#include <functional>
#include <nlohmann/json.hpp>

#include "axent/firmware/firmware_update_service.hpp"
#include "axtp_sdk.hpp"

namespace axent::firmware::detail {

class AxtpFirmwareBackend final : public IFirmwareUpdateBackend {
public:
    using SendErrorCount = std::function<std::uint64_t()>;

    explicit AxtpFirmwareBackend(axtp::sdk::AxtpClient& client,
                                 SendErrorCount send_error_count = {});

    FirmwareBeginResult begin(const FirmwareBeginRequest& request) override;
    FirmwareBackendStatus send_chunk(const FirmwareChunkRequest& request) override;
    FirmwareFinishResult finish(const FirmwareFinishRequest& request) override;

    const nlohmann::json& begin_payload() const noexcept;
    const nlohmann::json& finish_payload() const noexcept;

private:
    axtp::sdk::AxtpClient& client_;
    SendErrorCount send_error_count_;
    nlohmann::json begin_payload_ = nullptr;
    nlohmann::json finish_payload_ = nullptr;
};

} // namespace axent::firmware::detail
