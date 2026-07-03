# Axent NearCast-Style Foundation Design

## Goal

Bring Axent's base observability and command-line capabilities closer to
NearCast while keeping Axent native, portable, and suitable as both a library
and daemon foundation.

This work reuses NearCast's capability model, not its Windows-specific logging
implementation. `libaxent`, `axentd`, and `axent` should share one logging and
argument-parsing foundation.

## Scope

In scope:

- Cross-platform `libaxent` logging with levels, categories, memory records,
  optional console/file sinks, checkpoints, flush, file rotation, retention,
  and dropped-line diagnostics.
- Reusable command-line parsing for `axent` and `axentd`.
- `axent` global options for version, logging, debug level, JSON output, and
  existing management commands.
- `axentd` global options for foreground mode, bind host, port, logging, debug
  level, and mock-adapter selection.
- Early daemon logging and checkpoints around host/server lifecycle.
- Tests for logging behavior, parser behavior, and existing CLI smoke behavior.

Out of scope:

- NearCast MediaCore, rendering, overlay, window, and product-specific media
  options.
- Windows-only crash dump, `OutputDebugStringW`, and unhandled exception hooks.
- New third-party dependencies such as spdlog or CLI11.
- Changes to legacy `agent/`.
- HID transport behavior changes. HID/report-size reuse should be handled in a
  separate transport integration task.

## Architecture

`libaxent` owns the shared foundation:

```text
libaxent
  logging/
    Logger
    LogConfig
    LogLevel
    LogCategory
    sink implementations
  cli/
    CommonOptions
    AxentCliOptions
    AxentdOptions
    parse helpers

axentd
  parse AxentdOptions
  initialize Logger early
  start AxentHost and WebSocketServer
  write checkpoints and lifecycle logs

axent
  parse AxentCliOptions
  initialize Logger when requested
  execute status/list/reload/diagnostics command scaffold
```

The implementation should stay C++17 and use standard library facilities:
`std::filesystem`, `std::ofstream`, `std::mutex`, and standard containers.

## Logging Model

`LogLevel` values:

- `error`
- `warn`
- `info`
- `debug`
- `trace`

`LogCategory` values:

- `general`
- `daemon`
- `control`
- `transport`
- `adapter`
- `media`
- `diagnostics`
- `audit`

`Logger` keeps the current structured record behavior and expands it. Existing
callers using `core()`, `audit()`, and `adapter()` continue to work.

Public behavior:

- `write(level, category, message, fields)`
- `core(message, fields)`
- `audit(message, fields)`
- `adapter(message, fields)`
- `checkpoint(message, fields)`
- `enabled(level, category)`
- `flush()`
- `records()`
- `dropped_count()`
- `file_path()`

`LogConfig` controls:

- minimum level, default `info`
- console sink enabled/disabled
- file sink enabled/disabled
- log directory
- log file prefix
- queue or memory limit
- max segment bytes, default aligned with NearCast at 10 MiB
- retained segments, default aligned with NearCast at 5

The first implementation can write synchronously under a mutex. It must still
enforce bounded memory records and file rotation. If a bounded queue is added,
warn/error lines must be treated as mandatory and lower-priority lines should be
dropped first.

Log lines should be stable and human-readable:

```text
2026-07-03T12:34:56.789Z [INFO] [daemon] axentd listening bind=127.0.0.1 port=8765
```

Structured fields remain available in memory records for diagnostics and tests.
File output may render fields as compact key-value text or compact JSON.

## CLI Model

Create a reusable parser module instead of keeping parsing logic in executable
entrypoints. It should return typed option structs and error/help text without
terminating the process, so tests can cover parsing directly.

Common options:

- `--help`, `-h`
- `--version`
- `--log`
- `--log-dir <path>`
- `--log-file <name-or-prefix>`
- `--log-level error|warn|info|debug|trace`
- `--debug`, equivalent to `--log-level debug`
- `--json`

`axent` commands:

- `status [--offline]`
- `list`
- `reload`
- `diagnostics`

`axentd` options:

- `--foreground`
- `--bind <host>`
- `--port <number>`
- `--log`
- `--log-dir <path>`
- `--log-level <level>`
- `--debug`
- `--no-mock-adapter`

The parser should reject unknown options and missing option values with clear
messages. Existing smoke behavior for `axent status --offline` must remain.

## Daemon Integration

`axentd` initializes logging before constructing `AxentHost`.

Startup logs:

- product name and version
- foreground vs service scaffold mode
- bind host and effective port
- mock adapter enabled/disabled
- config source when available

Checkpoints:

- process start
- options parsed
- host starting
- host started
- WebSocket server starting
- WebSocket server listening
- shutdown requested
- server stopped
- host stopped

Errors that currently go only to `std::cerr` should also go through the logger.
Foreground mode may keep console output for developer ergonomics.

## Diagnostics

Diagnostics should continue to collect audit records. With the enhanced logger,
diagnostics may also expose:

- current minimum level
- active log file path, if any
- dropped log count
- recent audit records
- recent core/adapter records when sensitive output is allowed

Sensitive field redaction must continue to apply.

## Testing

Add focused tests:

- level filtering hides debug logs at `info`
- `--debug` maps to debug level
- checkpoint writes and flushes
- file rotation creates retained segments
- bounded memory records expose dropped count
- parser handles version/help/log-level/log-dir/bind/port/foreground
- parser rejects unknown options and missing values
- `axent status --offline` smoke remains green

Tests should use temporary log directories and avoid relying on wall-clock exact
timestamps.

## Branching And Integration

Implement on a new `codex/` branch from the current Axent core branch. Keep the
legacy `agent/` directory untouched and unstaged.

The implementation should not create a pull request until the user explicitly
asks for push and PR creation.

## Acceptance Criteria

- `libaxent` provides a reusable logging foundation with NearCast-level core
  capabilities in a cross-platform form.
- `axentd` and `axent` share parser and logging setup code.
- Existing Axent tests still pass.
- New logging and parser tests pass.
- No new third-party dependency is added.
- No implementation or staging touches legacy `agent/`.
