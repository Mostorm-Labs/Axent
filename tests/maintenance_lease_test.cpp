#include <stdexcept>
#include <string>
#include <utility>

#include "axent/host/axent_host.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

axent::DeviceSnapshot second_device()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-002";
    device.adapter = "mock";
    device.connection.online = true;
    return device;
}

} // namespace

int main()
{
    axent::AxentHost host;
    auto& provider = host.maintenance_lease_provider();
    std::string reason;
    require(!provider.try_acquire_maintenance("mock-device-001", reason) &&
                reason == "host not running",
            "stopped host should reject maintenance");

    require(host.start(), "host should start");
    host.upsert_device(second_device());
    reason.clear();
    auto maintenance = provider.try_acquire_maintenance("mock-device-001", reason);
    require(static_cast<bool>(maintenance) &&
                maintenance.device_id() == "mock-device-001",
            "idle device should grant maintenance");
    reason.clear();
    require(!provider.try_acquire_maintenance("mock-device-001", reason),
            "second maintenance lease should fail fast");

    axent::SessionAcquireRequest same_control{
        "control-client", "mock-device-001", false};
    axent::SessionAcquireRequest same_media{
        "media-client", "mock-device-001", true};
    require(host.acquire_session(same_control).status == axent::ControlStatus::Busy,
            "maintenance should block control");
    require(host.acquire_session(same_media).status == axent::ControlStatus::Busy,
            "maintenance should block media");

    axent::SessionAcquireRequest other_control{
        "other-client", "mock-device-002", false};
    const auto other = host.acquire_session(other_control);
    require(other.acquired, "different device should remain available");
    host.release_session(other.session_id, "done");

    auto moved = std::move(maintenance);
    require(!maintenance && moved,
            "maintenance lease should be move-only");
    moved.reset();
    const auto control = host.acquire_session(same_control);
    require(control.acquired, "control should acquire after maintenance release");
    reason.clear();
    require(!provider.try_acquire_maintenance("mock-device-001", reason),
            "control should block maintenance");
    host.release_session(control.session_id, "done");

    const auto media = host.acquire_session(same_media);
    require(media.acquired, "media should acquire on idle device");
    reason.clear();
    require(!provider.try_acquire_maintenance("mock-device-001", reason),
            "media should block maintenance");
    host.release_session(media.session_id, "done");

    {
        reason.clear();
        auto scoped = provider.try_acquire_maintenance("mock-device-001", reason);
        require(static_cast<bool>(scoped),
                "maintenance should reacquire after sessions release");
    }
    const auto after_scope = host.acquire_session(same_control);
    require(after_scope.acquired,
            "maintenance destructor should release the device");
    host.release_session(after_scope.session_id, "done");

    reason.clear();
    require(!provider.try_acquire_maintenance("missing-device", reason) &&
                reason == "device not found",
            "unknown device should be rejected");
    host.stop();
    return 0;
}
