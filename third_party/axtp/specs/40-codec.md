# AXTP 编码

本文定义 AXTP 数据类型、schema 模型、capability 模型、TLV 编码和 schema 字段编号。当前 schema 事实位于 `contract/registry/**` 和生成产物中。

## 类型系统

所有 AXTP 多字节 wire integer MUST 使用 Big-Endian / network byte order。

| 类型 | JSON 表达 | Binary/TLV 表达 | 规则 |
|---|---|---|---|
| `bool` | boolean | 1B | `0x00=false`，`0x01=true`；其他值 SHOULD 校验失败。 |
| `uint8/16/32/64` | number；大值可用 string | 固定宽度 unsigned BE | 范围 MUST 匹配类型宽度和 schema constraints。 |
| `int8/16/32/64` | number；大值可用 string | 固定宽度 signed BE | 二进制补码有符号整数。 |
| `string` | string | UTF-8 bytes | 无 NUL terminator；长度来自外层编码。 |
| `bytes` | base64/hex/profile-specific | raw bytes | Opaque bytes；schema/profile 限制长度。 |
| `enum` | string 或 numeric value | enum8/enum16/profile-specific | 合法值 MUST 已声明。 |
| `bitmap` | array/string/numeric profile-specific | bitmap8/16/32/64/bytes | bit0 是最低有效位。 |
| `array<T>` | JSON array | repeated 或 packed | Element type MUST 已声明。 |
| `object` | JSON object | nested TLV fields | 字段含义来自 schema。 |

AXTP v1 TLV 没有独立 null sentinel。缺失值使用 optional/default 表达。JSON `null` 只在 schema 显式允许时可用。

稳定字段的 type、unit、required/optional 语义和 range MUST NOT 发生不兼容变化。

JSON、TLV 和生成 SDK 视图是同一个 schema fact 的不同投影。它们 MUST 保留相同的 required/optional 状态、default 语义、enum values、array item type、range、unit 和 deprecation metadata。无法在声明编码之间一致表达的字段 MUST 被校验拒绝，或标记为 profile-specific。

## Schema 模型

Object schema fields MUST 拥有 schema-local `fieldId`。Field id 不是全局 ID。

最小 object 形状：

```yaml
schemas:
  ExampleLevelConfig:
    kind: object
    fields:
      - id: 0x01
        name: level
        type: uint8
        required: true
        min: 0
        max: 100
      - id: 0x02
        name: applyDelayMs
        type: uint16
        required: false
        default: 0
```

Schema 规则：

1. Required fields MUST present，即必需字段必须出现在编码数据中。
2. Optional fields MAY omitted；接收方只在 schema 声明 default 时应用默认值。
3. Unknown optional fields MUST 可跳过。
4. Object fields 引用 built-in types 或 named schemas。
5. Array fields MUST 声明 item type，并在适用时声明 item schema。
6. Empty request/response 使用已注册的 Empty schema。
7. 生成 SDK SHOULD 区分 wire types 和 host-language types。

## Capability 模型

Capability 描述设备在当前 firmware、configuration、session 和 authorization state 下支持什么。Capability 不改变 methodId、eventId、PayloadType、CONTROL、RPC 或 STREAM wire shape。

Capability 规则：

1. Capability names SHOULD 使用 `domain.feature`。
2. Capability schema MAY 描述 limits、supported modes、ranges、defaults、update policy 和 availability。
3. Capability references MUST 解析到已注册的 method/event/schema/profile facts。
4. Capability discovery 如果被采纳，是 RPC business method；它不是 CONTROL。
5. Unknown optional capability fields MUST 按 SDK policy 忽略或保留。

完整 capability reflection、capability trees 和复杂 profile negotiation 属于 future 或单独采纳的业务协议，除非已经出现在 generated contract 中。

## TLV 编码

TLV body encoding 使用 field id、length 和 raw value：

```text
TLV8  = fieldId:uint8 + length:uint8 + value(length)
TLV16 = fieldId:uint16 + length:uint16 + value(length)
```

CONTROL 使用 short TLV，并支持 optional extended length marker：

```text
type:uint8 + length:uint8 + value
type:uint8 + 0xFF + extendedLength:uint16 + value
```

TLV 规则：

1. Field order SHOULD 为 canonical generation 按升序排列。
2. 接收方 MUST 在读取 value bytes 前校验 length。
3. Unknown fields 只要 length 有效，MUST 被跳过。
4. Duplicate non-repeated fields SHOULD 在 strict validation 下失败。
5. Repeated fields MUST 由 schema/profile 显式声明。
6. TLV object decoding 使用 schema-local field ids；field names 不在 wire 上。
7. Packed array encoding 只在 schema/profile 定义 element width 和 layout 时允许。

## 字段编号

Schema field id 规则：

1. `fieldId=0x00` 保留，MUST NOT 用于普通字段。
2. 对 TLV8-compatible object fields，fieldId MUST 是 `0x01..0xFE`。
3. `0xFF` 在 short TLV contexts 中保留给 extended length marker。
4. Field id MUST 在 schema 内唯一。
5. Stable field id MUST NOT 被复用于不同名称或含义。
6. Deprecated fields 保留其 id，直到 compatibility policy 移除它们。
7. New fields SHOULD 默认为 optional，除非声明 breaking version boundary。
8. Rename SHOULD 使用新字段加 deprecated old field；禁止 silent replacement。

兼容性规则：

| 变更 | 默认兼容性 |
|---|---|
| 新增 optional field | 兼容。 |
| 新增 required field | 通常 breaking。 |
| 新增 enum/bitmap value | 只有定义 unknown-value policy 时兼容。 |
| 修改 type/unit/range/requiredness | 通常 breaking。 |
| 复用已移除的 field id | 对 stable contracts 禁止。 |

Runtime 或 generated codec MUST 校验 numeric ranges、UTF-8 strings、enum/bitmap values、required fields、object recursion、array element types 和 length boundaries。

Unknown-field policy：

- Unknown TLV fields 只要 length 有效，MUST 被 forward-compatible decoder 跳过。
- Unknown enum 或 bitmap values 在可能时 MUST 为诊断保留；strict business validation MAY 在 decode 后拒绝。
- Defaults 只在 successful decode 后应用，并且只在 schema 声明 default 时应用。
- Deprecated fields MAY 为兼容性发出，但新发送方 SHOULD NOT 依赖它们。
