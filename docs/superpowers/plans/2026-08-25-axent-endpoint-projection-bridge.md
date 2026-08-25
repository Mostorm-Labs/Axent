# Axent Endpoint Projection and Control Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Axent assign stable Endpoint IDs to legacy AXTP devices when reliable identity evidence exists, route multiple devices by Endpoint, and bridge external JSON-RPC 2.0 addressing to optional native AXTP Relay metadata.

**Architecture:** Axent Core stores opaque Endpoint/provider bindings and resolves them to provider-local devices; the AXTP adapter owns stable HID identity evidence and delegates canonical hashing to the runtime. External JSON-RPC remains top-level `src`/`dst`; Broker converts it to a JSON-free routed adapter request, and AxtpAdapter either projects locally for legacy peers or emits runtime `m.src`/`m.dst` only in explicit NativeRelay mode.

**Tech Stack:** C++17, CMake/CTest, nlohmann/json, axtp-cpp-runtime Endpoint Relay API, IXWebSocket control endpoint, hidapi provider tests.

**Spec:** `docs/superpowers/specs/2026-08-25-endpoint-relay-multi-device-design.md`

## Global Constraints

- Execute on Axent branch `codex/axent-multi-device-management`; do not merge this branch into `main` until NearCast passes the merge gate.
- Do not update the runtime submodule until the standalone runtime plan has a reviewed commit whose full unit and pinned-spec conformance suites pass.
- Axent public headers must not include cpp-runtime headers or expose `axtp::*` types; the runtime remains a PRIVATE dependency of `libaxent`.
- Endpoint IDs are opaque in Axent Core. Do not infer provider, device type, delivery mode, authorization, or capability from an `ep_` prefix.
- AxtpAdapter generates an automatic legacy-device Endpoint only from stable UUID, stable public key, or the exact HID VID/PID/serial key. The initial implementation has only VID/PID/serial evidence.
- The HID key is exactly `device:axtp:hid:<vid-lower-hex4>:<pid-lower-hex4>:<serial-utf8>`; serial bytes are not case-folded and HID path/interface/port is never included.
- A device without stable evidence keeps its provider-local ID and an empty `endpoint_id`, unless Host/deployment supplies an explicit persistent managed-resource binding.
- DeviceManager stores and validates explicit Endpoint IDs; it must not synthesize FNV, random, path-based, session-based, connection-based, or parent-Agent-based identities.
- Existing adapters and legacy callers remain source-compatible through default routed overloads and `EndpointDeliveryMode::LocalProjection`.
- `NativeRelay` is opt-in through adapter/provider configuration only; do not probe a peer, inspect a version string, or inspect an Endpoint ID to enable it.
- External Axent WebSocket remains JSON-RPC 2.0 with top-level `src`/`dst`; it does not change to AXTP `{sid, op, m, d}` in this work.
- Routed control never places `src`, `dst`, `deviceId`, or `serialNumber` inside downstream business params.
- Unknown Endpoint returns `NotFound`; known offline/retiring Endpoint and conflicts return `Unavailable`.
- Same physical device aliases share lane `physical:<adapter>:<provider-local-device-id>`; different devices may execute concurrently.

---

### Task 1: Pin the Reviewed Runtime and Add the Axent Identity Facade

**Files:**
- Modify: `third_party/axtp-cpp-runtime` gitlink
- Create: `include/axent/adapters/axtp_endpoint_identity.hpp`
- Create: `src/adapters/axtp_endpoint_identity.cpp`
- Create: `tests/axtp_endpoint_identity_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/cmake/verify_axent_dependency_boundary.cmake`

**Interfaces:**
- Consumes: reviewed runtime `std::string axtp::endpointIdFromKey(std::string_view)` from the runtime plan.
- Produces: `std::string axent::axtp_endpoint_id_from_key(std::string_view endpoint_key)` without public runtime type exposure.

- [ ] **Step 1: Update the detached submodule to the reviewed runtime commit**

After the runtime PR is merged, run:

```bash
git -C third_party/axtp-cpp-runtime fetch origin main
git -C third_party/axtp-cpp-runtime checkout origin/main
git -C third_party/axtp-cpp-runtime log -1 --oneline
```

Expected: the checked-out commit contains `endpointIdFromKey`, `CallOptions::endpoint`, Endpoint Relay codec tests, and the passing conformance commit from the runtime plan. Do not continue if any of these are absent.

- [ ] **Step 2: Add the failing facade test and dependency-boundary assertion**

The new test must assert:

```cpp
REQUIRE(axent::axtp_endpoint_id_from_key(
            "device:axtp:hid:1234:5678:SERIAL-1") ==
        "ep_3340a334b47934f471968db6b1470da6");
bool empty_key_rejected = false;
try {
    (void)axent::axtp_endpoint_id_from_key("");
} catch (const std::invalid_argument&) {
    empty_key_rejected = true;
}
REQUIRE(empty_key_rejected);
```

Extend the dependency check so `include/axent/adapters/axtp_endpoint_identity.hpp` fails if it contains `axtp::` or an include path from `axtp-cpp-runtime`.

- [ ] **Step 3: Register the source/test and confirm failure**

Add `src/adapters/axtp_endpoint_identity.cpp` to `libaxent`, add the focused test target, then run:

```bash
cmake -S . -B build/endpoint-projection
cmake --build build/endpoint-projection --target axtp_endpoint_identity_test
ctest --test-dir build/endpoint-projection -R '^(axtp_endpoint_identity_test|axent_dependency_boundary)$' --output-on-failure
```

Expected: compilation fails because the facade is not implemented.

- [ ] **Step 4: Implement the thin facade**

Public header:

```cpp
#pragma once
#include <string>
#include <string_view>

namespace axent {
std::string axtp_endpoint_id_from_key(std::string_view endpoint_key);
}  // namespace axent
```

Private implementation:

```cpp
#include "axent/adapters/axtp_endpoint_identity.hpp"
#include <core/protocol/endpoint/endpoint_identity.hpp>

std::string axent::axtp_endpoint_id_from_key(std::string_view endpoint_key) {
    return axtp::endpointIdFromKey(endpoint_key);
}
```

- [ ] **Step 5: Run the focused tests**

Run the Step 3 commands.

Expected: identity vector and dependency boundary pass.

- [ ] **Step 6: Commit the reviewed dependency and facade**

```bash
git add third_party/axtp-cpp-runtime CMakeLists.txt \
  include/axent/adapters/axtp_endpoint_identity.hpp \
  src/adapters/axtp_endpoint_identity.cpp tests/axtp_endpoint_identity_test.cpp \
  tests/cmake/verify_axent_dependency_boundary.cmake
git commit -m "feat: expose canonical axtp endpoint identity"
```

### Task 2: Explicit Device Binding Invariants

**Files:**
- Modify: `include/axent/core/types.hpp`
- Modify: `include/axent/core/device_manager.hpp`
- Modify: `src/core/device_manager.cpp`
- Modify: `src/core/json.cpp`
- Modify: `src/host/axent_host.cpp`
- Test: `tests/manager_registry_test.cpp`
- Test: `tests/core_types_test.cpp`
- Test: `tests/axent_host_test.cpp`

**Interfaces:**
- Consumes: opaque `DeviceSnapshot::endpoint_id` supplied by adapters or Host.
- Produces: `EndpointDeliveryMode`, `DeviceUpsertStatus`, `DeviceUpsertResult`, and `DeviceManager::upsert(DeviceSnapshot)` returning the explicit outcome.

- [ ] **Step 1: Replace fallback expectations with failing binding tests**

Add tests for all transitions:

```cpp
REQUIRE(devices.upsert(noEndpoint).status == DeviceUpsertStatus::Inserted);
REQUIRE(devices.get(noEndpoint.id)->endpoint_id.empty());

auto bound = noEndpoint;
bound.endpoint_id = "ep_explicit";
REQUIRE(devices.upsert(bound).status == DeviceUpsertStatus::EndpointBound);

auto refreshWithoutEndpoint = bound;
refreshWithoutEndpoint.endpoint_id.clear();
REQUIRE(devices.upsert(refreshWithoutEndpoint).status == DeviceUpsertStatus::Refreshed);
REQUIRE(devices.get(bound.id)->endpoint_id == "ep_explicit");

auto changed = bound;
changed.endpoint_id = "ep_changed";
REQUIRE(devices.upsert(changed).status ==
        DeviceUpsertStatus::EndpointChangeRejected);
REQUIRE(devices.get(bound.id)->endpoint_id == "ep_explicit");
```

Insert a second device with `ep_explicit`, assert `EndpointConflict`, and assert neither stored snapshot mutates. Mark the original offline and assert its Endpoint remains stored.

Add a device with no serial or device-owned stable identity, explicitly set its
Endpoint to `axtp_endpoint_id_from_key("service:lab:receiver-slot-a")`, and
assert DeviceManager accepts `ep_20d54d9fc87018d571995be978620d21` as a
deployment-owned managed-resource binding. In `core_types_test`, assert JSON
omits `endpointId` for an empty binding and emits it for the managed binding.

- [ ] **Step 2: Run manager/host tests and confirm failure**

```bash
cmake --build build/endpoint-projection --target manager_registry_test core_types_test axent_host_test
ctest --test-dir build/endpoint-projection -R '^(manager_registry_test|core_types_test|axent_host_test)$' --output-on-failure
```

Expected: the current manager synthesizes an FNV Endpoint and `upsert` has no outcome type.

- [ ] **Step 3: Add the delivery-mode and upsert result types**

Append after the existing enums/types so aggregate users remain compatible:

```cpp
enum class EndpointDeliveryMode {
    LocalProjection,
    NativeRelay,
};

// Append after DeviceSnapshot::endpoint_id.
EndpointDeliveryMode endpoint_delivery_mode =
    EndpointDeliveryMode::LocalProjection;
```

In `device_manager.hpp` add:

```cpp
enum class DeviceUpsertStatus {
    Inserted,
    Refreshed,
    EndpointBound,
    EndpointConflict,
    EndpointChangeRejected,
};

struct DeviceUpsertResult {
    DeviceUpsertStatus status = DeviceUpsertStatus::Inserted;
    bool accepted() const noexcept {
        return status == DeviceUpsertStatus::Inserted ||
               status == DeviceUpsertStatus::Refreshed ||
               status == DeviceUpsertStatus::EndpointBound;
    }
};
```

Change `upsert` to return `DeviceUpsertResult`; existing callers may ignore the return value.

- [ ] **Step 4: Implement fail-closed binding updates**

Delete `default_endpoint_id` and all FNV includes. Under the manager mutex:

1. Find the existing snapshot by provider-local `id` using the current compatibility key.
2. If the incoming non-empty Endpoint is used by another stored device, return `EndpointConflict` without mutation.
3. For a new snapshot, insert it unchanged and return `Inserted`.
4. If the stored Endpoint is non-empty and incoming is empty, copy the stored Endpoint into the refresh.
5. If both are non-empty and differ, return `EndpointChangeRejected` without mutation.
6. Replace the stored snapshot and return `EndpointBound` only for empty-to-non-empty; otherwise return `Refreshed`.

Update `src/core/json.cpp` so its comment states that `endpointId` is emitted
only for explicit stable/projected bindings; retain the existing conditional
omission for an empty value.

- [ ] **Step 5: Surface rejected Host bindings in diagnostics logs**

At mock discovery, AXTP discovery, and `AxentHost::upsert_device`, inspect the result. On rejection call:

```cpp
impl_->logger->write(
    LogLevel::Error,
    LogCategory::Diagnostics,
    "device.endpoint.binding_rejected",
    {{"deviceId", device.id},
     {"adapter", device.adapter},
     {"endpointId", device.endpoint_id},
     {"status", result.status == DeviceUpsertStatus::EndpointConflict
                    ? "endpoint_conflict"
                    : "endpoint_change_rejected"}});
```

Keep `AxentHost::upsert_device`'s existing void API; rejected writes are observable through the stored device state and diagnostic log.

- [ ] **Step 6: Run manager/host tests**

Run the Step 2 commands.

Expected: all transition, no-fallback, offline-preservation, and Host rejection tests pass.

- [ ] **Step 7: Commit binding invariants**

```bash
git add include/axent/core/types.hpp include/axent/core/device_manager.hpp \
  src/core/device_manager.cpp src/core/json.cpp src/host/axent_host.cpp \
  tests/manager_registry_test.cpp tests/core_types_test.cpp tests/axent_host_test.cpp
git commit -m "fix: require explicit stable endpoint bindings"
```

### Task 3: Legacy HID Endpoint Projection

**Files:**
- Modify: `include/axent/adapters/axtp_adapter.hpp`
- Modify: `src/adapters/axtp_adapter.cpp`
- Test: `tests/axtp_real_adapter_test.cpp`

**Interfaces:**
- Consumes: `axtp_endpoint_id_from_key` and `EndpointDeliveryMode`.
- Produces: serial-backed HID `DeviceSnapshot::endpoint_id`; `AxtpAdapterConfig::endpoint_delivery_mode` defaulting to LocalProjection.

- [ ] **Step 1: Add failing canonical projection tests**

For descriptor VID `0x1234`, PID `0x5678`, serial `SERIAL-1`, call the
compatibility-default overload and assert:

```cpp
REQUIRE(snapshot.endpoint_id == "ep_3340a334b47934f471968db6b1470da6");
REQUIRE(snapshot.endpoint_delivery_mode ==
        axent::EndpointDeliveryMode::LocalProjection);
```

Change only `descriptor.path` and assert the ID is unchanged. Change serial
case and assert the ID changes. Clear serial, set two different paths, and
assert both snapshots have empty Endpoint IDs. Call the explicit overload with
`EndpointDeliveryMode::NativeRelay` and assert the returned snapshot records
NativeRelay without changing its Endpoint ID.

- [ ] **Step 2: Run the adapter test and confirm failure**

```bash
cmake --build build/endpoint-projection --target axtp_real_adapter_test
ctest --test-dir build/endpoint-projection -R '^axtp_real_adapter_test$' --output-on-failure
```

Expected: current snapshots have no adapter-generated canonical Endpoint.

- [ ] **Step 3: Add the explicit provider mode**

Append to `AxtpAdapterConfig`:

```cpp
EndpointDeliveryMode endpoint_delivery_mode =
    EndpointDeliveryMode::LocalProjection;
```

The manager/leaf adapter copies this config unchanged; `na20_defaults()` remains LocalProjection.
Change the static projection helper to this source-compatible signature:

```cpp
static DeviceSnapshot snapshot_from_descriptor(
    const TransportDescriptor& descriptor,
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection);
```

- [ ] **Step 4: Generate an Endpoint only from stable serial evidence**

In `snapshot_from_descriptor`, format VID/PID with four lowercase hexadecimal digits and construct:

```cpp
const std::string endpoint_key =
    "device:axtp:hid:" + lower_hex4(descriptor.vendor_id) + ":" +
    lower_hex4(descriptor.product_id) + ":" + descriptor.serial_number;
snapshot.endpoint_id = axtp_endpoint_id_from_key(endpoint_key);
```

Execute this block only when `descriptor.kind == TransportKind::Hid` and
`serial_number` is non-empty. Never read `descriptor.id`, `path`,
`interface_number`, or AXTP session state while constructing the key. Assign
the function argument to `snapshot.endpoint_delivery_mode`; instance discovery
calls the helper with `config_.endpoint_delivery_mode`.

- [ ] **Step 5: Run the adapter test**

Run the Step 2 commands.

Expected: canonical, path-churn, serial-case, and path-only assertions pass.

- [ ] **Step 6: Commit legacy projection**

```bash
git add include/axent/adapters/axtp_adapter.hpp \
  src/adapters/axtp_adapter.cpp tests/axtp_real_adapter_test.cpp
git commit -m "feat: project stable hid devices as endpoints"
```

### Task 4: Detailed Endpoint Provider Resolution

**Files:**
- Modify: `include/axent/core/route_manager.hpp`
- Modify: `src/core/route_manager.cpp`
- Test: `tests/broker_flow_test.cpp`

**Interfaces:**
- Consumes: `DeviceSnapshot::endpoint_id`, online state, and delivery mode.
- Produces: `RouteTarget::endpoint_delivery_mode`, `RouteResolutionStatus`, `RouteResolution`, and `resolve_endpoint_route`.

- [ ] **Step 1: Add failing route-outcome tests**

Assert a unique online Endpoint is `Found`, a missing Endpoint is `NotFound`, and a stored offline Endpoint is `Unavailable`. Assert the compatibility `resolve_endpoint` wrapper returns a target only for `Found`. Update the previous duplicate test to assert DeviceManager rejects the second binding and preserves the first route.

- [ ] **Step 2: Run the broker test and confirm failure**

```bash
cmake --build build/endpoint-projection --target broker_flow_test
ctest --test-dir build/endpoint-projection -R '^broker_flow_test$' --output-on-failure
```

Expected: offline and unknown routes are both represented as empty optionals.

- [ ] **Step 3: Add exact route result types**

```cpp
struct RouteTarget {
    std::string adapter;
    std::string device_id;
    std::string endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection;
};

enum class RouteResolutionStatus { Found, NotFound, Unavailable, Conflict };

struct RouteResolution {
    RouteResolutionStatus status = RouteResolutionStatus::NotFound;
    std::optional<RouteTarget> target;
};
```

Expose `RouteResolution resolve_endpoint_route(const std::string&) const` and keep `resolve_endpoint`, `resolve_device`, and `resolve` as compatibility wrappers.

- [ ] **Step 4: Implement four-state lookup**

Scan snapshots matching the exact non-empty Endpoint. Zero matches returns `NotFound`; more than one returns `Conflict`; one offline match returns `Unavailable`; one online match returns `Found` with adapter, provider-local ID, Endpoint ID, and delivery mode. The optional wrapper returns `resolution.target` only when status is `Found`.

- [ ] **Step 5: Run the broker test**

Run the Step 2 commands.

Expected: route outcomes and compatibility wrappers pass.

- [ ] **Step 6: Commit provider resolution**

```bash
git add include/axent/core/route_manager.hpp src/core/route_manager.cpp \
  tests/broker_flow_test.cpp
git commit -m "feat: classify endpoint provider routes"
```

### Task 5: Routed Adapter Contract and Broker Dispatch

**Files:**
- Modify: `include/axent/core/adapter.hpp`
- Modify: `src/core/broker.cpp`
- Test: `tests/broker_flow_test.cpp`
- Test: `tests/adapter_skeleton_test.cpp`

**Interfaces:**
- Consumes: detailed route results and `EndpointDeliveryMode`.
- Produces: `AdapterControlRequest` plus routed sync, async, and firmware overloads; Broker-to-adapter source/destination propagation.

- [ ] **Step 1: Add a failing routed capture adapter test**

Create a test adapter that records the routed request. Dispatch JSON-RPC with `src=ep_app`, `dst=ep_device`, and params containing legacy selectors plus `detail=business`. Assert the adapter receives:

```cpp
REQUIRE(captured.device_id == "provider-device-1");
REQUIRE(captured.source_endpoint_id == "ep_app");
REQUIRE(captured.destination_endpoint_id == "ep_device");
REQUIRE(captured.endpoint_delivery_mode == EndpointDeliveryMode::NativeRelay);
REQUIRE(captured.params == nlohmann::json{{"detail", "business"}});
```

Also assert an adapter implementing only legacy signatures still works through the new default overloads.

- [ ] **Step 2: Add failing Broker status tests**

Dispatch to unknown and offline Endpoints. Assert unknown maps to
`ControlStatus::NotFound` and offline maps to `ControlStatus::Unavailable`.
Exercise both sync and async dispatch. Keep the Broker switch branch that maps
a defensive `Conflict` result to `Unavailable`, while duplicate registration
remains covered by DeviceManager's fail-closed test.

- [ ] **Step 3: Run focused tests and confirm failure**

```bash
cmake --build build/endpoint-projection --target broker_flow_test adapter_skeleton_test
ctest --test-dir build/endpoint-projection -R '^(broker_flow_test|adapter_skeleton_test)$' --output-on-failure
```

Expected: Adapter has no routed overload and Broker collapses route outcomes.

- [ ] **Step 4: Add the JSON-free routed contract**

```cpp
struct AdapterControlRequest {
    std::string device_id;
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
};
```

Add virtual overloads for `call`, `call_async`, and `start_firmware_update`. Their default implementations delegate to the existing signatures exactly, preserving AXDP, TEA, mock, and external adapter behavior.

- [ ] **Step 5: Route once and dispatch the routed request**

In Broker, use `resolve_endpoint_route` whenever `command.dst` is non-empty. Map status before adapter lookup. For a found route construct:

```cpp
AdapterControlRequest routed;
routed.device_id = target.device_id;
routed.source_endpoint_id = command.dst.empty() ? "" : command.src;
routed.destination_endpoint_id = command.dst.empty() ? "" : target.endpoint_id;
routed.endpoint_delivery_mode = target.endpoint_delivery_mode;
routed.method = command.method;
routed.params = command.params;
```

Use the routed overload for normal, async, and firmware calls. Legacy device selection leaves both Endpoint strings empty. Keep `route_key` canonicalized to the found physical adapter/device tuple.

- [ ] **Step 6: Run focused tests**

Run the Step 3 commands.

Expected: routed capture, compatibility delegation, status mapping, and physical lane keys pass.

- [ ] **Step 7: Commit the routed contract**

```bash
git add include/axent/core/adapter.hpp src/core/broker.cpp \
  tests/broker_flow_test.cpp tests/adapter_skeleton_test.cpp
git commit -m "feat: carry endpoint routes to adapters"
```

### Task 6: External JSON-RPC Validation and Compatibility Bridge

**Files:**
- Modify: `include/axent/control/protocol_codecs.hpp`
- Modify: `src/control/protocol_codecs.cpp`
- Modify: `src/control/control_plane.cpp`
- Test: `tests/protocol_codecs_test.cpp`
- Test: `tests/broker_flow_test.cpp`

**Interfaces:**
- Consumes: top-level JSON-RPC 2.0 `src`/`dst` and legacy selector params.
- Produces: `DecodedControlMessage::validation_error`; deterministic response/error address reversal; pre-Broker rejection of malformed routing fields.

- [ ] **Step 1: Add failing malformed-envelope table tests**

Cover: only `src`, only `dst`, numeric `src`, boolean `dst`, empty `src`, empty `dst`, and both known fields malformed. For every case assert `validation_error.has_value()`, ControlPlane returns `-32602`, no adapter invocation occurs, and the response contains no invented half-envelope.

- [ ] **Step 2: Add compatibility tests**

Assert a request with no routing fields still uses `params.deviceId`/`serialNumber` and has no response `src`/`dst`. Assert a valid addressed request removes both selectors before Broker dispatch and success/error responses emit `src=request.dst`, `dst=request.src`.

- [ ] **Step 3: Run codec/broker tests and confirm failure**

```bash
cmake --build build/endpoint-projection --target protocol_codecs_test broker_flow_test
ctest --test-dir build/endpoint-projection -R '^(protocol_codecs_test|broker_flow_test)$' --output-on-failure
```

Expected: two malformed non-string fields are currently indistinguishable from an absent legacy envelope at decode time.

- [ ] **Step 4: Decode routing fields with presence-aware validation**

Append:

```cpp
std::optional<std::string> validation_error;
```

to `DecodedControlMessage`. In `decode_routing_fields`, inspect `contains` independently. If neither field is present, leave legacy behavior unchanged. If either is present, require both values to be strings and non-empty; otherwise set exactly:

```text
JSON-RPC src and dst must be provided together as non-empty strings
```

Only copy validated values into `command.src`/`command.dst`. Remove legacy selectors only after a valid non-empty `dst` is established.

- [ ] **Step 5: Make ControlPlane consume decoder validation**

Replace the duplicate raw-JSON inspection with:

```cpp
if (decoded.validation_error) {
    return encode_control_response(
        decoded,
        {ControlStatus::InvalidArgument,
         {{"error", *decoded.validation_error}}});
}
```

Keep response reversal conditional on both validated command fields.

- [ ] **Step 6: Run codec/broker tests**

Run the Step 3 commands.

Expected: malformed tables, legacy requests, selector stripping, and response/error reversal pass.

- [ ] **Step 7: Commit control-plane validation**

```bash
git add include/axent/control/protocol_codecs.hpp \
  src/control/protocol_codecs.cpp src/control/control_plane.cpp \
  tests/protocol_codecs_test.cpp tests/broker_flow_test.cpp
git commit -m "fix: validate endpoint routing at json rpc ingress"
```

### Task 7: AxtpAdapter Local Projection and Native Relay Bridge

**Files:**
- Modify: `include/axent/adapters/axtp_adapter.hpp`
- Modify: `src/adapters/axtp_adapter.cpp`
- Modify: `src/adapters/axtp_adapter_test_seam.hpp`
- Modify: `src/adapters/axtp_adapter_test_seam.cpp`
- Test: `tests/axtp_real_adapter_test.cpp`

**Interfaces:**
- Consumes: `AdapterControlRequest`, `EndpointDeliveryMode`, runtime `sdk::CallOptions::endpoint`.
- Produces: AxtpAdapter routed overloads; queued control calls retaining logical addressing; explicit NativeRelay wire bridge.

- [ ] **Step 1: Add failing LocalProjection capture test**

Use the existing runtime-factory seam and a capturing mock peer. Submit a routed request with valid source/destination and `LocalProjection`. Decode the outbound AXTP request and assert:

```cpp
REQUIRE(!request.meta.endpoint.src.has_value());
REQUIRE(!request.meta.endpoint.dst.has_value());
REQUIRE(request.body == bytesOf(R"({"detail":"business"})"));
```

Complete the peer response and assert the Axent operation succeeds.

- [ ] **Step 2: Add failing NativeRelay capture test**

Repeat with `NativeRelay` and assert decoded outbound metadata is `src=ep_app`, `dst=ep_device`, params contain only the business field, and a legacy response without `m` still completes through request-ID correlation.

- [ ] **Step 3: Run the adapter test and confirm failure**

```bash
cmake --build build/endpoint-projection --target axtp_real_adapter_test
ctest --test-dir build/endpoint-projection -R '^axtp_real_adapter_test$' --output-on-failure
```

Expected: AxtpAdapter lacks routed overloads and its pending queue cannot retain Endpoint metadata.

- [ ] **Step 4: Add routed overloads without changing legacy entry points**

Override all three routed methods in the public class. Existing legacy `call(device_id, method, params)` constructs a LocalProjection `AdapterControlRequest` and delegates. The manager forwards the complete routed request to its leaf adapter; it must not reconstruct a request that loses source/destination or mode.

- [ ] **Step 5: Carry addressing through the FIFO**

Append these fields to `PendingControlCall` and copy them during enqueue:

```cpp
std::string source_endpoint_id;
std::string destination_endpoint_id;
EndpointDeliveryMode endpoint_delivery_mode =
    EndpointDeliveryMode::LocalProjection;
```

At dispatch, set runtime options only for NativeRelay:

```cpp
if (request->endpoint_delivery_mode == EndpointDeliveryMode::NativeRelay) {
    if (!request->source_endpoint_id.empty())
        call_options.endpoint.src = request->source_endpoint_id;
    if (!request->destination_endpoint_id.empty())
        call_options.endpoint.dst = request->destination_endpoint_id;
}
```

Never modify `request->params` to add routing fields.

- [ ] **Step 6: Run the adapter test**

Run the Step 3 commands.

Expected: LocalProjection omits metadata, NativeRelay emits it, legacy responses still correlate, and all existing session/media tests remain passing.

- [ ] **Step 7: Commit the adapter bridge**

```bash
git add include/axent/adapters/axtp_adapter.hpp src/adapters/axtp_adapter.cpp \
  src/adapters/axtp_adapter_test_seam.hpp src/adapters/axtp_adapter_test_seam.cpp \
  tests/axtp_real_adapter_test.cpp
git commit -m "feat: bridge routed controls to axtp endpoint relay"
```

### Task 8: Multi-Device Scheduling, Documentation, and Branch Verification

**Files:**
- Modify: `tests/websocket_server_test.cpp`
- Modify: `docs/architecture/MULTI_DEVICE_CONTROL.md`
- Verify: all files changed by Tasks 1-7.

**Interfaces:**
- Consumes: Endpoint provider routing, physical lane keys, and JSON-RPC bridge.
- Produces: documented multi-device behavior and a verified Axent feature branch ready for NearCast integration, but not merged to `main`.

- [ ] **Step 1: Strengthen the WebSocket lane regression test**

Keep one device blocked, submit its second request through the legacy device alias, and submit another request through a different Endpoint. Assert the other device responds before release and the first device's two requests complete FIFO. Add response-address assertions for both addressed calls and absence assertions for the legacy alias response.

- [ ] **Step 2: Run WebSocket and affected tests**

```bash
cmake --build build/endpoint-projection --target websocket_server_test \
  manager_registry_test broker_flow_test protocol_codecs_test \
  axtp_real_adapter_test axent_host_test
ctest --test-dir build/endpoint-projection \
  -R '^(websocket_server_test|manager_registry_test|broker_flow_test|protocol_codecs_test|axtp_real_adapter_test|axent_host_test)$' \
  --output-on-failure
```

Expected: all affected tests pass.

- [ ] **Step 3: Replace the preliminary architecture description**

Update `MULTI_DEVICE_CONTROL.md` with the exact ownership chain:

```text
NearCast persistent app key -> Axent JSON-RPC src
Axent Endpoint provider table -> adapter + provider-local device + delivery mode
LocalProjection -> legacy downstream request without m
NativeRelay -> runtime PayloadMeta.endpoint -> AXTP m.src/m.dst
```

Document the three legacy identity cases, exact HID key, no-FNV/no-path rule, DeviceManager rejection states, route outcomes, and the requirement that NearCast owns its persistent installation identity.

- [ ] **Step 4: Commit scheduling/docs changes**

```bash
git add tests/websocket_server_test.cpp docs/architecture/MULTI_DEVICE_CONTROL.md
git commit -m "docs: define endpoint-based multi-device control"
```

- [ ] **Step 5: Run dependency and full suite verification**

```bash
cmake -S . -B build/endpoint-projection-final
cmake --build build/endpoint-projection-final
ctest --test-dir build/endpoint-projection-final --output-on-failure
ctest --test-dir build/endpoint-projection-final -R '^axent_dependency_boundary$' --output-on-failure
git diff --check origin/codex/axent-multi-device-management...HEAD
git submodule status --recursive
```

Expected: all tests pass, dependency boundary passes, whitespace check is silent, and the runtime gitlink is the reviewed Endpoint Relay commit.

- [ ] **Step 6: Verify a clean recursive checkout before NearCast handoff**

In a fresh clone or worktree of this branch, run:

```bash
git submodule update --init --recursive
cmake -S . -B build/recursive-clean
cmake --build build/recursive-clean
ctest --test-dir build/recursive-clean --output-on-failure
```

Expected: the branch builds and tests using only committed recursive dependencies.

- [ ] **Step 7: Record the NearCast merge gate without merging**

```bash
git status --short --branch
git log --oneline origin/codex/axent-multi-device-management..HEAD
```

Expected: the worktree is clean and the feature commits are visible. Hand this branch to NearCast for persistent app identity, two-device routing, response reversal, reconnect stability, and no-stable-identity tests. Do not merge Axent to `main` in this task.
