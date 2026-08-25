# Axent Endpoint-Aware Embedded Host Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add product-neutral dynamic inventory, Endpoint-aware Host control, and AXTP Endpoint metadata preservation to Axent's embedded public API.

**Architecture:** `AxentHost` reconciles adapter discovery into the existing `DeviceManager` and routes public Endpoint requests through the existing Broker. `AxtpControlEndpoint` translates native AXTP metadata into Axent public request fields while runtime continues to own wire mechanics.

**Tech Stack:** C++17, CMake, CTest, nlohmann/json, Axent public contracts, pinned axtp-cpp-runtime `0e02f4b` / AXTP `spec/v0.15.0`.

**Spec:** `docs/superpowers/specs/2026-08-25-axent-endpoint-host-bridge-design.md`

## Global Constraints

- Dependency direction is exactly `NearCast product host -> Axent protocol bus -> axtp-cpp-runtime`.
- Axent public headers must not expose `axtp::*` or cpp-runtime headers.
- Runtime remains pinned to `0e02f4bd5d08fc083672e57207fd144835e6df92` and AXTP `spec/v0.15.0` in this plan.
- Do not add Endpoint inventory wire methods, RFC JSON-RPC embedded management, or product policy.
- Follow RED-GREEN-REFACTOR for every production behavior.
- Preserve existing wire behavior, optional AXTP `m`, media lifecycle ordering, and physical-lane isolation.

---

### Task 1: Dynamic Host Inventory Reconciliation

**Files:**
- Modify: `include/axent/host/axent_host.hpp`
- Modify: `src/host/axent_host.cpp`
- Test: `tests/axent_host_test.cpp`

**Interfaces:**
- Consumes: `Adapter::discover()`, `DeviceManager::upsert()`, `DeviceManager::mark_offline(adapter,id,reason)`.
- Produces: `std::vector<DeviceSnapshot> AxentHost::refresh_devices()`.

- [ ] **Step 1: Write the failing refresh test**

Add a stateful adapter through `AxentHostOptions::axtp_adapter_factory`. Its
first discovery returns Endpoint-bound devices A and B; its second returns B
and new C; its third returns A, B, and C with A's original Endpoint. Assert:

```cpp
auto refreshed = host.refresh_devices();
require(find(refreshed, "device-a").connection.online == false,
        "missing A must be retained offline");
require(find(refreshed, "device-b").connection.online,
        "B must remain online");
require(find(refreshed, "device-c").connection.online,
        "new C must be inserted");
require(find(restored, "device-a").endpoint_id == "ep-device-a",
        "reappearance must preserve the stable Endpoint");
```

Assert a stopped Host returns an empty list without incrementing discovery.

- [ ] **Step 2: Run the test and verify RED**

```bash
cmake --build build/multi-device-baseline --target axent_host_test --parallel 4
./build/multi-device-baseline/axent_host_test
```

Expected: compile failure because `refresh_devices()` does not exist.

- [ ] **Step 3: Implement minimal reconciliation**

Declare the method beside `discover_devices()`. Serialize against start/stop
with `dispatch_mutex`, do not hold the Host mutex while calling adapter
`discover()`, upsert current snapshots, mark previously known but absent
snapshots offline with reason `discovery-missing`, and return the full cached
list. Do not erase devices, leases, subscriptions, or media state.

- [ ] **Step 4: Run focused tests and verify GREEN**

```bash
cmake --build build/multi-device-baseline --target axent_host_test --parallel 4
ctest --test-dir build/multi-device-baseline -R "axent_host_test|manager_registry_test|axent_dependency_boundary" --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add include/axent/host/axent_host.hpp src/host/axent_host.cpp tests/axent_host_test.cpp
git commit -m "feat: refresh host device inventory"
```

### Task 2: Endpoint-Aware Host Control Call

**Files:**
- Modify: `include/axent/host/axent_host.hpp`
- Modify: `src/host/axent_host.cpp`
- Test: `tests/axent_host_test.cpp`

**Interfaces:**
- Consumes: `Broker::dispatch_async(ControlCommand, ControlCallOptions)` and existing Endpoint routes/physical lanes.
- Produces: `EndpointControlRequest` and `AxentHost::call_endpoint()` exactly as defined in the spec.

- [ ] **Step 1: Write failing public-contract and behavior tests**

Call the desired API:

```cpp
auto operation = host.call_endpoint({
    "ep-nearcast-source",
    "endpoint/mock-primary",
    "status.get",
    nlohmann::json::object(),
});
```

Assert success, unknown destination `NotFound`, offline destination
`Unavailable`, and missing source/destination/method `InvalidArgument`. A
recording async adapter must receive exact source/destination Endpoint IDs and
no injected device selectors in params. Gates must show same-physical aliases
are FIFO and different physical devices can enter in parallel.

- [ ] **Step 2: Run and verify RED**

```bash
cmake --build build/multi-device-baseline --target axent_host_test --parallel 4
./build/multi-device-baseline/axent_host_test
```

Expected: compile failure because the request and method do not exist.

- [ ] **Step 3: Implement the minimal Host facade**

Validate mandatory strings. Construct an Axent `ControlCommand` with source
`LocalCli`, `src`, `dst`, method and params; submit through
`Broker::dispatch_async()`. Track the operation under an Endpoint-scoped key
so `stop()` cancels it. Do not acquire a product session or add a queue.

- [ ] **Step 4: Run focused tests and verify GREEN**

```bash
cmake --build build/multi-device-baseline --target axent_host_test broker_flow_test --parallel 4
ctest --test-dir build/multi-device-baseline -R "axent_host_test|broker_flow_test|axent_dependency_boundary" --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add include/axent/host/axent_host.hpp src/host/axent_host.cpp tests/axent_host_test.cpp
git commit -m "feat: expose endpoint-aware host control"
```

### Task 3: Preserve AXTP Endpoint Metadata at Public Ingress

**Files:**
- Modify: `include/axent/control/control_contract.hpp`
- Modify: `src/control/axtp_control_endpoint.cpp`
- Test: `tests/control_contract_test.cpp`
- Test: `tests/axtp_control_endpoint_test.cpp`

**Interfaces:**
- Consumes: private runtime `RpcContext::endpoint` and runtime response Endpoint reversal.
- Produces: appended `ControlRequest::source_endpoint_id` and `destination_endpoint_id` strings.

- [ ] **Step 1: Write failing routed and legacy wire tests**

Send a real request with:

```json
"m": {"src": "ep-controller", "dst": "ep-nearcast"}
```

Return the public fields from the handler. Assert input reaches the handler and
response metadata equals `{"src":"ep-nearcast","dst":"ep-controller"}`.
Send a request without `m`; assert both public fields are empty and correlation
still uses the request ID. Retain three-field aggregate construction coverage.

- [ ] **Step 2: Run and verify RED**

```bash
cmake --build build/multi-device-baseline --target control_contract_test axtp_control_endpoint_test --parallel 4
./build/multi-device-baseline/axtp_control_endpoint_test
```

Expected: compile failure because public Endpoint fields do not exist.

- [ ] **Step 3: Implement metadata translation**

Append the strings to `ControlRequest`. In the private runtime lambda copy:

```cpp
control_request.source_endpoint_id = context.endpoint.src.value_or("");
control_request.destination_endpoint_id = context.endpoint.dst.value_or("");
```

Do not construct response `m` in Axent; runtime owns reversal.

- [ ] **Step 4: Run focused tests and verify GREEN**

```bash
cmake --build build/multi-device-baseline --target control_contract_test axtp_control_endpoint_test --parallel 4
ctest --test-dir build/multi-device-baseline -R "control_contract_test|axtp_control_endpoint_test|axent_dependency_boundary" --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add include/axent/control/control_contract.hpp src/control/axtp_control_endpoint.cpp tests/control_contract_test.cpp tests/axtp_control_endpoint_test.cpp
git commit -m "feat: expose control endpoint identities"
```

### Task 4: Phase Verification and Runtime-Concurrency Gate

**Files:**
- Verify: `third_party/axtp-cpp-runtime/AXTP_SPEC.lock.yaml`
- Verify: all changes since `20132947329ef66ecaed9325e9162646e7e79328`.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: a verified Axent integration commit and input for the separate external-device-dispatch/runtime plan.

- [ ] **Step 1: Verify dependency pins**

```bash
git submodule status --recursive
grep -n "spec/v0.15.0" third_party/axtp-cpp-runtime/AXTP_SPEC.lock.yaml
git diff --submodule=log 20132947329ef66ecaed9325e9162646e7e79328..HEAD -- third_party/axtp-cpp-runtime
```

Expected: runtime is `0e02f4bd5d08fc083672e57207fd144835e6df92`,
the lock names `spec/v0.15.0`, and the runtime gitlink has not changed.

- [ ] **Step 2: Run full Axent verification**

```bash
cmake --build build/multi-device-baseline --parallel 4
ctest --test-dir build/multi-device-baseline --output-on-failure
git diff --check
```

- [ ] **Step 3: Record the concurrency gate input**

Confirm the endpoint test still proves handlers execute on one endpoint
worker. The follow-on plan must add an end-to-end device dispatcher test where
A is blocked and B must complete first. This observation alone authorizes no
runtime edit; only the failing characterization does.
