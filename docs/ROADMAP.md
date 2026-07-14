# Axent 演进 Roadmap

这份 roadmap 用来约束 Axent、axentd、Nearcast 三者的长期演进顺序。核心原则是：

```text
先把 Axent 做成干净、可测试、可扩展的设备控制核心；
再让 axentd 成为 Axent 的设备服务宿主；
再让 Nearcast 通过 libaxent-client 接入 Axent；
最后再考虑组合宿主、统一入口、多产品扩展。
```

## 总体定位

```text
Axent 是设备控制运行时核心。
axentd 是基于 Axent 的设备控制 daemon。
Nearcast 是基于 Axent 的产品应用 / 投屏产品 Host。
```

目标结构：

```text
axtp-cpp-runtime
  ↓ 协议原语与当前过渡 profile
Axent
├── Axent Core
├── axent::axtp_tooling
├── axtpctl（主要 AXTP CLI）
├── axent axtp（兼容入口）
├── axentd Device Host
└── Nearcast Product Host
```

最终目标不是让每个项目依赖后随意修改 Axent，而是让 Axent 形成稳定平台：

```text
Axent Core 稳定；
差异通过 Extension / Host 消化；
产品通过 libaxent-client 使用 Axent；
axentd 通过 Axent Core 提供设备服务；
Nearcast 通过 Axent 控制设备，同时保留自己的产品状态机。
```

## Roadmap 总览

```text
Phase 0：架构冻结与文档约束
Phase 1：Axent 独立模块骨架
Phase 2：Axent Core 基础能力
Phase 3：Extension Model 与 Adapter 抽象
Phase 4：Fake Adapter 与测试闭环
Phase 5：AXTP / AXDP / TEA Adapter 接入
Phase 6：libaxent-client 与 BackendSelector
Phase 7：axentd Device Host
Phase 8：Nearcast 接入 Axent
Phase 9：StreamPlane 与低延迟拉流
Phase 10：Composite Host 可选模式
Phase 11：稳定化、CI、文档、发布
```

近期边界迁移按以下顺序独立实施，避免一次改动同时扩大多个回归面：

```text
1. Axent 持有 canonical axtpctl 与共享 AXTP tooling。
2. TCP concrete transport 已迁入 Axent；WebSocket / HID 按顺序继续迁移。
3. Media / Firmware profile 的 Host 流程编排迁入 Axent。
4. axtp-cpp-runtime 最终只保留协议、RPC、STREAM 与生成代码原语。
```

当前 cpp-runtime 中的 `axtpctl`、`axtp_toolkit` 与 mediahost demo 不再作为
Axent 的构建或依赖来源；现有 media / firmware profile 和 transport target
仍属于明确记录的过渡依赖。

## Phase 0：架构冻结与文档约束

目标：在继续写代码前，先把 Axent 的边界写清楚，避免后续为了 Nearcast、axentd 或其它产品的短期需求污染 Axent Core。

已建立或需要维护的文档：

```text
docs/architecture/AXENT_ARCHITECTURE.md
docs/architecture/AXENT_HOST_MODEL.md
docs/architecture/AXENT_EXTENSION_MODEL.md
docs/dev/CODEX_GUARDRAILS.md
docs/ROADMAP.md
```

硬规则：

```text
1. Axent Core 不允许出现 Nearcast / Launcher / Signage 等产品名。
2. Axent Core 只提供通用设备控制运行时能力。
3. Nearcast 不允许依赖 Axent Core 内部头文件。
4. Nearcast 只能依赖 libaxent-client、extension interface、control/common。
5. axentd 可以依赖 Axent Core。
6. axentd 不允许依赖 Nearcast。
7. axtp-cpp-runtime 不允许依赖 Axent 或 Nearcast。
8. 设备协议差异通过 DeviceAdapter 实现。
9. 产品差异通过 Product Host / Product Extension 实现。
10. 高带宽音视频流不能走普通 JSON-RPC 控制通道。
```

验收标准：

```text
- 文档中明确 Axent Core / Extension / Host / axentd / Nearcast 的边界。
- Codex 和研发后续任务能引用 guardrails。
- CMake 或目录结构开始体现依赖方向。
- 边界检查可以阻止 Core 引入产品名或上层模块 include。
```

## Phase 1：Axent 独立模块骨架

目标：把 Axent 从产品业务中独立出来，形成可单独构建、测试和发布的模块。

推荐结构：

```text
axent/
  include/axent/
    common/
    core/
    client/
    extension/
    stream/
    control/
    host/
    tooling/

  src/
    core/
    client/
    extension/
    stream/
    control/
    host/

  adapters/
    axtp/
    axdp/
    tea/

  daemon/
  cli/
  tests/
  docs/
```

CMake target 演进目标：

```text
axent_common
axent_extension
axent_core
axent_client
axent_control
axent_stream
axent_host

axent_adapter_axtp
axent_adapter_axdp
axent_adapter_tea

axentd
axent_cli
axent_axtp_tooling
axtpctl
```

当前仓库仍处于 first-stage `libaxent` 聚合 target 阶段。后续拆分 target 时，必须保持依赖方向清晰。

验收标准：

```text
- Axent 可以独立编译。
- 没有产品仓库依赖。
- 没有 axentd 专属逻辑进入 core。
- 公共头文件和内部头文件边界清晰。
```

## Phase 2：Axent Core 基础能力

目标：实现真正的 Axent 核心运行时，但先不绑定真实设备协议。

核心模块：

```text
AxentRuntime
DeviceManager
DeviceSessionManager
ControlSessionManager
BindingSessionManager
TransportManager
TransportLeaseManager
CapabilityRegistry
DeviceStateStore
DeviceRouter
ControlBroker
DeviceBroker
EventBus
StreamPlane
ErrorMapper
TraceContext
PermissionPolicy
```

`AxentRuntime` 职责：

```text
1. 管理 start / stop 生命周期。
2. 注册 adapter / endpoint / stream provider。
3. 管理 device / session / event / stream。
4. 提供本地 client facade。
5. 不关心 Nearcast 产品流程。
```

`TransportLeaseManager` 是关键模块，用来防止多个 host 或多个进程争抢同一个物理设备。第一版可以先做进程内 lease，第二版再补系统级锁：

```text
Windows: named mutex
Linux/macOS: lock file + flock
```

验收标准：

```text
- AxentRuntime 可以 start / stop。
- 可以注册 fake device。
- 可以创建 DeviceSession。
- 可以创建 ControlSession。
- 可以通过 EventBus 分发事件。
- 可以获取和释放 TransportLease。
```

## Phase 3：Extension Model 与 Adapter 抽象

目标：让 Core 稳定，所有差异通过 extension / adapter 接入。第一版只做静态注册，不做动态 C++ 插件。

Extension 类型：

```text
IDeviceAdapter
IProductExtension
IControlEndpoint
IStreamProvider
ICapabilityProvider
IAxentHostModule
```

边界规则：

```text
DeviceAdapter 负责设备协议差异。
ProductExtension 负责产品级方法和事件注册。
ControlEndpoint 负责外部控制入口。
StreamProvider 负责高带宽流通道。
CapabilityProvider 负责能力声明。
HostModule 负责 host 组合和生命周期。
```

验收标准：

```text
- 可以静态注册 extension。
- Core 不依赖具体 adapter。
- DeviceRouter 通过 AdapterRegistry 路由。
- Extension manifest / info 可查询。
- 动态插件保持 out of scope。
```

## Phase 4：Fake Adapter 与测试闭环

目标：先用 FakeAdapter 把 Axent Core 的控制链路跑通，再接真实设备协议。

FakeAdapter 能力：

```text
1. 模拟一个在线设备。
2. 支持 device.info.get。
3. 支持 display.setPower。
4. 支持 stream.openPreview。
5. 支持 device.online / device.offline / stream.lost 事件。
6. 支持模拟错误、超时和离线。
```

测试场景：

```text
1. device.list 返回 fake device。
2. attachDevice 创建 DeviceSession。
3. call("display.setPower") 能路由到 FakeAdapter。
4. 多个 ControlSession 订阅同一个设备事件。
5. observe / control / exclusive 权限生效。
6. TransportLease 被正确获取和释放。
7. stream.open 返回 StreamHandle。
8. 设备 offline 后 DeviceSession 状态更新。
```

验收标准：

```text
- Axent Core 不依赖真实设备也能完整跑通。
- 单元测试覆盖 device/session/event/call/stream/lease。
- 真实 adapter 接入前，核心行为已经稳定。
```

## Phase 5：AXTP / AXDP / TEA Adapter 接入

目标：Core 稳定后，再接入真实协议。

优先级：

```text
1. AxtpAdapter
2. AxdpAdapter
3. TeaAdapter
```

AxtpAdapter 职责：

```text
- 基于 axtp-cpp-runtime。
- 把 Axent DeviceCommand 转成 AXTP RPC。
- 把 AXTP event 转成 Axent DeviceEvent。
- 把 AXTP stream 转成 Axent StreamPlane 输入。
```

AxdpAdapter 职责：

```text
- 源码级集成 AXDP。
- 屏蔽 AXDP commandId / packet / HID 细节。
- 对上层只暴露 DeviceCommand / DeviceResult。
```

TeaAdapter 职责：

```text
- 包装第三方 TEA API。
- 把第三方 API response 转成 DeviceResult。
- 把 TEA event / callback 转成 DeviceEvent。
```

验收标准：

```text
- 三种 adapter 对外返回统一 DeviceInfo / Capability。
- 产品不需要知道协议是 AXTP / AXDP / TEA。
- 错误能映射到统一 AxentError。
- AXTP Adapter 能完整跑通 open / capability / RPC / stream。
```

## Phase 6：libaxent-client 与 BackendSelector

目标：给 Nearcast、CLI、测试工具提供统一 SDK。产品默认依赖 `libaxent-client`，不直接依赖 Axent Core 内部。

Client 能力：

```text
Client
ClientOptions
BackendSelector
DaemonBackend
InProcessBackend
StreamReader
backendInfo()
```

BackendSelector 规则：

```text
1. 默认优先连接 axentd。
2. axentd 可用时使用 DaemonBackend。
3. axentd 不可用时，按配置尝试 auto-start。
4. safeFallback=true 时才允许 fallback 到 InProcessBackend。
5. fallback 前必须获取 TransportLease。
6. 拉流期间不允许静默切换 backend。
```

验收标准：

```text
- libaxent-client 可以连接 InProcessBackend。
- libaxent-client 可以连接 DaemonBackend。
- backend=auto 时优先 daemon。
- safeFallback=false 时不会静默 fallback。
- fallback 前检查 TransportLease。
- backendInfo() 能打印当前 backend。
```

## Phase 7：axentd Device Host

目标：基于 Axent Core 实现设备 daemon。它是正式生产环境的默认设备 owner。

axentd 职责：

```text
1. 启动 AxentRuntime。
2. 加载 AXTP / AXDP / TEA adapters。
3. 打开设备物理连接。
4. 管理 DeviceSession。
5. 管理 TransportLease。
6. 提供本地 IPC / WebSocket 控制 API。
7. 提供 StreamPlane。
8. 做事件 fan-out。
```

推荐命令：

```bash
axent daemon
axentd
```

设备级 API：

```text
device.list
device.info
device.capability.get
device.attach
device.detach
device.call
device.subscribe
stream.open
stream.close
axent.hello
axent.status
```

验收标准：

```text
- axentd 可以独立启动。
- CLI 或测试 client 可以连接 axentd。
- device.list / attach / call / stream.open 可用。
- axentd 是设备物理连接唯一 owner。
- 多个 client 连接 axentd，不会重复打开同一个 HID 设备。
```

## Phase 8：Nearcast 接入 Axent

目标：Nearcast 不再直接管理设备协议和设备物理连接，只通过 `libaxent-client` 控制设备。

Nearcast 内部分层建议：

```text
nearcast/
  domain/
    CastSession
    PreviewSession
    NearcastDevice

  application/
    StartCastUseCase
    StopCastUseCase
    OpenPreviewUseCase
    ReconnectPolicy
    NearcastCommandBus

  infrastructure/
    axent/
      AxentNearcastDeviceGateway
      AxentEventMapper
      AxentStreamAdapter

  control/
    NearcastControlServer
    NearcastProductApi

  player/
    decoder
    renderer
    jitter_buffer
```

Nearcast 只依赖：

```text
libaxent-client
axent_extension interfaces
axent_control common
```

Nearcast 不允许依赖：

```text
Axent Core 内部头文件
axtp-cpp-runtime internal
hidapi
AXDP packet implementation
TEA raw API
```

所有入口必须进入 Nearcast CommandBus：

```text
UI click
External remote control
Device event
Stream lost
Timer reconnect
```

验收标准：

```text
- Nearcast 不再直接打开 HID。
- Nearcast 不直接发送 AXTP RPC。
- Nearcast 不判断 AXTP / AXDP / TEA 协议差异。
- StartCastUseCase 通过 AxentNearcastDeviceGateway 控制设备。
- 外部 nearcast.startCast 进入 Nearcast CommandBus。
- 设备事件通过 AxentEventMapper 转成 NearcastCommand。
```

## Phase 9：StreamPlane 与低延迟拉流

目标：控制面走 Axent，流数据面走 fast path。

流路径优先级：

```text
1. Direct Media Handoff
2. InProcessStreamReader
3. SharedMemory Ring Buffer
4. Local Binary Socket / Named Pipe
5. WebSocket Binary
6. JSON-RPC chunk，仅兜底，不用于主视频路径
```

daemon 模式：

```text
Device
  -> axentd StreamPlane
  -> SharedMemory / LocalSocket
  -> Nearcast StreamReader
  -> Decoder
  -> Renderer
```

in-process / composite 模式：

```text
Device
  -> Axent StreamPlane
  -> InProcessStreamReader
  -> Nearcast Decoder
  -> Renderer
```

不允许：

```text
不要让每个视频帧走 JSON-RPC。
不要让每个视频帧走普通 ControlBroker 队列。
不要逐帧做重日志、重 tracing、JSON encode/decode。
```

验收标准：

```text
- stream.open 返回 StreamHandle，而不是直接返回帧数据。
- Nearcast 通过 StreamReader 读帧。
- daemon 模式支持 shared memory 或 local socket。
- in-process/composite 模式支持 InProcessStreamReader。
- 有 dropped frame / latency / throughput 指标。
```

## Phase 10：Composite Host 可选模式

目标：支持一个进程里同时加载 daemon module 和 nearcast module，适合 demo、便携模式、低延迟集成。

推荐命令：

```bash
axent run --modules daemon,nearcast
```

开发编排仍推荐：

```bash
axent up --with daemon,nearcast
```

不要引入 `axent --daemon --nearcast` 这类混合布尔开关。组合模式必须通过明确的 module 列表或 supervisor 命令表达。

Composite Host 规则：

```text
1. 单进程只能有一个 AxentRuntime。
2. DeviceDaemonModule 注册 adapter 并持有设备物理连接。
3. NearcastProductModule 通过 HostContext.localClient() 访问设备。
4. NearcastProductModule 不能创建第二个 AxentRuntime。
5. Nearcast 外部控制请求仍进入 CommandBus。
6. Stream 仍走 StreamPlane / InProcessStreamReader。
7. 关闭时先停 Nearcast，再停 DeviceDaemonModule，再停 AxentRuntime。
```

验收标准：

```text
- axent run --modules daemon,nearcast 可以启动单进程组合模式。
- 只有一个 AxentRuntime。
- Nearcast 不创建第二个 InProcessBackend。
- 设备事件仍通过 EventBus -> Nearcast CommandBus。
- 拉流可以走 InProcessStreamReader。
```

## Phase 11：稳定化、CI、文档、发布

目标：让 Axent 从项目内部模块变成稳定可复用基础设施。

测试矩阵：

```text
Unit Tests:
  DeviceManager
  DeviceSessionManager
  ControlSessionManager
  TransportLeaseManager
  CapabilityRegistry
  DeviceRouter
  EventBus
  StreamPlane
  BackendSelector

Integration Tests:
  FakeAdapter + InProcessBackend
  FakeAdapter + axentd + DaemonBackend
  Nearcast Gateway + FakeAdapter
  stream.open + StreamReader
  multiple clients + single device lease

Stress Tests:
  repeated attach/detach
  stream open/close loops
  event fan-out
  daemon restart
  backend disconnect/reconnect
```

发布物：

```text
libaxent-core
libaxent-client
axentd
axent CLI
axtpctl
headers
examples
test fake adapter
```

验收标准：

```text
- Nearcast 可以基于 Axent 完成设备发现、控制、拉流。
- axentd 可以作为独立设备 daemon 运行。
- Composite Host 可以作为可选模式运行。
- 文档足够约束 Codex 不乱改 Core。
- 新产品接入时不需要复制 Nearcast 设备控制代码。
```

## 推荐版本里程碑

### v0.1：Axent Core Skeleton

```text
目标：
  Axent 模块独立编译；
  有基本类型、Runtime、Registry、FakeAdapter。

包含：
  axent_common
  axent_extension
  axent_core
  FakeAdapter
  基础测试
```

### v0.2：Session / Lease / Event

```text
目标：
  设备会话、控制会话、Lease、事件分发跑通。

包含：
  DeviceSessionManager
  ControlSessionManager
  TransportLeaseManager
  EventBus
  DeviceStateStore
```

### v0.3：DeviceRouter / Capability / Stream

```text
目标：
  统一能力模型和 stream.open 流程跑通。

包含：
  CapabilityRegistry
  DeviceRouter
  StreamPlane
  StreamHandle
  StreamReader interface
```

### v0.4：真实 Adapter

```text
目标：
  接入 AXTP / AXDP / TEA 至少一个真实协议。

优先：
  AxtpAdapter
  AxdpAdapter
  TeaAdapter
```

### v0.5：axentd

```text
目标：
  axentd 可以独立运行并成为设备 owner。

包含：
  axent daemon
  axentd
  local IPC
  axent.hello
  device.list
  device.call
  stream.open
```

### v0.6：libaxent-client

```text
目标：
  Nearcast 通过统一 client API 访问 Axent。

包含：
  Client
  DaemonBackend
  InProcessBackend
  BackendSelector
  backendInfo
```

### v0.7：Nearcast 集成

```text
目标：
  Nearcast 不再直接实现设备协议。

包含：
  AxentNearcastDeviceGateway
  NearcastCommandBus
  NearcastControlServer
  StartCastUseCase 改造
  StreamReader 接入
```

### v0.8：Stream Fast Path

```text
目标：
  拉流性能达标。

包含：
  InProcessStreamReader
  SharedMemoryStreamProvider
  LocalSocketStreamProvider
  stream stats
  dropped frame policy
```

### v0.9：Composite Host

```text
目标：
  支持单进程组合模式。

包含：
  AxentHostManager
  DeviceDaemonModule
  NearcastProductModule
  axent run --modules daemon,nearcast
```

### v1.0：稳定发布

```text
目标：
  API 稳定，可给多个产品使用。

包含：
  完整文档
  CI
  fake + real integration tests
  API versioning
  extension manifest
  release package
```

## 当前阶段推荐执行顺序

现在最应该先做：

```text
1. 维护文档和 guardrails。
2. 建 Axent 独立模块。
3. 做 FakeAdapter 闭环。
4. 做 Axent Core target 拆分。
5. 做 libaxent-client。
6. 做 axentd。
7. 再改 Nearcast。
```

不要一开始就做：

```text
1. 直接改 Nearcast。
2. 直接做动态插件。
3. 直接把 axentd 和 Nearcast 合在一起。
4. 直接接所有真实协议。
5. 直接优化视频流。
```

## 给 Codex 和研发的执行口径

```text
Stage 1: Build Axent as an independent module first.
Axent must be a stable device-control runtime core with extension points.
Do not put product-specific logic into Axent Core.

Stage 2: Implement Axent Core primitives:
AxentRuntime, DeviceManager, DeviceSessionManager, ControlSessionManager,
TransportLeaseManager, CapabilityRegistry, DeviceRouter, EventBus,
StreamPlane, AdapterRegistry, ExtensionRegistry, ControlBroker, DeviceBroker.

Stage 3: Add static extension interfaces:
IDeviceAdapter, IProductExtension, IControlEndpoint, IStreamProvider,
ICapabilityProvider, IAxentHostModule.
Do not implement dynamic C++ plugins in this phase.

Stage 4: Add FakeAdapter and integration tests.
Use FakeAdapter to validate Axent Core before adding real protocols.

Stage 5: Implement real adapters:
AxtpDeviceAdapter using axtp-cpp-runtime,
AxdpDeviceAdapter using AXDP source-level integration,
TeaDeviceAdapter using third-party TEA API wrapper.

Stage 6: Implement libaxent-client:
Client, ClientOptions, BackendSelector, DaemonBackend, InProcessBackend,
StreamReader, backendInfo().

Stage 7: Implement axentd as Device Host:
axent daemon / axentd.

Stage 8: Integrate Nearcast through libaxent-client.
All UI actions, external control requests, device events, stream events, and
timers must enter NearcastCommandBus.

Stage 9: Implement StreamPlane fast paths.
High-bandwidth video/audio frames must not go through normal JSON-RPC control
path.

Stage 10: Optionally implement Composite Host mode:
axent run --modules daemon,nearcast.
Composite mode must create exactly one AxentRuntime.
```

硬规则再次确认：

```text
- Axent Core must not contain product-specific workflow logic.
- axentd must not depend on Nearcast.
- Nearcast must not depend on Axent Core internals.
- axtp-cpp-runtime remains protocol runtime and must not depend on Axent or Nearcast.
- Product-specific behavior belongs in Product Host / Product Extension.
- Device protocol differences belong in DeviceAdapter.
- Stream transport differences belong in StreamProvider.
```
