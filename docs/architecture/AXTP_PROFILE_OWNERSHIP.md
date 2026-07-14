# AXTP Media / Firmware Profile 分层与迁移规范

状态：架构决策草案，作为后续拆分任务的边界依据

更新时间：2026-07-14

## 1. 背景与目标

此前 `axtp-cpp-runtime` 中的 media profile 和 firmware profile 同时包含了两类职责；
当前两类 profile 均已在消费者迁移完成后退役：

- AXTP 协议字段、RPC、STREAM 数据等协议语义；
- 流打开/关闭、文件更新、重试、状态管理等 Host 侧流程编排。

这使 cpp-runtime 不再只是协议运行时，也让 Nearcast 和 Axent 容易直接依赖
runtime 的 Host 类型。长期目标是让依赖方向保持单向且职责清晰：

```text
axtp-cpp-runtime
  AXTP 帧、RPC、Session、StreamPayload、生成代码和协议编解码

Axent
  AXTP Adapter、设备会话、媒体交付契约、固件更新服务、具体 transport

Nearcast
  投屏源抢占、解码、渲染、时钟、缓冲、窗口和产品控制面
```

本规范回答以下问题：

1. media profile 和 firmware profile 最终分别归哪一层所有；
2. 当前三个仓库需要怎样改动；
3. 对 Nearcast WebSocket、投屏抢占和流传输是否有行为影响；
4. 哪些 legacy 路径可以直接退役，不再提供兼容层。

## 2. 核心决策

### 2.1 Runtime 只保留协议能力

`axtp-cpp-runtime` 最终只负责：

- AXTP wire frame 的解析和编码；
- RPC / Event / Session / STREAM 的协议原语；
- generated schema、method ID、event ID 和错误码；
- `axtp::StreamPayload` 等 wire-level 数据结构；
- 不持有产品状态的、确定性的协议事务 helper。

Runtime 不再负责：

- 媒体 Host、媒体流注册表和拉流策略；
- 面向产品的 open/close coordinator；
- 调试 WebSocket control bridge；
- 文件读取、MD5、更新进度、恢复、重试和取消；
- Nearcast 抢占、解码、渲染或 UI 行为。

判断标准不是代码是否“通用”，而是它是否需要持有 Host 状态、产品策略或资源所有权。

### 2.2 Media 使用 Axent 稳定交付契约

不把 cpp-runtime 的整个 `media_profile` 原样搬入 Axent。Axent 只持有主链路需要的最小、
稳定媒体契约：

- `axent::MediaKind`；
- `axent::MediaCodec`；
- `axent::MediaFrame` 及 key frame、config、discontinuity 等 flags；
- 媒体流描述信息；
- `IMediaFrameSink` / `MediaSubscription` 等 producer-consumer 契约；
- 有界队列、丢帧策略和交付统计；
- `AxtpAdapter` 从 `axtp::StreamPayload` 到 Axent 媒体类型的映射。

Axent 当前已经具备以下基础类型，应在此基础上演进，而不是另起一套 profile：

```text
include/axent/media/media_frame.hpp
include/axent/media/media_subscription.hpp
include/axent/media/media_relay.hpp
```

仍需补足的主要能力是 stream opened/closed 生命周期以及稳定的 stream descriptor。
该 descriptor 应使用 Axent 类型，不应把 `axtp::mediahost::MediaStreamInfo` 或
`nlohmann::json` 直接泄漏到产品公共边界。

以下旧 media profile 组件不迁入 Axent：

- `MediaStreamRegistry`；
- `MediaPullCoordinator`；
- `MediaCloseCoordinator`；
- `MediaControlBridge`；
- mediahost demo；
- dump directory、调试日志等 Host options。

其中仍有价值的协议字段映射应进入 `AxtpAdapter`；产品策略应继续留在 Nearcast。

### 2.3 Firmware 由 Axent 服务负责完整生命周期

固件更新应分成两层：

```text
Axent FirmwareUpdateService
  文件读取、MD5、manifest 准备、lease、进度、重试、取消、恢复、错误映射
                       |
                       v
AXTP 协议事务
  firmware.beginUpdate -> STREAM chunks -> firmware.finishUpdate
```

`beginUpdate -> STREAM -> finishUpdate` 是确定性的 AXTP 协议事务，现已由 Axent
`AxtpFirmwareBackend` 私有实现。cpp-runtime 的 `FirmwareUpdateProfile` 已删除；该事务
不能成为产品直接依赖的公共接口，也不改变 `FirmwareUpdateService` 对上的稳定契约。

Axent 固件服务应负责：

- 获取并释放设备/transport lease；
- 校验文件、大小、版本、target 和 hash；
- 维护 task ID、状态和进度；
- 处理超时、重试、取消、掉线和可恢复失败；
- 将 AXTP 错误映射为 Axent 错误；
- 为 `axtpctl`、`axentd` 和产品 Host 提供同一能力入口。

Runtime 不读取本地文件，也不决定重试、恢复或升级授权策略。

## 3. 目标所有权矩阵

| 能力或类型 | 当前主要位置 | 目标所有者 | 说明 |
| --- | --- | --- | --- |
| AXTP frame / RPC / Event / Session | cpp-runtime | cpp-runtime | 纯协议能力 |
| `axtp::StreamPayload` | cpp-runtime | cpp-runtime | 保持 wire-level 类型 |
| generated schema / IDs / errors | cpp-runtime | cpp-runtime | 保持协议单一事实源 |
| `MediaKind` / codec / frame flags | runtime 与 Axent 重复 | Axent 公共媒体契约 | 产品不再引用 runtime mediahost 类型 |
| stream descriptor / media sink | cpp-runtime media profile | Axent | 作为 adapter 与消费者之间的稳定契约 |
| media registry / pull / close coordinator | cpp-runtime media profile | 不迁移 | 主链路由 Axent adapter 和产品状态机替代 |
| media control bridge / mediahost demo | cpp-runtime | 退役 | 不再维护 |
| AXTP 媒体 RPC 与 STREAM 映射 | 多处重复 | Axent `AxtpAdapter` | 隐藏协议差异 |
| `beginUpdate -> STREAM -> finishUpdate` | cpp-runtime firmware profile | Axent `AxtpFirmwareBackend` 私有实现 | 不作为产品 API；runtime helper 已退役 |
| 文件、MD5、进度、lease、恢复 | tooling / Host 分散 | Axent firmware service | 资源与业务流程所有者 |
| 投屏源抢占 | Nearcast | Nearcast | 继续由产品协调器持有 |
| 解码、渲染、时钟、缓冲 | Nearcast | Nearcast | 不进入 Axent 或 runtime |

## 4. Nearcast 主链路与影响

### 4.1 当前主链路

Embedded Axent 已经是 Nearcast 的生产主路径：

```text
Device AXTP STREAM
  -> axtp-cpp-runtime StreamPayload
  -> Axent AxtpAdapter
  -> axent::MediaFrame
  -> MediaSubscription / MediaRelay
  -> Nearcast AxentMediaBridge
  -> MediaCore
  -> decoder / renderer
```

迁移的核心是让 `AxentMediaBridge`、`EmbeddedAxentSource` 和 `MediaCore` 不再把
`axent::MediaFrame` 转回 `axtp::mediahost::*` 类型。它们应直接消费 Axent 媒体契约，
或在 Nearcast 内部转换为产品自己的 decode 类型。

因此，这不是“完全零改动”的编译切换：include、类型签名、CMake target 和测试都要调整。
但 AXTP wire 数据、线程模型和产品状态机无需重写，运行时行为可以做到等价切换。

### 4.2 WebSocket 必须区分三种用途

#### Nearcast 产品控制面 WebSocket

`ws://127.0.0.1:7020` 上的 `cast.*` ControlPlane 不依赖 cpp-runtime media profile。
它的 method routing、JSON schema 和产品命令语义不应因本次迁移改变。

状态查询中展示的媒体状态仍需来自 Embedded Axent 主链路，但这只是底层数据源换型，
不是产品控制协议变化。

#### Legacy HID 调试 WebSocket

`--hid-ws-port` 默认使用的 7010 WebSocket 与 `HidAxtpRuntimeHost` 属于旧调试路径。
该路径已明确停止维护，可以直接退役：

- 删除 `--hid-ws-port` 入口和相关配置；
- 从构建中移除 `HidAxtpRuntimeHost`；
- 删除 `hid_axtp_websocket_control_test` 等只验证 legacy bridge 的测试；
- 不提供兼容 wrapper，也不把 `MediaControlBridge` 迁入 Axent。

执行删除前只需确认仓库外没有仍被承诺支持的 7010 消费者。确认后，7010 不再属于
Nearcast 的兼容接口。

#### AXTP WebSocket transport

AXTP 自身的 WebSocket transport 是设备协议传输方式，与上述两个产品控制端口不同。
它的所有权迁移属于 concrete transport 拆分任务，不应与 media profile 换型绑成一次改动。

### 4.3 投屏抢占

投屏抢占继续由 Nearcast 的 `CastSourceCoordinator` 和 `EmbeddedAxentSource` 持有，
不属于 media profile。AirPlay 与 Embedded Axent 的互斥、隐私停止和 source release
策略均不应迁入 Axent。

本次迁移必须保持以下触发语义：

- 仍在首个有效视频帧到达时请求激活 Embedded Axent source；
- 新 source 激活时，旧 source 的 stop 回调顺序不变；
- source close、release 和新 stream generation 的顺序不变；
- 音频帧不能错误地触发视频投屏抢占；
- 隐私模式和显式 stop 的优先级不变。

只要这些不变量有测试保护，类型迁移不会改变投屏抢占业务逻辑。

### 4.4 流传输

本次不修改 AXTP STREAM wire format，也不改变设备发送的数据。必须逐字段保持：

```text
streamId
seqId / sequenceId
cursor
timestamp
sessionId
deviceId
keyFrame
config
discontinuity
endOfFrame
payload bytes
```

同时必须保持以下时序和并发不变量：

- stream open 通知先于该 generation 的正常帧交付；
- 首帧到达时机不因额外队列发生变化；
- callback 所在线程或显式 dispatch 策略保持可预测；
- close 后的旧 generation 帧不得进入新 stream；
- stream ID 复用时必须重置 seq/cursor/discontinuity 状态；
- 有界队列、丢帧和 key-frame recovery 策略不退化；
- shutdown/cancel 后不再回调已销毁的 Nearcast 对象。

## 5. 当前影响面

2026-07-11 的静态盘点中，Nearcast 约有 200 处 cpp-runtime media profile 引用：

- 约 111 处位于 `src/hid/HidAxtpRuntimeHost.cpp`；
- 约 24 处位于 legacy HID protocol/WebSocket 测试；
- 排除可退役路径后，活跃主链路约剩 65 处。

活跃引用主要集中在：

```text
src/media/MediaCore.*
src/sources/EmbeddedAxentSource.cpp
src/sources/AxentMediaBridge.*
src/media/render/*
tests/axent_media_bridge_test.cpp
tests/embedded_axent_source_test.cpp
CMakeLists.txt
```

这意味着大部分文本引用可以通过删除 legacy 路径消失；真正需要谨慎换型的是约三组
生产模块：Axent bridge、Embedded source 和 MediaCore/render boundary。

## 6. 分仓库改动建议

### 6.1 Axent

1. 以现有 `axent::MediaFrame`、`MediaSubscription` 和 `MediaRelay` 为基础补足
   stream descriptor 与 open/close 生命周期。
2. 保持 `axtp::StreamPayload` 只出现在 AXTP adapter 实现边界，不进入产品公共接口。
3. 让 `AxtpAdapter` 完成所有 runtime-to-Axent 媒体字段映射。
4. 由 Axent firmware service 和私有 backend 持有底层确定性 AXTP 更新事务。
5. 增加边界检查，阻止 Core 和产品公共头文件包含 `profiles/media/*` 或
   `profiles/firmware/*`。

### 6.2 Nearcast

1. 确认无仓库外 7010 / `HidAxtpRuntimeHost` 消费者。
2. 删除 legacy Host、7010 CLI 参数、legacy bridge 测试及其构建项。
3. 将 `MediaCore` 从 `axtp::mediahost::IMediaStreamSink` 解耦。
4. 将 `AxentMediaBridge` 和 `EmbeddedAxentSource` 改为 Axent 媒体类型；若 bridge
   只剩字段复制，可进一步删除该层。
5. 将 renderer 边界中的 runtime mediahost alias 改为 Nearcast 或 Axent 稳定类型。
6. 从 Nearcast CMake 移除 `axtp_media_profile` 链接和存在性检查。

### 6.3 axtp-cpp-runtime

在 Axent 和 Nearcast 都不再消费旧 profile 后：

1. 删除 mediahost demo target；
2. 删除 media registry、pull/close coordinator 和 control bridge；
3. 删除不再被使用的 `axtp_media_profile` target 和 Host headers；
4. 保留 AXTP STREAM、RPC、schema 和协议错误类型；
5. 删除已无消费者的 firmware profile、公开头文件、测试和安装导出；
6. 保持确定性固件事务位于 Axent 私有 backend，不在 runtime 重新引入 Host/profile 层。

## 7. 推荐迁移顺序

为控制回归面，按以下顺序拆成可独立验收的任务：

1. **确认 legacy 消费者**：确认 7010 和 `HidAxtpRuntimeHost` 不再对外承诺。
2. **退役 legacy 路径**：移除 Host、CLI 参数、构建项和专属测试。
3. **补全 Axent media contract**：增加 descriptor 和 stream lifecycle，先写契约测试。
4. **切换 Nearcast 主链路类型**：依次改 bridge、Embedded source、MediaCore 和 renderer。
5. **验证抢占与时序**：确保首帧激活、双向抢占、generation reset 和 shutdown 正确。
6. **移除 `axtp_media_profile` 依赖**：清理 Nearcast include、link 和 CMake 检查。
7. **清理 cpp-runtime media profile**：在全仓无消费者后删除旧实现。
8. **完善 firmware service**：将文件和任务生命周期完整收口到 Axent。

不建议一次提交同时迁移 media、firmware、具体 transport 和产品 WebSocket；它们的所有权
方向一致，但回归面和验收方式不同。

## 8. 验证要求

### Media

- `AxtpAdapter` 字段映射单元测试覆盖 video/audio、flags、seq、cursor 和 payload；
- `axent_media_bridge_test` 在换型期间保留，bridge 删除后由等价 contract test 替代；
- `embedded_axent_source_test` 覆盖首帧激活、close/reopen、stream ID 复用和掉线；
- AirPlay -> Embedded Axent 与 Embedded Axent -> AirPlay 双向抢占均有集成测试；
- 验证 config/key frame/discontinuity 不丢失；
- 验证队列上限、丢帧统计和停止后的 callback 安全性；
- 边界脚本确认 Nearcast 不再 include/link `profiles/media/*` 或 `axtp_media_profile`。

### WebSocket

- 7020 `cast.*` smoke test 保持通过；
- 状态查询仍能展示当前 source、stream 和错误状态；
- 7010 legacy 测试随接口退役删除，不设置兼容性验收项；
- AXTP WebSocket transport 测试不因 media 类型迁移发生协议变化。

### Firmware

- 文件不存在、不可读、hash 错误和非法 chunk size；
- `beginUpdate`、STREAM chunks、`finishUpdate` 的成功与失败；
- 更新期间 lease 独占；
- 进度、取消、掉线、可恢复失败和重试；
- `axtpctl` 与 Host API 通过同一 Axent service 获得一致结果。

## 9. 验收标准

完成本轮分层后应满足：

- Nearcast 生产 target 不依赖 `axtp_media_profile`；
- Nearcast 公共和产品代码不出现 `axtp::mediahost::*`；
- cpp-runtime 不包含 media Host、产品策略或调试控制面；
- Axent 对上暴露稳定 media contract 和 firmware service；
- 7020 产品控制面行为不变，7010 legacy 控制面明确退役；
- 投屏抢占、首帧时机和 STREAM 字段/顺序均有回归测试；
- cpp-runtime 不依赖 Axent 或 Nearcast，Nearcast 不依赖 runtime Host 层。

## 10. 非目标

本规范不要求在同一次改动中完成：

- AXTP HID/TCP/WebSocket concrete transport 所有权迁移；
- axentd 跨进程共享内存或 local socket fast path；
- Nearcast decoder、renderer 或 jitter buffer 重写；
- 7020 产品控制 API 改版；
- 对已退役 7010 legacy HID 调试接口提供兼容层。

这些任务可以在边界稳定后独立推进，但不得重新把 Host 或产品职责放回
`axtp-cpp-runtime`。
