# AXTP 术语表

本文定义 AXTP 的共享词汇。它不定义字段布局、注册表事实、生成产物或发布状态。

## 分层

| 术语 | 含义 |
|---|---|
| AXTP | Auditoryworks Transport Protocol，由 runtime、SDK、工具链、mock server 和 conformance 仓库消费的协议标准。 |
| Transport | 承载 AXTP 字节流或 JSON 消息的传输载体，例如 TCP、USB HID、WebSocket、BLE、UART 或 mock transport。 |
| Transport Profile | 固定某种 transport 如何承载 AXTP 的规则，包括是否使用 Standard Framed Binary 或 WebSocket Unframed JSON。 |
| Frame Layer | Standard Frame 标准帧解析层，处理 magic、version、payload length、PayloadType、分片和 CRC。 |
| Payload Layer | 根据 PayloadType 分发后的 CONTROL、RPC 或 STREAM 解析层。 |
| Registry Layer | method、event、error、schema、capability 和 profile 的机器事实层。 |
| Business Layer | 设备或产品语义层，例如 `audio`、`video`、`network`、`firmware` 或 `room`。 |

## Payload 层

| 术语 | 含义 |
|---|---|
| PayloadType | 顶层解析器选择值：CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。它不是业务类型。 |
| CONTROL | Standard Framed 链路控制，用于 OPEN / ACCEPT、heartbeat 和 close。 |
| RPC | 业务控制面，用于 Hello / Identify / Identified、Request / Response 和 Event。 |
| STREAM | 为已经建立的 stream context 承载连续数据的数据面。 |
| Control Plane | 管理 session 和业务命令的 CONTROL 加 RPC 行为。 |
| Data Plane | 承载媒体、firmware、file、log 或 sensor chunk 等连续数据的 STREAM 行为。 |

## 上下文与 ID

| 术语 | 含义 |
|---|---|
| Transport connection | 底层 socket、WebSocket、HID connection 或等价 transport handle。 |
| Framed Link Context | 通过 CONTROL OPEN / ACCEPT 建立的 Standard Framed 上下文。 |
| RPC Session | 通过 RPC Hello / Identify / Identified 建立的应用 session。 |
| Stream Context | 发送 STREAM 数据前，由已采纳 RPC method 或 profile 创建的单 stream 元数据。 |
| `sessionId` | 可选的 CONTROL link identifier，用于 trace 或未来 resume；不是业务 session id。 |
| `sid` | Identify 成功后由 Logical Server 分配的 RPC Session ID。 |
| `requestId` | RPC request/response 关联 id。 |
| `messageId` | Standard Frame 分片、重组和调试 id。 |
| `streamId` | STREAM context id。 |
| `seqId` | STREAM packet sequence id。 |
| `cursor` | STREAM 位置或时间游标，单位由 stream context 定义。 |

## 注册表术语

| 术语 | 含义 |
|---|---|
| Domain | 稳定的业务或协议分类，例如 `audio`、`video`、`device`、`system`、`network` 或 `capability`。 |
| Feature | domain 内可评审的能力块，例如 `audio.algorithm` 或 `network.wifi`。 |
| Method | 拥有稳定名称和可选 binary methodId 的 RPC 业务操作。 |
| Event | 表示状态、进度、结果或报告语义的 RPC 异步通知。 |
| ErrorCode | RPC status、CONTROL status 和 STREAM/profile error mapping 共用的数值错误码。 |
| Schema | method params/result、event payload、capability 或 profile data 的结构化对象定义。 |
| Capability | 设备声明的某个 domain.feature 的可用性或限制。 |
| Profile | 对 transport、method、event、error、type 和 capability 的命名实现要求集合。 |
| Protocol IR | 生成的机器可读模型，位于 `contract/protocol/axtp.protocol.yaml`。 |
| Generated Reference | `contract/generated/**` 下生成的人读和机器读参考。 |

## 角色

| 术语 | 含义 |
|---|---|
| Physical Client | 主动建立底层 transport connection 的一侧。 |
| Physical Server | 接受底层 transport connection 的一侧。 |
| Logical Client | 向 Logical Server identify，并主要消费其 method/event/stream 的一侧。 |
| Logical Server | 发送 Hello、分配 `sid`，并暴露 method/event/stream 的一侧。 |

物理方向和逻辑方向可以不同。例如设备主动连接云端时可以是 Physical Client，但仍然作为 Logical Server 暴露能力。

## 编码

| 术语 | 含义 |
|---|---|
| JSON | `{ sid, op, d }` 的默认 RPC 对象编码。 |
| CBOR | 同一 RPC 语义的可选紧凑对象编码。 |
| MSGPACK | 同一 RPC 语义的可选紧凑对象编码。 |
| JSON_BINARY | AXTP RPC 固定二进制 envelope 编码；不是通用 binary JSON 格式。 |
| TLV8 | `fieldId:uint8 + length:uint8 + value` body 编码。 |
| TLV16 | 更长的 TLV body 编码，除非 profile 要求，否则属于 optional/future。 |
