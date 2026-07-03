# AXTP Conformance

AXTP conformance 用例维护在根目录 `conformance/` 下，是 runtime、SDK、mock server 和工具仓库共享的行为验收输入。runtime 不应重新定义这些用例，而应加载本目录，并根据自己声明的 support level 执行匹配 case。

> Runtime 支持等级、profile 声明和 `AXTP_CONFORMANCE.yaml` 示例见 [Conformance Levels](CONFORMANCE_LEVELS.md)。
> Runtime 实现清单和常见排障见 [Runtime / SDK Guide](../docs/guides/runtime.md)。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `manifest.yaml` | 声明 conformance levels 以及每个 level 的 required cases。 |
| `schemas/` | case 和 result 的 JSON Schema。 |
| `profiles/` | profile 级验收期望。 |
| `fixtures/` | 协议行为用例使用的设备画像。 |
| `cases/` | YAML 格式的用例描述。 |

## Phase 1 MVP Scope

| 能力 | WebSocket JSON runtime | Standard Framed runtime |
| --- | ---: | ---: |
| RPC Hello / Identify / Identified | 是 | 是 |
| RPC Request / RequestResponse / Event | 是 | 是 |
| `sid` 固定 8 位 hex string / binary `uint32` | 是 | 是 |
| `requestId` 匹配未完成请求 | 是 | 是 |
| 标准错误响应形状 | 是 | 是 |
| Standard Frame Header / CRC16 | 否 | 是 |
| CONTROL OPEN / ACCEPT | 否 | 是 |
| CONTROL HEARTBEAT / HEARTBEAT_ACK | 否 | 是 |
| CONTROL CLOSE / CLOSE_ACK | 否 | 是 |
| STREAM open / data / close | 否 | 是，P0 用于 audio/video 媒体流 |

Hello / Identify / Identified 字段规则不在 conformance 文档中重复定义；完整规范见 [RPC Session Spec](../specs/20-core.md)。

## Required Levels

| runtime 类型 | 最小声明 level | 可选增强 level |
| --- | --- | --- |
| WebSocket JSON | `core`、`websocket-jsonrpc` | `capability`、`event` |
| Standard Framed | `core`、`framed-binary` | `capability`、`event`、`stream` |
| mock server | 按声明 profile：WS-only mock 使用 `core`、`websocket-jsonrpc`；当前 Node TCP mock-server 使用 `core`、`framed-binary` | `capability`、`event`、`stream` |
| 固件 / MCU | `core`、`framed-binary` | `stream` 或后续低带宽 profile |

runtime 必须显式声明自己支持的 level。某 case 属于未声明 level 时，可以不执行；属于已声明 level 时，不应在 runner 内偷偷 skip。

mock server 不再被固定为 WebSocket JSON。测试时先读取 mock server 的 profile 声明：当前作为跨 runtime 基准的 generated Node mock-server 应覆盖 TCP Standard Framed 的 `core + framed-binary`；只提供 RPC-only WebSocket 控制面的 mock 才声明 `websocket-jsonrpc`。

推荐 runtime 在自己的 conformance 配置或测试命令中声明 level，例如：

```yaml
axtp_conformance:
  levels:
    - core
    - websocket-jsonrpc
```

Standard Framed runtime 的最小声明应为：

```yaml
axtp_conformance:
  levels:
    - core
    - framed-binary
```

## WS-JSON 必测项

声明 `websocket-jsonrpc` 的 runtime 至少覆盖：

| 行为 | Required cases |
|---|---|
| Hello / Identify / Identified 顺序 | `session.hello_identify_identified` |
| Identified 前拒绝业务请求 | `session.request_before_identified` |
| JSON Request / RequestResponse | `rpc.request_response_json` |
| 未知 method | `rpc.method_not_found` |
| `requestId` 匹配 | `rpc.request_id_match` |

WebSocket JSON 不承载 CONTROL、Frame Header、CRC16 或 STREAM data packet。

## Standard Framed 必测项

声明 `framed-binary` 的 runtime 至少覆盖：

| 行为 | Required cases |
|---|---|
| CONTROL OPEN / ACCEPT | `handshake.open_accept` |
| CONTROL HEARTBEAT / HEARTBEAT_ACK | `handshake.heartbeat` |
| CONTROL CLOSE / CLOSE_ACK | `handshake.close` |
| RPC `requestId` 匹配 | `rpc.request_id_match` |
| STREAM open / data / close | `stream.stream_open`、`stream.stream_data`、`stream.stream_close` |

Standard Framed runtime 同时需要通过 `core` level 的 RPC 和错误形状 case。

## STREAM P0 范围

Phase 1 STREAM conformance 验收的是 audio/video 媒体流数据面：

| 内容 | Phase 1 期望 |
|---|---|
| 建流 / 关流 | 通过已采纳或测试 profile 中的业务 RPC 创建 / 关闭 Stream Context。 |
| STREAM Header | 使用 16B header：`streamId:uint32`、`seqId:uint32`、`cursor:uint64`。 |
| STREAM data | Standard Framed `PayloadType=STREAM`，不走 WebSocket JSON。 |
| ACK/NACK | 不作为 Phase 1 strict retransmission 前置要求。 |

`stream.stream_open` / `stream.stream_close` 验收的是 P0 媒体流业务域建流，不是要求存在通用 `stream.open` / `stream.close` 业务 method。

## Deferred 范围

Phase 1 暂不验收：

- ACK/NACK 严格重传。
- RESUME / SESSION_RESET。
- PING/PONG RTT 测量。
- STREAM 固件 / 文件 / 日志 profile。
- 链路加密、证书、token 或签名协商。
- 低带宽 BLE / UART / HID-64 降级 profile。

## 运行命令

主库验证 conformance 源文件：

```bash
pnpm --dir tooling/generators install --frozen-lockfile
pnpm --dir tooling/generators build
tooling/scripts/validate-conformance.sh
```

runtime 仓库接入时应设置：

```bash
export AXTP_SPEC_PATH=/path/to/axtp-spec-or-release-artifact
```

runtime runner 应读取 canonical conformance 路径：

```text
$AXTP_SPEC_PATH/conformance
```

## 失败分类

| 分类 | 判断方式 | 处理 |
|---|---|---|
| runtime bug | case 与 spec/generated 一致，runtime 行为不符合 | 修 runtime。 |
| spec-case mismatch | case 与 `specs/**` 或 `contract/protocol/axtp.protocol.yaml` 冲突 | 先修主库 conformance case 或 spec。 |
| generated mismatch | case 依赖 method/event/schema，但 generated 中不存在 | 回到 registry / Generator，不手写 generated。 |
| profile declaration error | runtime 未实现某 level 却声明支持 | 调整 runtime support level 或补实现。 |
| test environment | 设备画像、transport、mock 配置或 spec path 错误 | 修 runner 配置和 fixtures。 |

若 case 与 Phase 1 裁决冲突，应先修正主库 conformance case 和 manifest，再要求 runtime 通过。
