# Axent Host Model

Axent supports one command family with multiple explicit host modes. A host mode
is a runtime role, not a pair of unrelated boolean flags.

## Recommended Commands

```bash
# Start the device-control daemon host.
axent daemon

# Service-friendly daemon entry point.
axentd

# Start a product host through the Axent launcher model.
axent run nearcast

# Start multiple hosts for development without merging their process roles.
axent up --with daemon,nearcast

# Inspect runtime composition.
axent extension list
axent host list
axent doctor
```

Do not add a mixed role such as:

```bash
axent --daemon --nearcast
```

That shape makes it unclear whether the process is a device daemon, a product
host, or a supervisor. It also invites product responsibilities to leak back
into Axent Core.

## Device Host

`axent daemon` and `axentd` represent the Axent Device Host.

Responsibilities:

- start Axent Core
- load device adapters
- load control endpoints
- load stream providers
- own physical device transports
- manage device sessions and leases
- expose device-level control APIs
- fan out device events

`axentd` may depend on Axent Core, extension interfaces, adapters, stream
providers, and control endpoints. It must not depend on product hosts such as
Nearcast, Launcher, or Signage.

## Product Host

`axent run nearcast` represents a product host launched through the Axent host
model. Nearcast remains a product application; it is not a plugin loaded into
`axentd`.

Responsibilities:

- start product state machines and command bus
- expose product-level control APIs
- connect to `axentd` through the Axent client boundary
- map product commands to device operations
- maintain UI, renderer, media, and product policy outside Axent Core

Product hosts may depend on Axent client APIs and extension interfaces. They
must not include Core internals or patch Core to express product workflows.

## Dev Supervisor

`axent up --with daemon,nearcast` is a development and integration-test
supervisor.

Responsibilities:

- start requested hosts as separate roles
- wait for health checks
- print service endpoints
- stop children in reverse dependency order

The supervisor must not merge host responsibilities into one Core instance
unless a test explicitly asks for an in-process fixture. Even then, test-only
fixtures must keep product behavior outside Core.
