# Axent Daemon Embedded Media Relay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first implementation slice where `libaxent` is the shared core for both `axentd` and embedded product hosts, with Axent-owned concrete transport dependencies and an in-process encoded media relay.

**Architecture:** `libaxent` owns device/session/control/media relay logic. `axentd` constructs and hosts an `AxentHost`; NearCast can also link the same host API in embedded mode. Axent relays encoded media frames and preserves metadata; NearCast keeps `MediaCore` and all rendering policy.

**Tech Stack:** C++17, CMake 3.21, CTest, `axtp-cpp-runtime`, `hidapi`, `IXWebSocket`, nlohmann/json.

---

## Scope

This plan implements the reusable boundary from the design. It does not migrate NearCast, build production shared-memory IPC, or add production authentication. It creates the code shape needed for those follow-on changes.

The existing untracked `agent/` directory and `agent.zip` are legacy artifacts. Do not stage or modify them while executing this plan.

## File Structure

Create:

- `tests/cmake/verify_axent_dependency_boundary.cmake`: static CMake dependency-boundary check.
- `include/axent/transport/types.hpp`: public transport selector, descriptor, and diagnostics types.
- `include/axent/media/media_frame.hpp`: encoded media frame and metadata types.
- `include/axent/media/media_relay.hpp`: bounded in-process media relay API.
- `src/media/media_relay.cpp`: media relay implementation.
- `include/axent/host/axent_host.hpp`: top-level library host API for daemon and embedded mode.
- `src/host/axent_host.cpp`: host implementation using existing managers, broker, mock adapter, and media relay.
- `tests/transport_media_types_test.cpp`: type-level smoke test.
- `tests/media_relay_test.cpp`: bounded relay and drop/discontinuity test.
- `tests/axent_host_test.cpp`: host session acquisition and mock media consumption test.

Modify:

- `.gitmodules`: add top-level `third_party/hidapi` and `third_party/IXWebSocket`.
- `third_party/axtp-cpp-runtime`: move gitlink to the clean runtime-boundary commit.
- `cmake/AxentDependencies.cmake`: make Axent own HID/IXWebSocket before adding cpp-runtime.
- `CMakeLists.txt`: add new sources and tests; add dependency-boundary CTest.
- `src/daemon/main.cpp`: construct `AxentHost` instead of manually wiring core managers.
- `README.md`: document `libaxent` first, `axentd`, embedded host mode, and media relay boundary.
- `THIRD_PARTY_LOCK.md`: record new dependency pins.

## Task 1: Dependency Boundary Test

**Files:**
- Create: `tests/cmake/verify_axent_dependency_boundary.cmake`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing CMake boundary test**

Create `tests/cmake/verify_axent_dependency_boundary.cmake`:

```cmake
if(NOT DEFINED AXENT_REPO_ROOT)
    message(FATAL_ERROR "AXENT_REPO_ROOT is required")
endif()

set(axent_deps_file "${AXENT_REPO_ROOT}/cmake/AxentDependencies.cmake")
set(gitmodules_file "${AXENT_REPO_ROOT}/.gitmodules")

if(NOT EXISTS "${axent_deps_file}")
    message(FATAL_ERROR "Missing ${axent_deps_file}")
endif()
if(NOT EXISTS "${gitmodules_file}")
    message(FATAL_ERROR "Missing ${gitmodules_file}")
endif()

file(READ "${axent_deps_file}" axent_deps)
file(READ "${gitmodules_file}" axent_gitmodules)

function(assert_text_contains text pattern message)
    if(NOT "${text}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

function(assert_text_does_not_contain text pattern message)
    if("${text}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

assert_text_contains(
    "${axent_deps}"
    "add_subdirectory\\(third_party/IXWebSocket EXCLUDE_FROM_ALL\\)"
    "Axent must add IXWebSocket before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_deps}"
    "add_subdirectory\\(third_party/hidapi EXCLUDE_FROM_ALL\\)"
    "Axent must add hidapi before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_deps}"
    "function\\(axent_select_hidapi_target output_variable\\)"
    "Axent must normalize hidapi target names for cross-platform builds"
)
assert_text_contains(
    "${axent_deps}"
    "set\\(AXTP_CPP_RUNTIME_BUILD_TOOLS OFF CACHE BOOL \"\" FORCE\\)"
    "Axent product builds must not build cpp-runtime tools"
)
assert_text_contains(
    "${axent_deps}"
    "set\\(AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS OFF CACHE BOOL \"\" FORCE\\)"
    "Axent product builds must keep cpp-runtime tool dependency fetching off"
)
assert_text_does_not_contain(
    "${axent_deps}"
    "set\\(AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS ON"
    "Axent must not enable cpp-runtime tool dependency fetching"
)
assert_text_contains(
    "${axent_gitmodules}"
    "path = third_party/hidapi"
    "Axent must track hidapi as a top-level submodule"
)
assert_text_contains(
    "${axent_gitmodules}"
    "path = third_party/IXWebSocket"
    "Axent must track IXWebSocket as a top-level submodule"
)

foreach(required_file
        "third_party/hidapi/CMakeLists.txt"
        "third_party/IXWebSocket/CMakeLists.txt"
        "third_party/axtp-cpp-runtime/CMakeLists.txt")
    if(NOT EXISTS "${AXENT_REPO_ROOT}/${required_file}")
        message(FATAL_ERROR "Required dependency path is missing: ${required_file}")
    endif()
endforeach()

message(STATUS "Axent dependency boundary check passed")
```

- [ ] **Step 2: Register the boundary test**

In `CMakeLists.txt`, inside `if(BUILD_TESTING)` before the first executable test, add:

```cmake
    add_test(
        NAME axent_dependency_boundary
        COMMAND ${CMAKE_COMMAND}
            -DAXENT_REPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/verify_axent_dependency_boundary.cmake
    )
```

- [ ] **Step 3: Run the boundary test and verify it fails**

Run:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
ctest --test-dir build -R axent_dependency_boundary --output-on-failure
```

Expected: `axent_dependency_boundary` fails because Axent has not yet added top-level `hidapi` and `IXWebSocket`, and `cmake/AxentDependencies.cmake` still adds cpp-runtime before providing concrete transport dependencies.

- [ ] **Step 4: Commit the failing boundary test**

```bash
git add CMakeLists.txt tests/cmake/verify_axent_dependency_boundary.cmake
git commit -m "test: guard axent transport dependency boundary"
```

## Task 2: Axent-Owned Concrete Transport Dependencies

**Files:**
- Modify: `.gitmodules`
- Modify: `third_party/axtp-cpp-runtime`
- Create gitlinks: `third_party/hidapi`, `third_party/IXWebSocket`
- Modify: `cmake/AxentDependencies.cmake`
- Modify: `THIRD_PARTY_LOCK.md`

- [ ] **Step 1: Move cpp-runtime to the clean boundary commit**

Run:

```bash
git -C third_party/axtp-cpp-runtime fetch origin codex/clean-runtime-boundaries
git -C third_party/axtp-cpp-runtime checkout b8f927223476c3c9a89cbeea1a038668cf81a03c
```

Expected:

```bash
git -C third_party/axtp-cpp-runtime rev-parse HEAD
```

prints:

```text
b8f927223476c3c9a89cbeea1a038668cf81a03c
```

- [ ] **Step 2: Add top-level concrete dependency submodules**

Run:

```bash
git submodule add https://github.com/machinezone/IXWebSocket.git third_party/IXWebSocket
git -C third_party/IXWebSocket checkout 2efe037c9cc96fd536774f17bdb5215161ee5087
git submodule add https://github.com/libusb/hidapi.git third_party/hidapi
git -C third_party/hidapi checkout c3509c11174fe80ff59a47119433a7db5299af85
```

Expected:

```bash
git submodule status third_party/IXWebSocket third_party/hidapi
```

prints gitlinks for:

```text
2efe037c9cc96fd536774f17bdb5215161ee5087 third_party/IXWebSocket
c3509c11174fe80ff59a47119433a7db5299af85 third_party/hidapi
```

- [ ] **Step 3: Replace Axent dependency wiring**

Replace the transport/runtime section in `cmake/AxentDependencies.cmake` with this complete file content:

```cmake
set(AXENT_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

set(AXENT_BUILD_CONCRETE_TRANSPORT_DEPS ON CACHE BOOL "Build Axent-owned concrete transport dependencies")

function(axent_select_hidapi_target output_variable)
    foreach(candidate
            hidapi::hidapi
            hidapi::hidapi-shared
            hidapi::hidapi-static
            hidapi::hidapi-darwin
            hidapi::hidapi-hidraw
            hidapi::hidapi-libusb
            hidapi::darwin
            hidapi::hidraw
            hidapi::libusb)
        if(TARGET ${candidate})
            set(${output_variable} ${candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${output_variable} "" PARENT_SCOPE)
endfunction()

if(AXENT_BUILD_CONCRETE_TRANSPORT_DEPS)
    set(_axent_previous_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)

    set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    if(EXISTS "${AXENT_THIRD_PARTY_DIR}/IXWebSocket/CMakeLists.txt")
        add_subdirectory(third_party/IXWebSocket EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "Missing third_party/IXWebSocket. Run git submodule update --init --recursive.")
    endif()

    set(HIDAPI_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
    set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
    set(HIDAPI_WITH_TESTS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${AXENT_THIRD_PARTY_DIR}/hidapi/CMakeLists.txt")
        add_subdirectory(third_party/hidapi EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "Missing third_party/hidapi. Run git submodule update --init --recursive.")
    endif()
    axent_select_hidapi_target(AXENT_SELECTED_HIDAPI_TARGET)
    if(AXENT_SELECTED_HIDAPI_TARGET AND NOT TARGET hidapi::hidapi)
        add_library(hidapi::hidapi ALIAS ${AXENT_SELECTED_HIDAPI_TARGET})
    endif()

    set(BUILD_SHARED_LIBS "${_axent_previous_build_shared_libs}")
    unset(_axent_previous_build_shared_libs)
endif()

set(AXTP_CPP_RUNTIME_BUILD_SDK ON CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS OFF CACHE BOOL "" FORCE)
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

if(AXTP_CPP_RUNTIME_BUILD_TOOLS OR AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS)
    message(FATAL_ERROR "Axent owns concrete AXTP transport dependencies; do not enable cpp-runtime tool dependency fetching.")
endif()
if(AXENT_BUILD_CONCRETE_TRANSPORT_DEPS AND NOT TARGET ixwebsocket::ixwebsocket)
    message(FATAL_ERROR "Axent must provide ixwebsocket::ixwebsocket before adding axtp-cpp-runtime.")
endif()
if(AXENT_BUILD_CONCRETE_TRANSPORT_DEPS AND NOT TARGET hidapi::hidapi)
    message(FATAL_ERROR "Axent must provide hidapi::hidapi before adding axtp-cpp-runtime.")
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

- [ ] **Step 4: Update `THIRD_PARTY_LOCK.md`**

Add or update entries for these pins:

```markdown
| `third_party/axtp-cpp-runtime` | `https://github.com/Mostorm-Labs/axtp-cpp-runtime.git` | `b8f927223476c3c9a89cbeea1a038668cf81a03c` | Clean runtime boundary branch used by Axent. |
| `third_party/IXWebSocket` | `https://github.com/machinezone/IXWebSocket.git` | `2efe037c9cc96fd536774f17bdb5215161ee5087` | Axent-owned WebSocket concrete dependency. |
| `third_party/hidapi` | `https://github.com/libusb/hidapi.git` | `c3509c11174fe80ff59a47119433a7db5299af85` | Axent-owned HID concrete dependency. |
```

- [ ] **Step 5: Run dependency boundary verification**

Run:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
ctest --test-dir build -R axent_dependency_boundary --output-on-failure
```

Expected: `axent_dependency_boundary` passes.

- [ ] **Step 6: Commit dependency boundary implementation**

```bash
git add .gitmodules cmake/AxentDependencies.cmake THIRD_PARTY_LOCK.md third_party/axtp-cpp-runtime third_party/IXWebSocket third_party/hidapi
git commit -m "build: make axent own concrete transport deps"
```

## Task 3: Public Transport And Media Types

**Files:**
- Create: `include/axent/transport/types.hpp`
- Create: `include/axent/media/media_frame.hpp`
- Create: `tests/transport_media_types_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the type smoke test**

Create `tests/transport_media_types_test.cpp`:

```cpp
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "axent/media/media_frame.hpp"
#include "axent/transport/types.hpp"

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
    axent::TransportSelector selector;
    selector.kind = axent::TransportKind::Hid;
    selector.vendor_id = 0x0581;
    selector.product_id = 0x2581;
    selector.usage_page = 0x0081;
    selector.report_id = 0x05;
    selector.input_report_size = 0;
    selector.output_report_size = 0;

    require(selector.kind == axent::TransportKind::Hid, "selector kind mismatch");
    require(selector.input_report_size == 0, "zero input report size should mean auto");
    require(selector.output_report_size == 0, "zero output report size should mean auto");

    axent::TransportDescriptor descriptor;
    descriptor.id = "hid:mock";
    descriptor.kind = axent::TransportKind::Hid;
    descriptor.path = "mock-path";
    descriptor.vendor_id = selector.vendor_id;
    descriptor.product_id = selector.product_id;
    descriptor.online = true;

    require(descriptor.id == "hid:mock", "descriptor id mismatch");
    require(descriptor.online, "descriptor online mismatch");

    axent::MediaFrame frame;
    frame.session_id = "session-1";
    frame.stream_id = 0x1001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = 7;
    frame.cursor = 123456;
    frame.flags = axent::MediaFrameFlag::KeyFrame | axent::MediaFrameFlag::EndOfFrame;
    frame.payload = {0x00, 0x00, 0x01, 0x65};

    require(axent::has_flag(frame.flags, axent::MediaFrameFlag::KeyFrame), "keyframe flag missing");
    require(axent::has_flag(frame.flags, axent::MediaFrameFlag::EndOfFrame), "end-of-frame flag missing");
    require(!axent::has_flag(frame.flags, axent::MediaFrameFlag::Discontinuity), "unexpected discontinuity");
    require(frame.payload.size() == 4, "payload size mismatch");

    return 0;
}
```

- [ ] **Step 2: Register the test**

In `CMakeLists.txt`, inside `if(BUILD_TESTING)`, add:

```cmake
    add_executable(transport_media_types_test tests/transport_media_types_test.cpp)
    target_link_libraries(transport_media_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(transport_media_types_test)
    add_test(NAME transport_media_types_test COMMAND transport_media_types_test)
```

- [ ] **Step 3: Run the test and verify it fails**

Run:

```bash
cmake --build build --target transport_media_types_test
```

Expected: build fails because `axent/media/media_frame.hpp` and `axent/transport/types.hpp` do not exist.

- [ ] **Step 4: Add transport types**

Create `include/axent/transport/types.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace axent {

enum class TransportKind {
    Unknown,
    Hid,
    WebSocket,
    Tcp,
    Axdp,
    Tea,
    Mock,
};

struct TransportSelector {
    TransportKind kind = TransportKind::Unknown;
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    std::uint8_t report_id = 0;
    std::size_t input_report_size = 0;
    std::size_t output_report_size = 0;
    std::size_t read_buffer_size = 4096;
    std::size_t max_reports_per_poll = 32;
    std::string path;
    std::string serial_number;
};

struct TransportDescriptor {
    std::string id;
    TransportKind kind = TransportKind::Unknown;
    bool online = false;
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    int interface_number = -1;
    std::string path;
    std::string serial_number;
    std::string manufacturer;
    std::string product;
    std::string bus_type;
};

struct TransportDiagnostics {
    bool open = false;
    std::size_t negotiated_input_report_size = 0;
    std::size_t negotiated_output_report_size = 0;
    std::size_t read_buffer_size = 0;
    std::size_t preferred_frame_size = 0;
    std::uint64_t read_reports = 0;
    std::uint64_t write_reports = 0;
    std::uint64_t read_errors = 0;
    std::uint64_t write_errors = 0;
    std::uint64_t dropped_reports = 0;
    std::uint64_t queued_reports = 0;
    std::string last_event;
};

} // namespace axent
```

- [ ] **Step 5: Add media frame types**

Create `include/axent/media/media_frame.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace axent {

using Bytes = std::vector<std::uint8_t>;

enum class MediaKind {
    Unknown,
    Video,
    Audio,
};

enum class MediaCodec {
    Unknown,
    H264,
    Aac,
    Pcm,
    Opaque,
};

enum class MediaFrameFlag : std::uint32_t {
    None = 0,
    KeyFrame = 1U << 0U,
    Config = 1U << 1U,
    Discontinuity = 1U << 2U,
    EndOfFrame = 1U << 3U,
};

inline MediaFrameFlag operator|(MediaFrameFlag lhs, MediaFrameFlag rhs)
{
    return static_cast<MediaFrameFlag>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

inline MediaFrameFlag& operator|=(MediaFrameFlag& lhs, MediaFrameFlag rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool has_flag(MediaFrameFlag flags, MediaFrameFlag flag)
{
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

struct MediaFrame {
    std::string session_id;
    std::uint32_t stream_id = 0;
    MediaKind kind = MediaKind::Unknown;
    MediaCodec codec = MediaCodec::Unknown;
    std::uint64_t sequence_id = 0;
    std::uint64_t cursor = 0;
    std::uint64_t timestamp_us = 0;
    MediaFrameFlag flags = MediaFrameFlag::None;
    Bytes payload;
};

} // namespace axent
```

- [ ] **Step 6: Run the type test**

Run:

```bash
cmake --build build --target transport_media_types_test
ctest --test-dir build -R transport_media_types_test --output-on-failure
```

Expected: `transport_media_types_test` passes.

- [ ] **Step 7: Commit public types**

```bash
git add CMakeLists.txt include/axent/transport/types.hpp include/axent/media/media_frame.hpp tests/transport_media_types_test.cpp
git commit -m "feat: add axent transport and media frame types"
```

## Task 4: Bounded In-Process Media Relay

**Files:**
- Create: `include/axent/media/media_relay.hpp`
- Create: `src/media/media_relay.cpp`
- Create: `tests/media_relay_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the relay test**

Create `tests/media_relay_test.cpp`:

```cpp
#include <stdexcept>

#include "axent/media/media_relay.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

axent::MediaFrame make_frame(std::uint64_t sequence_id, std::size_t bytes)
{
    axent::MediaFrame frame;
    frame.session_id = "session-1";
    frame.stream_id = 0x1001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = sequence_id;
    frame.cursor = sequence_id * 1000U;
    frame.flags = sequence_id == 1 ? axent::MediaFrameFlag::KeyFrame : axent::MediaFrameFlag::EndOfFrame;
    frame.payload.assign(bytes, static_cast<std::uint8_t>(sequence_id));
    return frame;
}

} // namespace

int main()
{
    axent::MediaRelayOptions options;
    options.max_frames = 2;
    options.max_bytes = 0;

    axent::MediaStreamRelay relay(options);
    relay.publish(make_frame(1, 4));
    relay.publish(make_frame(2, 5));
    relay.publish(make_frame(3, 6));

    const auto stats_after_drop = relay.stats();
    require(stats_after_drop.published_frames == 3, "published frame count mismatch");
    require(stats_after_drop.dropped_frames == 1, "drop count mismatch");
    require(stats_after_drop.dropped_bytes == 4, "drop byte count mismatch");
    require(stats_after_drop.queued_frames == 2, "queued frame count mismatch");
    require(stats_after_drop.queued_bytes == 11, "queued byte count mismatch");

    auto first = relay.read();
    require(first.has_value(), "first frame missing");
    require(first->sequence_id == 2, "oldest retained frame should be sequence 2");
    require(axent::has_flag(first->flags, axent::MediaFrameFlag::Discontinuity),
            "first retained frame should carry discontinuity");

    auto second = relay.read();
    require(second.has_value(), "second frame missing");
    require(second->sequence_id == 3, "second retained frame should be sequence 3");

    auto empty = relay.read();
    require(!empty.has_value(), "relay should be empty");

    relay.close();
    require(relay.closed(), "relay should report closed");
    relay.publish(make_frame(4, 7));
    require(!relay.read().has_value(), "closed relay should not accept frames");

    return 0;
}
```

- [ ] **Step 2: Register relay source and test**

Add `src/media/media_relay.cpp` to the `libaxent` source list in `CMakeLists.txt`.

Inside `if(BUILD_TESTING)`, add:

```cmake
    add_executable(media_relay_test tests/media_relay_test.cpp)
    target_link_libraries(media_relay_test PRIVATE axent::libaxent)
    axent_enable_warnings(media_relay_test)
    add_test(NAME media_relay_test COMMAND media_relay_test)
```

- [ ] **Step 3: Run the test and verify it fails**

Run:

```bash
cmake --build build --target media_relay_test
```

Expected: build fails because `axent/media/media_relay.hpp` does not exist.

- [ ] **Step 4: Add relay API**

Create `include/axent/media/media_relay.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

#include "axent/media/media_frame.hpp"

namespace axent {

struct MediaRelayOptions {
    std::size_t max_frames = 64;
    std::size_t max_bytes = 16 * 1024 * 1024;
};

struct MediaRelayStats {
    std::uint64_t published_frames = 0;
    std::uint64_t read_frames = 0;
    std::uint64_t dropped_frames = 0;
    std::uint64_t dropped_bytes = 0;
    std::size_t queued_frames = 0;
    std::size_t queued_bytes = 0;
};

class MediaStreamRelay {
public:
    explicit MediaStreamRelay(MediaRelayOptions options = {});

    void publish(MediaFrame frame);
    std::optional<MediaFrame> read();
    void close();
    bool closed() const;
    MediaRelayStats stats() const;

private:
    bool over_limit_locked() const;
    void drop_front_locked();

    MediaRelayOptions options_;
    mutable std::mutex mutex_;
    std::deque<MediaFrame> queue_;
    std::size_t queued_bytes_ = 0;
    MediaRelayStats stats_;
    bool closed_ = false;
};

} // namespace axent
```

- [ ] **Step 5: Implement relay behavior**

Create `src/media/media_relay.cpp`:

```cpp
#include "axent/media/media_relay.hpp"

#include <algorithm>

namespace axent {

MediaStreamRelay::MediaStreamRelay(MediaRelayOptions options)
    : options_(options)
{
}

void MediaStreamRelay::publish(MediaFrame frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
        return;
    }

    queued_bytes_ += frame.payload.size();
    queue_.push_back(std::move(frame));
    ++stats_.published_frames;

    bool dropped = false;
    while (over_limit_locked() && !queue_.empty()) {
        drop_front_locked();
        dropped = true;
    }

    if (dropped && !queue_.empty()) {
        queue_.front().flags |= MediaFrameFlag::Discontinuity;
    }

    stats_.queued_frames = queue_.size();
    stats_.queued_bytes = queued_bytes_;
}

std::optional<MediaFrame> MediaStreamRelay::read()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }

    MediaFrame frame = std::move(queue_.front());
    queued_bytes_ -= frame.payload.size();
    queue_.pop_front();
    ++stats_.read_frames;
    stats_.queued_frames = queue_.size();
    stats_.queued_bytes = queued_bytes_;
    return frame;
}

void MediaStreamRelay::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    queue_.clear();
    queued_bytes_ = 0;
    stats_.queued_frames = 0;
    stats_.queued_bytes = 0;
}

bool MediaStreamRelay::closed() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

MediaRelayStats MediaStreamRelay::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    MediaRelayStats snapshot = stats_;
    snapshot.queued_frames = queue_.size();
    snapshot.queued_bytes = queued_bytes_;
    return snapshot;
}

bool MediaStreamRelay::over_limit_locked() const
{
    const bool frame_limited = options_.max_frames != 0 && queue_.size() > options_.max_frames;
    const bool byte_limited = options_.max_bytes != 0 && queued_bytes_ > options_.max_bytes;
    return frame_limited || byte_limited;
}

void MediaStreamRelay::drop_front_locked()
{
    const auto dropped_bytes = queue_.front().payload.size();
    queued_bytes_ -= dropped_bytes;
    queue_.pop_front();
    ++stats_.dropped_frames;
    stats_.dropped_bytes += dropped_bytes;
}

} // namespace axent
```

- [ ] **Step 6: Run relay test**

Run:

```bash
cmake --build build --target media_relay_test
ctest --test-dir build -R media_relay_test --output-on-failure
```

Expected: `media_relay_test` passes.

- [ ] **Step 7: Commit relay**

```bash
git add CMakeLists.txt include/axent/media/media_relay.hpp src/media/media_relay.cpp tests/media_relay_test.cpp
git commit -m "feat: add bounded media relay"
```

## Task 5: AxentHost Library API

**Files:**
- Create: `include/axent/host/axent_host.hpp`
- Create: `src/host/axent_host.cpp`
- Create: `tests/axent_host_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write host API test**

Create `tests/axent_host_test.cpp`:

```cpp
#include <stdexcept>

#include "axent/host/axent_host.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

axent::MediaFrame make_media_frame(std::uint64_t sequence_id)
{
    axent::MediaFrame frame;
    frame.session_id = "filled-by-test";
    frame.stream_id = 0x2001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = sequence_id;
    frame.cursor = sequence_id * 1000U;
    frame.flags = sequence_id == 1 ? axent::MediaFrameFlag::KeyFrame : axent::MediaFrameFlag::EndOfFrame;
    frame.payload = {0, 0, 1, static_cast<std::uint8_t>(sequence_id)};
    return frame;
}

} // namespace

int main()
{
    axent::AxentHost host;
    axent::AxentHostOptions options;
    options.enable_mock_adapter = true;
    require(host.start(options), "host should start");

    const auto devices = host.discover_devices();
    require(devices.size() == 1, "mock host should discover one device");
    require(devices[0].id == "mock-device-001", "mock device id mismatch");

    axent::SessionAcquireRequest request;
    request.client_id = "nearcast-test";
    request.device_id = "mock-device-001";
    request.media = true;
    const auto lease = host.acquire_session(request);
    require(lease.acquired, "media lease should be acquired");
    require(!lease.session_id.empty(), "session id should be assigned");

    auto denied_request = request;
    denied_request.client_id = "second-renderer";
    const auto denied = host.acquire_session(denied_request);
    require(!denied.acquired, "second media owner should be denied");
    require(denied.reason == "media lease busy", "denied reason mismatch");

    auto consumer = host.create_media_consumer(lease.session_id, {});
    require(consumer != nullptr, "media consumer should be created");

    auto frame = make_media_frame(1);
    frame.session_id = lease.session_id;
    require(host.publish_media_frame(lease.session_id, frame), "publish should succeed");

    const auto received = consumer->read();
    require(received.has_value(), "consumer should read a frame");
    require(received->sequence_id == 1, "received sequence mismatch");
    require(received->session_id == lease.session_id, "received session mismatch");

    const auto result = host.call(lease.session_id, "status.get", {});
    require(result.status == axent::ControlStatus::Ok, "host call should dispatch through broker");
    require(result.body.at("health") == "ok", "host call result mismatch");

    host.release_session(lease.session_id, "test complete");
    require(!host.publish_media_frame(lease.session_id, frame), "publish after release should fail");

    host.stop();
    return 0;
}
```

- [ ] **Step 2: Register host source and test**

Add `src/host/axent_host.cpp` to the `libaxent` source list in `CMakeLists.txt`.

Inside `if(BUILD_TESTING)`, add:

```cmake
    add_executable(axent_host_test tests/axent_host_test.cpp)
    target_link_libraries(axent_host_test PRIVATE axent::libaxent)
    axent_enable_warnings(axent_host_test)
    add_test(NAME axent_host_test COMMAND axent_host_test)
```

- [ ] **Step 3: Run the host test and verify it fails**

Run:

```bash
cmake --build build --target axent_host_test
```

Expected: build fails because `axent/host/axent_host.hpp` does not exist.

- [ ] **Step 4: Add host API header**

Create `include/axent/host/axent_host.hpp`:

```cpp
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/adapters/mock_adapter.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/core/session_manager.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_relay.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

struct AxentHostOptions {
    bool enable_mock_adapter = true;
};

struct SessionAcquireRequest {
    std::string client_id;
    std::string device_id;
    bool media = false;
};

struct SessionLease {
    bool acquired = false;
    std::string session_id;
    std::string device_id;
    std::string client_id;
    std::string reason;
};

class MediaConsumer {
public:
    explicit MediaConsumer(std::shared_ptr<MediaStreamRelay> relay);

    std::optional<MediaFrame> read();
    MediaRelayStats stats() const;

private:
    std::shared_ptr<MediaStreamRelay> relay_;
};

class AxentHost {
public:
    AxentHost();
    ~AxentHost();

    AxentHost(const AxentHost&) = delete;
    AxentHost& operator=(const AxentHost&) = delete;

    bool start(AxentHostOptions options = {});
    void stop();
    bool running() const;

    std::vector<DeviceSnapshot> discover_devices() const;
    SessionLease acquire_session(const SessionAcquireRequest& request);
    void release_session(const std::string& session_id, const std::string& reason);

    std::unique_ptr<MediaConsumer> create_media_consumer(const std::string& session_id,
                                                         MediaRelayOptions options);
    bool publish_media_frame(const std::string& session_id, MediaFrame frame);

    ControlResult call(const std::string& session_id,
                       const std::string& method,
                       const nlohmann::json& params);

    Broker& broker();

private:
    void reset();
    std::optional<SessionLease> lease_for_session(const std::string& session_id) const;

    bool running_ = false;
    AxentHostOptions options_;
    std::unique_ptr<Logger> logger_;
    std::unique_ptr<MockAdapter> mock_adapter_;
    std::unique_ptr<DeviceManager> devices_;
    std::unique_ptr<RouteManager> routes_;
    std::unique_ptr<Middleware> middleware_;
    std::unique_ptr<FlowControl> flow_;
    std::unique_ptr<Broker> broker_;
    SessionManager sessions_;
    std::map<std::string, SessionLease> leases_;
    std::map<std::string, std::shared_ptr<MediaStreamRelay>> relays_;
    std::string media_owner_client_id_;
};

} // namespace axent
```

- [ ] **Step 5: Implement host**

Create `src/host/axent_host.cpp`:

```cpp
#include "axent/host/axent_host.hpp"

#include <utility>

namespace axent {

MediaConsumer::MediaConsumer(std::shared_ptr<MediaStreamRelay> relay)
    : relay_(std::move(relay))
{
}

std::optional<MediaFrame> MediaConsumer::read()
{
    return relay_ ? relay_->read() : std::nullopt;
}

MediaRelayStats MediaConsumer::stats() const
{
    return relay_ ? relay_->stats() : MediaRelayStats{};
}

AxentHost::AxentHost() = default;

AxentHost::~AxentHost()
{
    stop();
}

bool AxentHost::start(AxentHostOptions options)
{
    stop();
    options_ = options;
    logger_ = std::make_unique<Logger>();
    devices_ = std::make_unique<DeviceManager>();
    routes_ = std::make_unique<RouteManager>(*devices_);
    middleware_ = std::make_unique<Middleware>(*logger_);
    flow_ = std::make_unique<FlowControl>();
    broker_ = std::make_unique<Broker>(*routes_, *middleware_, *flow_);

    if (options_.enable_mock_adapter) {
        mock_adapter_ = std::make_unique<MockAdapter>();
        for (const auto& device : mock_adapter_->discover()) {
            devices_->upsert(device);
        }
        broker_->register_adapter(*mock_adapter_);
    }

    running_ = true;
    return true;
}

void AxentHost::stop()
{
    if (!running_ && !broker_) {
        return;
    }
    reset();
}

bool AxentHost::running() const
{
    return running_;
}

std::vector<DeviceSnapshot> AxentHost::discover_devices() const
{
    return devices_ ? devices_->list() : std::vector<DeviceSnapshot>{};
}

SessionLease AxentHost::acquire_session(const SessionAcquireRequest& request)
{
    if (!running_ || !devices_) {
        return {false, "", request.device_id, request.client_id, "host not running"};
    }
    const auto device = devices_->get(request.device_id);
    if (!device.has_value()) {
        return {false, "", request.device_id, request.client_id, "device not found"};
    }
    if (request.media &&
        !media_owner_client_id_.empty() &&
        media_owner_client_id_ != request.client_id) {
        return {false, "", request.device_id, request.client_id, "media lease busy"};
    }

    const std::string session_id = sessions_.device().open(request.device_id, device->adapter);
    SessionLease lease{true, session_id, request.device_id, request.client_id, ""};
    leases_[session_id] = lease;
    if (request.media) {
        media_owner_client_id_ = request.client_id;
        relays_[session_id] = std::make_shared<MediaStreamRelay>();
    }
    return lease;
}

void AxentHost::release_session(const std::string& session_id, const std::string&)
{
    const auto lease = lease_for_session(session_id);
    if (lease.has_value() && lease->client_id == media_owner_client_id_) {
        media_owner_client_id_.clear();
    }
    const auto relay = relays_.find(session_id);
    if (relay != relays_.end()) {
        relay->second->close();
        relays_.erase(relay);
    }
    leases_.erase(session_id);
}

std::unique_ptr<MediaConsumer> AxentHost::create_media_consumer(const std::string& session_id,
                                                                MediaRelayOptions options)
{
    const auto lease = lease_for_session(session_id);
    if (!lease.has_value()) {
        return nullptr;
    }

    auto& relay = relays_[session_id];
    if (!relay) {
        relay = std::make_shared<MediaStreamRelay>(options);
    }
    return std::make_unique<MediaConsumer>(relay);
}

bool AxentHost::publish_media_frame(const std::string& session_id, MediaFrame frame)
{
    const auto relay = relays_.find(session_id);
    if (relay == relays_.end() || !relay->second) {
        return false;
    }
    frame.session_id = session_id;
    relay->second->publish(std::move(frame));
    return true;
}

ControlResult AxentHost::call(const std::string& session_id,
                              const std::string& method,
                              const nlohmann::json& params)
{
    if (!broker_) {
        return {ControlStatus::Unavailable, {{"error", "host not running"}}};
    }
    const auto lease = lease_for_session(session_id);
    if (!lease.has_value()) {
        return {ControlStatus::NotFound, {{"error", "session not found"}}};
    }

    ControlCommand command;
    command.request_id = session_id + ":" + method;
    command.control_session_id = lease->client_id;
    command.device_id = lease->device_id;
    command.method = method;
    command.params = params;
    return broker_->dispatch(command);
}

Broker& AxentHost::broker()
{
    return *broker_;
}

void AxentHost::reset()
{
    for (auto& entry : relays_) {
        if (entry.second) {
            entry.second->close();
        }
    }
    relays_.clear();
    leases_.clear();
    media_owner_client_id_.clear();
    broker_.reset();
    flow_.reset();
    middleware_.reset();
    routes_.reset();
    devices_.reset();
    mock_adapter_.reset();
    logger_.reset();
    sessions_ = SessionManager{};
    running_ = false;
}

std::optional<SessionLease> AxentHost::lease_for_session(const std::string& session_id) const
{
    const auto it = leases_.find(session_id);
    if (it == leases_.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace axent
```

- [ ] **Step 6: Run host test**

Run:

```bash
cmake --build build --target axent_host_test
ctest --test-dir build -R axent_host_test --output-on-failure
```

Expected: `axent_host_test` passes.

- [ ] **Step 7: Commit host API**

```bash
git add CMakeLists.txt include/axent/host/axent_host.hpp src/host/axent_host.cpp tests/axent_host_test.cpp
git commit -m "feat: add axent host API"
```

## Task 6: Make axentd Host libaxent Through AxentHost

**Files:**
- Modify: `src/daemon/main.cpp`
- Test: existing `version_smoke_test`, `websocket_server_test`, `cli_smoke_test`, `axent_host_test`

- [ ] **Step 1: Refactor daemon entry to use `AxentHost`**

Replace the manager setup block in `src/daemon/main.cpp` with an `AxentHost` instance. The resulting include block should include:

```cpp
#include "axent/config/config.hpp"
#include "axent/control/control_plane.hpp"
#include "axent/control/websocket_server.hpp"
#include "axent/host/axent_host.hpp"
#include "axent/version.hpp"
```

The core setup inside `main()` should become:

```cpp
    const axent::AxentConfig config = axent::AxentConfig::dev_trial_defaults();
    axent::AxentHost host;
    axent::AxentHostOptions host_options;
    host_options.enable_mock_adapter = true;
    if (!host.start(host_options)) {
        std::cerr << "failed to start axent host\n";
        return 2;
    }

    axent::ControlPlane control_plane(host.broker());
    axent::WebSocketServer server;
    if (!server.start(control_plane, config.server.bind_host, static_cast<std::uint16_t>(config.server.port))) {
        std::cerr << "failed to start axentd websocket server\n";
        host.stop();
        return 2;
    }
```

Before returning from `main()`, stop both server and host:

```cpp
    server.stop();
    host.stop();
    std::cout << "axentd stopped\n";
    return 0;
```

- [ ] **Step 2: Run daemon-related tests**

Run:

```bash
cmake --build build --target axentd axent_host_test websocket_server_test cli_smoke_test
ctest --test-dir build -R "axent_host_test|websocket_server_test|cli_smoke_test|version_smoke_test" --output-on-failure
```

Expected: all selected tests pass.

- [ ] **Step 3: Run a foreground smoke**

Run:

```bash
./build/axentd --foreground
```

Expected: process prints an `axentd listening on` line. Stop it with `Ctrl-C`; it prints `axentd stopped`.

- [ ] **Step 4: Commit daemon host integration**

```bash
git add src/daemon/main.cpp
git commit -m "refactor: host axentd through axent host"
```

## Task 7: Documentation And Full Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/specs/2026-07-02-axent-daemon-embedded-media-relay-design.md` if implementation names diverged from the spec.

- [ ] **Step 1: Update README architecture section**

In `README.md`, after the opening paragraph, add:

```markdown
## Axent Library And Daemon Model

`libaxent` is the core implementation. It owns device discovery, concrete
transport management, AXTP device sessions, routing, diagnostics, and encoded
media relay.

`axentd` is the default daemon host for `libaxent`. It is responsible for
owning physical devices on a PC so multiple frontend products do not compete
for the same HID/AXTP transport.

Products such as NearCast can also link `libaxent` directly as an embedded
host. Embedded mode and daemon mode use the same logical Axent session and
media-source contract.

Axent relays encoded media frames and metadata. It does not own NearCast
rendering policy. NearCast keeps `MediaCore`, H.264/AAC assembly, render clock,
late drop, catch-up, D3D11, WASAPI, overlay, and UI behavior.
```

- [ ] **Step 2: Run full local verification**

Run:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
git status --short --branch
```

Expected:

- CMake configure succeeds.
- Build succeeds.
- CTest passes, including:
  - `axent_dependency_boundary`
  - `transport_media_types_test`
  - `media_relay_test`
  - `axent_host_test`
  - existing control, adapter, CLI, and WebSocket tests
- `git diff --check` has no output.
- `git status --short --branch` shows only intended tracked changes plus the pre-existing untracked `agent/` and `agent.zip`.

- [ ] **Step 3: Commit docs**

```bash
git add README.md docs/superpowers/specs/2026-07-02-axent-daemon-embedded-media-relay-design.md
git commit -m "docs: describe axent host media relay model"
```

## Final Review Before Handoff

- [ ] **Step 1: Confirm submodule pins**

Run:

```bash
git submodule status third_party/axtp-cpp-runtime third_party/IXWebSocket third_party/hidapi
```

Expected pins:

```text
b8f927223476c3c9a89cbeea1a038668cf81a03c third_party/axtp-cpp-runtime
2efe037c9cc96fd536774f17bdb5215161ee5087 third_party/IXWebSocket
c3509c11174fe80ff59a47119433a7db5299af85 third_party/hidapi
```

- [ ] **Step 2: Confirm no legacy artifacts are staged**

Run:

```bash
git status --short
git diff --cached --name-only
```

Expected: `agent/` and `agent.zip` are not staged.

- [ ] **Step 3: Summarize resulting commits**

Run:

```bash
git log --oneline --decorate -8
```

Expected: the branch contains separate commits for boundary test, dependency ownership, public types, media relay, host API, daemon integration, and docs.
