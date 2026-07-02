# Axent Daemon And Embedded Media Relay Design

Date: 2026-07-02

Project: Axent

## 1. Goal

Axent becomes the standard integration layer between AXTP protocol runtimes,
physical device transports, and product business logic.

The core implementation lives in `libaxent`. `axentd` uses `libaxent` as the
default system daemon, and products such as NearCast may also link `libaxent`
directly as an embedded host. Both modes must expose the same logical device,
control, and media-session contract.

This design extends the initial Axent core design. It clarifies ownership of
concrete transports, daemon resource arbitration, and low-latency media relay
for display products such as NearCast.

## 2. Architectural Principle

`libaxent` is the source of truth.

```text
libaxent
  owns transport/session/control/media relay logic
        |
        +-- axentd
        |     system daemon using libaxent
        |
        +-- NearCast embedded mode
        |     product process links libaxent directly
        |
        +-- other products
              link libaxent or connect to axentd
```

`axentd` must not fork a separate implementation of device management. NearCast
must not need to duplicate HID/AXTP transport opening logic when it wants an
embedded integration.

## 3. Responsibility Boundaries

`axtp-cpp-runtime` stays focused on protocol/runtime/profile primitives:

- AXTP frame, payload, RPC, event, and stream concepts.
- Runtime endpoint and broker primitives.
- `ITransport` abstraction.
- Protocol profiles such as media and stream profiles.
- Optional transport adapter code that can consume externally provided
  concrete dependencies.

`libaxent` owns product integration responsibilities:

- Concrete transport selection and lifecycle.
- HID/WebSocket/TCP/AXDP/TEA dependency ownership for Axent builds.
- Device discovery, device leases, and media-session leases.
- AXTP device-session creation and app-ready/session handshakes.
- Stream open/close and keyframe/control forwarding.
- Bounded media relay buffers and relay diagnostics.
- Control-plane routing, event fanout, capability routing, and diagnostics.

NearCast owns presentation responsibilities:

- `MediaCore`.
- H.264/AAC assembly policy.
- Render clock, late drop, catch-up, keyframe rebase, and decoder reset policy.
- D3D11, Media Foundation, WASAPI, overlay, and UI state.

Axent must relay encoded media data; it must not become a renderer.

## 4. NearCast Reference Architecture

Current NearCast is a single-process host:

```text
NearCast.exe
  opens HID/AXTP transport
  creates AXTP endpoint and media session
  opens video/audio streams
  receives H.264/AAC stream chunks
  MediaCore assembles, queues, drops, catches up, and renders
```

This works for one product, but it makes the UI process the physical device
owner. Multiple NearCast-like products on one PC can then compete for the same
HID/AXTP device.

The new model moves physical ownership into Axent, while preserving NearCast's
rendering model:

```text
Device
  HID / AXTP / AXDP / TEA
        |
        v
libaxent host
  owns physical transport
  owns AXTP session
  owns device and media leases
  opens/closes streams
  relays encoded stream data
        |
        v
NearCast MediaCore
  assembles and renders media
```

The `libaxent host` can be either `axentd` or an embedded `AxentHost` inside the
NearCast process.

## 5. Two NearCast Integration Modes

NearCast must use one source-provider contract with two implementations.

```text
INearCastSource
  listDevices()
  acquireCastSession()
  releaseCastSession()
  openMediaStreams()
  closeMediaStreams()
  requestKeyFrame()
  setFlowPolicy()
  setRenderFpsHint()
  subscribeEvents()
  readMediaFrames()
```

Embedded mode:

```text
Device -> libaxent inside NearCast -> NearCast MediaCore -> Renderer
```

Daemon mode:

```text
Device -> axentd(libaxent) -> local media relay -> NearCast MediaCore -> Renderer
```

The two modes should produce the same logical events, stream metadata,
capability model, and control responses. NearCast chooses the mode at startup
or by product configuration.

Embedded mode is useful for development, portable product builds, diagnostics,
and deployments where a system daemon is not desired.

Daemon mode is the default system integration. It ensures a PC has one device
owner and lets multiple frontends observe or request sessions without racing
for the same physical transport.

## 6. Device And Media Lease Model

`libaxent` owns two related leases:

- Device lease: the host process owns the physical device transport.
- Media lease: one active frontend owns the right to receive and control a
  live media session for a device.

`axentd` holds device leases while it runs. Frontends never open the HID/AXTP
device directly in daemon mode.

Media lease policy:

- Only one active renderer may consume live media for a device by default.
- Other clients may observe status, diagnostics, and low-frequency events.
- A client may request explicit transfer or revoke, but it must not silently
  steal an active media session.
- Release, disconnect, crash, or heartbeat timeout returns the media lease to
  the daemon.

Embedded mode may skip inter-process arbitration, but it must still use the same
internal lease state so behavior matches daemon mode.

## 7. Control Plane

Control commands can use WebSocket JSON-RPC, AXTP JSON-RPC, or a future local
IPC wrapper over the same command model.

Required commands include:

- `device.list`
- `device.getStatus`
- `session.acquire`
- `session.release`
- `media.openStream`
- `media.closeStream`
- `media.requestKeyFrame`
- `media.setFlowPolicy`
- `media.setRenderFpsHint`
- `media.getFlowState`
- `events.subscribe`
- `diagnostics.collect`

Control-plane WebSocket is suitable for commands, status, diagnostics, and
event notification. It is not the preferred high-rate media data path.

## 8. Media Data Plane

Axent relays encoded media, not rendered frames.

The relay unit is an `AxentMediaFrame`:

```text
sessionId
streamId
kind: video | audio
codec: h264 | aac | pcm | opaque
sequenceId
cursor
timestamp
flags: keyframe, config, discontinuity, endOfFrame
payload
```

The relay preserves AXTP stream metadata and adds delivery diagnostics. It does
not decide D3D/WASAPI renderer timing.

Preferred daemon-mode transport:

- A local shared-memory ring buffer for high-rate encoded media frames.
- A small control/notify channel for descriptors, producer cursor, consumer
  cursor, heartbeat, and errors.

Fallback transport:

- Binary WebSocket frames or local socket frames can be used for early
  integration and diagnostics, but they are not the long-term low-latency path
  for high-bitrate video.

Embedded mode can deliver frames by callback or in-process ring buffer. The
public semantics should match daemon mode.

## 9. Relay Backpressure And Stability

Axent must protect the physical device session from slow consumers.

Relay behavior:

- Use bounded buffers.
- Track producer cursor, consumer cursor, dropped frames, dropped bytes,
  keyframe count, consumer lag, and stream age.
- Drop old media when the consumer falls behind.
- Prefer preserving keyframe/config boundaries.
- Emit discontinuity markers when data is dropped.
- Request a keyframe when drops make the next decodable video frame uncertain.
- Keep control/event delivery independent from high-rate media delivery.

NearCast `MediaCore` still owns render-specific policies:

- Video access-unit assembly.
- Render clock mapping.
- Late frame drop.
- Catch-up to latest keyframe.
- Keyframe rebase.
- Decoder reset.
- Audio/video presentation behavior.

NearCast should send feedback to Axent, such as:

- consumer lag
- desired flow mode
- render FPS hint
- keyframe request
- paused/resumed state

Axent uses this feedback for stream control and relay pressure, not for
renderer-specific scheduling.

## 10. HID AXTP Transport Behavior To Standardize

Axent should absorb the host-level HID behavior proven in NearCast:

- Enumerate HID candidates by VID/PID.
- Filter by path, serial, usage page, and usage.
- Log candidate metadata for diagnostics.
- Allow input/output report sizes to be set to auto.
- Open by path when available.
- Read negotiated input report size, output report size, read-buffer size, and
  preferred frame size after open.
- Attach the opened transport to an AXTP endpoint.
- Run app-ready/session initialization before exposing a session as ready.
- Record HID read timeout, read error, write error, dropped report id, and
  queued-report statistics.
- Trigger reconnect or cleanup when transport errors indicate device removal.

This behavior belongs in Axent's transport/session host layer. It should not be
copied into every product and should not be pushed into `axtp-cpp-runtime` as a
product policy.

## 11. Public Library Shape

The first public surface should be centered on `AxentHost`.

```text
AxentHost
  start()
  stop()
  discoverDevices()
  acquireSession(selector, options)
  releaseSession(sessionId)
  openMediaStream(sessionId, options)
  closeMediaStream(sessionId, streamId)
  call(sessionId, method, params)
  subscribeEvents(handler)
  createMediaConsumer(sessionId, options)
  diagnostics()
```

`AxentHost` uses lower-level managers internally:

- `TransportManager`
- `DeviceManager`
- `DeviceSessionManager`
- `MediaSessionManager`
- `MediaStreamRelay`
- `ControlPlane`
- `CapabilityRegistry`
- `RouteManager`

These managers may have public test hooks, but product code should start with
`AxentHost` unless it needs advanced integration.

## 12. Build And Dependency Boundary

Axent owns concrete transport dependencies for Axent builds.

Planned CMake behavior:

- Axent switches `third_party/axtp-cpp-runtime` to the clean runtime-boundary
  branch or a commit containing that boundary.
- Axent provides `hidapi::hidapi` and `ixwebsocket::ixwebsocket` before adding
  `axtp-cpp-runtime`.
- Axent keeps `AXTP_CPP_RUNTIME_BUILD_TOOLS=OFF`.
- Axent does not enable cpp-runtime tool dependency fetching for product builds.
- `libaxent` links the optional AXTP transport adapter targets only after Axent
  has provided their concrete dependencies.

NearCast embedded mode links `libaxent`; it does not need to separately own the
same HID/AXTP opening logic.

NearCast daemon mode links a small Axent client library or uses the documented
control/media IPC protocol.

## 13. First Implementation Slice

The first implementation slice should not attempt to port all NearCast media
features. It should establish the reusable boundary.

Required first slice:

- Update Axent design docs and README to describe `libaxent` first,
  `axentd` host, and embedded product host modes.
- Move Axent's cpp-runtime dependency to the clean runtime-boundary commit.
- Add Axent-owned HID/IXWebSocket dependency wiring.
- Add transport/session types:
  - `TransportDescriptor`
  - `TransportSelector`
  - `MediaFrame`
  - `MediaConsumer`
  - `AxentHost`
- Add an in-process media relay with bounded buffering.
- Add a mock media producer/consumer test.
- Add HID option mapping and diagnostics types, even if hardware tests remain
  behind platform/hardware guards.
- Keep NearCast `MediaCore` outside Axent.

Deferred:

- Production shared-memory ring buffer.
- Full NearCast migration.
- Authentication and authorization.
- Multi-client transfer UI.
- Full real-device hardware certification.

## 14. Acceptance Criteria

Architecture acceptance:

- `libaxent` is documented as the only core implementation.
- `axentd` and embedded product hosts are documented as peer hosts.
- Axent media relay is explicitly not a renderer.
- NearCast `MediaCore` remains a business/product layer component.

Build acceptance:

- Axent owns concrete HID/WebSocket dependencies in its top-level build.
- `axtp-cpp-runtime` is not configured to build tools for Axent product builds.
- The default `libaxent` build does not rely on cpp-runtime fetching concrete
  transport dependencies.

API acceptance:

- Tests can create an `AxentHost` or equivalent first-slice host object.
- Tests can acquire a mock session and consume mock media frames.
- Slow consumer tests show bounded buffering and reported drops.
- Control/event tests continue to pass.

NearCast integration acceptance:

- The design supports both `EmbeddedAxentSource` and `DaemonAxentSource`.
- Both source modes feed the same NearCast media-source contract.
- The contract carries enough metadata for NearCast `MediaCore` to keep its
  render-specific queue, catch-up, and keyframe behavior.
