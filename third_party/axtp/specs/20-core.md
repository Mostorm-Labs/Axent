# AXTP 核心协议

本文是 AXTP 传输路径、Standard Framed Binary、CONTROL、RPC、STREAM 和低带宽边界的规范性 runtime 合同。

## 核心模型

AXTP 用两条生产路径暴露同一套业务注册表：

| 路径 | Transport 示例 | Wire 形态 | 必需能力 |
|---|---|---|---|
| Standard Framed | `AXTP-TCP`、`AXTP-USB-HID` | `Standard Frame Header(12B) + Payload(N) + CRC16(2B)` | CONTROL / RPC / STREAM |
| WebSocket Unframed JSON | `AXTP-WS-JSON`、`AXTP-WS-CLOUD-REVERSE` | WebSocket message payload 是 JSON `{ sid, op, d }` | 仅 RPC |

PayloadType 只选择解析器：

| PayloadType | 值 | 解析器 |
|---|---:|---|
| CONTROL | `0x01` | 链路运行时控制。 |
| RPC | `0x02` | Session、request/response 和 event 控制面。 |
| STREAM | `0x03` | 连续数据面。 |

PayloadType MUST NOT 编码 video、audio、firmware、file、domain.feature、method、event 或 capability 等业务类型。

## Standard Frame 标准帧

Standard Framed Binary runtime MUST 实现：

```text
Standard Frame Header(12B) + Payload(N) + CRC16(2B)
```

Header 布局：

| 字段 | 偏移 | 大小 | 规则 |
|---|---:|---:|---|
| `Magic[0]` | 0 | 1B | MUST 为 `0x41` (`A`)。 |
| `Magic[1]` | 1 | 1B | MUST 为 `0x58` (`X`)。 |
| `Version` | 2 | 1B | 当前值为 `0x01`。 |
| `PayloadType` | 3 | 1B | CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。 |
| `PayloadLength` | 4 | 2B | 仅 payload 字节数，不包含 header 和 CRC。 |
| `SourceId` | 6 | 1B | 发送方 logical node。 |
| `DestinationId` | 7 | 1B | 接收方 logical node。 |
| `MessageId` | 8 | 2B | 用于分片和调试的 frame/message 关联。 |
| `FrameIndex` | 10 | 1B | 分片序号，从 0 开始。 |
| `FrameCount` | 11 | 1B | 分片总数；未分片 message 使用 1。 |
| `CRC16` | Footer | 2B | 对 header + payload 计算 CRC16-CCITT-FALSE。 |

所有 AXTP 多字节 wire integer MUST 使用 Big-Endian / network byte order。

Frame parser MUST 在分发前校验 magic、version、PayloadType、`PayloadLength + 14 <= maxFrameSize`、`FrameCount >= 1`、`FrameIndex < FrameCount`、CRC 和完整 payload 可用性。

分片属于 Frame layer。Request/Response 匹配使用 RPC request id，不使用 `MessageId`。STREAM 排序使用 `seqId`，不使用 `MessageId`。

## Transport Profile 传输配置

Transport profile 固定外层 envelope。AXTP 不在运行时协商 Standard Frame 形态。

| Profile | Envelope | 说明 |
|---|---|---|
| `AXTP-TCP` | TCP 字节流上的 Standard Framed。 | 接收方 SHOULD 扫描 magic、解析 12B header、读取 payload 和 CRC，然后分发。 |
| `AXTP-USB-HID` | report/packet 上的 Standard Framed。 | Packet 边界不能替代 header/CRC 校验。 |
| `AXTP-WS-JSON` | WebSocket Unframed JSON。 | 仅 RPC；没有 CONTROL、STREAM、CRC、Standard Frame Header 或 JSON_BINARY fixed header。 |
| `AXTP-WS-CLOUD-REVERSE` | WebSocket Unframed JSON。 | 同样是仅 RPC 的 wire shape；物理连接方向可以不同于逻辑角色。 |

Standard Framed 启动顺序：

```text
Transport connected
CONTROL OPEN
CONTROL ACCEPT
RPC Hello
RPC Identify(randomSeed)
RPC Identified
APP_READY
```

WebSocket JSON 启动顺序：

```text
WebSocket connected
RPC Hello
RPC Identify(randomSeed)
RPC Identified
APP_READY
```

Runtime gate 状态：

| 状态 | 允许的流量 | 拒绝的流量 |
|---|---|---|
| `LINK_CONNECTED` | Standard Framed：只允许 CONTROL OPEN。WebSocket JSON：可以开始 RPC Hello。 | 业务 RPC 和 STREAM。 |
| `FRAMING_READY` | RPC Hello / Identify / Identified。 | 业务 Request、Event 和 STREAM。 |
| `APP_READY` | Request / RequestResponse / Event；如果 profile 和业务 Stream Context 允许，也可以 STREAM。 | 未知 method、未授权 method、无效 sid、未知 streamId。 |
| `CLOSING` | CLOSE_ACK 和本地清理。 | 新业务 RPC 或新 STREAM context。 |

Request-before-identified MUST 按 profile policy 拒绝或关闭 session；它 MUST NOT 被当作业务流量处理。

## CONTROL

CONTROL 只存在于 Standard Framed profile。WebSocket Unframed JSON MUST NOT 发送或要求 CONTROL。

CONTROL payload 为：

```text
opcode:uint8 + controlId:uint16 + statusCode:uint16 + TLV body
```

`controlId` 和 `statusCode` 使用 Big-Endian / network byte order。`statusCode=0x0000` 表示 SUCCESS；非零值使用 ErrorCode registry。

必需 CONTROL opcode：

| Opcode | 名称 | 必需行为 |
|---:|---|---|
| `0x01` | `OPEN` | Physical Client 发起 framed link 协商。 |
| `0x02` | `ACCEPT` | Physical Server 接受或拒绝 OPEN。 |
| `0x04` | `HEARTBEAT` | 保活。 |
| `0x05` | `HEARTBEAT_ACK` | 保活响应。 |
| `0x0A` | `CLOSE` | 优雅关闭请求。 |
| `0x0B` | `CLOSE_ACK` | 优雅关闭响应。 |

不存在 `REJECT` opcode。被拒绝的 OPEN 是一个带非零 `statusCode` 的 `ACCEPT`。

对 OPEN、HEARTBEAT 和 CLOSE 的 response MUST 回显 request `controlId`。如果接收方无法解析 CONTROL payload length、TLV length、opcode 或必需协商字段，在还能 framing 出合法 response 时，SHOULD 返回最接近的 CONTROL error；否则 MAY 关闭 transport。

必需 OPEN / ACCEPT TLV：

| TLV | 名称 | 规则 |
|---:|---|---|
| `0x04` | `maxFrameSize` | 必需；总 frame size，包含 12B header 和 2B CRC。 |
| `0x07` | `supportedPayloadTypes` | 必需 bitmap。 |
| `0x08` | `supportedRpcEncodings` | OPEN 中必需。 |
| `0x0A` | `heartbeatIntervalMs` | 必需。 |
| `0x0B` | `ackMode` | 必需；Phase 1 默认值为 `NONE`。 |
| `0x1E` | `selectedRpcEncoding` | 成功 ACCEPT 中必需。 |

`sessionId(0x01)` MAY 为 trace 或未来 resume 而解析，但 MUST NOT 路由 RPC 或 STREAM 业务状态。

新实现 SHOULD 省略 deprecated CONTROL `protocolVersion(0x02)`，且 MUST NOT 因为一个有效 v1 handshake 缺少它而拒绝握手。`maxPayloadSize(0x05)` 已 deprecated/reserved；使用 `maxFrameSize`。

READY is optional / 可选。默认握手只要求 OPEN / ACCEPT。Runtime MUST NOT 把 READY 当作第三个默认握手步骤。

Phase 1 不要求 runtime 实现 ACK/NACK。ACK、NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、PING、PONG、GOAWAY 和 VENDOR 属于 future/profile-specific 行为，除非已发布 profile 明确要求。

最小 CONTROL 示例，按 payload level 展示：

```text
OPEN:
01 00 01 00 00
04 02 10 00        # maxFrameSize = 4096
07 01 07           # CONTROL + RPC + STREAM
08 01 09           # JSON + JSON_BINARY supported
0a 02 03 e8        # heartbeatIntervalMs = 1000
0b 01 00           # ackMode = NONE

ACCEPT:
02 00 01 00 00
04 02 10 00        # maxFrameSize = 4096
07 01 07           # CONTROL + RPC + STREAM
1e 01 01           # selectedRpcEncoding = JSON
0a 02 03 e8        # heartbeatIntervalMs = 1000
0b 01 00           # ackMode = NONE
```

Standard Frame Header、payload length 和 CRC 会包裹这个 CONTROL payload。启动流程摘要见 `docs/guides/core-protocol-flow.md`。

## RPC

RPC 是业务控制面。它运行在 Standard Framed `PayloadType=RPC` 中，或直接作为 WebSocket Unframed JSON。

JSON/CBOR/MSGPACK RPC envelope：

```json
{ "sid": "12345678", "op": 7, "d": {} }
```

| 字段 | 规则 |
|---|---|
| `sid` | 分配前为空字符串；Identified 后携带 Logical Server 分配的 session 字符串。AXTP-native 生成端使用 8 位 hex；接收端按 opaque string 兼容。 |
| `op` | uint8 操作码。 |
| `d` | op-specific object；允许 empty object。 |

AXTP-native `sid` 生成使用非零 `uint32`，在 JSON / CBOR / MSGPACK envelope 中渲染为固定 8 位十六进制字符串，例如 `"00000003"` 或 `"12345678"`。对象编码接收端 MUST NOT 要求收到的 `sid` 一定是 8 位 hex；Identified 后应把 Logical Server 分配的非空字符串按 opaque value 保存，并在后续 Request / Response / Event 中精确携带。JSON_BINARY fixed header 中仍使用 4B Big-Endian / network byte order `uint32`，未分配前为 `0`。

必需 RPC op：

| op | 名称 | 必需行为 |
|---:|---|---|
| `0` | `Hello` | Logical Server 公告可选 AXTP version diagnostics 和 auth requirements。 |
| `2` | `Identify` | Logical Client 发送 identity、`randomSeed`、auth 和 event subscription intent。 |
| `3` | `Identified` | Logical Server 分配 `sid`；session 进入 app-ready。 |
| `6` | `Event` | 低频 event 投递。 |
| `7` | `Request` | Identified 后的业务 method request。 |
| `8` | `RequestResponse` | 业务 result 或 error。 |

RequestResponse 的 `d.status` 在 JSON / CBOR / MSGPACK envelope 中 MUST 是 object，且 MUST 携带 `status.ok` 和 `status.code`。成功响应 MUST 使用 `status.ok=true`、`status.code=0`；失败响应 MUST 使用 `status.ok=false` 和非零 `status.code`，且 MUST NOT 携带业务 `result`。JSON_BINARY fixed header 中的 `statusCode:uint16` 与 `status.code` 语义一致，但 JSON envelope MUST NOT 使用数字形式的 `status` 简写。

最小 RPC JSON 序列：

```json
{ "sid": "", "op": 0, "d": { "axtpVersion": "1.0.0" } }
{ "sid": "", "op": 2, "d": { "randomSeed": 305419896, "eventMasks": "090101" } }
{ "sid": "12345678", "op": 3, "d": { "accepted": true } }
{ "sid": "12345678", "op": 7, "d": { "id": 1, "method": "audio.getAlgorithmConfig", "params": {} } }
{ "sid": "12345678", "op": 8, "d": { "id": 1, "status": { "ok": true, "code": 0 }, "result": {} } }
{ "sid": "12345678", "op": 6, "d": { "event": "audio.algorithmConfigChanged", "data": { "reason": "user_request", "applyState": "applied" } } }
```

Event 的业务 payload 不重复携带 `sid`。发送方 MUST 在 RPC envelope 或 JSON_BINARY fixed header 中携带当前 RPC session 的 `sid`；接收方按 envelope/header 校验、路由和鉴权后，再把 `d.data` 作为 event payload 交给业务处理。

在 Standard Framed JSON RPC 中，RPC payload 是 `rpcEncoding(1B) + JSON bytes`；当 `selectedRpcEncoding=JSON` 时，`rpcEncoding=0x01`。在 WebSocket Unframed JSON 中，WebSocket message payload 正好就是 JSON object。

Hello 中的 `axtpVersion` 是 optional advisory diagnostics metadata。它不协商 feature，也不是 AXTP compatibility authority。无论该字段缺失、不是合法 SemVer，还是 major、minor 或 patch 不同，接收方 MUST NOT 因此拒绝或延迟 `Hello -> Identify -> Identified`、改变 retry/reconnect 策略，或影响不相关 RPC 的可用性。实现 MAY 把观测值分类并记录到日志、telemetry 或 diagnostics，但分类结果 MUST NOT 成为 session admission gate。

`protocolVersion`、`rpcVersion` 和 `negotiatedRpcVersion` 是 deprecated compatibility inputs；新发送方 SHOULD 省略它们。接收方 MAY 为兼容性读取它们，但 MUST NOT 把它们变成 session admission gate。

RPC `Hello.axtpVersion` 与 Standard Frame Header 的 `Version` 是不同层次的字段。Frame Header `Version` 是 hard wire parsing boundary；如果接收方无法安全解析该 frame layout，MUST 使用 `FRAME_VERSION_UNSUPPORTED` 拒绝该 frame。这个 frame parsing rule 不得被推导为对 RPC `axtpVersion` 的拒绝规则。

Identify MUST 包含 `randomSeed:uint32`。Logical Server MUST 在生成 `sid` 时把 `randomSeed` 与本地状态混合；它 MUST NOT 直接把 `randomSeed` 当作 `sid`。`randomSeed` 不是认证 secret。

生成新 `sid` 时，AXTP-native Logical Server MUST 避免 `0` 和当前仍有效 RPC Session 的 `sid` 冲突，并 SHOULD 使用大写 8 位 hex 作为 JSON canonical sender form。对象编码 receiver MUST 接受非空字符串形式的已分配 `sid`，包括非 8 位 hex 的 legacy / external 值；它 MUST 按精确字符串匹配 session，不得把 `sid` 当作认证 secret 或用户 token。

Identified 之后，如果 method 的 domain.feature、capability 或 role policy 允许，双方 MAY 发起 RPC Request。这不改变 Hello / Identify / Identified 的逻辑角色，也不改变 CONTROL 的物理角色。

APP_READY 后，JSON / CBOR / MSGPACK envelope 中缺失、非字符串、空字符串或未知 session 的 `sid` MUST 被拒绝；JSON_BINARY 中 zero 或未知 session 的 `sid` MUST 被拒绝。

Standard Framed RPC MUST 在 payload 前添加 `rpcEncoding`。JSON (`0x01`) 是 Phase 1 互操作必需编码。高吞吐或嵌入式 Standard Framed profile SHOULD 实现 JSON_BINARY (`0x04`)。

JSON_BINARY fixed header 为 15B：

```text
rpcEncoding(1) + rpcOp(1) + sid(4) + requestId(4)
  + methodOrEventId(2) + statusCode(2) + bodyEncoding(1)
  + body(N)
```

JSON_BINARY 多字节字段使用 Big-Endian / network byte order。Event 使用 requestId `0`。`bodyEncoding` 的值为 NONE=`0x00`、TLV8=`0x01`、TLV16=`0x02`。

Request/Response matching MUST 使用 RPC request id。Unknown method MUST 返回 RPC error，例如 `RPC_METHOD_NOT_FOUND`；CONTROL MUST NOT 处理业务 method error。

`eventMasks` 编码 domain-scoped event subscription。每个 entry 为 `domainId:uint8 + maskLen:uint8 + bitmask(maskLen)`；bit 0 映射到该 domain 中 registry `bitOffset=0` 的 event。空 mask 或缺失 mask 表示不订阅 event，除非 profile 另有规定。

## STREAM

STREAM 只存在于 Standard Framed profile。WebSocket Unframed JSON 是仅 RPC profile，MUST NOT 承载 STREAM。

STREAM Payload 为：

```text
STREAM Header(16B) + data(N)
```

STREAM Header 布局：

| 字段 | 类型 | 大小 | 规则 |
|---|---|---:|---|
| `streamId` | uint32 | 4B | 非零 stream context id。 |
| `seqId` | uint32 | 4B | stream 内的 packet sequence id。 |
| `cursor` | uint64 | 8B | 由 Stream Context 解释的位置或时间游标。 |

16B STREAM Header 字段 `streamId:uint32`、`seqId:uint32` 和 `cursor:uint64` 使用 Big-Endian / network byte order。`Frame.payloadLength` MUST 等于 `16 + dataLength`。

STREAM Header MUST NOT 携带 codec、file type、firmware metadata、offset field、timestamp field、flag、domain、event 或 capability。业务含义来自已采纳 RPC method 或 profile 创建的 Stream Context。

StreamParser MUST 校验 `payloadLength >= 16`、`streamId != 0`、已知 Stream Context、data size limit 和 profile-specific `seqId` 行为。它 MUST 把 `data` 视为 opaque bytes，并分发给 profile handler。

CONTROL CLOSE、transport loss 或 session teardown MUST 释放该 session 的 Stream Context。

可靠重传、resume、window update、chunk-level CRC 和 object-level verification 是 future/profile-specific 行为，且 MUST 保持 16B STREAM Header 不变。

## 低带宽边界

Compact/HID-64/BLE/UART framing 是低带宽降级路径，不是 AXTP v1 Core 要求。

低带宽 profile MUST 保留：

- PayloadType 值：CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。
- CONTROL 5B payload header。
- 使用 JSON_BINARY 时的 JSON_BINARY 15B fixed header。
- STREAM 16B Header。
- MethodId、EventId、ErrorCode、schema、capability 和 profile registry 语义。

低带宽 profile MUST NOT 为普通 Standard Framed runtime 重新定义 Standard Frame、PayloadType、RPC envelope 或 STREAM Header 语义。
