# Axent
Axent is an AX endpoint agent that exposes an AXTP-based WebSocket control plane and routes device commands to AXDP, AXTP, and TEA adapters.Axent 是一个 AX 设备端 Agent，对外提供基于 AXTP 的 WebSocket 控制平面，对内统一管理设备 Session，并将设备控制请求路由到 AXDP、AXTP 和 TEA 适配器。

## Axent Library And Daemon Model

`libaxent` is the core implementation. It owns device discovery, concrete
transport management, AXTP device sessions, routing, diagnostics, and encoded
media relay.

`axentd` is the default daemon host for `libaxent`. It is responsible for
owning physical devices on a PC so multiple frontend products do not compete
for the same HID/AXTP transport.

Products such as NearCast can also link `libaxent` directly as an embedded
host. Embedded mode and daemon mode use the same logical Axent session and
media-source contract.

Axent relays encoded media frames and metadata. It does not own NearCast
rendering policy. NearCast keeps `MediaCore`, H.264/AAC assembly, render clock,
late drop, catch-up, D3D11, WASAPI, overlay, and UI behavior.

## New Axent Mainline

The legacy implementation remains under `agent/` and is not used as the architecture base for the new core.

New targets:

- `libaxent`: core library
- `axentd`: daemon and WebSocket control plane
- `axent`: local management CLI

Developer smoke:

    git submodule update --init --recursive
    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build
    ctest --test-dir build --output-on-failure
    build/axent status --offline
    build/axentd --foreground

The development profile binds WebSocket to LAN by default and has no authentication. Use only on trusted networks until the hardening stage adds authentication and authorization.
