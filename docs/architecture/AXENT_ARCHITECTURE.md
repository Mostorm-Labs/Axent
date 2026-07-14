# Axent Architecture

Axent is a device-control runtime core with a stable extension model. It is not
just a common library for products to patch directly. Axent provides stable
runtime primitives, extension contracts, and host integration points so product
differences can live outside the core.

## Architecture Layers

```text
Axent Hosts
  axentd, product hosts

Axent Tooling and Client Surfaces
  axtpctl, axent axtp compatibility alias, diagnostic tools, client APIs

Axent Extension Layer
  Device Adapters, Product Extensions, Control Endpoints,
  Stream Providers, Capability Providers

Axent Core
  Runtime, sessions, leases, routing, broker, capability registry,
  event bus, stream plane, tracing, errors, permissions

Protocol and transport runtimes
  axtp-cpp-runtime, AXDP SDK, TEA SDK, HID/WebSocket/TCP transports
```

The dependency direction must flow downward through stable contracts. Axent Core
must not know about product hosts, product workflows, UI policy, renderers, or
product-specific command names.

## Tooling Boundary

Axent owns the canonical `axtpctl` executable and the shared
`axent::axtp_tooling` command runner. `axtpctl` is the primary command; `axent
axtp` is a compatibility alias backed by exactly the same runner.

The tooling layer may depend on protocol APIs and concrete transports, but it
must not depend on `libaxent`, Axent Core internals, product hosts, or the
retired cpp-runtime `axtp_toolkit`. Axent Core must not include tooling headers.

The current cpp-runtime checkout still contains transitional concrete
transport targets and media/profile code, so it is not yet a protocol-only
library. Firmware file handling, maintenance leases, progress, and the
deterministic AXTP update transaction are now Axent-owned; Axent no longer
consumes the runtime firmware profile. The remaining ownership migrations are
separate work. The retired cpp-runtime `axtpctl`, `axtp_toolkit`, and
mediahost demo are not build or dependency sources for Axent.

## Core Responsibilities

Axent Core owns generic runtime behavior:

- runtime lifecycle
- device and control sessions
- transport and control leases
- device routing
- control broker and device broker
- capability registry
- adapter and extension registries
- event dispatch
- stream plane coordination
- trace context
- error mapping
- permission policy

Core code only handles abstract objects such as device commands, device results,
device events, capabilities, sessions, stream requests, stream handles, and
control leases.

## Non-Core Responsibilities

Device protocol differences belong in Device Adapters:

- AXTP adapter
- AXDP adapter
- TEA adapter
- mock or test adapters

External control ingress belongs in Control Endpoints:

- WebSocket endpoint
- local IPC endpoint
- CLI endpoint
- product-specific remote-control endpoint

High-bandwidth media or data paths belong in Stream Providers:

- shared memory
- local socket
- direct media
- in-process test transport

Product workflows belong in Product Hosts or Product Extensions. For example,
Nearcast start/stop/status behavior must live in Nearcast code and enter its
own command bus before it maps to Axent device operations.

## Product Boundary

Axent Core must not contain product-specific names or behavior. Product names
are allowed in architecture documents, integration examples, tests written for a
specific product boundary, and external product repositories. They are not
allowed in `include/axent/core` or `src/core`.

Forbidden examples for Core:

- Nearcast
- Launcher
- Signage
- CastSession
- Preview UI
- Renderer

The rule is simple:

```text
Product differences go through extensions or hosts.
Shared runtime behavior may enter Core only after it is product-neutral.
```

## First-Version Plugin Policy

Axent v1 uses compile-time/static extensions and explicit registries. Dynamic
C++ plugins are intentionally out of scope because they introduce ABI,
allocator, exception, RTTI, compiler-version, and dependency-conflict risks.

If dynamic plugins are needed later, the boundary must use a small C ABI entry
point plus a manifest, not unstable C++ class ABI across shared libraries.
