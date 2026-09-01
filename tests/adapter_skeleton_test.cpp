#include <stdexcept>

#include "axent/adapters/axdp_adapter.hpp"
#include "axent/adapters/axtp_adapter.hpp"
#include "axent/adapters/tea_adapter.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    axent::AxdpAdapter axdp;
    axent::AxtpAdapter axtp;
    axent::TeaAdapter tea;

    require(axdp.metadata().name == "axdp", "AXDP adapter name mismatch");
    require(axtp.metadata().name == "axtp", "AXTP adapter name mismatch");
    require(tea.metadata().name == "tea", "TEA adapter name mismatch");
    require(axtp.metadata().available, "AXTP adapter should be available");
    require(!axdp.capabilities().empty(), "AXDP adapter should expose skeleton capabilities");
    require(!axtp.capabilities().empty(), "AXTP adapter should expose skeleton capabilities");
    require(!tea.capabilities().empty(), "TEA adapter should expose skeleton capabilities");

    const auto axdp_result = axdp.call("missing", "status.get", {});
    require(axdp_result.status == axent::ControlStatus::Unavailable, "AXDP skeleton call should be unavailable");

    axent::AdapterControlRequest routed;
    routed.device_id = "missing";
    routed.source_endpoint_id = "ep_app";
    routed.destination_endpoint_id = "ep_device";
    routed.method = "status.get";
    axent::Adapter& legacy_adapter = axdp;
    require(legacy_adapter.call(routed).status == axent::ControlStatus::Unavailable,
            "routed sync compatibility must delegate to the legacy signature");
    require(legacy_adapter.call_async(routed)->wait().status ==
                axent::ControlStatus::Unavailable,
            "routed async compatibility must delegate to the legacy signature");
    require(legacy_adapter.start_firmware_update(routed, "/tmp/fw.bin").status ==
                axent::ControlStatus::Unavailable,
            "routed firmware compatibility must delegate to the legacy signature");

    return 0;
}
