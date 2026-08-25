# Axent Deferred Endpoint Relay Design

## Context

`AxtpControlEndpoint` currently executes every registered handler synchronously
on its single endpoint polling worker. That behavior is correct for short
NearCast-owned `cast.*` handlers, but it cannot relay a request to an
asynchronous physical device operation: if device A is blocked while the
handler waits, the endpoint worker cannot admit an already-received request for
device B.

The previously completed Host bridge already provides the required outbound
operation:

```cpp
ControlOperationPtr AxentHost::call_endpoint(
    EndpointControlRequest request,
    ControlCallOptions options = {});
```

The remaining gap is deferred response completion between the public Axent
control endpoint and the private AXTP runtime broker. This is protocol
mechanics, not a new wire fact. The AXTP envelope, generated registries, spec
lock, Endpoint metadata semantics, request IDs, and response address reversal
remain unchanged.

## Characterization gate

Before modifying the runtime, add an end-to-end Axent test using native AXTP
WebSocket messages and an `AxentHost` with two Endpoint-routed devices:

- route A returns a pending operation;
- route B can complete immediately;
- send A, then B without releasing A;
- require B's response to arrive first with its own request ID and reversed
  Endpoint metadata;
- then release A and require A to complete;
- require two requests for the same physical target to remain FIFO.

The current synchronous handler path is expected to fail the B-before-A
assertion. That failure is the evidence authorizing the runtime change. A
source-code observation that the endpoint has one worker is not sufficient by
itself.

## Axent public contract

Keep the existing synchronous `ControlHandler` for product handlers. Add a
separate asynchronous handler contract using only public Axent types:

```cpp
using EndpointControlHandler =
    std::function<ControlOperationPtr(const ControlRequest&)>;

RegistrationToken register_endpoint_handler(
    ControlRoute route,
    EndpointControlHandler handler);
```

NearCast registers `cast.*` with `register_handler()` and registers device
routes with `register_endpoint_handler()`. An Endpoint handler normally returns
`AxentHost::call_endpoint()` directly. NearCast does not construct `{sid, op,
m, d}`, does not wait synchronously, and does not include runtime headers.

The Axent endpoint maps the final Axent `ControlStatus` to the existing AXTP
error codes and serializes only the result body. It tracks accepted operations
so handler removal and endpoint stop cancel pending work. Registration and Host
lifetime remain owned by the embedding application; the endpoint never owns an
`AxentHost`.

## Runtime deferred completion

Add a ManualPoll-compatible broker contract:

```cpp
using DeferredRpcPoll =
    std::function<std::optional<RpcResponseData>()>;
using DeferredRawRpcHandler =
    std::function<DeferredRpcPoll(const RpcContext&, const RpcRequestView&)>;
```

`BusinessRouter` prepares the normal response template and either produces an
immediate `RpcResponseData` or stores a deferred poll function. `BasicBroker`
polls pending completions on later ticks and emits the normal `BrokerResult`
when one becomes ready. It creates no thread, future, executor, condition
variable, transport dependency, or upper-layer dependency.

Deferred and immediate responses use the same request `RpcPayload` template,
so the existing core encoder continues to own request correlation, status
encoding, and `m.src/m.dst` reversal. Exceptions and an empty deferred poll
fail closed as `InternalError` for only that request. Pending responses are
discarded when the broker is destroyed.

## Compatibility and boundaries

- No AXTP spec or generated artifact changes.
- Runtime remains locked to `spec/v0.15.0` until Axent pins the new runtime
  implementation commit.
- Existing `registerRawMethod`, `registerJsonMethod`, `registerTlvMethod`, and
  synchronous `AxtpControlEndpoint::register_handler` behavior remains intact.
- Messages without `m` remain valid. Endpoint handlers may fail closed when a
  destination Endpoint is required; ordinary product handlers still receive
  empty Endpoint strings.
- Runtime does not learn about Axent, devices, physical lanes, NearCast, or
  product policy.
- NearCast production code continues to depend only on Axent public headers.

## Verification

- Runtime unit tests cover immediate/deferred mixing, B-before-blocked-A,
  deferred errors/exceptions, request ID preservation, response metadata
  reversal, and destruction with pending work.
- Axent tests cover native WebSocket A/B relay, same-device FIFO, stop/token
  cancellation, legacy no-`m`, and public dependency boundaries.
- Full runtime and Axent builds/tests, format checks, spec-lock checks,
  submodule pin checks, and clean recursive checkout validation are required.
