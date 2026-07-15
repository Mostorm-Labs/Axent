# Axent Protocol-Bus Layer Boundaries

Read this document before changing Axent code. The dependency direction is:

```text
NearCast product host -> Axent protocol bus -> axtp-cpp-runtime
```

## Axent owns

- JSON-free public device, control, media and firmware contracts.
- Device discovery, session/maintenance leases and fail-fast ownership gates.
- AXTP/AXDP/TEA adapters and concrete TCP, WebSocket and HID providers.
- Control endpoints, firmware services and the canonical `axtpctl` tooling.

## Axent does not own

- NearCast `cast.*` product handlers, casting arbitration, privacy, retry,
  AirPlay integration, overlay, decoding or rendering.
- AXTP wire facts, generated IDs, frame codecs, RPC/session mechanics or the
  abstract `ITransport`; those remain in cpp-runtime.

Runtime types are private adapter details. Public Axent headers must not include
cpp-runtime headers or expose `axtp::*`. Core must not contain a product name or
product workflow. Concrete platform dependencies belong to provider targets and
must not leak through public contracts.

## Placement rule

Put reusable device-facing capability in an Axent contract, adapter, provider
or service. Put protocol mechanics in cpp-runtime. Put product policy and UI in
the product host. `axtpctl` may consume Axent tooling and providers but must not
link product hosts or make Axent Core depend on tooling.
