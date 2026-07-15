# AGENTS.md

Axent is the extensible protocol bus between product hosts and protocol
runtimes. Read `docs/architecture/LAYER_BOUNDARIES.md` and
`docs/dev/CODEX_GUARDRAILS.md` before changing code.

## Dependency direction

```text
NearCast product host -> Axent protocol bus -> axtp-cpp-runtime
```

- Axent owns public device/control/media/firmware contracts, sessions and
  leases, adapters, concrete transport providers, and the canonical `axtpctl`.
- Axent public headers must not expose `axtp::*` or cpp-runtime headers.
- Product routes, UI, rendering, casting arbitration and privacy policy stay in
  product hosts such as NearCast.
- cpp-runtime owns protocol mechanics and must not depend on Axent.

Classify a change before editing: generic Axent contract/core, protocol adapter,
transport provider, control endpoint, tooling, or product-host behavior. Do not
put product behavior into Core to avoid defining a narrow boundary.

## Validation

Run the dependency-boundary test, the affected unit tests, `git diff --check`,
and a clean recursive checkout/build before merging changes that alter public
contracts or submodule pins.
