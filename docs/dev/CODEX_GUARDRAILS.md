# Codex Guardrails For Axent

Use this file as the first checklist before changing Axent. The goal is to keep
Axent Core stable while allowing products, tools, and protocol integrations to
evolve through explicit boundaries.

## Hard Rules

1. Axent Core must not contain product-specific names or workflow logic.
2. Product differences must be implemented through Product Extensions or inside
   product hosts.
3. Device protocol differences must be implemented as Device Adapters.
4. Stream transport differences must be implemented as Stream Providers.
5. External control ingress must be implemented as Control Endpoints.
6. `axentd` is a Device Host built on Axent Core. It must not depend on
   product hosts.
7. Nearcast is an Axent-powered Product Host. It may depend on Axent client APIs
   and extension interfaces, but not on Axent Core internals.
8. `axtp-cpp-runtime` remains the AXTP protocol runtime. It must not depend on
   Axent or product code.
9. High-bandwidth stream data must not be tunneled through normal JSON-RPC
   control messages.
10. If a feature appears to require a Core change, classify it before editing:
    generic runtime primitive, device adapter feature, stream provider feature,
    control endpoint feature, product extension, or host-specific behavior.
11. Axent owns the canonical `axtpctl` and `axent::axtp_tooling`; cpp-runtime
    tools and the mediahost demo must remain disabled.
12. AXTP tooling must not link `libaxent` or include Axent Core internals.

## Forbidden In Core

Do not add these names or concepts to `include/axent/core` or `src/core`:

- Nearcast
- Launcher
- Signage
- CastSession
- Preview UI
- Renderer

Do not include upper-layer module headers from Core:

- `axent/adapters/...`
- `axent/cli/...`
- `axent/control/...`
- `axent/daemon/...`
- `axent/host/...`
- `axent/tooling/...`

## Change Classification

| Change type | Destination |
| --- | --- |
| DeviceSession, ControlSession, lease, route, broker | Axent Core |
| Capability metadata and generic permission checks | Axent Core |
| AXTP frame, payload, session, method-id behavior | axtp-cpp-runtime |
| AXTP, AXDP, or TEA device mapping | Device Adapter |
| Shared memory or local socket media path | Stream Provider |
| WebSocket or local IPC server | Control Endpoint |
| `nearcast.startCast`, `nearcast.stopCast`, product status | Product Extension or Product Host |
| UI, renderer, decoder, product state machine | Product Host |
| One-command developer startup | `axent up` supervisor |
| AXTP control, inspection, and diagnostics | `axent::axtp_tooling` / `axtpctl` |

## AXTP Tooling Guardrails

`axtpctl` is the primary CLI. `axent axtp` is a compatibility alias and must
delegate to the same shared runner. Do not fork their parsers, command
implementations, output formats, or exit-code behavior.

```text
axtpctl -----\
              -> axent::axtp_tooling -> AXTP protocol/profile APIs + transports
axent axtp --/

Axent Core must not depend on axent::axtp_tooling.
```

Do not enable or link the retired cpp-runtime `axtpctl`, `axtp_toolkit`, or
mediahost demo. Concrete transport targets and profile coordinators are current
transition dependencies and must be migrated in dedicated boundary changes.

## Host Command Guardrails

Prefer explicit host commands:

```bash
axent daemon
axentd
axent run nearcast
axent up --with daemon,nearcast
```

Do not add ambiguous mixed-role flags:

```bash
axent --daemon --nearcast
```

If a one-command workflow is needed, implement it as a supervisor that starts
separate hosts. Do not turn Axent Core into a combined product runtime.
