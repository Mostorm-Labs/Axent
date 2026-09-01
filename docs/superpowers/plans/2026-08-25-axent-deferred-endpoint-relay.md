# Axent Deferred Endpoint Relay Implementation Plan

> **For Codex:** Execute with subagent-driven development and strict TDD.

**Goal:** Allow native AXTP device Endpoint requests to complete asynchronously
through `AxtpControlEndpoint -> AxentHost -> Broker -> AxtpAdapter`, preserving
per-device FIFO and cross-device parallelism without changing AXTP wire facts.

**Architecture:** The runtime broker gains a ManualPoll deferred-response
mechanism. Axent exposes an Endpoint handler returning a public
`ControlOperationPtr`; its private runtime adapter polls that operation and
lets the runtime emit the ordinary correlated/reversed AXTP response.

**Spec:**
`docs/superpowers/specs/2026-08-25-axent-deferred-endpoint-relay-design.md`

### Task 1: Prove the synchronous external-dispatch bottleneck

**Files:**
- Modify: `tests/axtp_control_endpoint_test.cpp`

- [ ] Add a two-device Host/Adapter fixture and native AXTP WebSocket A/B test.
- [ ] Route through the existing synchronous handler and
  `AxentHost::call_endpoint()->wait()` only for characterization.
- [ ] Run the test and capture the expected failure: B cannot complete while A
  blocks the endpoint worker.
- [ ] Keep the test ready to migrate to `register_endpoint_handler()`; do not
  commit a permanently failing state.

### Task 2: Add runtime ManualPoll deferred RPC completion

**Repository:** `third_party/axtp-cpp-runtime`

**Files:**
- Modify: `include/core/runtime/broker/business_router.hpp`
- Modify: `include/core/runtime/broker/business_executor.hpp`
- Modify: `include/core/runtime/broker/basic_broker.hpp`
- Test: `tests/core/phase7_broker_test.cpp`
- Test as needed: `tests/core/phase5_transport_test.cpp`

- [ ] Create branch `codex/endpoint-relay-async-completion` from `0e02f4b`.
- [ ] Write failing deferred handler tests covering blocked A / ready B,
  request IDs, Endpoint reversal, errors, exceptions, and pending destruction.
- [ ] Implement the smallest thread-free deferred poll/result mechanism.
- [ ] Run focused tests, format checks, spec-lock/generated checks, and full
  runtime CTest.
- [ ] Commit the runtime change without modifying spec/generated facts.

### Task 3: Expose the Axent Endpoint handler bridge

**Files:**
- Modify: `include/axent/control/control_contract.hpp`
- Modify: `include/axent/control/axtp_control_endpoint.hpp`
- Modify: `src/control/axtp_control_endpoint.cpp`
- Modify: `tests/control_contract_test.cpp`
- Modify: `tests/axtp_control_endpoint_test.cpp`
- Modify: `third_party/axtp-cpp-runtime`

- [ ] Pin Axent to the Task 2 runtime commit.
- [ ] Add `EndpointControlHandler` and `register_endpoint_handler()` using only
  Axent public types.
- [ ] Translate a returned operation to the private runtime deferred poll;
  retain runtime-owned correlation and Endpoint reversal.
- [ ] Migrate the characterization test and require B-before-A to pass.
- [ ] Cover same-device FIFO, no-`m` compatibility, handler/token cancellation,
  endpoint stop, status mapping, and null-operation fail-closed behavior.
- [ ] Run focused tests and boundary checks, then commit.

### Task 4: Cross-repository verification

- [ ] Verify runtime spec lock remains `spec/v0.15.0` and generated artifacts
  are unchanged.
- [ ] Run full runtime build/CTest/format/spec-lock checks.
- [ ] Run full Axent build/CTest/boundary/diff checks.
- [ ] Validate both repositories from clean recursive checkouts.
- [ ] Record the new runtime commit and final Axent integration commit for the
  NearCast pin.
