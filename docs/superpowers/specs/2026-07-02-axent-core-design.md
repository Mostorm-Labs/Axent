# Axent Core Design

Date: 2026-07-02

Project: Axent

Full name: Axtp Endpoint Agent

## 1. Goal

Axent is the new AXTP endpoint agent for device governance. It exposes an AXTP-oriented WebSocket control plane, manages endpoint devices through a reusable core, and routes commands to AXDP, AXTP, and TEA adapters.

The first stage builds a new cross-platform Axent core beside the existing legacy implementation. It does not replace the old `agent/` immediately. Instead, it establishes the new repository structure, core model, WebSocket server, daemon/service lifecycle, adapter contracts, mock governance flow, and phased runtime integration path.

## 2. Naming

- Project name: `Axent`
- Full name: `Axtp Endpoint Agent`
- Repository name: `axent`
- C++ namespace: `axent`
- Core library: `libaxent`
- CLI command: `axent`
- CLI module: `axent_cli`
- Daemon process: `axentd`
- Daemon module: `axent_daemon`
- Service name: `axentd.service`
- Config file: `axent.toml`

`axentd` is the background daemon/service. `axent` is the local management CLI for commands such as `status`, `list`, `reload`, and `diagnostics`. The CLI is not a separate governance server; the primary remote control surface is the WebSocket server hosted by `axentd`.

## 3. Repository Structure

The existing `agent/` directory remains as a complete legacy implementation. It keeps its current CMake files, `agent/ThirdParty`, WebSocket client behavior, AXDP/TEA calls, Windows hidden-window behavior, and old protocol implementation. It is a regression reference and legacy runnable version, not the architectural base for the new core.

The repository root becomes the new standard C++ project:

- `CMakeLists.txt`: root build for the new Axent project.
- `include/`: public headers for `libaxent`.
- `src/`: new implementation for core, control plane, daemon, CLI, and adapters.
- `tests/`: unit, contract, and smoke tests.
- `cmake/`: build helpers.
- `config/`: default `axent.toml` and examples.
- `packaging/`: Windows Service and macOS launchd assets.
- `third_party/`: shared dependencies for the new implementation.

Dependency layout:

- Move root `axdp/` to `third_party/axdp`.
- Move root `axtp/` to `third_party/axtp`.
- Move root `TeaSdkMacOS_1.0.0.36/` to a clear TEA SDK path such as `third_party/tea/macos/TeaSdkMacOS_1.0.0.36`.
- Add `third_party/axtp-cpp-runtime` as a git submodule.
- Do not move or rewrite `agent/ThirdParty/` in the first stage.

## 4. Architecture

The accepted architecture is:

```text
AXTP WebSocket Control Plane
  External communication / RPC / Event / Stream / local management bridge
        |
        v
Axent Core
  DeviceManager
  SessionManager
  ControlSessionManager
  DeviceSessionManager
  RouteManager
  CapabilityRegistry
  AdapterRegistry
  Broker / Middleware / FlowControl / Logger
        |
        +-- AXDP Adapter  source-level integration
        +-- AXTP Adapter  runtime integration
        +-- TEA Adapter   third-party SDK wrapper
```

`libaxent` owns the core model and must not depend on legacy `agent/` globals, the legacy Windows message loop, or legacy protocol parsing code.

## 5. Core Components

`DeviceManager` maintains device instances, online/offline lifecycle, device snapshots, and adapter ownership. It does not call SDKs directly.

`SessionManager` is the lifecycle and index root for sessions. It coordinates `ControlSessionManager` and `DeviceSessionManager`, but does not carry business logic.

`ControlSessionManager` manages external clients connected to Axent through RPC, event, and stream sessions.

`DeviceSessionManager` manages adapter-to-device or adapter-to-runtime protocol sessions.

`CapabilityRegistry` registers fixed core capabilities and adapter-specific extension capabilities. Core capabilities include identity, connection, status, control, events, stream/flow-control, diagnostics, config, and firmware.

`AdapterRegistry` statically registers adapters in the first stage. It tracks adapter metadata, capability declarations, discovery entry points, and device session factories. Dynamic plugin loading is out of scope for the first stage.

`RouteManager` maps normalized control commands to the target adapter and device session according to method, capability, device selector, and session state.

`Broker` executes normalized commands through middleware and routing, then returns responses, events, stream frames, and task updates to the relevant control session.

`Middleware` handles logging, request/session id injection, error normalization, audit records, and flow-control hooks. Authentication and authorization are reserved for the production hardening stage.

`FlowControl` models stream and high-frequency event backpressure, throttling, pause/resume, and statistics. Mock tests must cover it before real stream integration is complete.

`Logger` provides core logs, audit logs, and adapter logs. Diagnostics gathers evidence through Logger and core managers.

## 6. Control Plane

The new control plane is server-first. `axentd` listens for WebSocket clients and accepts connections from external governance platforms, debugging clients, and future UI clients.

The first-stage dev/trial profile defaults to LAN-accessible WebSocket binding with no authentication. This is explicitly not a production security posture. Audit logging is required from the first stage for connections, requests, dangerous operations, adapter calls, and firmware tasks. Token, pairing, authorization, and mTLS are reserved for later hardening.

The same WebSocket server supports two protocol entries:

1. AXTP/JSON-RPC style messages for new clients.
2. Legacy `op/d/sid` messages compatible with the old agent protocol.

Legacy methods to map include `GetDeviceList`, `GetDeviceInfo`, `StartDeviceUpgrade`, `StartAgentUpgrade`, `StartDeviceReboot`, `StartAgentReboot`, `SetDeviceMute`, `GetDeviceMute`, `SetDeviceVolume`, and `GetDeviceVolume`.

Both protocols are normalized before entering core:

```text
wire message -> protocol codec -> ControlCommand -> Broker -> RouteManager -> Adapter -> protocol codec -> wire response
```

`ControlCommand` contains request id, control session id, method, protocol source, device selector, params, task metadata, and stream metadata. Core components and adapters do not branch on JSON-RPC versus legacy `op/d/sid`.

## 7. Adapter Strategy

The first stage is mock and contract first. `libaxent` defines the adapter contract before real hardware integration is required. The common adapter contract covers:

- discovery
- capability declaration
- device session creation and close
- RPC call
- event subscription
- stream and flow-control
- config read/write
- diagnostics collection
- firmware update entry point

`MockAdapter` must exercise the complete governance flow without hardware.

`AXDP Adapter` uses source-level integration through `third_party/axdp`. Legacy `agent/` can be consulted only for behavior and business logic, such as enumeration flow, device information fields, control flows, and upgrade flow. The new implementation must not copy the legacy architecture, global state, Windows message loop, or code style.

`AXTP Adapter` uses runtime integration through `third_party/axtp-cpp-runtime`, added as a git submodule. It is a real runtime adapter that creates AXTP device sessions and connects RPC, events, streams, and flow-control into `DeviceSessionManager`.

`TEA Adapter` wraps the TEA third-party SDK under `third_party/tea/...`. It exposes TEA-specific operations as capabilities and device sessions, without leaking TEA SDK semantics into the core.

All three adapters reserve the same firmware update interface. The core firmware task model owns task state, progress, error, and recovery metadata. Each adapter executes the actual device-specific update when its real implementation is available.

## 8. Firmware, Config, Diagnostics, And Logs

Firmware update is a first-class task model. It supports queued, validating, preparing, transferring, flashing, rebooting, verifying, succeeded, failed, and recoverable states. Mock firmware tasks must be testable before real adapter update support is complete.

Config is represented through `axent.toml` for daemon configuration and through device config capabilities for per-device operations. Adapter-specific config must pass through the capability and route model.

Diagnostics must collect a default-sanitized evidence package. The package includes device snapshots, sessions, capabilities, adapter status, route state, recent control requests, firmware task history, flow-control statistics, core logs, audit logs, adapter logs, platform info, and config summary.

Logs are split into:

- core log
- audit log
- adapter log

Audit log is required in the first stage because the dev/trial profile exposes LAN WebSocket without authentication.

## 9. Service And Lifecycle

`axentd` supports foreground development mode and system service/daemon mode.

Windows first-stage acceptance includes a Windows Service entry, install/uninstall/start/stop support or scripts, and log path conventions.

macOS first-stage acceptance includes a launchd plist, start/stop support or scripts, and log path conventions.

`axent` local management commands can connect to the running daemon when needed. Required first-stage commands include `status`, `list`, `reload`, and `diagnostics`.

The daemon owns core state. Local management commands and WebSocket clients must not create competing core instances for normal operation.

## 10. First-Stage Acceptance

Repository acceptance:

- New root C++ project exists.
- `agent/` remains complete as legacy.
- Shared dependencies are organized under `third_party/`.
- `third_party/axtp-cpp-runtime` is a submodule.
- `.superpowers/` is ignored.

Core acceptance:

- Unit tests cover `DeviceManager`, session managers, registries, route manager, broker, middleware, flow-control, logger, diagnostics, and firmware task model.
- Mock adapter completes discovery, capability, RPC, event, stream, config, diagnostics, and firmware task flows.

Control-plane acceptance:

- `axentd` exposes a WebSocket server.
- The dev/trial default is LAN accessible with no authentication.
- AXTP/JSON-RPC and legacy `op/d/sid` messages both route through the same `ControlCommand` path.
- Responses are encoded back to the caller's protocol.
- Audit logs capture connections, requests, dangerous operations, and adapter calls.

Adapter acceptance:

- AXDP, AXTP, and TEA adapter interfaces and static registrations exist.
- AXTP runtime submodule can be found by CMake and linked to the AXTP adapter skeleton.
- AXDP and TEA SDK/header discovery is represented in CMake.
- Real adapter functionality can be unavailable in the first stage only if the capability metadata marks it as unavailable or mock-only.

Service acceptance:

- `axentd` can run in foreground.
- Windows Service and macOS launchd assets are present and smoke-testable where platform support is available.
- `axent status`, `axent list`, `axent reload`, and `axent diagnostics` have defined behavior.

Legacy acceptance:

- New code does not depend on legacy global state, Windows message loop, or protocol parser.
- Legacy method mapping has tests for representative old methods.
- The old `agent/` remains available as a parallel implementation.

## 11. Out Of Scope For First Stage

- Replacing or deleting legacy `agent/`.
- Dynamic adapter plugin loading.
- Production authentication and authorization.
- Cloud account management.
- Full UI or desktop app.
- Complete real-device support for every AXDP, AXTP, and TEA capability.
- Copying legacy code style or global-state architecture into the new core.

## 12. Design Decisions Confirmed

- Keep `agent/` as a side-by-side legacy implementation.
- Build the new mainline under root `src/`, `include/`, and `tests/`.
- Move root AXDP, AXTP, and TEA SDK assets into `third_party/` for the new project.
- Add AXTP C++ runtime as a third-party submodule.
- Prioritize cross-platform core before real adapter completeness.
- Use WebSocket server mode for the new control plane.
- Default dev/trial profile is LAN accessible and unauthenticated, with audit logging.
- Model sessions according to AXTP concepts: external control sessions and adapter/device sessions.
- Use mock and adapter contracts first.
- Implement a full governance loop, not only RPC.
- Use unified firmware task model across all adapters.
- Support dual protocol entry: AXTP/JSON-RPC and legacy `op/d/sid`.
- Include Windows Service and macOS launchd in first-stage acceptance.
- Use `axentd` for daemon and `axent` for local management CLI.
