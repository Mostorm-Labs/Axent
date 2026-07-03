---
status: generated
contract: true
generated: true
domain: cast
feature: cast.flowControl
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.flowControl

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getFlowControlState`、`cast.setRenderFps`、`cast.setFlowPolicy`、`cast.flowControlChanged` 和低频统计字段外形。 |
| 未采纳 | 外部 keyframe request 维持不暴露；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

控制投屏接收端本地渲染 fps、视频队列上限、late frame 阈值、丢帧策略和 overlay。它不要求 AirPlay source 降低输入 fps，也不定义 STREAM 流控。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getFlowControlState` | 查询本地流控配置和统计。 | `CastGetFlowControlStateParams` -> `CastFlowControlState` | query |
| `cast.setRenderFps` | 设置本地目标渲染 fps。 | `CastSetRenderFpsParams` -> `CastFlowControlState` | command |
| `cast.setFlowPolicy` | 设置队列、late frame、drop policy 和 overlay。 | `CastSetFlowPolicyParams` -> `CastFlowControlState` | command |
| `cast.flowControlChanged` | 流控状态或统计变化。 | `CastFlowControlChangedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getFlowControlState`

返回目标 fps、输入 fps、实际渲染 fps、队列和丢帧统计。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3601,
  "method": "cast.getFlowControlState",
  "params": {
    "includeStats": true,
    "includePolicy": true
  }
}
```

success:

```json
{
  "id": 3601,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10.1,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 2,
    "audioQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "droppedFrames": 240,
    "lateFrames": 18,
    "keyframeRequestCount": 1,
    "sampledAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.setRenderFps`

设置接收端本地目标渲染 fps；`fps=0` 表示不限速。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3602,
  "method": "cast.setRenderFps",
  "params": {
    "fps": 10,
    "scope": "currentSession"
  }
}
```

success:

```json
{
  "id": 3602,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "changedFields": [
      "targetRenderFps"
    ],
    "sampledAt": "2026-06-22T10:31:00Z"
  }
}
```

### 3.3 `cast.setFlowPolicy`

设置视频队列和丢帧策略。默认 `dropMode=drop-late`。

#### 3.3.1 d block 示例

request:

```json
{
  "id": 3603,
  "method": "cast.setFlowPolicy",
  "params": {
    "videoQueueFrames": 3,
    "lateFrameThresholdMs": 120,
    "dropMode": "drop-late",
    "overlayEnabled": true
  }
}
```

success:

```json
{
  "id": 3603,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "keyFrameOnDropBurst": true,
    "changedFields": [
      "videoQueueFrames",
      "lateFrameThresholdMs",
      "dropMode",
      "overlayEnabled"
    ],
    "sampledAt": "2026-06-22T10:32:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `targetRenderFps` | 配置目标；`0` 表示不限速。 |
| `inputFps` | 当前输入 fps，不因本地 cap 自动降低。 |
| `renderFps` | 实际本地渲染 fps；通常不高于 `inputFps` 和非零 `targetRenderFps`。 |
| `dropMode` | `drop-late`、`drop-oldest`、`render-latest`。 |
| `videoQueueFrames` | 视频队列上限。 |
| `lateFrameThresholdMs` | late frame 判定阈值。 |
| `overlayEnabled` | 是否显示诊断 overlay。 |
| `droppedFrames` / `lateFrames` | 低频诊断统计。 |

`cast.flowControlChanged` 只承载低频状态和统计摘要；高频媒体帧不进入该事件。

### 4.1 Event 示例

event:

```json
{
  "event": "cast.flowControlChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "targetRenderFps",
      "renderFps",
      "videoQueueDepth"
    ],
    "state": {
      "targetRenderFps": 10,
      "inputFps": 25.0,
      "renderFps": 10.0,
      "dropMode": "drop-late",
      "videoQueueFrames": 3,
      "videoQueueDepth": 1,
      "droppedFrames": 244,
      "lateFrames": 19,
      "overlayEnabled": true
    },
    "reason": "manualFlowControl",
    "sampledAt": "2026-06-22T10:31:00Z"
  }
}
```

## 5. Rules

- 目标 fps 高于输入 fps 时，`targetRenderFps` 保留配置值，实际 `renderFps` 按输入 fps 运行。
- 手动 fps cap 不得阻塞 HID 输入消费或 AXTP 数据解析。
- 视频队列必须有上限，未渲染帧不得无限排队。
- 外部控制端不直接请求关键帧；内部可根据丢帧、解码和 fps 变化自动触发。
- overlay / diagnostics 是正式能力，但不得变成高频状态流。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | 非法 drop mode 或字段组合。 |
| `OUT_OF_RANGE` | fps、queue 或 threshold 越界。 |
| `UNAVAILABLE` | media / render pipeline 不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 是否暴露外部 keyframe request？ | scope | 不暴露；内部自动触发。 | `[REVIEW-OK]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetFlowControlStateParams

Selector for cast receiver flow control state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `includeStats` | no | `bool` | `0x01` | default=true | Whether to include low-frequency diagnostic counters. |
| `includePolicy` | no | `bool` | `0x02` | default=true | Whether to include current flow policy fields. |
| `sessionId` | no | `string` | `0x03` | maxLength=128 | Optional receiver-local session id. |

### CastFlowControlState

Receiver-local cast flow control policy and low-frequency statistics.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `targetRenderFps` | yes | `number` | `0x01` | min=0 | Configured target render fps; zero means uncapped. |
| `inputFps` | no | `number` | `0x02` | min=0 | Estimated incoming media frame rate. |
| `renderFps` | no | `number` | `0x03` | min=0 | Estimated local render frame rate. |
| `dropMode` | yes | `enum` | `0x04` | enum=drop-late/drop-oldest/render-latest | Local frame drop policy. |
| `videoQueueFrames` | yes | `uint32` | `0x05` | min=1 | Maximum queued video frames. |
| `videoQueueDepth` | no | `uint32` | `0x06` | - | Current queued video frame depth. |
| `audioQueueDepth` | no | `uint32` | `0x07` | - | Current queued audio frame depth when known. |
| `lateFrameThresholdMs` | yes | `uint32` | `0x08` | - | Late-frame threshold in milliseconds. |
| `overlayEnabled` | yes | `bool` | `0x09` | - | Whether diagnostics overlay is enabled. |
| `droppedFrames` | no | `uint64` | `0x0A` | - | Low-frequency dropped-frame counter. |
| `lateFrames` | no | `uint64` | `0x0B` | - | Low-frequency late-frame counter. |
| `keyframeRequestCount` | no | `uint32` | `0x0C` | - | Internal keyframe requests triggered by receiver policy. |
| `keyFrameOnDropBurst` | no | `bool` | `0x0D` | - | Whether the receiver may internally request a keyframe after a drop burst. |
| `changedFields` | no | `Array<string>` | `0x0E` | itemType=string | Field names changed by the latest operation or event. |
| `sampledAt` | no | `string` | `0x0F` | maxLength=64 | Timestamp for this flow sample. |

### CastSetRenderFpsParams

Request to set receiver-local target render fps.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `fps` | yes | `number` | `0x01` | min=0 | Target local render fps; zero means uncapped. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x03` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastSetFlowPolicyParams

Request to update receiver-local queue, late-frame, drop, and overlay policy.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `videoQueueFrames` | no | `uint32` | `0x01` | min=1 | Maximum queued video frames. |
| `lateFrameThresholdMs` | no | `uint32` | `0x02` | - | Late-frame threshold in milliseconds. |
| `dropMode` | no | `enum` | `0x03` | enum=drop-late/drop-oldest/render-latest | Local frame drop policy. |
| `overlayEnabled` | no | `bool` | `0x04` | - | Whether receiver diagnostics overlay is enabled. |
| `sessionId` | no | `string` | `0x05` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x06` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastFlowControlChangedEvent

Event payload for cast flow control changes or low-frequency diagnostic samples.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed or sampled by this event. |
| `state` | yes | `CastFlowControlState` | `0x02` | - | Flow control state after the change or sample. |
| `reason` | no | `enum` | `0x03` | enum=manualFlowControl/diagnosticsSample/sessionStarted/sessionStopped/unknown | Change or sampling reason. |
| `sampledAt` | no | `string` | `0x04` | maxLength=64 | Timestamp for this event. |

### CastFlowControlCapability

Capability descriptor for cast.flowControl.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `supportsRenderFps` | no | `bool` | `0x01` | default=true | Whether receiver-local target render fps can be controlled. |
| `supportsQueuePolicy` | no | `bool` | `0x02` | default=true | Whether queue and late-frame policy can be controlled. |
| `supportsOverlay` | no | `bool` | `0x03` | default=true | Whether diagnostics overlay can be controlled. |
| `supportsStats` | no | `bool` | `0x04` | default=true | Whether low-frequency flow diagnostics are reported. |
| `exposesExternalKeyframeRequest` | no | `bool` | `0x05` | default=false | Whether a public keyframe request method is exposed; current draft keeps this false. |
