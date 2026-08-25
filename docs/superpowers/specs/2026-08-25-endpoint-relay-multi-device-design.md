# Endpoint Relay Multi-Device Design

## Status

- Design approved in chat on 2026-08-25.
- Implementation branch: `codex/axent-multi-device-management`.
- Axent baseline: `e3eea0a`; the branch currently contains the preliminary
  multi-device implementation at `2e2c78f`.
- AXTP authority: Mostorm-Labs/axtp PR #11, merged to `main` as `2d88ff4`.
- Runtime dependency at design time: `axtp-cpp-runtime@1e09da3`, which does
  not yet implement Endpoint Relay metadata.
- The feature branch must not merge into Axent `main` until the corresponding
  NearCast integration is ready and the integration gates in this document
  pass.

## Goal

Implement AXTP Endpoint Relay for multi-device control without collapsing
protocol mechanics, Axent provider routing, or NearCast product policy into one
layer. Axent's existing external JSON-RPC 2.0 control API remains compatible.
Axent either consumes its top-level `src` / `dst` through local Endpoint
Projection for a legacy device, or bridges them to AXTP object-RPC
`m.src` / `m.dst` for a peer explicitly configured for Native Relay.

## Non-goals

- Do not turn AXTP into a general multi-hop routing protocol.
- Do not put `route`, `routeId`, `nextHop`, `ttl`, `hops`, `trace`, or
  `deadline` into Endpoint Relay v1 metadata.
- Do not change the JSON_BINARY 15-byte fixed header.
- Do not encode multiple destinations in `m.dst`.
- Do not put physical `deviceId`, serial number, HID path, or provider topology
  into the Endpoint Relay wire envelope.
- Do not move NearCast device selection, rendering, privacy, arbitration, or
  multi-device media composition into Axent Core.
- Do not add a new Axent WebSocket event subscription product in this change.
  Existing or future event delivery must obey the Endpoint Relay metadata
  rules, but subscription policy remains outside this scope.
- Do not infer native Endpoint Relay support by probing a legacy peer or by
  inspecting its Endpoint ID. Native Relay is an explicit provider/profile
  capability.

## Normative AXTP Rules

Object-encoded AXTP RPC uses the backward-compatible envelope:

```json
{
  "sid": "12345678",
  "op": 7,
  "m": {
    "src": "ep_app",
    "dst": "ep_device"
  },
  "d": {}
}
```

The implementation must enforce these rules from axtp PR #11:

1. `m` is optional. A legacy `{sid, op, d}` message remains valid.
2. If present, `m` must be an object.
3. `m.src` and `m.dst`, when present, must be non-empty strings and should be
   no longer than 128 UTF-8 bytes.
4. `m.dst` is zero-or-one destination and must never be an array.
5. Structurally valid unknown optional metadata fields are ignored or retained
   according to SDK policy without invalidating the message or session.
6. A response produced for an addressed request uses:

   ```text
   response.m.src = request.m.dst
   response.m.dst = request.m.src
   ```

7. An Event preserves the originating `m.src`. It may omit `m.dst` so local
   subscription or relay policy chooses recipients. Each fanout copy has at
   most one string `m.dst`.
8. Standard Frame `SourceId` / `DestinationId` remain per-link addresses and
   are not Endpoint IDs.
9. JSON_BINARY remains unchanged and cannot carry Endpoint Relay v1 metadata.

## Layer Ownership

### axtp-cpp-runtime: protocol mechanics

The runtime is the single source of truth for the object-RPC metadata shape and
canonical Endpoint ID algorithm. It owns:

- the `PayloadMeta` representation of optional logical source and destination;
- JSON object-RPC parsing, validation, and encoding of `m`;
- response address reversal in every response construction path, including
  normal business responses, business errors, unknown-method responses, local
  handlers, and generated protocol errors that have valid request metadata;
- Event metadata preservation and encoding;
- SDK request APIs that allow an embedding adapter to supply Endpoint metadata
  without putting it in method params;
- conformance coverage for compatibility, single-destination relay, response
  reversal, and Event fanout metadata;
- the canonical Endpoint ID derivation helper.

The runtime does not own device discovery, physical provider selection,
Endpoint Projection, `endpointId -> provider` tables, NearCast identity
persistence, or product authorization policy.

The runtime model is:

```cpp
struct EndpointMetadata {
    std::optional<std::string> src;
    std::optional<std::string> dst;
};

struct PayloadMeta {
    // Existing runtime metadata remains unchanged.
    EndpointMetadata endpoint;
};
```

The public helper is:

```cpp
std::string axtp::endpointIdFromKey(std::string_view endpointKey);
```

It throws `std::invalid_argument` for an empty key. The metadata fields remain
distinct from session IDs, frame node IDs, and internal ingress provenance.

The SDK must expose a typed way to attach Endpoint metadata to a JSON request.
`sdk::CallOptions` receives an `EndpointMetadata endpoint` member and the
dynamic/typed JSON request builders copy it into `RpcPayload::meta.endpoint`.
The adapter must not construct routing fields inside `d.params`.
Supplying non-empty Endpoint metadata with `RpcEncoding::JsonBinary` fails
locally with `InvalidArgument`; the runtime must never silently drop requested
Endpoint addressing onto the unchanged JSON_BINARY v1 envelope.

The runtime Endpoint ID helper implements exactly:

```text
canonicalInput = UTF-8("AXTP-ENDPOINT-v1|" + endpointKey)
digest         = SHA-256(canonicalInput)
endpointId     = "ep_" + lowerHex(digest[0..15])
```

The implementation must be dependency-light and available to runtime
consumers. The same algorithm must not be independently reimplemented in
Axent.

### Axent: identity evidence, providers, projection, and local dispatch

Axent owns public JSON-free control/device contracts and local provider facts.
It owns:

- the explicit `DeviceSnapshot.endpoint_id` contract;
- provider registration represented by an Endpoint-bound device snapshot;
- uniqueness checks and fail-closed binding behavior;
- `endpointId -> adapter + provider-local device/session` resolution;
- online/offline availability classification;
- multi-device FIFO scheduling lanes and cross-device parallelism;
- external JSON-RPC 2.0 decoding/encoding and compatibility behavior;
- the private AxtpAdapter bridge from an Axent routed control request to
  runtime `PayloadMeta.endpoint`;
- stable identity evidence selection for AXTP physical devices;
- transparent Endpoint Projection for legacy devices that do not understand
  `m.src` / `m.dst`.

Axent Core treats Endpoint IDs as opaque non-empty strings. It must not parse
the `ep_` prefix to infer a device type and public Axent headers must not expose
runtime headers or `axtp::*` types.

Canonical ID generation remains implemented once in the runtime. Axent exposes
this adapter-surface facade for NearCast and other Axent consumers:

```cpp
// include/axent/adapters/axtp_endpoint_identity.hpp
std::string axent::axtp_endpoint_id_from_key(std::string_view endpoint_key);
```

The public header contains only standard/Axent types. Its implementation in
`src/adapters/axtp_endpoint_identity.cpp` delegates directly to
`axtp::endpointIdFromKey`; it does not contain a second SHA-256 implementation.
This facade belongs to the AXTP adapter/integration surface, not Axent Core
protocol mechanics.

All Axent device adapters that project resources into the AXTP Endpoint address
space use this facade. The adapters remain responsible for selecting a stable
`endpointKey`; the facade centralizes generation, while DeviceManager only
registers the resulting opaque ID.

### NearCast: product identity and policy

NearCast owns:

- generation-once and persistence of its installation identity;
- the app Endpoint key, using
  `app:<productNamespace>:<persistentInstallationId>`;
- device choice and any group/batch behavior;
- product authorization, rendering, privacy, and source arbitration;
- consumption of device Endpoint IDs returned by Axent;
- multi-device media composition or isolation;
- event subscription and product-level fanout consumption policy.

Axent does not generate or persist NearCast's installation identity. NearCast
may use an Axent facade to derive the canonical ID from its persistent key, but
NearCast remains the owner of the key and its lifecycle.

## Endpoint Identity and Generation

### Receiver behavior

Every receiver treats an Endpoint ID as an opaque non-empty string. Legacy or
external IDs such as `endpoint/receiver-a` remain accepted. Only IDs generated
by the canonical AXTP helper are required to have the
`ep_<32-lowercase-hex>` form.

### AXTP physical and legacy devices

The AxtpAdapter is the Endpoint owner for devices it discovers. Identity
evidence priority is fixed per provider/profile:

1. a device-persistent UUID, when the profile makes it available before
   projection;
2. a stable device public-key identity;
3. canonical vendor ID + product ID + serial number;
4. no stable Endpoint projection.

The initial HID implementation only has evidence item 3 during discovery. For
a device with a non-empty serial number it uses this exact key shape:

```text
device:axtp:hid:<vid-lower-hex4>:<pid-lower-hex4>:<serial-utf8>
```

The serial component uses the exact stable UTF-8 value supplied by the HID
provider; it is not case-folded and USB path/interface/port data is not added.
If a vendor reuses a serial for two devices with the same VID/PID, the resulting
binding conflict fails closed instead of selecting one device.

Once a projected Endpoint has selected an identity evidence class, discovery
refresh, reconnect, replug, `sid` changes, process restart, or parent-Agent
changes must not switch it silently. Future adoption of device UUID/public-key
evidence therefore requires an explicit identity migration/provisioning design
instead of automatically replacing a deployed serial-derived Endpoint ID.

If serial and stronger stable evidence are unavailable, the snapshot keeps its
provider-local `device.id`, but `endpoint_id` remains empty. HID path is valid
for opening the current transport only; it is never an Endpoint key.

A legacy device does not need to generate, store, parse, or receive its
Endpoint ID. Axent projects the device as an Endpoint using the stable evidence
obtained by its adapter. The generated ID is an Axent provider-table identity;
it does not imply that the downstream peer implements Endpoint Relay.

Legacy device identity has exactly three supported cases:

| Available identity | Axent behavior |
|---|---|
| Stable device UUID, public key, or VID/PID/serial | Adapter constructs the fixed stable key and Axent generates a canonical Endpoint ID automatically. |
| No device-owned stable identity, but deployment supplies a persistent managed resource ID | Host/configuration owns a key such as `service:<namespace>:<managedAssetId>` and explicitly binds its generated Endpoint ID to the provider. The Endpoint represents that managed logical resource/slot, not cryptographic proof of a particular physical unit. |
| Neither stable device identity nor persistent managed binding | Keep only the provider-local device ID and omit `endpoint_id`. |

Axent must not generate a random Endpoint ID on every discovery or persist a
random ID indexed only by USB path. Without stable evidence it could not prove
that a reconnected device is the same Endpoint. Explicit managed bindings are
loaded by Axent Host/configuration and supplied as explicit Endpoint bindings;
they are not invented by DeviceManager.

### Axent software endpoints

An Axent daemon or software service uses a configured or persisted key:

```text
agent:<namespace>:<persistentInstallationId>
service:<namespace>:<stableResourceId>
```

The installation/resource identity must survive process restarts and
reconnections. Deleting it during uninstall/re-provisioning creates a new
logical Endpoint and may produce a new ID.

### DeviceManager binding rules

`DeviceManager` must stop synthesizing `endpoint/<FNV>` values from
`adapter + device.id`. Its responsibilities are limited to storing explicit
bindings, maintaining indexes, and enforcing invariants:

- a new snapshot with an empty Endpoint ID is valid but cannot be routed by
  Endpoint ID;
- refreshing an existing device with an empty Endpoint ID preserves an
  existing explicit binding;
- adding an Endpoint ID to a previously provider-local device is allowed if it
  is unique;
- binding one Endpoint ID to two different provider-local devices is rejected
  without mutating the existing binding;
- silently replacing a non-empty Endpoint ID for an existing device is
  rejected; identity migration requires a separate explicit operation;
- marking a device offline preserves its Endpoint binding so it can be
  classified as known-but-unavailable.

Upsert returns these explicit Axent Core types:

```cpp
enum class DeviceUpsertStatus {
    Inserted,
    Refreshed,
    EndpointBound,
    EndpointConflict,
    EndpointChangeRejected,
};

struct DeviceUpsertResult {
    DeviceUpsertStatus status;
    bool accepted() const noexcept;
};

DeviceUpsertResult DeviceManager::upsert(DeviceSnapshot snapshot);
```

Existing callers may ignore successful results, but Host discovery must
surface rejected bindings through diagnostics/logging.

## Provider and Route Model

Endpoint delivery mode is an Axent provider fact:

```cpp
enum class EndpointDeliveryMode {
    LocalProjection,
    NativeRelay,
};
```

`DeviceSnapshot` stores `endpoint_delivery_mode` after `endpoint_id`, defaulting
to `LocalProjection`. `RouteTarget` and `AdapterControlRequest` carry the
resolved mode internally. It is not encoded into `endpointId` and NearCast does
not need it to route a request.

- `LocalProjection` is the default. Axent resolves the external Endpoint and
  invokes the legacy provider without downstream Endpoint metadata.
- `NativeRelay` is opt-in. It is selected only by explicit adapter
  configuration or an adopted provider/profile capability that guarantees
  `m.src` / `m.dst` support.

The initial adapter configuration exposes:

```cpp
EndpointDeliveryMode AxtpAdapterConfig::endpoint_delivery_mode =
    EndpointDeliveryMode::LocalProjection;
```

There is no trial request, version-string heuristic, or Endpoint-prefix test
for changing this mode.

For the current physical-device scope, an Endpoint Provider binding is
represented by:

```text
endpointId -> DeviceSnapshot(
    adapter,
    provider-local device.id,
    online state,
    endpoint delivery mode)
```

A projected child Endpoint may use the same representation: its `device.id` is
provider-local and its `endpoint_id` is the stable address visible upstream.
The upstream control plane never needs the parent/child Agent path.

Route lookup has four meaningful outcomes:

| Outcome | Meaning | Control status |
|---|---|---|
| Found | Unique Endpoint and online provider | Dispatch to adapter |
| Unknown | No registered binding | `NotFound` |
| Unavailable | Binding is known but provider is offline/retiring | `Unavailable` |
| Conflict | Ambiguous/corrupt binding | Fail closed; `Unavailable` plus diagnostics |

The route API represents those outcomes directly while retaining the current
optional-returning functions as compatibility wrappers:

```cpp
enum class RouteResolutionStatus {
    Found,
    NotFound,
    Unavailable,
    Conflict,
};

struct RouteResolution {
    RouteResolutionStatus status = RouteResolutionStatus::NotFound;
    std::optional<RouteTarget> target;
};

RouteResolution RouteManager::resolve_endpoint_route(
    const std::string& endpoint_id) const;
```

Broker uses `resolve_endpoint_route`; it does not collapse every unsuccessful
lookup into `NotFound`.

Legacy lookup by `deviceId` or serial number remains available only for
requests that omit Endpoint addressing. It is a compatibility selector, not an
Endpoint ID source.

The scheduling lane is canonicalized to:

```text
physical:<adapter>:<provider-local-device-id>
```

Therefore an Endpoint address and a legacy alias for the same device serialize
in one FIFO lane, while different resolved devices execute concurrently.

## Axent Routed Adapter Contract

The current `Adapter::call(device_id, method, params)` boundary loses logical
source/destination information. The broker must use a JSON-free routed adapter
request, with a compatibility default for adapters that do not understand
Endpoint metadata:

```cpp
struct AdapterControlRequest {
    std::string device_id;               // provider-local target
    std::string source_endpoint_id;       // optional logical source
    std::string destination_endpoint_id;  // resolved logical destination
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection;
    std::string method;
    nlohmann::json params;
};
```

`Adapter` adds these overloads while retaining the existing signatures:

```cpp
virtual ControlResult call(const AdapterControlRequest& request);
virtual ControlOperationPtr call_async(
    const AdapterControlRequest& request,
    ControlCallOptions options = {});
virtual ControlResult start_firmware_update(
    const AdapterControlRequest& request,
    const std::string& file_path);
```

Their default implementations delegate to the existing provider-local
signatures, so AXDP, TEA, and mock adapters remain source-compatible.
AxtpAdapter overrides the routed overloads. The behavioral requirements are:

- Broker resolves `ControlCommand.dst` before invoking an adapter.
- Broker passes the resolved target Endpoint rather than a physical selector
  copied from params.
- Existing AXDP, TEA, and mock adapters may use a default implementation that
  delegates to their legacy method signature.
- In `LocalProjection`, AxtpAdapter sends the legacy downstream AXTP request
  without `m`, even though the external request was routed by Endpoint ID.
- In `NativeRelay`, AxtpAdapter supplies source/destination through the runtime
  SDK metadata API.
- Routing fields are never inserted into business params.
- `src` is address/provenance metadata and is not, by itself, proof of
  authorization. Current middleware and future authenticated control-session
  bindings remain the authorization authority.

Internal health checks, media configuration, and other adapter-owned RPCs that
are not relayed from an external control request may continue without Endpoint
metadata unless the device/profile requires an explicitly configured local
Agent Endpoint.

## External WebSocket Compatibility Bridge

Axent's external management endpoint continues to accept JSON-RPC 2.0:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "src": "ep_nearcast",
  "dst": "ep_receiver",
  "method": "status.get",
  "params": {}
}
```

This is not the AXTP object-RPC wire format. The bridge is:

```text
Axent JSON-RPC 2.0 src/dst
  -> ControlCommand src/dst
  -> RouteManager resolves dst to provider
  -> AdapterControlRequest source/destination Endpoint IDs + delivery mode
  -> LocalProjection: downstream legacy AXTP {sid, op, d}
  -> NativeRelay: downstream AXTP {sid, op, m:{src,dst}, d}
```

On response:

```text
AXTP response m.src/m.dst
  -> routed adapter result/correlation
  -> Axent JSON-RPC response src/dst
```

For the current synchronous request/response bridge, the external response
envelope deterministically reverses the validated request addresses. Runtime
tests independently prove that the downstream AXTP response also reverses its
metadata. Existing `sid` and request-ID response correlation remains
authoritative. When a peer emits response Endpoint metadata, the runtime keeps
it available for validation/diagnostics; a legacy response that omits optional
`m` remains compatible as allowed by the AXTP specification.

External compatibility rules are:

- routed JSON-RPC requests require `src` and `dst` together and both must be
  non-empty strings;
- when `dst` is present, `params.deviceId` and `params.serialNumber` are not
  routing authorities and are removed before adapter dispatch;
- requests without `src` / `dst` retain legacy `deviceId` / `serialNumber`
  behavior during the migration window;
- successful responses and errors reverse validated `src` / `dst`;
- malformed half-envelopes return `InvalidArgument` without inventing a
  missing peer;
- `devices.list` advertises `endpointId` only for stably bound/projected
  devices; provider-local devices may omit it.

Changing this endpoint itself to AXTP `{sid, op, m?, d}` is a separate protocol
migration and is not part of this implementation.

`DecodedControlMessage` receives
`std::optional<std::string> validation_error`. The JSON-RPC decoder sets it
when either routing field is present but the pair is incomplete, non-string,
or empty. `ControlPlane::handle_text` returns `InvalidArgument` before Broker
dispatch when this value is set. This prevents two malformed fields from being
mistaken for an unrouted legacy request.

## Event Semantics

The runtime must be able to decode, preserve, and encode Event Endpoint
metadata. If an Axent control endpoint or relay fans an Event out in the
future, it must preserve the originating source and create individual copies:

```text
input:   src=ep_device, dst absent
copy A:  src=ep_device, dst=ep_app_a
copy B:  src=ep_device, dst=ep_app_b
```

An array destination is always invalid. This change does not introduce a new
subscription registry in Axent; subscription and product fanout policy are
separate concerns.

## Error Handling and Security

- Unknown Endpoint: `NotFound`.
- Known but offline/retiring Endpoint: `Unavailable`.
- Duplicate Endpoint binding: reject registration and fail closed.
- Non-object `m`, empty/non-string known metadata, or array `m.dst`: reject the
  RPC payload without tearing down an otherwise healthy session.
- Unknown optional metadata: ignore or retain, keep the session usable.
- A claimed `src` never grants permission. Authorization uses authenticated
  session/provider context and middleware policy.
- Endpoint keys are local identity inputs and are never sent on wire or
  returned by `devices.list`.
- Logs may contain Endpoint IDs for diagnostics but must not substitute them
  for authentication identities.

## Repository and Branch Sequence

Implementation is intentionally split across repositories:

1. In `axtp-cpp-runtime`, create a dedicated branch such as
   `codex/endpoint-relay-runtime` from its latest `main`.
2. Implement and validate protocol metadata, SDK support, canonical ID
   derivation, and conformance there. Open/merge the runtime PR independently.
3. Only after a reviewable runtime commit exists, update Axent's
   `third_party/axtp-cpp-runtime` pin on
   `codex/axent-multi-device-management`.
4. Correct the preliminary Axent implementation on that branch: remove FNV
   fallback identity, enforce provider bindings, carry routed metadata through
   the adapter boundary, default old devices to Local Projection, and bridge
   metadata only for explicitly configured Native Relay peers in AxtpAdapter.
5. NearCast adapts on its own feature branch and supplies its persistent app
   identity and endpoint-aware product behavior.
6. Merge Axent to `main` only after the NearCast integration gate passes.

The runtime submodule must not be pinned to an unreviewed working tree or to
the axtp specification repository. axtp PR #11 is the protocol authority but
is not a runtime implementation commit.

## Test Strategy

### axtp-cpp-runtime

Tests must cover:

- legacy `{sid, op, d}` messages without `m`;
- encode/decode round trips for request, response, and Event metadata;
- non-object `m`, empty/non-string `src` / `dst`, and array `dst` rejection;
- unknown optional metadata preserving session liveness;
- response reversal for success, business error, unknown method, and generated
  local error paths;
- Event source preservation and singular/absent destinations;
- unchanged JSON_BINARY offsets and length;
- canonical Endpoint ID vectors, including repeatability and exact lowercase
  32-hex output;
- the axtp endpoint-relay conformance profile.

### Axent

Tests must cover:

- no Endpoint ID is synthesized from a provider-local device ID or HID path;
- stable VID/PID/serial evidence generates the expected canonical vector;
- a path-only HID device remains provider-local and has no `endpointId`;
- a legacy device with stable identity receives an Axent-projected canonical
  Endpoint ID without receiving downstream `m` metadata;
- an explicit persistent managed-resource binding projects a device that lacks
  device-owned stable identity;
- DeviceManager preserves explicit bindings across refresh/offline transitions;
- duplicate and silent identity-change bindings fail closed;
- route lookup distinguishes unknown from known-offline targets;
- Endpoint and legacy aliases for one physical device share a FIFO lane;
- different devices execute concurrently;
- external JSON-RPC validates, strips legacy selectors, and reverses addresses;
- AxtpAdapter omits metadata in default `LocalProjection`, passes `src` /
  resolved `dst` in explicit `NativeRelay`, and never inserts them into params;
- public Axent headers remain free of runtime headers and `axtp::*` types.

### Recursive integration

Before the Axent branch is considered ready for NearCast:

```text
runtime unit tests
runtime endpoint-relay conformance
Axent dependency-boundary test
affected Axent unit/integration tests
full Axent ctest suite
git diff --check
clean recursive checkout/configure/build/test
```

### NearCast merge gate

Before merging Axent to `main`, an integration test must demonstrate:

1. NearCast reuses the same app Endpoint across reconnect and process restart.
2. Two devices receive distinct stable Endpoint IDs.
3. NearCast controls each device using `dst`, without selecting by serial or
   physical `deviceId` in the routed request.
4. Same-device calls remain FIFO and different-device calls can overlap.
5. Responses reverse source/destination correctly through both bridge layers.
6. Device reconnect, USB port change, and AXTP `sid` change do not change the
   device Endpoint ID.
7. A device without stable identity is visible for diagnostics but is not
   advertised as a stable routable Endpoint.

## Documentation Deliverables

Implementation must update `docs/architecture/MULTI_DEVICE_CONTROL.md` to
replace the preliminary FNV fallback description with the canonical identity
and provider model. Runtime release notes/conformance declarations must state
Endpoint Relay support, and the NearCast integration must document ownership of
its persistent installation identity.
