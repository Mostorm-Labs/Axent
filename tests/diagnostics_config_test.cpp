#include <stdexcept>
#include <string>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/config/config.hpp"
#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/diagnostics/diagnostics.hpp"
#include "axent/firmware/firmware_task.hpp"
#include "axent/logging/logger.hpp"

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    const auto config = axent::AxentConfig::dev_trial_defaults();
    require(config.server.bind_host == "0.0.0.0", "dev defaults should bind all interfaces");
    require(config.server.port == 6060, "dev defaults should use port 6060");
    require(config.server.authentication == "none", "dev defaults should disable authentication");
    require(config.logging.audit, "dev defaults should enable audit logging");

    axent::FirmwareTask task("fw-1", "mock-device-001", "/tmp/mock.bin");
    require(task.task_id() == "fw-1", "firmware task should expose task id");
    require(task.device_id() == "mock-device-001", "firmware task should expose device id");
    require(task.file_path() == "/tmp/mock.bin", "firmware task should expose file path");
    task.mark_validating("1.2.3");
    require(task.state_name() == "validating", "firmware task should enter validating state");
    require(task.progress().target_version == "1.2.3", "firmware task should track target version");
    task.mark_transferring(50, 100);
    require(task.progress().percent == 50, "50 of 100 bytes should be 50 percent");
    task.mark_transferring(50, 0);
    require(task.progress().percent == 0, "zero total should not divide by zero");
    task.mark_succeeded();
    require(task.state_name() == "succeeded", "firmware task should enter succeeded state");
    require(task.progress().percent == 100, "succeeded firmware task should be 100 percent");
    task.mark_failed("checksum mismatch");
    require(task.state_name() == "failed", "firmware task should enter failed state");
    require(!task.progress().recoverable, "failed firmware task should not be recoverable by default");
    task.mark_recoverable("device disconnected during reboot");
    require(task.state_name() == "recoverable", "firmware task should enter recoverable state");
    require(task.progress().recoverable, "recoverable firmware task should be marked recoverable");

    axent::Logger logger;
    logger.core("server.started", {{"port", config.server.port}});
    logger.audit("control.request", {{"method", "devices.list"}});
    logger.adapter("mock.discovery", {{"count", 1}});
    require(logger.records().size() == 3, "logger should store all channels in memory");

    axent::DeviceManager devices;
    axent::MockAdapter adapter;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }

    axent::CapabilityRegistry capabilities;
    capabilities.register_core_capabilities();
    axent::Diagnostics diagnostics(devices, capabilities, logger);

    const auto bundle = diagnostics.collect("mock-device-001", false);
    require(bundle.at("sanitized") == true, "diagnostics should mark non-sensitive bundle sanitized");
    require(bundle.at("device").at("id") == "mock-device-001", "diagnostics should include requested device");
    require(!bundle.at("capabilities").empty(), "diagnostics should include capabilities");
    require(bundle.at("auditLog").size() == 1, "diagnostics should include only audit log records");
    require(bundle.at("auditLog").at(0).at("message") == "control.request", "diagnostics should preserve audit message");
    require(bundle.at("flowControl").at("dropped") == 0, "diagnostics should include default dropped count");
    require(bundle.at("flowControl").at("paused") == false, "diagnostics should include default paused state");
    require(bundle.at("firmwareTasks").empty(), "diagnostics should include empty firmware task list");
    require(bundle.at("platform").at("product") == "Axent", "diagnostics should include platform product");
    require(!bundle.contains("accessToken"), "sanitized diagnostics should not include accessToken");
    require(!bundle.contains("debug"), "sanitized diagnostics should not include debug sensitive flag");

    const auto sensitive_bundle = diagnostics.collect("mock-device-001", true);
    require(sensitive_bundle.at("sanitized") == false, "sensitive bundle should not be marked sanitized");
    require(sensitive_bundle.at("debug").at("sensitiveIncluded") == true,
            "sensitive bundle should include debug sensitive flag");
    require(!sensitive_bundle.contains("accessToken"), "sensitive diagnostics should still not expose accessToken");

    return 0;
}
