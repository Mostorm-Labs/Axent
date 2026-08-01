# Multi-device control and logical endpoints

This branch keeps the Axent multi-device implementation isolated until the
NearCast integration is ready.  A public `AxtpAdapter` is a device manager;
each discovered physical HID device is backed by a private adapter context with
its own runtime client, transport, session pump, recovery worker, media stream
table, video-parameter state, and diagnostics.

## Control-plane addressing

Axent's WebSocket JSON-RPC envelope addresses logical endpoints rather than
physical device IDs or serial numbers:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "src": "controller:nearcast",
  "dst": "endpoint/receiver-a",
  "method": "status.get",
  "params": {}
}
```

`dst` is resolved through `DeviceSnapshot.endpoint_id`.  A product or adapter
may provide a human-readable endpoint (for example
`endpoint/receiver-a`).  If it does not, `DeviceManager` creates a deterministic
opaque `endpoint/<token>` value; the token does not embed the physical ID,
serial number, or HID path. `devices.list` returns the endpoint as
`endpointId`. Existing `id` and `identity.serialNumber` fields remain visible
as compatibility/diagnostic metadata, but endpoint-aware clients must not use
them as the routing address.

Responses and errors reverse the envelope:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "src": "endpoint/receiver-a",
  "dst": "controller:nearcast",
  "result": {}
}
```

`src` and `dst` must be non-empty strings when either is present.  JSON-RPC
requests that omit the envelope remain accepted as a migration path; their
legacy `params.deviceId`/`params.serialNumber` selectors are removed before
adapter dispatch when a logical `dst` is present.
The legacy `op`/`d` protocol continues to use its existing serial/device
fallback and does not acquire the endpoint extension.

The WebSocket server schedules requests in endpoint lanes: requests for the
same `dst` remain FIFO, while different physical endpoints can run
concurrently. Logical and legacy aliases that resolve to the same physical
device share one canonical lane. Synchronous control-plane dispatch lazily
opens an AXTP context, so a daemon client does not need to expose a physical
session/lease operation first.

## Host and NearCast boundary

Axent retains one media lease per physical device and the existing same-device
control/media/maintenance arbitration.  Leases, diagnostics, media stream
descriptors, and release/reset operations are device-scoped; a reset or
recovery on device A cannot close streams or transports on device B.

NearCast remains responsible for product-level device selection, rendering,
source arbitration, privacy policy, and any multi-device media composition.
NearCast can continue selecting one device, or can add its own per-device
session/media collection after adapting to the endpoint and device-scoped
interfaces.  This branch intentionally does not change the NearCast
repository; merge it only after that adaptation is complete.
