# NA20/NT10 Device Streaming 需求

## 背景

NA20 和 NT10 是配对使用的投屏设备，目标是提供极简投屏流程。

- NA20：投屏接收端，内置 Wi-Fi 模块作为 AP，通过 USB 把收到的音视频数据交给上位机。
- NT10：投屏发射端，内置 Wi-Fi 模块作为 STA，连接 NA20 后进行推流投屏。

## 用户目标

- 用户将 NA20 插到 PC 上作为投屏接收设备。
- 用户将 NT10 插到 PC 上作为投屏发射端。
- 开始投屏后，NA20 收到 NT10 发出的 H.264 / AAC 音视频流。
- 上位机软件在合适的时机接收、播放或转发这些流数据。

## 范围

- 上位机、NA20、NT10 三方在投屏音视频过程中的业务目标。
- 音频和视频流从设备到上位机的可用性、状态和错误提示。
- 投屏开始、进行中、结束和异常中断时的用户可见状态。
- 接收端状态需要能映射到统一 `receiverPhase`：收到源、准备建流、开始收流、开始渲染、停止或失败。

## 非目标

- 不解决设备升级。
- 不解决 NA20 / NT10 配对流程；配对需求见 `workspace/business/cast-rxtx-pairing.md`。
- 不在 business 文档中定义 STREAM header、payload、method/event/schema 或 HID 细节。

## 场景

- 用户完成 NA20 / NT10 配对后开始投屏。
- NA20 收到 NT10 的 H.264 / AAC 音视频流，并把数据提供给上位机。
- 上位机检测到可播放流后开始播放。
- 投屏断开、设备拔出、带宽不足或音视频异常时，上位机给出明确状态。

## 约束

- Flow 需要区分业务控制面和连续数据面。
- 如果通过 USB HID 承载部分数据，需要在 flow 中明确控制消息、数据流、大小限制和低带宽降级。
- HID/NA20 主流程不要求用户侧密码鉴权；统一阶段里的 `authenticating` 通常跳过。

## 旧协议线索

- [REVIEW-ASK] 待补充现有 HID 数据格式、投屏状态命令、日志和 SDK 行为。

## 开放问题

- [REVIEW-ASK] 音频和视频是否都通过同一条数据通道进入上位机，还是音频单独通过 HID？
- [REVIEW-ASK] 上位机何时判定“可以播放”：收到首帧、收到元数据，还是收到开始事件？
- [REVIEW-ASK] 投屏异常中断时用户需要看到哪些原因？

## 下一步

- 交互和协议覆盖由 `workspace/flows/device-streaming-audio-video.md` 承接。
- Flow 识别出的协议缺口再进入 `video.stream`、`audio.stream` 或相关 protocol 草案。
