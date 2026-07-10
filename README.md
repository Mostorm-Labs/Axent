# Axent

Axent is a device-control runtime core with a stable extension and host model.
It exposes AXTP-oriented control surfaces, manages device sessions, and routes
device commands through AXDP, AXTP, TEA, or mock adapters without accepting
product-specific workflow logic into Core.

Architecture guardrails:

- [Axent Roadmap](docs/ROADMAP.md)
- [Axent Architecture](docs/architecture/AXENT_ARCHITECTURE.md)
- [Axent Host Model](docs/architecture/AXENT_HOST_MODEL.md)
- [Axent Extension Model](docs/architecture/AXENT_EXTENSION_MODEL.md)
- [Codex Guardrails](docs/dev/CODEX_GUARDRAILS.md)

## Axent Library And Daemon Model

`libaxent` is the current library target for the first-stage implementation. It
contains the core runtime and the present static adapter/host pieces while the
project moves toward narrower `core`, `extension`, `client`, `adapter`, and
`host` targets.

`axentd` is the default daemon host for `libaxent`. It is responsible for
owning physical devices on a PC so multiple frontend products do not compete
for the same HID/AXTP transport.

Products such as Nearcast should normally use the Axent client and extension
boundaries. Controlled embedded-host scenarios may link `libaxent`, but product
logic, rendering policy, and product state machines must remain outside Axent
Core.

Axent relays encoded media frames and metadata. It does not own Nearcast
rendering policy. Nearcast keeps `MediaCore`, H.264/AAC assembly, render clock,
late drop, catch-up, D3D11, WASAPI, overlay, and UI behavior.

## New Axent Mainline

The legacy implementation remains under `agent/` and is not used as the
architecture base for the new core.

New targets:

- `libaxent`: first-stage library target
- `axentd`: daemon and WebSocket control plane
- `axent`: local management CLI and future host launcher

Target host command model:

```bash
axent daemon
axentd
axent run nearcast
axent up --with daemon,nearcast
axent extension list
axent host list
axent doctor
```

Do not add ambiguous mixed-role commands such as `axent --daemon --nearcast`.

Developer smoke:

```bash
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
build/axent status --offline
build/axentd --foreground
```

## Logging And CLI

`axent` and `axentd` share Axent-native logging options:

```bash
build/axent --version
build/axent status --offline --log --debug
build/axentd --foreground --bind 127.0.0.1 --port 6060 --log --log-level debug
```

`--log` enables file logging, `--debug` maps to `--log-level debug`, and
`--log-dir` selects the log directory. The logger supports level filtering,
structured in-memory diagnostics, checkpoints, bounded memory records, and
rotating file segments without adding a third-party logging dependency.

## Real AXTP Adapter And Daemon Mode

`axentd` can host the first real AXTP HID adapter path with NA20-style defaults:

```bash
build/axentd --foreground --axtp-real --bind 127.0.0.1 --port 6060
```

Default NA20 HID selector values are VID `0x0581`, PID `0x2581`, usage page
`0x0081`, report id `0x05`, and automatic input/output report sizes. Override
them with `--hid-vid`, `--hid-pid`, `--hid-usage-page`, `--hid-usage`,
`--hid-report-id`, `--hid-input-report-size`, `--hid-output-report-size`,
`--hid-path`, and `--hid-serial`.

When no matching HID device is present, the real adapter simply discovers zero
devices and daemon startup continues. When a discovered AXTP device is selected,
the adapter lazily opens the HID transport, runs the AXTP app-ready handshake,
and forwards JSON control calls through the runtime SDK. This lets `axentd`
remain the single resource owner on a PC without making development machines
require hardware. Nearcast can either connect to this daemon through the Axent
client boundary or use a controlled embedded-host path; it must keep
`MediaCore` and renderer policy outside Axent.

The development profile binds WebSocket to LAN by default and has no
authentication. Use only on trusted networks until the hardening stage adds
authentication and authorization.
