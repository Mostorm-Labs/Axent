# Axent Real Adapter And Daemon Mode Design

## Scope

This phase adds the first real AXTP adapter path to Axent and makes `axentd`
the default long-running owner of device access. The implementation follows the
NA20 HID/AXTP behavior proven in NearCast, but keeps NearCast `MediaCore`,
render clocks, decoder reset, and late-frame policy outside Axent.

## Real Adapter

`AxtpAdapter` gains an explicit configuration object centered on a HID
`TransportSelector`. The default device profile is NA20:

- VID `0x0581`
- PID `0x2581`
- usage page `0x0081`
- report id `0x05`
- input and output report sizes default to `0`, meaning auto-negotiate

The adapter enumerates HID candidates through `axtp::enumerateHidDevices`,
filters by path, serial number, usage page, and usage, and converts matches into
Axent `DeviceSnapshot` records. It also maps NearCast-style trace events into
`TransportDiagnostics` counters: read timeouts, read/write errors, dropped
report id, queued reports, negotiated report sizes, and preferred frame size.

When hardware is absent, the adapter remains available but reports zero devices
instead of failing host startup. When a matching device is routed, the adapter
opens the HID transport lazily, runs the AXTP app-ready handshake, and forwards
JSON control calls through the runtime SDK. If the transport target or device is
not available, calls return `Unavailable` without failing daemon startup.

## Daemon Mode

`AxentHostOptions` controls whether mock and real AXTP adapters are registered.
`axentd` exposes CLI flags for this so service deployments can run either:

- mock-only development mode
- real AXTP HID mode
- mixed mode for diagnostics

`axentd` owns the host and WebSocket control plane for the process lifetime.
That process is the resource arbitrator; NearCast and other UI clients should
connect to it or embed `libaxent`, not open the same HID device concurrently in
daemon mode.

## Tests

The implementation is testable without hardware by checking deterministic
mapping and diagnostics functions, host registration behavior, daemon CLI
parsing, and a scripted AXTP transport that proves app-ready plus a business
control call. Hardware certification and media streaming are intentionally left
behind later integration tests.
