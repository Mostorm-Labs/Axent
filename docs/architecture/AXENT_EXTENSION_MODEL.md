# Axent Extension Model

Axent extensions are the approved way to add product differences, protocol
differences, external control ingress, and stream transports without modifying
Axent Core.

## Extension Types

### Device Adapter

Device Adapters connect device protocols to Axent's abstract device model.

Examples:

- AXTP device adapter
- AXDP device adapter
- TEA device adapter
- mock device adapter

Adapters own protocol mapping and protocol SDK usage. They must not add product
workflow policy to Core.

### Product Extension

Product Extensions register product-level methods and events with a host or
control surface. They bridge to the product's own command bus instead of
bypassing product state machines.

Examples:

- `nearcast.startCast`
- `nearcast.stopCast`
- `nearcast.status`

These methods belong in the product host or product extension, not in Axent
Core.

### Control Endpoint

Control Endpoints expose external ingress.

Examples:

- WebSocket endpoint
- local IPC endpoint
- CLI endpoint
- product remote-control endpoint

Endpoints normalize wire messages and then call the broker, product command
bus, or client boundary appropriate for their host role.

### Stream Provider

Stream Providers implement high-bandwidth stream transport.

Examples:

- shared memory
- local socket
- direct media
- in-process test stream

Normal JSON-RPC control paths must not be used as high-bandwidth media pipes.

### Capability Provider

Capability Providers declare additional capabilities, methods, events, and
requirements. Capability metadata is how hosts perform startup checks,
permission checks, conflict checks, and diagnostics.

## Manifest Shape

Every extension should have enough metadata for runtime validation:

```json
{
  "id": "axent.adapter.axtp",
  "type": "device-adapter",
  "version": "1.0.0",
  "apiVersion": "axent.extension.v1",
  "capabilities": ["device", "display", "audio", "stream", "system"],
  "requires": ["axent.core", "axent.stream"]
}
```

The first version may encode this manifest as static C++ metadata. A JSON file
or dynamic plugin descriptor can come later if deployment requires it.

## Version Policy

Extension APIs are public contracts. Changing them requires compatibility
review:

- additive fields and optional methods are preferred
- breaking changes require an API version bump
- product-only behavior must stay outside Core
- adapter-only behavior must stay in the adapter
