# Multi-device control and Endpoint ownership

This branch keeps Axent's multi-device implementation isolated until NearCast
passes the integration gate. The dependency and identity flow is:

```text
NearCast persistent app key -> Axent JSON-RPC src
Axent Endpoint provider table -> adapter + provider-local device + delivery mode
LocalProjection -> legacy downstream request without m
NativeRelay -> runtime PayloadMeta.endpoint -> AXTP m.src/m.dst
```

NearCast owns its persistent application installation identity and device
selection policy. Axent owns device/provider bindings and control scheduling.
`axtp-cpp-runtime` owns the AXTP object-RPC wire mechanics; its headers and
`axtp::*` types remain private to the AXTP adapter implementation.

## External control-plane compatibility

Axent's external WebSocket API remains JSON-RPC 2.0. It does not become the
AXTP `{sid, op, m, d}` profile:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "src": "ep_controller",
  "dst": "ep_receiver",
  "method": "status.get",
  "params": {}
}
```

If either `src` or `dst` is present, both must be non-empty strings. Malformed
routing is rejected with JSON-RPC `-32602` before Broker or adapter dispatch;
Axent never invents a half-envelope. Responses and errors reverse the valid
request envelope:

```text
response.src = request.dst
response.dst = request.src
```

Requests with no `src` and `dst` remain fully supported. They continue to use
`params.deviceId` or `params.serialNumber`, and their responses contain no
routing fields. For a valid addressed request, those legacy selectors are
removed before adapter dispatch, so business params cannot override `dst` or
leak a physical selector into a downstream RPC.

The legacy `op`/`d` control protocol is unchanged and continues to use its
existing serial/device fallback.

## Endpoint identity and old devices

`DeviceManager` stores and validates an Endpoint supplied by its owner; it does
not generate one. In particular, it never hashes a provider-local device ID,
HID/USB path, connection, session ID, current parent Agent, IP address, or
request ID.

There are three legacy-device cases:

1. A device exposes stable identity evidence. The owning adapter projects it
   into a canonical Endpoint. The initial AXTP HID rule uses VID, PID, and a
   non-empty serial number only.
2. A device lacks device-owned identity, but the deployment has a persistent
   managed-resource binding. The Host may explicitly assign the Endpoint for
   that stable resource.
3. Neither stable source exists. `endpoint_id` remains empty; the device is
   controllable only through its provider-local legacy selector and is not
   advertised as a stable Endpoint.

The canonical HID key is exactly:

```text
device:axtp:hid:<vid-lower-hex4>:<pid-lower-hex4>:<serial-utf8>
```

The canonical AXTP Endpoint algorithm is:

```text
canonicalInput = UTF-8("AXTP-ENDPOINT-v1|" + endpointKey)
digest         = SHA-256(canonicalInput)
endpointId     = "ep_" + lowerHex(digest[0..15])
```

The serial bytes are not case-folded. HID path, interface number, USB port,
and discovery order never participate in the key.

Endpoint bindings are fail-closed. A duplicate binding is rejected as
`EndpointConflict`; changing an existing non-empty binding is rejected as
`EndpointChangeRejected`. Refreshes that omit an already bound Endpoint retain
the stored binding, and marking a device offline does not erase it.

## Provider resolution and delivery modes

Endpoint resolution returns one of four states:

```text
Found       -> one online provider/device binding
NotFound    -> no stored binding
Unavailable -> a known binding is offline or retiring
Conflict    -> defensive ambiguity; dispatch fails closed
```

Unknown Endpoints map to `ControlStatus::NotFound`. Unavailable and conflicting
routes map to `ControlStatus::Unavailable`. A successful route carries the
adapter name, provider-local device ID, logical Endpoint, and an explicit
delivery mode.

`EndpointDeliveryMode::LocalProjection` is the default. It lets Axent address
an old physical peer without adding Endpoint metadata to the downstream RPC.
`EndpointDeliveryMode::NativeRelay` is opt-in adapter/provider configuration;
Axent must not infer it from a version string, Endpoint prefix, or trial probe.

For NativeRelay, the private AXTP adapter maps the routed Axent request to the
runtime metadata object:

```json
{
  "sid": "12345678",
  "op": 7,
  "m": {"src": "ep_controller", "dst": "ep_receiver"},
  "d": {}
}
```

The runtime treats `m` as optional. A downstream WebSocket object-RPC message
without `m` is decoded as an unaddressed legacy message, and an unaddressed
response omits `m`. `JSON_BINARY` keeps its existing fixed envelope and cannot
carry Endpoint metadata.

## Scheduling and device isolation

Each physical AXTP device has a private context with its own runtime client,
transport, session pump, recovery worker, media stream table, video-parameter
state, diagnostics, and Endpoint/provider binding.

WebSocket requests are scheduled by canonical physical lane:

```text
physical:<adapter>:<provider-local-device-id>
```

Endpoint and legacy aliases for the same device therefore execute FIFO in one
lane. Different physical devices may execute concurrently, so a blocked call
for device A does not starve device B.

## Integration and merge gate

The runtime feature branch is pinned to `spec/v0.15.0` and declares all three
Endpoint Relay conformance cases. Axent pins that reviewed runtime commit,
projects serial-backed HID devices with the canonical algorithm, and maps only
explicit `NativeRelay` requests into runtime `CallOptions::endpoint`.
`LocalProjection` and legacy calls continue to omit downstream metadata, and a
legacy response without `m` still completes through request-ID correlation.

NearCast must now verify persistent app identity, two-device routing, response
reversal, reconnect stability, and the no-stable-identity case. Keep both the
runtime and Axent feature branches unmerged from `main` until those integration
checks pass.
