# Axent Endpoint-Aware Embedded Host Bridge Design

## Purpose

This change supplies the generic Axent public contracts required by an
embedded multi-device product host. NearCast remains a product host and talks
to Axent only through in-process C++ APIs. External clients use the native AXTP
WebSocket envelope `{sid, op, m?, d}` at `AxtpControlEndpoint`; NearCast does
not add a loopback WebSocket, an RFC JSON-RPC 2.0 client, or runtime types.

## Dependency and protocol constraints

The dependency direction remains:

```text
NearCast product host -> Axent protocol bus -> axtp-cpp-runtime
```

- Axent public headers must not expose `axtp::*` or cpp-runtime headers.
- Axent Core must not contain NearCast names, `cast.*` policy, rendering,
  privacy, media selection, or retry policy.
- The runtime remains pinned to
  `0e02f4bd5d08fc083672e57207fd144835e6df92` and AXTP `spec/v0.15.0` unless
  the concurrency characterization gate below proves a runtime mechanics gap.
- No `endpoint.list`, `endpoint.get`, or `endpoint.changed` method is added.
  Embedded inventory is a C++ Host concern, not a new AXTP protocol fact.
- The RFC JSON-RPC management server remains a legacy/daemon surface and is
  not used by embedded NearCast.

## Integrated dependency baseline

The Axent multi-device branch contains a normal merge of the codec adaptation
commits `a013398` and `6e326b6`. The merge retains endpoint routing, physical
lane isolation, H.264/H.265 negotiation, and the H.264-open/H.265-decode
compatibility override. The merge does not force-push or rewrite either parent
branch.

## Dynamic inventory refresh

`AxentHost` adds:

```cpp
std::vector<DeviceSnapshot> refresh_devices();
```

The operation serializes against Host start/stop, calls `discover()` on every
configured adapter without holding the Host state mutex, then reconciles the
results into `DeviceManager`.

For each adapter:

- Devices returned by the latest discovery are upserted by the existing
  `(adapter, provider-local id)` key.
- A previously known device from that adapter that is not returned is marked
  offline with reason `discovery-missing`.
- A reappearing device is refreshed online without changing an existing stable
  Endpoint binding.
- Endpoint conflicts and Endpoint changes continue to fail closed through
  `DeviceManager::upsert()`.
- A missing device is retained; it is not erased. Sessions, media state, and
  unrelated devices are not cleared by inventory refresh.
- A stopped Host returns an empty list and does not call adapter discovery.

`discover_devices()` remains a read-only cached snapshot accessor.

## Endpoint-aware outbound control

`AxentHost` adds the following public request and method:

```cpp
struct EndpointControlRequest {
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
};

ControlOperationPtr call_endpoint(
    EndpointControlRequest request,
    ControlCallOptions options = {});
```

Both Endpoint IDs and the method are mandatory. Invalid requests complete
immediately with `ControlStatus::InvalidArgument`. A stopped Host completes
with `ControlStatus::Unavailable`.

A valid call is converted to an Axent `ControlCommand` with `src` and `dst`;
it is sent through `Broker::dispatch_async()`. The Broker resolves `dst` to a
known stable device Endpoint, selects the existing physical lane, and passes
Endpoint IDs to the adapter through `AdapterControlRequest`. No device ID or
serial number is added to routed params. Unknown, conflicting, or offline
Endpoint routes fail closed through existing Broker semantics.

The returned operation is tracked by the Host and cancelled on `stop()`. This
API does not require a product session lease and does not introduce a global
queue; Broker physical route keys preserve FIFO for aliases of one device and
parallelism between different devices.

## AXTP ingress metadata

The public control request becomes:

```cpp
struct ControlRequest {
    std::uint32_t request_id = 0;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
};
```

The Endpoint fields are appended to preserve existing aggregate initializer
compatibility. `AxtpControlEndpoint` copies `RpcContext.endpoint.src/dst` into
these Axent strings. Missing `m` produces empty strings and remains valid.
Runtime continues to own SID, op, request correlation, and response metadata
reversal; a routed response must emit `m.src=request.m.dst` and
`m.dst=request.m.src`.

Handlers receive only Axent public types. No runtime context or header crosses
the public boundary.

## External device dispatch and runtime gate

The next integration step will let `AxtpControlEndpoint` dispatch requests
whose destination is a physical device Endpoint through
`AxentHost::call_endpoint()`. Before selecting an implementation, an automated
characterization must block device A while issuing a request to device B.

- If B completes before A is released, no runtime change is allowed.
- If B is serialized behind A, the evidence permits a minimal runtime
  deferred-response mechanism. That mechanism remains protocol-neutral,
  preserves `spec/v0.15.0`, preserves optional `m`, and adds no generated
  method/schema facts.

The endpoint bridge must never wait synchronously for a device operation on a
single global endpoint worker, because that would defeat Broker physical-lane
parallelism.

## Verification

The Axent phase must prove:

- refresh adds new devices, marks missing devices offline, restores reappearing
  devices, preserves stable Endpoint IDs, and leaves other adapters untouched;
- `call_endpoint()` routes by Endpoint, rejects incomplete requests, fails
  closed for unknown/offline/conflicting routes, preserves per-device FIFO,
  and permits different-device operations to run in parallel;
- inbound AXTP `m.src/m.dst` reaches the public handler;
- response `m` is reversed;
- an old request without `m` still completes by request ID;
- public headers and targets pass the Axent dependency-boundary test;
- affected tests, full CTest, and `git diff --check` pass.
