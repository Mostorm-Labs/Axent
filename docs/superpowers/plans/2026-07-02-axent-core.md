# Axent Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first Axent v1 skeleton: a new root C++ project with `libaxent`, `axentd`, `axent`, WebSocket dual-protocol control (`AXTP/JSON-RPC` and legacy `op/d/sid`), mock governance flow, service assets, and adapter skeletons beside the preserved legacy `agent/`.

**Architecture:** Keep `agent/` untouched as the legacy implementation. Build the new mainline under root `include/`, `src/`, and `tests/`; use `libaxent` for core state and routing, `axentd` for the daemon/WebSocket server, `axent` for local management, and static adapters for Mock, AXDP, AXTP, and TEA. Start with mock-first tests so the governance loop is verifiable before real devices are attached.

**Tech Stack:** C++17, CMake 3.21+, CTest, nlohmann/json from `axtp-cpp-runtime`, IXWebSocket via `axtp-cpp-runtime`, AXDP source under `third_party/axdp`, AXTP spec under `third_party/axtp`, TEA SDK under `third_party/tea`, Windows Service assets, macOS launchd assets.

---

## Scope Check

The approved spec spans repository layout, core model, dual protocol control (`AXTP/JSON-RPC` plus legacy `op/d/sid`), daemon/service lifecycle, diagnostics, and three adapter families. This plan keeps those in one ordered roadmap because the first useful delivery is a vertical mock-governance slice. Real device deep integration is represented by adapter skeletons, dependency discovery, capability metadata, and compile/link smoke tests; deeper hardware behavior should be planned after this v1 skeleton is passing.

## Target File Structure

Create or modify these files under `/Users/qing/Desktop/sources/gitee/Axent`:

```text
.gitmodules
CMakeLists.txt
THIRD_PARTY_LOCK.md
cmake/AxentDependencies.cmake
cmake/CompilerWarnings.cmake
config/axent.toml
include/axent/version.hpp
include/axent/core/types.hpp
include/axent/core/json.hpp
include/axent/core/device_manager.hpp
include/axent/core/session_manager.hpp
include/axent/core/capability_registry.hpp
include/axent/core/adapter.hpp
include/axent/core/adapter_registry.hpp
include/axent/core/route_manager.hpp
include/axent/core/broker.hpp
include/axent/core/middleware.hpp
include/axent/core/flow_control.hpp
include/axent/config/config.hpp
include/axent/diagnostics/diagnostics.hpp
include/axent/firmware/firmware_task.hpp
include/axent/logging/logger.hpp
include/axent/adapters/mock_adapter.hpp
include/axent/adapters/axdp_adapter.hpp
include/axent/adapters/axtp_adapter.hpp
include/axent/adapters/tea_adapter.hpp
include/axent/control/protocol_codecs.hpp
include/axent/control/control_plane.hpp
include/axent/control/websocket_server.hpp
src/version.cpp
src/core/json.cpp
src/core/device_manager.cpp
src/core/session_manager.cpp
src/core/capability_registry.cpp
src/core/adapter_registry.cpp
src/core/route_manager.cpp
src/core/broker.cpp
src/core/middleware.cpp
src/core/flow_control.cpp
src/config/config.cpp
src/diagnostics/diagnostics.cpp
src/firmware/firmware_task.cpp
src/logging/logger.cpp
src/adapters/mock_adapter.cpp
src/adapters/axdp_adapter.cpp
src/adapters/axtp_adapter.cpp
src/adapters/tea_adapter.cpp
src/control/protocol_codecs.cpp
src/control/control_plane.cpp
src/control/websocket_server.cpp
src/daemon/main.cpp
src/cli/main.cpp
tests/test_json.hpp
tests/version_smoke_test.cpp
tests/core_types_test.cpp
tests/manager_registry_test.cpp
tests/mock_adapter_test.cpp
tests/broker_flow_test.cpp
tests/protocol_codecs_test.cpp
tests/diagnostics_config_test.cpp
tests/adapter_skeleton_test.cpp
tests/cli_smoke_test.cpp
packaging/windows/axentd-service.xml
packaging/macos/com.mostorm.axentd.plist
```

## Task 1: Third-Party Layout And Build Baseline

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/.gitmodules`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/THIRD_PARTY_LOCK.md`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/cmake/AxentDependencies.cmake`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/cmake/CompilerWarnings.cmake`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/config/axent.toml`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/version.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/version.cpp`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/version_smoke_test.cpp`

- [ ] **Step 1: Organize third-party directories**

Run:

```bash
cd /Users/qing/Desktop/sources/gitee/Axent
mkdir -p third_party/tea/macos
mv TeaSdkMacOS_1.0.0.36 third_party/tea/macos/TeaSdkMacOS_1.0.0.36
mv TeaSdkMacOS_1.0.0.36.tar.gz third_party/tea/macos/TeaSdkMacOS_1.0.0.36.tar.gz
mv axdp third_party/axdp
mv axtp third_party/axtp
rm -rf third_party/axdp/.git third_party/axtp/.git
git submodule add https://github.com/Mostorm-Labs/axtp-cpp-runtime.git third_party/axtp-cpp-runtime
```

Expected:

```text
third_party/axdp
third_party/axtp
third_party/axtp-cpp-runtime
third_party/tea/macos/TeaSdkMacOS_1.0.0.36
```

If `git submodule add` reports that the target already exists, remove only the incomplete `third_party/axtp-cpp-runtime` directory and run the same submodule command again.

`third_party/axdp` and `third_party/axtp` are vendor snapshots for this plan, not git submodules. The `rm -rf .../.git` command removes only their nested checkout metadata after moving them into this repository; it does not delete their source files. `third_party/axtp-cpp-runtime` is the only new submodule in this plan.

- [ ] **Step 2: Write the failing version smoke test**

Create `tests/version_smoke_test.cpp`:

```cpp
#include <cassert>
#include <string>

#include "axent/version.hpp"

int main()
{
    assert(std::string(axent::product_name()) == "Axent");
    assert(std::string(axent::full_name()) == "Axtp Endpoint Agent");
    assert(std::string(axent::version()) == "0.1.0-dev");
    return 0;
}
```

- [ ] **Step 3: Add CMake baseline**

Create `cmake/CompilerWarnings.cmake`:

```cmake
function(axent_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
```

Create `cmake/AxentDependencies.cmake`:

```cmake
set(AXENT_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

set(AXTP_CPP_RUNTIME_BUILD_SDK ON CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(AXTP_BUILD_JSON_RPC ON CACHE BOOL "" FORCE)
set(AXTP_BUILD_OPTIONAL_TRANSPORTS ON CACHE BOOL "" FORCE)

if(EXISTS "${AXENT_THIRD_PARTY_DIR}/axtp-cpp-runtime/CMakeLists.txt")
    add_subdirectory(third_party/axtp-cpp-runtime)
else()
    message(FATAL_ERROR "Missing third_party/axtp-cpp-runtime. Run git submodule update --init --recursive.")
endif()

set(AXENT_AXDP_ROOT "${AXENT_THIRD_PARTY_DIR}/axdp" CACHE PATH "AXDP source root")
set(AXENT_AXTP_SPEC_ROOT "${AXENT_THIRD_PARTY_DIR}/axtp" CACHE PATH "AXTP spec root")
set(AXENT_TEA_MACOS_ROOT "${AXENT_THIRD_PARTY_DIR}/tea/macos/TeaSdkMacOS_1.0.0.36" CACHE PATH "TEA macOS SDK root")

set(AXENT_HAS_AXDP_HEADERS OFF)
if(EXISTS "${AXENT_AXDP_ROOT}/include/axdp_api.h")
    set(AXENT_HAS_AXDP_HEADERS ON)
endif()

set(AXENT_HAS_TEA_MACOS_SDK OFF)
if(EXISTS "${AXENT_TEA_MACOS_ROOT}/include/tea_inside_api.h")
    set(AXENT_HAS_TEA_MACOS_SDK ON)
endif()
```

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(Axent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(cmake/CompilerWarnings.cmake)
include(cmake/AxentDependencies.cmake)

add_library(libaxent
    src/version.cpp
)
add_library(axent::libaxent ALIAS libaxent)
target_include_directories(libaxent PUBLIC include)
target_link_libraries(libaxent PUBLIC axtp::core axtp::json_rpc)
axent_enable_warnings(libaxent)

if(BUILD_TESTING)
    add_executable(version_smoke_test tests/version_smoke_test.cpp)
    target_link_libraries(version_smoke_test PRIVATE axent::libaxent)
    axent_enable_warnings(version_smoke_test)
    add_test(NAME version_smoke_test COMMAND version_smoke_test)
endif()
```

Create `config/axent.toml`:

```toml
[server]
bind_host = "0.0.0.0"
port = 6060
authentication = "none"

[logging]
level = "info"
directory = "logs"
audit = true

[adapters]
mock = true
axdp = true
axtp = true
tea = true
```

Create `THIRD_PARTY_LOCK.md`:

```markdown
# Third Party Lock

This file records the source of third-party assets vendored or referenced by Axent v1.

| Dependency | Path | Source | Reference |
| --- | --- | --- | --- |
| AXDP | `third_party/axdp` | vendor snapshot from existing local checkout | source checkout was `https://gitee.com/auditoryworks_hamedal/axdp.git`, observed at `c8a1068` before vendoring |
| AXTP spec | `third_party/axtp` | vendor snapshot from existing local checkout | source checkout was `https://github.com/Mostorm-Labs/axtp.git`, observed at `6e9594a` before vendoring |
| AXTP C++ runtime | `third_party/axtp-cpp-runtime` | git submodule | `https://github.com/Mostorm-Labs/axtp-cpp-runtime.git` |
| TEA macOS SDK | `third_party/tea/macos/TeaSdkMacOS_1.0.0.36` | vendor binary SDK from existing local package | `TeaSdkMacOS_1.0.0.36` |
```

Create `include/axent/version.hpp`:

```cpp
#pragma once

namespace axent {

const char* product_name();
const char* full_name();
const char* version();

} // namespace axent
```

Create `src/version.cpp`:

```cpp
#include "axent/version.hpp"

namespace axent {

const char* product_name()
{
    return "Axent";
}

const char* full_name()
{
    return "Axtp Endpoint Agent";
}

const char* version()
{
    return "0.1.0-dev";
}

} // namespace axent
```

- [ ] **Step 4: Build and run the test**

Run:

```bash
cd /Users/qing/Desktop/sources/gitee/Axent
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target version_smoke_test
ctest --test-dir build -R version_smoke_test --output-on-failure
```

Expected: `version_smoke_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add .gitmodules CMakeLists.txt THIRD_PARTY_LOCK.md cmake config include src tests
git add third_party/axtp-cpp-runtime third_party/axdp third_party/axtp third_party/tea/macos
git commit -m "chore: scaffold axent root project"
```

Do not add `agent/` in this commit. If `git status` shows `third_party/axdp` or `third_party/axtp` as embedded repositories, stop and confirm that `third_party/axdp/.git` and `third_party/axtp/.git` were removed before staging.

## Task 2: Core Types And JSON Conversion

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/types.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/json.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/json.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/test_json.hpp`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/core_types_test.cpp`

- [ ] **Step 1: Write the failing core type test**

Create `tests/test_json.hpp`:

```cpp
#pragma once

#include <cassert>
#include <nlohmann/json.hpp>

inline void assert_json_eq(const nlohmann::json& actual, const nlohmann::json& expected)
{
    if (actual != expected) {
        assert(actual.dump(2) == expected.dump(2));
    }
}
```

Create `tests/core_types_test.cpp`:

```cpp
#include <cassert>
#include <string>

#include "axent/core/json.hpp"
#include "axent/core/types.hpp"
#include "test_json.hpp"

int main()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-001";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK001";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.status.health = "ok";

    assert_json_eq(axent::to_json(device), {
        {"id", "mock-device-001"},
        {"adapter", "mock"},
        {"identity", {
            {"vendor", "Mostorm"},
            {"model", "MockCam"},
            {"serialNumber", "MOCK001"},
            {"firmwareVersion", ""},
            {"hardwareVersion", ""}
        }},
        {"connection", {
            {"online", true},
            {"transport", "mock"},
            {"lastChangeReason", ""}
        }},
        {"status", {{"health", "ok"}}}
    });

    axent::Capability capability;
    capability.name = "firmware";
    capability.domain = "core";
    capability.available = true;
    capability.methods.push_back({"firmware.update", axent::RiskLevel::Dangerous, true, true});
    assert(axent::risk_name(axent::RiskLevel::Dangerous) == std::string("dangerous"));
    assert(axent::to_json(capability).at("methods").at(0).at("name") == "firmware.update");
    return 0;
}
```

- [ ] **Step 2: Register the failing test**

Add to `CMakeLists.txt` inside `add_library(libaxent ...)`:

```cmake
    src/core/json.cpp
```

Add inside `if(BUILD_TESTING)`:

```cmake
    add_executable(core_types_test tests/core_types_test.cpp)
    target_link_libraries(core_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(core_types_test)
    add_test(NAME core_types_test COMMAND core_types_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target core_types_test
```

Expected: build fails because `axent/core/types.hpp` does not exist.

- [ ] **Step 3: Implement core types**

Create `include/axent/core/types.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

enum class RiskLevel {
    Safe,
    Confirm,
    Dangerous,
};

enum class ProtocolSource {
    JsonRpc,
    LegacyOp,
    LocalCli,
};

enum class ControlStatus {
    Ok,
    Accepted,
    NotFound,
    Forbidden,
    InvalidArgument,
    Unavailable,
    InternalError,
};

struct DeviceIdentity {
    std::string vendor;
    std::string model;
    std::string serial_number;
    std::string firmware_version;
    std::string hardware_version;
};

struct DeviceConnection {
    bool online = false;
    std::string transport;
    std::string last_change_reason;
};

struct DeviceStatus {
    std::string health = "unknown";
};

struct DeviceSnapshot {
    std::string id;
    std::string adapter;
    DeviceIdentity identity;
    DeviceConnection connection;
    DeviceStatus status;
};

struct CapabilityMethod {
    std::string name;
    RiskLevel risk = RiskLevel::Safe;
    bool async = false;
    bool requires_confirmation = false;
};

struct Capability {
    std::string name;
    std::string domain;
    bool available = true;
    std::string unavailable_reason;
    std::vector<CapabilityMethod> methods;
    std::vector<std::string> events;
};

struct ControlCommand {
    std::string request_id;
    std::string control_session_id;
    std::string method;
    std::string device_id;
    ProtocolSource source = ProtocolSource::JsonRpc;
    nlohmann::json params = nlohmann::json::object();
};

struct ControlResult {
    ControlStatus status = ControlStatus::Ok;
    nlohmann::json body = nlohmann::json::object();
};

const char* risk_name(RiskLevel risk);
const char* protocol_source_name(ProtocolSource source);
const char* control_status_name(ControlStatus status);

} // namespace axent
```

Create `include/axent/core/json.hpp`:

```cpp
#pragma once

#include <nlohmann/json.hpp>

#include "axent/core/types.hpp"

namespace axent {

nlohmann::json to_json(const DeviceSnapshot& device);
nlohmann::json to_json(const Capability& capability);
nlohmann::json to_json(const ControlResult& result);

} // namespace axent
```

Create `src/core/json.cpp`:

```cpp
#include "axent/core/json.hpp"

namespace axent {

const char* risk_name(RiskLevel risk)
{
    switch (risk) {
    case RiskLevel::Safe: return "safe";
    case RiskLevel::Confirm: return "confirm";
    case RiskLevel::Dangerous: return "dangerous";
    }
    return "safe";
}

const char* protocol_source_name(ProtocolSource source)
{
    switch (source) {
    case ProtocolSource::JsonRpc: return "json-rpc";
    case ProtocolSource::LegacyOp: return "legacy-op";
    case ProtocolSource::LocalCli: return "local-cli";
    }
    return "json-rpc";
}

const char* control_status_name(ControlStatus status)
{
    switch (status) {
    case ControlStatus::Ok: return "ok";
    case ControlStatus::Accepted: return "accepted";
    case ControlStatus::NotFound: return "not_found";
    case ControlStatus::Forbidden: return "forbidden";
    case ControlStatus::InvalidArgument: return "invalid_argument";
    case ControlStatus::Unavailable: return "unavailable";
    case ControlStatus::InternalError: return "internal_error";
    }
    return "internal_error";
}

nlohmann::json to_json(const DeviceSnapshot& device)
{
    return {
        {"id", device.id},
        {"adapter", device.adapter},
        {"identity", {
            {"vendor", device.identity.vendor},
            {"model", device.identity.model},
            {"serialNumber", device.identity.serial_number},
            {"firmwareVersion", device.identity.firmware_version},
            {"hardwareVersion", device.identity.hardware_version}
        }},
        {"connection", {
            {"online", device.connection.online},
            {"transport", device.connection.transport},
            {"lastChangeReason", device.connection.last_change_reason}
        }},
        {"status", {{"health", device.status.health}}}
    };
}

nlohmann::json to_json(const Capability& capability)
{
    nlohmann::json methods = nlohmann::json::array();
    for (const auto& method : capability.methods) {
        methods.push_back({
            {"name", method.name},
            {"risk", risk_name(method.risk)},
            {"async", method.async},
            {"requiresConfirmation", method.requires_confirmation}
        });
    }
    return {
        {"name", capability.name},
        {"domain", capability.domain},
        {"available", capability.available},
        {"unavailableReason", capability.unavailable_reason},
        {"methods", methods},
        {"events", capability.events}
    };
}

nlohmann::json to_json(const ControlResult& result)
{
    return {
        {"status", control_status_name(result.status)},
        {"body", result.body}
    };
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target core_types_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R core_types_test --output-on-failure
```

Expected: `core_types_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/core src/core tests/test_json.hpp tests/core_types_test.cpp
git commit -m "feat: add axent core data types"
```

## Task 3: Managers, Registries, And Sessions

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/device_manager.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/session_manager.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/capability_registry.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/adapter_registry.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/device_manager.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/session_manager.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/capability_registry.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/adapter_registry.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/manager_registry_test.cpp`

- [ ] **Step 1: Write the failing manager and registry test**

Create `tests/manager_registry_test.cpp`:

```cpp
#include <cassert>

#include "axent/core/adapter_registry.hpp"
#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/session_manager.hpp"

int main()
{
    axent::DeviceManager devices;
    axent::DeviceSnapshot device;
    device.id = "dev-1";
    device.adapter = "mock";
    device.connection.online = true;
    devices.upsert(device);
    assert(devices.list().size() == 1);
    assert(devices.get("dev-1")->adapter == "mock");
    devices.mark_offline("dev-1", "test-remove");
    assert(!devices.get("dev-1")->connection.online);

    axent::CapabilityRegistry capabilities;
    capabilities.register_core_capabilities();
    assert(capabilities.has("identity"));
    assert(capabilities.has_method("firmware.update"));
    assert(capabilities.all().size() == 9);

    axent::SessionManager sessions;
    const auto control = sessions.control().open("json-rpc");
    const auto device_session = sessions.device().open("dev-1", "mock");
    sessions.map_control_to_device(control, device_session);
    assert(sessions.device_session_for_control(control) == device_session);

    axent::AdapterRegistry adapters;
    adapters.register_adapter({"mock", "Mock Adapter", true, ""});
    adapters.register_adapter({"tea", "TEA Adapter", false, "SDK not loaded"});
    assert(adapters.find("mock")->available);
    assert(!adapters.find("tea")->available);
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add these sources to `add_library(libaxent ...)`:

```cmake
    src/core/device_manager.cpp
    src/core/session_manager.cpp
    src/core/capability_registry.cpp
    src/core/adapter_registry.cpp
```

Add this test inside `if(BUILD_TESTING)`:

```cmake
    add_executable(manager_registry_test tests/manager_registry_test.cpp)
    target_link_libraries(manager_registry_test PRIVATE axent::libaxent)
    axent_enable_warnings(manager_registry_test)
    add_test(NAME manager_registry_test COMMAND manager_registry_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target manager_registry_test
```

Expected: build fails because the manager headers do not exist.

- [ ] **Step 3: Implement managers and registries**

Create `include/axent/core/device_manager.hpp`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "axent/core/types.hpp"

namespace axent {

class DeviceManager {
public:
    void upsert(DeviceSnapshot snapshot);
    void mark_offline(const std::string& id, const std::string& reason);
    std::optional<DeviceSnapshot> get(const std::string& id) const;
    std::vector<DeviceSnapshot> list() const;

private:
    std::vector<DeviceSnapshot> devices_;
};

} // namespace axent
```

Create `src/core/device_manager.cpp`:

```cpp
#include "axent/core/device_manager.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void DeviceManager::upsert(DeviceSnapshot snapshot)
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == snapshot.id;
    });
    if (existing == devices_.end()) {
        devices_.push_back(std::move(snapshot));
        return;
    }
    *existing = std::move(snapshot);
}

void DeviceManager::mark_offline(const std::string& id, const std::string& reason)
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == id;
    });
    if (existing != devices_.end()) {
        existing->connection.online = false;
        existing->connection.last_change_reason = reason;
    }
}

std::optional<DeviceSnapshot> DeviceManager::get(const std::string& id) const
{
    auto existing = std::find_if(devices_.begin(), devices_.end(), [&](const auto& current) {
        return current.id == id;
    });
    if (existing == devices_.end()) {
        return std::nullopt;
    }
    return *existing;
}

std::vector<DeviceSnapshot> DeviceManager::list() const
{
    return devices_;
}

} // namespace axent
```

Create `include/axent/core/capability_registry.hpp`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "axent/core/types.hpp"

namespace axent {

class CapabilityRegistry {
public:
    void register_core_capabilities();
    void register_capability(Capability capability);
    bool has(const std::string& name) const;
    bool has_method(const std::string& method) const;
    std::optional<CapabilityMethod> find_method(const std::string& method) const;
    const std::vector<Capability>& all() const;

private:
    std::vector<Capability> capabilities_;
};

} // namespace axent
```

Create `src/core/capability_registry.cpp`:

```cpp
#include "axent/core/capability_registry.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void CapabilityRegistry::register_core_capabilities()
{
    capabilities_.clear();
    register_capability({"identity", "core", true, "", {{"identity.get", RiskLevel::Safe, false, false}}, {"identity.changed"}});
    register_capability({"connection", "core", true, "", {{"connection.get", RiskLevel::Safe, false, false}}, {"connection.changed"}});
    register_capability({"status", "core", true, "", {{"status.get", RiskLevel::Safe, false, false}}, {"status.changed"}});
    register_capability({"control", "core", true, "", {{"control.reboot", RiskLevel::Dangerous, false, true}}, {"control.completed"}});
    register_capability({"events", "core", true, "", {{"events.subscribe", RiskLevel::Safe, false, false}}, {"events.subscriptionChanged"}});
    register_capability({"stream.flowControl", "core", true, "", {{"stream.flowControl.get", RiskLevel::Safe, false, false}, {"stream.flowControl.set", RiskLevel::Confirm, false, true}}, {"stream.flowControl.changed"}});
    register_capability({"diagnostics", "core", true, "", {{"diagnostics.collect", RiskLevel::Safe, true, false}}, {"diagnostics.ready"}});
    register_capability({"config", "core", true, "", {{"config.export", RiskLevel::Safe, true, false}, {"config.import", RiskLevel::Dangerous, true, true}}, {"config.changed"}});
    register_capability({"firmware", "core", true, "", {{"firmware.update", RiskLevel::Dangerous, true, true}, {"firmware.task", RiskLevel::Safe, false, false}}, {"firmware.progress"}});
}

void CapabilityRegistry::register_capability(Capability capability)
{
    auto existing = std::find_if(capabilities_.begin(), capabilities_.end(), [&](const auto& current) {
        return current.name == capability.name;
    });
    if (existing == capabilities_.end()) {
        capabilities_.push_back(std::move(capability));
        return;
    }
    *existing = std::move(capability);
}

bool CapabilityRegistry::has(const std::string& name) const
{
    return std::any_of(capabilities_.begin(), capabilities_.end(), [&](const auto& current) {
        return current.name == name;
    });
}

bool CapabilityRegistry::has_method(const std::string& method) const
{
    return find_method(method).has_value();
}

std::optional<CapabilityMethod> CapabilityRegistry::find_method(const std::string& method) const
{
    for (const auto& capability : capabilities_) {
        for (const auto& candidate : capability.methods) {
            if (candidate.name == method) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

const std::vector<Capability>& CapabilityRegistry::all() const
{
    return capabilities_;
}

} // namespace axent
```

Create `include/axent/core/session_manager.hpp`:

```cpp
#pragma once

#include <map>
#include <optional>
#include <string>

namespace axent {

struct ControlSession {
    std::string id;
    std::string protocol;
};

struct DeviceSession {
    std::string id;
    std::string device_id;
    std::string adapter;
};

class ControlSessionManager {
public:
    std::string open(const std::string& protocol);
    std::optional<ControlSession> get(const std::string& id) const;

private:
    int next_id_ = 1;
    std::map<std::string, ControlSession> sessions_;
};

class DeviceSessionManager {
public:
    std::string open(const std::string& device_id, const std::string& adapter);
    std::optional<DeviceSession> get(const std::string& id) const;

private:
    int next_id_ = 1;
    std::map<std::string, DeviceSession> sessions_;
};

class SessionManager {
public:
    ControlSessionManager& control();
    DeviceSessionManager& device();
    void map_control_to_device(std::string control_session_id, std::string device_session_id);
    std::string device_session_for_control(const std::string& control_session_id) const;

private:
    ControlSessionManager control_;
    DeviceSessionManager device_;
    std::map<std::string, std::string> control_to_device_;
};

} // namespace axent
```

Create `src/core/session_manager.cpp`:

```cpp
#include "axent/core/session_manager.hpp"

#include <sstream>
#include <utility>

namespace axent {

std::string ControlSessionManager::open(const std::string& protocol)
{
    std::ostringstream id;
    id << "ctrl-" << next_id_++;
    sessions_[id.str()] = {id.str(), protocol};
    return id.str();
}

std::optional<ControlSession> ControlSessionManager::get(const std::string& id) const
{
    auto found = sessions_.find(id);
    if (found == sessions_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string DeviceSessionManager::open(const std::string& device_id, const std::string& adapter)
{
    std::ostringstream id;
    id << "devsess-" << next_id_++;
    sessions_[id.str()] = {id.str(), device_id, adapter};
    return id.str();
}

std::optional<DeviceSession> DeviceSessionManager::get(const std::string& id) const
{
    auto found = sessions_.find(id);
    if (found == sessions_.end()) {
        return std::nullopt;
    }
    return found->second;
}

ControlSessionManager& SessionManager::control()
{
    return control_;
}

DeviceSessionManager& SessionManager::device()
{
    return device_;
}

void SessionManager::map_control_to_device(std::string control_session_id, std::string device_session_id)
{
    control_to_device_[std::move(control_session_id)] = std::move(device_session_id);
}

std::string SessionManager::device_session_for_control(const std::string& control_session_id) const
{
    auto found = control_to_device_.find(control_session_id);
    return found == control_to_device_.end() ? std::string() : found->second;
}

} // namespace axent
```

Create `include/axent/core/adapter_registry.hpp`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace axent {

struct AdapterMetadata {
    std::string name;
    std::string display_name;
    bool available = false;
    std::string unavailable_reason;
};

class AdapterRegistry {
public:
    void register_adapter(AdapterMetadata metadata);
    std::optional<AdapterMetadata> find(const std::string& name) const;
    const std::vector<AdapterMetadata>& all() const;

private:
    std::vector<AdapterMetadata> adapters_;
};

} // namespace axent
```

Create `src/core/adapter_registry.cpp`:

```cpp
#include "axent/core/adapter_registry.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void AdapterRegistry::register_adapter(AdapterMetadata metadata)
{
    auto existing = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& current) {
        return current.name == metadata.name;
    });
    if (existing == adapters_.end()) {
        adapters_.push_back(std::move(metadata));
        return;
    }
    *existing = std::move(metadata);
}

std::optional<AdapterMetadata> AdapterRegistry::find(const std::string& name) const
{
    auto found = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& current) {
        return current.name == name;
    });
    if (found == adapters_.end()) {
        return std::nullopt;
    }
    return *found;
}

const std::vector<AdapterMetadata>& AdapterRegistry::all() const
{
    return adapters_;
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target manager_registry_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R manager_registry_test --output-on-failure
```

Expected: `manager_registry_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/core src/core tests/manager_registry_test.cpp
git commit -m "feat: add axent managers and registries"
```

## Task 4: Adapter Interface And Mock Adapter

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/adapter.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/adapters/mock_adapter.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/adapters/mock_adapter.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/mock_adapter_test.cpp`

- [ ] **Step 1: Write the failing mock adapter test**

Create `tests/mock_adapter_test.cpp`:

```cpp
#include <cassert>

#include "axent/adapters/mock_adapter.hpp"

int main()
{
    axent::MockAdapter adapter;
    assert(adapter.metadata().name == "mock");
    const auto devices = adapter.discover();
    assert(devices.size() == 1);
    assert(devices[0].id == "mock-device-001");
    assert(adapter.capabilities()[0].name == "mock.identity");

    const auto status = adapter.call("mock-device-001", "status.get", {});
    assert(status.status == axent::ControlStatus::Ok);
    assert(status.body.at("health") == "ok");

    const auto firmware = adapter.start_firmware_update("mock-device-001", "/tmp/mock.bin");
    assert(firmware.status == axent::ControlStatus::Accepted);
    assert(firmware.body.at("taskId").get<std::string>().find("fw-mock-") == 0);
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add `src/adapters/mock_adapter.cpp` to `libaxent`.

Add this test inside `if(BUILD_TESTING)`:

```cmake
    add_executable(mock_adapter_test tests/mock_adapter_test.cpp)
    target_link_libraries(mock_adapter_test PRIVATE axent::libaxent)
    axent_enable_warnings(mock_adapter_test)
    add_test(NAME mock_adapter_test COMMAND mock_adapter_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target mock_adapter_test
```

Expected: build fails because adapter headers do not exist.

- [ ] **Step 3: Implement adapter interface and mock adapter**

Create `include/axent/core/adapter.hpp`:

```cpp
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/core/adapter_registry.hpp"
#include "axent/core/types.hpp"

namespace axent {

class Adapter {
public:
    virtual ~Adapter() = default;
    virtual AdapterMetadata metadata() const = 0;
    virtual std::vector<Capability> capabilities() const = 0;
    virtual std::vector<DeviceSnapshot> discover() = 0;
    virtual ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) = 0;
    virtual ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) = 0;
};

} // namespace axent
```

Create `include/axent/adapters/mock_adapter.hpp`:

```cpp
#pragma once

#include "axent/core/adapter.hpp"

namespace axent {

class MockAdapter final : public Adapter {
public:
    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;

private:
    int next_task_id_ = 1;
};

} // namespace axent
```

Create `src/adapters/mock_adapter.cpp`:

```cpp
#include "axent/adapters/mock_adapter.hpp"

#include <sstream>

namespace axent {

AdapterMetadata MockAdapter::metadata() const
{
    return {"mock", "Mock Adapter", true, ""};
}

std::vector<Capability> MockAdapter::capabilities() const
{
    return {
        {"mock.identity", "mock", true, "", {{"identity.get", RiskLevel::Safe, false, false}}, {"identity.changed"}},
        {"mock.status", "mock", true, "", {{"status.get", RiskLevel::Safe, false, false}}, {"status.changed"}},
        {"mock.firmware", "mock", true, "", {{"firmware.update", RiskLevel::Dangerous, true, true}}, {"firmware.progress"}},
        {"mock.stream", "mock", true, "", {{"stream.flowControl.get", RiskLevel::Safe, false, false}}, {"stream.flowControl.changed"}}
    };
}

std::vector<DeviceSnapshot> MockAdapter::discover()
{
    DeviceSnapshot device;
    device.id = "mock-device-001";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK001";
    device.identity.firmware_version = "mock-fw-1.0.0";
    device.identity.hardware_version = "mock-hw-revA";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.connection.last_change_reason = "mock-discovered";
    device.status.health = "ok";
    return {device};
}

ControlResult MockAdapter::call(const std::string& device_id, const std::string& method, const nlohmann::json&)
{
    if (device_id != "mock-device-001") {
        return {ControlStatus::NotFound, {{"error", "device not found"}}};
    }
    if (method == "status.get") {
        return {ControlStatus::Ok, {{"health", "ok"}}};
    }
    if (method == "identity.get") {
        return {ControlStatus::Ok, {{"serialNumber", "MOCK001"}, {"model", "MockCam"}}};
    }
    if (method == "stream.flowControl.get") {
        return {ControlStatus::Ok, {{"paused", false}, {"dropped", 0}}};
    }
    return {ControlStatus::NotFound, {{"error", "method not found"}}};
}

ControlResult MockAdapter::start_firmware_update(const std::string& device_id, const std::string& file_path)
{
    if (device_id != "mock-device-001") {
        return {ControlStatus::NotFound, {{"error", "device not found"}}};
    }
    std::ostringstream task_id;
    task_id << "fw-mock-" << next_task_id_++;
    return {ControlStatus::Accepted, {{"taskId", task_id.str()}, {"file", file_path}, {"state", "queued"}}};
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target mock_adapter_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R mock_adapter_test --output-on-failure
```

Expected: `mock_adapter_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/core/adapter.hpp include/axent/adapters src/adapters tests/mock_adapter_test.cpp
git commit -m "feat: add adapter contract and mock adapter"
```

## Task 5: Firmware Task, Config, Logger, And Diagnostics

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/firmware/firmware_task.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/config/config.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/logging/logger.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/diagnostics/diagnostics.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/firmware/firmware_task.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/config/config.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/logging/logger.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/diagnostics/diagnostics.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/diagnostics_config_test.cpp`

- [ ] **Step 1: Write the failing diagnostics and config test**

Create `tests/diagnostics_config_test.cpp`:

```cpp
#include <cassert>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/config/config.hpp"
#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/diagnostics/diagnostics.hpp"
#include "axent/firmware/firmware_task.hpp"
#include "axent/logging/logger.hpp"

int main()
{
    const auto config = axent::AxentConfig::dev_trial_defaults();
    assert(config.server.bind_host == "0.0.0.0");
    assert(config.server.authentication == "none");
    assert(config.logging.audit);

    axent::FirmwareTask task("fw-1", "mock-device-001", "/tmp/mock.bin");
    task.mark_validating("1.2.3");
    task.mark_transferring(50, 100);
    assert(task.progress().percent == 50);
    task.mark_recoverable("device disconnected during reboot");
    assert(task.state_name() == "recoverable");

    axent::Logger logger;
    logger.audit("control.request", {{"method", "devices.list"}});

    axent::DeviceManager devices;
    axent::MockAdapter adapter;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }
    axent::CapabilityRegistry capabilities;
    capabilities.register_core_capabilities();
    axent::Diagnostics diagnostics(devices, capabilities, logger);
    const auto bundle = diagnostics.collect("mock-device-001", false);
    assert(bundle.at("sanitized") == true);
    assert(bundle.at("device").at("id") == "mock-device-001");
    assert(bundle.at("auditLog").size() == 1);
    assert(!bundle.contains("accessToken"));
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add these files to `libaxent`:

```cmake
    src/config/config.cpp
    src/diagnostics/diagnostics.cpp
    src/firmware/firmware_task.cpp
    src/logging/logger.cpp
```

Add this test:

```cmake
    add_executable(diagnostics_config_test tests/diagnostics_config_test.cpp)
    target_link_libraries(diagnostics_config_test PRIVATE axent::libaxent)
    axent_enable_warnings(diagnostics_config_test)
    add_test(NAME diagnostics_config_test COMMAND diagnostics_config_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target diagnostics_config_test
```

Expected: build fails because config, diagnostics, firmware, and logger headers do not exist.

- [ ] **Step 3: Implement the components**

Create `include/axent/config/config.hpp`:

```cpp
#pragma once

#include <string>

namespace axent {

struct ServerConfig {
    std::string bind_host = "0.0.0.0";
    int port = 6060;
    std::string authentication = "none";
};

struct LoggingConfig {
    std::string level = "info";
    std::string directory = "logs";
    bool audit = true;
};

struct AxentConfig {
    ServerConfig server;
    LoggingConfig logging;
    static AxentConfig dev_trial_defaults();
};

} // namespace axent
```

Create `src/config/config.cpp`:

```cpp
#include "axent/config/config.hpp"

namespace axent {

AxentConfig AxentConfig::dev_trial_defaults()
{
    return {};
}

} // namespace axent
```

Create `include/axent/firmware/firmware_task.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace axent {

struct FirmwareProgress {
    std::string stage = "queued";
    std::uint64_t transferred_bytes = 0;
    std::uint64_t total_bytes = 0;
    int percent = 0;
    std::string target_version;
    std::string last_error;
    bool recoverable = false;
};

class FirmwareTask {
public:
    FirmwareTask(std::string task_id, std::string device_id, std::string file_path);
    const std::string& task_id() const;
    const std::string& device_id() const;
    const std::string& file_path() const;
    const FirmwareProgress& progress() const;
    std::string state_name() const;
    void mark_validating(std::string target_version);
    void mark_transferring(std::uint64_t transferred, std::uint64_t total);
    void mark_succeeded();
    void mark_failed(std::string error);
    void mark_recoverable(std::string error);

private:
    std::string task_id_;
    std::string device_id_;
    std::string file_path_;
    std::string state_ = "queued";
    FirmwareProgress progress_;
};

} // namespace axent
```

Create `src/firmware/firmware_task.cpp`:

```cpp
#include "axent/firmware/firmware_task.hpp"

#include <utility>

namespace axent {

FirmwareTask::FirmwareTask(std::string task_id, std::string device_id, std::string file_path)
    : task_id_(std::move(task_id)), device_id_(std::move(device_id)), file_path_(std::move(file_path))
{
}

const std::string& FirmwareTask::task_id() const { return task_id_; }
const std::string& FirmwareTask::device_id() const { return device_id_; }
const std::string& FirmwareTask::file_path() const { return file_path_; }
const FirmwareProgress& FirmwareTask::progress() const { return progress_; }
std::string FirmwareTask::state_name() const { return state_; }

void FirmwareTask::mark_validating(std::string target_version)
{
    state_ = "validating";
    progress_.stage = "validating";
    progress_.target_version = std::move(target_version);
}

void FirmwareTask::mark_transferring(std::uint64_t transferred, std::uint64_t total)
{
    state_ = "transferring";
    progress_.stage = "transferring";
    progress_.transferred_bytes = transferred;
    progress_.total_bytes = total;
    progress_.percent = total == 0 ? 0 : static_cast<int>((transferred * 100U) / total);
}

void FirmwareTask::mark_succeeded()
{
    state_ = "succeeded";
    progress_.stage = "succeeded";
    progress_.percent = 100;
}

void FirmwareTask::mark_failed(std::string error)
{
    state_ = "failed";
    progress_.stage = "failed";
    progress_.last_error = std::move(error);
    progress_.recoverable = false;
}

void FirmwareTask::mark_recoverable(std::string error)
{
    state_ = "recoverable";
    progress_.stage = "recoverable";
    progress_.last_error = std::move(error);
    progress_.recoverable = true;
}

} // namespace axent
```

Create `include/axent/logging/logger.hpp`:

```cpp
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace axent {

struct LogRecord {
    std::string channel;
    std::string message;
    nlohmann::json fields;
};

class Logger {
public:
    void core(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void audit(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    void adapter(const std::string& message, nlohmann::json fields = nlohmann::json::object());
    const std::vector<LogRecord>& records() const;

private:
    std::vector<LogRecord> records_;
};

} // namespace axent
```

Create `src/logging/logger.cpp`:

```cpp
#include "axent/logging/logger.hpp"

#include <utility>

namespace axent {

void Logger::core(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"core", message, std::move(fields)});
}

void Logger::audit(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"audit", message, std::move(fields)});
}

void Logger::adapter(const std::string& message, nlohmann::json fields)
{
    records_.push_back({"adapter", message, std::move(fields)});
}

const std::vector<LogRecord>& Logger::records() const
{
    return records_;
}

} // namespace axent
```

Create `include/axent/diagnostics/diagnostics.hpp`:

```cpp
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "axent/core/capability_registry.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

class Diagnostics {
public:
    Diagnostics(const DeviceManager& devices, const CapabilityRegistry& capabilities, const Logger& logger);
    nlohmann::json collect(const std::string& device_id, bool include_sensitive) const;

private:
    const DeviceManager& devices_;
    const CapabilityRegistry& capabilities_;
    const Logger& logger_;
};

} // namespace axent
```

Create `src/diagnostics/diagnostics.cpp`:

```cpp
#include "axent/diagnostics/diagnostics.hpp"

#include "axent/core/json.hpp"

namespace axent {

Diagnostics::Diagnostics(const DeviceManager& devices, const CapabilityRegistry& capabilities, const Logger& logger)
    : devices_(devices), capabilities_(capabilities), logger_(logger)
{
}

nlohmann::json Diagnostics::collect(const std::string& device_id, bool include_sensitive) const
{
    nlohmann::json capabilities = nlohmann::json::array();
    for (const auto& capability : capabilities_.all()) {
        capabilities.push_back(to_json(capability));
    }

    nlohmann::json audit_log = nlohmann::json::array();
    for (const auto& record : logger_.records()) {
        if (record.channel == "audit") {
            audit_log.push_back({{"message", record.message}, {"fields", record.fields}});
        }
    }

    nlohmann::json bundle = {
        {"sanitized", !include_sensitive},
        {"deviceId", device_id},
        {"capabilities", capabilities},
        {"auditLog", audit_log},
        {"flowControl", {{"dropped", 0}, {"paused", false}}},
        {"firmwareTasks", nlohmann::json::array()},
        {"platform", {{"product", "Axent"}}}
    };
    const auto device = devices_.get(device_id);
    bundle["device"] = device ? to_json(*device) : nlohmann::json(nullptr);
    if (include_sensitive) {
        bundle["debug"] = {{"sensitiveIncluded", true}};
    }
    return bundle;
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target diagnostics_config_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R diagnostics_config_test --output-on-failure
```

Expected: `diagnostics_config_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/config include/axent/diagnostics include/axent/firmware include/axent/logging src/config src/diagnostics src/firmware src/logging tests/diagnostics_config_test.cpp
git commit -m "feat: add config diagnostics logging and firmware task"
```

## Task 6: Route Manager, Middleware, Flow Control, And Broker

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/route_manager.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/middleware.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/flow_control.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/core/broker.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/route_manager.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/middleware.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/flow_control.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/core/broker.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/broker_flow_test.cpp`

- [ ] **Step 1: Write the failing broker flow test**

Create `tests/broker_flow_test.cpp`:

```cpp
#include <cassert>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/logging/logger.hpp"

int main()
{
    axent::MockAdapter adapter;
    axent::DeviceManager devices;
    for (const auto& device : adapter.discover()) {
        devices.upsert(device);
    }

    axent::RouteManager routes(devices);
    axent::Logger logger;
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(&adapter);

    axent::ControlCommand command;
    command.request_id = "req-1";
    command.method = "status.get";
    command.device_id = "mock-device-001";
    const auto result = broker.dispatch(command);
    assert(result.status == axent::ControlStatus::Ok);
    assert(result.body.at("health") == "ok");
    assert(logger.records().size() >= 1);
    assert(!flow.snapshot().paused);
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add these sources to `libaxent`:

```cmake
    src/core/route_manager.cpp
    src/core/middleware.cpp
    src/core/flow_control.cpp
    src/core/broker.cpp
```

Add this test:

```cmake
    add_executable(broker_flow_test tests/broker_flow_test.cpp)
    target_link_libraries(broker_flow_test PRIVATE axent::libaxent)
    axent_enable_warnings(broker_flow_test)
    add_test(NAME broker_flow_test COMMAND broker_flow_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target broker_flow_test
```

Expected: build fails because broker headers do not exist.

- [ ] **Step 3: Implement routing, middleware, flow control, and broker**

Create `include/axent/core/route_manager.hpp`:

```cpp
#pragma once

#include <optional>
#include <string>

#include "axent/core/device_manager.hpp"

namespace axent {

struct RouteTarget {
    std::string adapter;
    std::string device_id;
};

class RouteManager {
public:
    explicit RouteManager(const DeviceManager& devices);
    std::optional<RouteTarget> resolve(const std::string& device_id) const;

private:
    const DeviceManager& devices_;
};

} // namespace axent
```

Create `src/core/route_manager.cpp`:

```cpp
#include "axent/core/route_manager.hpp"

namespace axent {

RouteManager::RouteManager(const DeviceManager& devices)
    : devices_(devices)
{
}

std::optional<RouteTarget> RouteManager::resolve(const std::string& device_id) const
{
    const auto device = devices_.get(device_id);
    if (!device || !device->connection.online) {
        return std::nullopt;
    }
    return RouteTarget{device->adapter, device->id};
}

} // namespace axent
```

Create `include/axent/core/middleware.hpp`:

```cpp
#pragma once

#include "axent/core/types.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

class Middleware {
public:
    explicit Middleware(Logger& logger);
    void before_dispatch(const ControlCommand& command);
    void after_dispatch(const ControlCommand& command, const ControlResult& result);

private:
    Logger& logger_;
};

} // namespace axent
```

Create `src/core/middleware.cpp`:

```cpp
#include "axent/core/middleware.hpp"

#include "axent/core/json.hpp"

namespace axent {

Middleware::Middleware(Logger& logger)
    : logger_(logger)
{
}

void Middleware::before_dispatch(const ControlCommand& command)
{
    logger_.audit("control.request", {
        {"requestId", command.request_id},
        {"method", command.method},
        {"deviceId", command.device_id},
        {"source", protocol_source_name(command.source)}
    });
}

void Middleware::after_dispatch(const ControlCommand& command, const ControlResult& result)
{
    logger_.audit("control.response", {
        {"requestId", command.request_id},
        {"method", command.method},
        {"status", control_status_name(result.status)}
    });
}

} // namespace axent
```

Create `include/axent/core/flow_control.hpp`:

```cpp
#pragma once

namespace axent {

struct FlowControlSnapshot {
    bool paused = false;
    int dropped = 0;
};

class FlowControl {
public:
    void pause();
    void resume();
    void record_drop();
    FlowControlSnapshot snapshot() const;

private:
    FlowControlSnapshot snapshot_;
};

} // namespace axent
```

Create `src/core/flow_control.cpp`:

```cpp
#include "axent/core/flow_control.hpp"

namespace axent {

void FlowControl::pause() { snapshot_.paused = true; }
void FlowControl::resume() { snapshot_.paused = false; }
void FlowControl::record_drop() { ++snapshot_.dropped; }
FlowControlSnapshot FlowControl::snapshot() const { return snapshot_; }

} // namespace axent
```

Create `include/axent/core/broker.hpp`:

```cpp
#pragma once

#include <map>
#include <string>

#include "axent/core/adapter.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"

namespace axent {

class Broker {
public:
    Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control);
    void register_adapter(Adapter* adapter);
    ControlResult dispatch(const ControlCommand& command);

private:
    RouteManager& routes_;
    Middleware& middleware_;
    FlowControl& flow_control_;
    std::map<std::string, Adapter*> adapters_;
};

} // namespace axent
```

Create `src/core/broker.cpp`:

```cpp
#include "axent/core/broker.hpp"

namespace axent {

Broker::Broker(RouteManager& routes, Middleware& middleware, FlowControl& flow_control)
    : routes_(routes), middleware_(middleware), flow_control_(flow_control)
{
}

void Broker::register_adapter(Adapter* adapter)
{
    if (adapter != nullptr) {
        adapters_[adapter->metadata().name] = adapter;
    }
}

ControlResult Broker::dispatch(const ControlCommand& command)
{
    middleware_.before_dispatch(command);
    ControlResult result;
    const auto target = routes_.resolve(command.device_id);
    if (!target) {
        result = {ControlStatus::NotFound, {{"error", "route not found"}}};
    } else {
        auto adapter = adapters_.find(target->adapter);
        if (adapter == adapters_.end()) {
            result = {ControlStatus::Unavailable, {{"error", "adapter unavailable"}}};
        } else if (command.method == "firmware.update") {
            result = adapter->second->start_firmware_update(target->device_id, command.params.value("file", ""));
        } else {
            result = adapter->second->call(target->device_id, command.method, command.params);
        }
    }
    if (flow_control_.snapshot().paused) {
        flow_control_.record_drop();
    }
    middleware_.after_dispatch(command, result);
    return result;
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target broker_flow_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R broker_flow_test --output-on-failure
```

Expected: `broker_flow_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/core src/core tests/broker_flow_test.cpp
git commit -m "feat: add broker route middleware and flow control"
```

## Task 7: AXTP/JSON-RPC And Legacy Protocol Codecs

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/control/protocol_codecs.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/control/protocol_codecs.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/protocol_codecs_test.cpp`

- [ ] **Step 1: Write the failing dual-codec test**

Create `tests/protocol_codecs_test.cpp`:

```cpp
#include <cassert>

#include "axent/control/protocol_codecs.hpp"

int main()
{
    const auto json_rpc = axent::decode_control_message({
        {"jsonrpc", "2.0"},
        {"id", "jr-1"},
        {"method", "status.get"},
        {"params", {{"deviceId", "mock-device-001"}}}
    });
    assert(json_rpc.command.method == "status.get");
    assert(json_rpc.command.device_id == "mock-device-001");
    assert(json_rpc.command.source == axent::ProtocolSource::JsonRpc);

    const auto legacy = axent::decode_control_message({
        {"op", 7},
        {"sid", 123},
        {"d", {
            {"id", "legacy-1"},
            {"method", "GetDeviceList"},
            {"params", nlohmann::json::object()}
        }}
    });
    assert(legacy.command.method == "devices.list");
    assert(legacy.command.source == axent::ProtocolSource::LegacyOp);

    axent::ControlResult result;
    result.status = axent::ControlStatus::Ok;
    result.body = {{"devices", nlohmann::json::array()}};
    const auto legacy_response = axent::encode_control_response(legacy, result);
    assert(legacy_response.at("op") == 8);
    assert(legacy_response.at("d").at("method") == "GetDeviceList");

    const auto json_response = axent::encode_control_response(json_rpc, result);
    assert(json_response.at("jsonrpc") == "2.0");
    assert(json_response.at("result").at("devices").is_array());
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add `src/control/protocol_codecs.cpp` to `libaxent`.

Add this test:

```cmake
    add_executable(protocol_codecs_test tests/protocol_codecs_test.cpp)
    target_link_libraries(protocol_codecs_test PRIVATE axent::libaxent)
    axent_enable_warnings(protocol_codecs_test)
    add_test(NAME protocol_codecs_test COMMAND protocol_codecs_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target protocol_codecs_test
```

Expected: build fails because protocol codec files do not exist.

- [ ] **Step 3: Implement protocol codecs**

Create `include/axent/control/protocol_codecs.hpp`:

```cpp
#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "axent/core/types.hpp"

namespace axent {

struct DecodedControlMessage {
    ControlCommand command;
    std::string wire_method;
    nlohmann::json original;
};

DecodedControlMessage decode_control_message(const nlohmann::json& message);
nlohmann::json encode_control_response(const DecodedControlMessage& decoded, const ControlResult& result);

} // namespace axent
```

Create `src/control/protocol_codecs.cpp`:

```cpp
#include "axent/control/protocol_codecs.hpp"

#include "axent/core/json.hpp"

namespace axent {
namespace {

std::string map_legacy_method(const std::string& method)
{
    static const std::map<std::string, std::string> mapping = {
        {"GetDeviceList", "devices.list"},
        {"GetDeviceInfo", "device.info"},
        {"StartDeviceUpgrade", "firmware.update"},
        {"StartAgentUpgrade", "agent.update"},
        {"StartDeviceReboot", "control.reboot"},
        {"StartAgentReboot", "agent.reboot"},
        {"SetDeviceMute", "audio.mute.set"},
        {"GetDeviceMute", "audio.mute.get"},
        {"SetDeviceVolume", "audio.volume.set"},
        {"GetDeviceVolume", "audio.volume.get"}
    };
    auto found = mapping.find(method);
    return found == mapping.end() ? method : found->second;
}

} // namespace

DecodedControlMessage decode_control_message(const nlohmann::json& message)
{
    DecodedControlMessage decoded;
    decoded.original = message;

    if (message.contains("jsonrpc")) {
        decoded.command.source = ProtocolSource::JsonRpc;
        decoded.command.request_id = message.value("id", "");
        decoded.command.method = message.value("method", "");
        decoded.command.params = message.value("params", nlohmann::json::object());
        decoded.command.device_id = decoded.command.params.value("deviceId", "");
        decoded.wire_method = decoded.command.method;
        return decoded;
    }

    decoded.command.source = ProtocolSource::LegacyOp;
    const auto d = message.value("d", nlohmann::json::object());
    decoded.command.request_id = d.value("id", "");
    decoded.wire_method = d.value("method", "");
    decoded.command.method = map_legacy_method(decoded.wire_method);
    decoded.command.params = d.value("params", nlohmann::json::object());
    decoded.command.device_id = decoded.command.params.value("serialNumber", decoded.command.params.value("deviceId", ""));
    return decoded;
}

nlohmann::json encode_control_response(const DecodedControlMessage& decoded, const ControlResult& result)
{
    if (decoded.command.source == ProtocolSource::JsonRpc) {
        if (result.status == ControlStatus::Ok || result.status == ControlStatus::Accepted) {
            return {{"jsonrpc", "2.0"}, {"id", decoded.command.request_id}, {"result", result.body}};
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", decoded.command.request_id},
            {"error", {{"message", control_status_name(result.status)}, {"data", result.body}}}
        };
    }

    return {
        {"op", 8},
        {"sid", decoded.original.value("sid", 0)},
        {"d", {
            {"id", decoded.command.request_id},
            {"method", decoded.wire_method},
            {"status", {
                {"result", result.status == ControlStatus::Ok || result.status == ControlStatus::Accepted},
                {"code", result.status == ControlStatus::Ok || result.status == ControlStatus::Accepted ? 100 : 500},
                {"comment", control_status_name(result.status)}
            }},
            {"result", result.body}
        }}
    };
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target protocol_codecs_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R protocol_codecs_test --output-on-failure
```

Expected: `protocol_codecs_test` passes.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/control src/control tests/protocol_codecs_test.cpp
git commit -m "feat: add dual protocol codecs"
```

## Task 8: Control Plane, WebSocket Server, Daemon, And CLI

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/control/control_plane.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/control/websocket_server.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/control/control_plane.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/control/websocket_server.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/daemon/main.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/cli/main.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/cli_smoke_test.cpp`

- [ ] **Step 1: Write the failing control plane and CLI smoke test**

Create `tests/cli_smoke_test.cpp`:

```cpp
#include <array>
#include <cassert>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define AXENT_POPEN _popen
#define AXENT_PCLOSE _pclose
#else
#define AXENT_POPEN popen
#define AXENT_PCLOSE pclose
#endif

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string command = std::string(argv[1]) + " status --offline";
    std::array<char, 256> buffer{};
    std::string output;
    FILE* pipe = AXENT_POPEN(command.c_str(), "r");
    assert(pipe != nullptr);
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int code = AXENT_PCLOSE(pipe);
    assert(code == 0);
    assert(output.find("axentd") != std::string::npos);
    assert(output.find("offline") != std::string::npos);
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add to `libaxent`:

```cmake
    src/control/control_plane.cpp
    src/control/websocket_server.cpp
```

After `libaxent`, add:

```cmake
add_executable(axentd src/daemon/main.cpp)
target_link_libraries(axentd PRIVATE axent::libaxent axtp::transport_websocket_ix)
axent_enable_warnings(axentd)

add_executable(axent src/cli/main.cpp)
target_link_libraries(axent PRIVATE axent::libaxent)
axent_enable_warnings(axent)
```

Inside `if(BUILD_TESTING)`, add:

```cmake
    add_executable(cli_smoke_test tests/cli_smoke_test.cpp)
    axent_enable_warnings(cli_smoke_test)
    add_test(NAME cli_smoke_test COMMAND cli_smoke_test $<TARGET_FILE:axent>)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target cli_smoke_test
```

Expected: build fails because daemon and CLI files do not exist.

- [ ] **Step 3: Implement control plane and WebSocket server**

Create `include/axent/control/control_plane.hpp`:

```cpp
#pragma once

#include <nlohmann/json.hpp>

#include "axent/control/protocol_codecs.hpp"
#include "axent/core/broker.hpp"

namespace axent {

class ControlPlane {
public:
    explicit ControlPlane(Broker& broker);
    nlohmann::json handle_text(const nlohmann::json& message);

private:
    Broker& broker_;
};

} // namespace axent
```

Create `src/control/control_plane.cpp`:

```cpp
#include "axent/control/control_plane.hpp"

namespace axent {

ControlPlane::ControlPlane(Broker& broker)
    : broker_(broker)
{
}

nlohmann::json ControlPlane::handle_text(const nlohmann::json& message)
{
    auto decoded = decode_control_message(message);
    const auto result = broker_.dispatch(decoded.command);
    return encode_control_response(decoded, result);
}

} // namespace axent
```

Create `include/axent/control/websocket_server.hpp`:

```cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "axent/control/control_plane.hpp"

namespace axent {

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    bool start(ControlPlane& control_plane, const std::string& bind_host, std::uint16_t port);
    void stop();
    std::uint16_t local_port() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
```

Create `src/control/websocket_server.cpp`:

```cpp
#include "axent/control/websocket_server.hpp"

#include <chrono>

#include <nlohmann/json.hpp>

#include "core/runtime/transport/transport.hpp"
#include "transports/websocket/ix/websocket_transport.hpp"

namespace axent {
namespace {

class ControlPlaneSink final : public axtp::IByteSink {
public:
    ControlPlaneSink(ControlPlane& control_plane, axtp::WebSocketTransport& transport)
        : control_plane_(control_plane), transport_(transport)
    {
    }

    void onBytes(const axtp::Byte* data, std::size_t size) override
    {
        const auto request = nlohmann::json::parse(std::string(reinterpret_cast<const char*>(data), size), nullptr, false);
        if (request.is_discarded()) {
            return;
        }
        const auto response = control_plane_.handle_text(request).dump();
        transport_.sendBytes(reinterpret_cast<const axtp::Byte*>(response.data()), response.size());
    }

private:
    ControlPlane& control_plane_;
    axtp::WebSocketTransport& transport_;
};

} // namespace

struct WebSocketServer::Impl {
    std::unique_ptr<axtp::WebSocketTransport> transport;
    std::unique_ptr<ControlPlaneSink> sink;
    std::thread poll_thread;
    std::atomic<bool> running{false};

    void stop()
    {
        running.store(false);
        if (poll_thread.joinable()) {
            poll_thread.join();
        }
        if (transport) {
            transport->close();
        }
        sink.reset();
        transport.reset();
    }
};

WebSocketServer::WebSocketServer()
    : impl_(std::make_unique<Impl>())
{
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(ControlPlane& control_plane, const std::string& bind_host, std::uint16_t port)
{
    stop();
    impl_->transport = std::make_unique<axtp::WebSocketTransport>(port, bind_host.c_str());
    impl_->sink = std::make_unique<ControlPlaneSink>(control_plane, *impl_->transport);
    impl_->transport->bind(*impl_->sink);
    impl_->transport->open();
    if (impl_->transport->localPort() == 0) {
        stop();
        return false;
    }
    impl_->running.store(true);
    impl_->poll_thread = std::thread([this]() {
        while (impl_->running.load()) {
            impl_->transport->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    return true;
}

void WebSocketServer::stop()
{
    impl_->stop();
}

std::uint16_t WebSocketServer::local_port() const
{
    return impl_->transport ? impl_->transport->localPort() : 0;
}

} // namespace axent
```

- [ ] **Step 4: Implement daemon and CLI**

Create `src/daemon/main.cpp`:

```cpp
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/config/config.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/control/websocket_server.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/logging/logger.hpp"
#include "axent/version.hpp"

namespace {
std::atomic<bool> running{true};
void stop_signal(int) { running.store(false); }
}

int main(int argc, char** argv)
{
    bool foreground = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--foreground") {
            foreground = true;
        }
    }

    std::signal(SIGINT, stop_signal);
    std::signal(SIGTERM, stop_signal);

    axent::AxentConfig config = axent::AxentConfig::dev_trial_defaults();
    axent::Logger logger;
    axent::MockAdapter mock;
    axent::DeviceManager devices;
    for (const auto& device : mock.discover()) {
        devices.upsert(device);
    }
    axent::RouteManager routes(devices);
    axent::Middleware middleware(logger);
    axent::FlowControl flow;
    axent::Broker broker(routes, middleware, flow);
    broker.register_adapter(&mock);
    axent::ControlPlane control_plane(broker);
    axent::WebSocketServer server;
    if (!server.start(control_plane, config.server.bind_host, static_cast<std::uint16_t>(config.server.port))) {
        std::cerr << "failed to start axentd websocket server\n";
        return 2;
    }

    std::cout << axent::product_name() << " " << axent::version()
              << " axentd listening on " << config.server.bind_host
              << ":" << server.local_port() << "\n";

    if (!foreground) {
        std::cout << "service mode scaffold active\n";
    }

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    server.stop();
    std::cout << "axentd stopped\n";
    return 0;
}
```

Create `src/cli/main.cpp`:

```cpp
#include <iostream>
#include <string>

#include "axent/version.hpp"

int main(int argc, char** argv)
{
    const std::string command = argc >= 2 ? argv[1] : "help";
    if (command == "status") {
        const bool offline = argc >= 3 && std::string(argv[2]) == "--offline";
        std::cout << "axentd status: " << (offline ? "offline" : "unknown") << "\n";
        return 0;
    }
    if (command == "list") {
        std::cout << "axent list: connect to axentd websocket for live devices\n";
        return 0;
    }
    if (command == "reload") {
        std::cout << "axent reload: requested\n";
        return 0;
    }
    if (command == "diagnostics") {
        std::cout << "axent diagnostics: requested\n";
        return 0;
    }
    std::cout << axent::product_name() << " " << axent::version()
              << "\nusage: axent status|list|reload|diagnostics\n";
    return command == "help" ? 0 : 1;
}
```

- [ ] **Step 5: Run build, tests, and foreground smoke**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target axent axentd cli_smoke_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R cli_smoke_test --output-on-failure
/Users/qing/Desktop/sources/gitee/Axent/build/axent status --offline
```

Expected:

```text
axentd status: offline
```

Manual daemon smoke:

```bash
/Users/qing/Desktop/sources/gitee/Axent/build/axentd --foreground
```

Expected: output includes `Axent 0.1.0-dev axentd listening on 0.0.0.0:6060`. Stop with `Ctrl+C`; output includes `axentd stopped`.

- [ ] **Step 6: Commit**

Run:

```bash
git add CMakeLists.txt include/axent/control src/control src/daemon src/cli tests/cli_smoke_test.cpp
git commit -m "feat: add axentd control plane and axent cli"
```

## Task 9: AXDP, AXTP, And TEA Adapter Skeletons

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/adapters/axdp_adapter.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/adapters/axtp_adapter.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/include/axent/adapters/tea_adapter.hpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/adapters/axdp_adapter.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/adapters/axtp_adapter.cpp`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/src/adapters/tea_adapter.cpp`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/CMakeLists.txt`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/cmake/AxentDependencies.cmake`
- Test: `/Users/qing/Desktop/sources/gitee/Axent/tests/adapter_skeleton_test.cpp`

- [ ] **Step 1: Write the failing adapter skeleton test**

Create `tests/adapter_skeleton_test.cpp`:

```cpp
#include <cassert>

#include "axent/adapters/axdp_adapter.hpp"
#include "axent/adapters/axtp_adapter.hpp"
#include "axent/adapters/tea_adapter.hpp"

int main()
{
    axent::AxdpAdapter axdp;
    axent::AxtpAdapter axtp;
    axent::TeaAdapter tea;

    assert(axdp.metadata().name == "axdp");
    assert(axtp.metadata().name == "axtp");
    assert(tea.metadata().name == "tea");
    assert(axtp.metadata().available);
    assert(!axdp.capabilities().empty());
    assert(!axtp.capabilities().empty());
    assert(!tea.capabilities().empty());

    const auto axdp_result = axdp.call("missing", "status.get", {});
    assert(axdp_result.status == axent::ControlStatus::Unavailable);
    return 0;
}
```

- [ ] **Step 2: Register and run the failing test**

Add these sources to `libaxent`:

```cmake
    src/adapters/axdp_adapter.cpp
    src/adapters/axtp_adapter.cpp
    src/adapters/tea_adapter.cpp
```

Add CMake includes after `target_link_libraries(libaxent ...)`:

```cmake
if(AXENT_HAS_AXDP_HEADERS)
    target_include_directories(libaxent PRIVATE third_party/axdp/include)
endif()

if(AXENT_HAS_TEA_MACOS_SDK)
    target_include_directories(libaxent PRIVATE third_party/tea/macos/TeaSdkMacOS_1.0.0.36/include)
endif()

target_link_libraries(libaxent PUBLIC axtp::runtime axtp::sdk)
```

Add this test:

```cmake
    add_executable(adapter_skeleton_test tests/adapter_skeleton_test.cpp)
    target_link_libraries(adapter_skeleton_test PRIVATE axent::libaxent)
    axent_enable_warnings(adapter_skeleton_test)
    add_test(NAME adapter_skeleton_test COMMAND adapter_skeleton_test)
```

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target adapter_skeleton_test
```

Expected: build fails because adapter skeleton headers do not exist.

- [ ] **Step 3: Implement adapter skeletons**

Create `include/axent/adapters/axdp_adapter.hpp`, `include/axent/adapters/axtp_adapter.hpp`, and `include/axent/adapters/tea_adapter.hpp` with the same structure, changing the class name:

```cpp
#pragma once

#include "axent/core/adapter.hpp"

namespace axent {

class AxdpAdapter final : public Adapter {
public:
    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;
};

} // namespace axent
```

For `axtp_adapter.hpp`, use `class AxtpAdapter final`. For `tea_adapter.hpp`, use `class TeaAdapter final`.

Create `src/adapters/axdp_adapter.cpp`:

```cpp
#include "axent/adapters/axdp_adapter.hpp"

namespace axent {

AdapterMetadata AxdpAdapter::metadata() const
{
    return {"axdp", "AXDP Adapter", false, "real AXDP device session not connected in skeleton"};
}

std::vector<Capability> AxdpAdapter::capabilities() const
{
    return {{"axdp.device", "axdp", false, "AXDP runtime wiring pending", {{"status.get", RiskLevel::Safe, false, false}, {"firmware.update", RiskLevel::Dangerous, true, true}}, {"axdp.device.changed"}}};
}

std::vector<DeviceSnapshot> AxdpAdapter::discover()
{
    return {};
}

ControlResult AxdpAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "AXDP adapter skeleton only"}}};
}

ControlResult AxdpAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "AXDP firmware update skeleton only"}}};
}

} // namespace axent
```

Create `src/adapters/axtp_adapter.cpp`:

```cpp
#include "axent/adapters/axtp_adapter.hpp"

#include "axtp_runtime.hpp"
#include "axtp_sdk.hpp"

namespace axent {

AdapterMetadata AxtpAdapter::metadata() const
{
    return {"axtp", "AXTP Runtime Adapter", true, ""};
}

std::vector<Capability> AxtpAdapter::capabilities() const
{
    return {{"axtp.runtime", "axtp", true, "", {{"status.get", RiskLevel::Safe, false, false}, {"stream.flowControl.get", RiskLevel::Safe, false, false}, {"firmware.update", RiskLevel::Dangerous, true, true}}, {"axtp.session.changed"}}};
}

std::vector<DeviceSnapshot> AxtpAdapter::discover()
{
    return {};
}

ControlResult AxtpAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "AXTP device session not open"}}};
}

ControlResult AxtpAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "AXTP firmware update skeleton only"}}};
}

} // namespace axent
```

Create `src/adapters/tea_adapter.cpp`:

```cpp
#include "axent/adapters/tea_adapter.hpp"

namespace axent {

AdapterMetadata TeaAdapter::metadata() const
{
    return {"tea", "TEA Adapter", false, "TEA SDK wrapper skeleton only"};
}

std::vector<Capability> TeaAdapter::capabilities() const
{
    return {{"tea.device", "tea", false, "TEA SDK wrapper skeleton only", {{"status.get", RiskLevel::Safe, false, false}, {"audio.mute.get", RiskLevel::Safe, false, false}, {"firmware.update", RiskLevel::Dangerous, true, true}}, {"tea.device.changed"}}};
}

std::vector<DeviceSnapshot> TeaAdapter::discover()
{
    return {};
}

ControlResult TeaAdapter::call(const std::string&, const std::string&, const nlohmann::json&)
{
    return {ControlStatus::Unavailable, {{"error", "TEA adapter skeleton only"}}};
}

ControlResult TeaAdapter::start_firmware_update(const std::string&, const std::string&)
{
    return {ControlStatus::Unavailable, {{"error", "TEA firmware update skeleton only"}}};
}

} // namespace axent
```

- [ ] **Step 4: Run the test**

Run:

```bash
cmake --build /Users/qing/Desktop/sources/gitee/Axent/build --target adapter_skeleton_test
ctest --test-dir /Users/qing/Desktop/sources/gitee/Axent/build -R adapter_skeleton_test --output-on-failure
```

Expected: `adapter_skeleton_test` passes and proves `axtp-cpp-runtime` headers link through the AXTP adapter skeleton.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt cmake/AxentDependencies.cmake include/axent/adapters src/adapters tests/adapter_skeleton_test.cpp
git commit -m "feat: add axdp axtp and tea adapter skeletons"
```

## Task 10: Service Assets, README Update, And Release Gate

**Files:**
- Create: `/Users/qing/Desktop/sources/gitee/Axent/packaging/windows/axentd-service.xml`
- Create: `/Users/qing/Desktop/sources/gitee/Axent/packaging/macos/com.mostorm.axentd.plist`
- Modify: `/Users/qing/Desktop/sources/gitee/Axent/README.md`

- [ ] **Step 1: Add Windows service manifest**

Create `packaging/windows/axentd-service.xml`:

```xml
<service>
  <id>axentd</id>
  <name>Axent Daemon</name>
  <description>Axtp Endpoint Agent daemon</description>
  <executable>%BASE%\axentd.exe</executable>
  <arguments></arguments>
  <log mode="roll-by-size">
    <sizeThreshold>10485760</sizeThreshold>
    <keepFiles>5</keepFiles>
  </log>
</service>
```

- [ ] **Step 2: Add macOS launchd plist**

Create `packaging/macos/com.mostorm.axentd.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.mostorm.axentd</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/axentd</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/Library/Logs/axentd/stdout.log</string>
    <key>StandardErrorPath</key>
    <string>/Library/Logs/axentd/stderr.log</string>
</dict>
</plist>
```

- [ ] **Step 3: Update README**

Append this content to `README.md`:

```markdown
## New Axent Mainline

The legacy implementation remains under `agent/` and is not used as the architecture base for the new core.

New targets:

- `libaxent`: core library
- `axentd`: daemon and WebSocket control plane
- `axent`: local management CLI

Developer smoke:

    git submodule update --init --recursive
    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build
    ctest --test-dir build --output-on-failure
    build/axent status --offline
    build/axentd --foreground

The development profile binds WebSocket to LAN by default and has no authentication. Use only on trusted networks until the hardening stage adds authentication and authorization.
```

- [ ] **Step 4: Run full verification**

Run:

```bash
cd /Users/qing/Desktop/sources/gitee/Axent
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
build/axent status --offline
```

Expected:

```text
100% tests passed
axentd status: offline
```

Run daemon smoke manually:

```bash
build/axentd --foreground
```

Expected: output includes `axentd listening on 0.0.0.0:6060`. Stop with `Ctrl+C`.

- [ ] **Step 5: Commit**

Run:

```bash
git add README.md packaging
git commit -m "docs: add axent service assets and smoke instructions"
```

## Final Release Gate

Run from `/Users/qing/Desktop/sources/gitee/Axent`:

```bash
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
build/axent status --offline
```

Expected:

- All CTest tests pass.
- `build/axent status --offline` prints `axentd status: offline`.
- `build/axentd --foreground` starts a WebSocket server on `0.0.0.0:6060`.
- `agent/` remains present and untouched.
- `.superpowers/` remains ignored.
- `third_party/axtp-cpp-runtime` is a git submodule pointing at `https://github.com/Mostorm-Labs/axtp-cpp-runtime.git`.
- AXDP, AXTP, and TEA adapters expose capability metadata.
- Real-device adapter calls return `unavailable` unless implemented in a dedicated hardware integration task.

## Plan Self-Review

Spec coverage:

- Legacy `agent/` side-by-side preservation: Task 1 and final gate.
- Root C++ project under `include/`, `src/`, and `tests/`: Task 1.
- Third-party layout, AXDP/AXTP vendor snapshot handling, and `axtp-cpp-runtime` submodule: Task 1.
- Naming (`axent`, `axentd`, `libaxent`, namespace `axent`): Tasks 1 and 8.
- Core managers and registries: Tasks 2 and 3.
- Adapter contract and mock governance flow: Task 4.
- Firmware, config, diagnostics, logs: Task 5.
- Broker, middleware, route, flow control: Task 6.
- Dual protocol entry: Task 7.
- WebSocket server, daemon, CLI: Task 8.
- AXDP, AXTP, TEA skeletons: Task 9.
- Windows Service and macOS launchd assets: Task 10.

Placeholder scan:

- No placeholder markers are used.
- Every task has a concrete failing test, implementation content, verification command, and commit command.
- Real hardware work is represented as explicit skeleton behavior returning `unavailable`, not as an unspecified implementation gap.

Type consistency:

- `ControlCommand`, `ControlResult`, `Capability`, `DeviceSnapshot`, `Adapter`, `Broker`, and manager names are introduced before use.
- `axentd` is consistently the daemon target.
- `axent` is consistently the local management CLI target.
- Protocol codec methods map legacy `op/d/sid` names into core method names before routing.
