# Axent Real Adapter And Daemon Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first NA20-style real AXTP HID adapter path and make `axentd` able to host it as the default long-running daemon owner.

**Architecture:** `libaxent` keeps the shared host and adapter implementation. `AxtpAdapter` owns HID/AXTP discovery, selector mapping, and diagnostics, while `AxentHost` decides which adapters to register. `axentd` only parses daemon options, starts `AxentHost`, and exposes the existing WebSocket control plane.

**Tech Stack:** C++17, CMake, existing AXTP C++ runtime HID transport adapter, hidapi through Axent-owned dependency wiring, nlohmann/json.

---

## Tasks

- [x] Add NA20/HID adapter mapping and diagnostics tests.
- [x] Implement `AxtpAdapterConfig`, diagnostics mapping, HID discovery, and unavailable session behavior.
- [x] Add host options for real AXTP adapter registration.
- [x] Add daemon CLI flags for real adapter selection and HID options.
- [x] Verify full build, CTest, and daemon smoke.
