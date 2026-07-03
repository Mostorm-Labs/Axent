# Event Registry

| eventId | name | domain | status | schema |
|---:|---|---|---|---|
| `0x0102` | `device.enrollmentStateChanged` | device | draft | DeviceEnrollmentStateChangedEvent |
| `0x0402` | `firmware.updateProgressReported` | firmware | draft | FirmwareUpdateProgressEvent |
| `0x0403` | `firmware.updateStateChanged` | firmware | draft | FirmwareUpdateStateChangedEvent |
| `0x0806` | `video.streamStateChanged` | video | draft | VideoStreamStateChangedEvent |
| `0x0807` | `video.streamSourceStateChanged` | video | draft | VideoStreamSourceStateChangedEvent |
| `0x0808` | `video.streamStatsReported` | video | draft | VideoStreamStatsReportedEvent |
| `0x0901` | `audio.algorithmConfigChanged` | audio | stable | AudioAlgorithmConfigChangedEvent |
| `0x0902` | `audio.streamStateChanged` | audio | draft | AudioStreamStateChangedEvent |
| `0x0903` | `audio.streamSourceStateChanged` | audio | draft | AudioStreamSourceStateChangedEvent |
| `0x0904` | `audio.streamStatsReported` | audio | draft | AudioStreamStatsReportedEvent |
| `0x0D01` | `signage.playlistConfigChanged` | signage | draft | SignagePlaylistConfigChangedEvent |
| `0x0E01` | `network.interfaceStateChanged` | network | draft | NetworkInterfaceStateChangedEvent |
| `0x0E02` | `network.ipConfigChanged` | network | draft | NetworkIpConfigChangedEvent |
| `0x0E03` | `network.wifiConfigChanged` | network | draft | NetworkWifiConfigChangedEvent |
| `0x0E04` | `network.wifiStateChanged` | network | draft | NetworkWifiStateChangedEvent |
| `0x0E05` | `network.wifiScanResultReported` | network | draft | NetworkWifiScanResultReportedEvent |
| `0x0E06` | `network.apConfigChanged` | network | draft | NetworkApConfigChangedEvent |
| `0x0E07` | `network.apStateChanged` | network | draft | NetworkApStateChangedEvent |
| `0x0E08` | `network.apClientChanged` | network | draft | NetworkApClientChangedEvent |
| `0x1601` | `cast.sessionIncoming` | cast | draft | CastSessionIncomingEvent |
| `0x1602` | `cast.sessionStateChanged` | cast | draft | CastSessionStateChangedEvent |
| `0x1603` | `cast.sessionStarted` | cast | draft | CastSessionStartedEvent |
| `0x1604` | `cast.sessionStopped` | cast | draft | CastSessionStoppedEvent |
| `0x1605` | `cast.sessionFailed` | cast | draft | CastSessionFailedEvent |
| `0x1606` | `cast.audioChanged` | cast | draft | CastAudioChangedEvent |
| `0x1607` | `cast.pinCodeChanged` | cast | draft | CastPinCodeChangedEvent |
| `0x1608` | `cast.pinCodeRequired` | cast | draft | CastPinCodeRequiredEvent |
| `0x1609` | `cast.pinCodeAuthFailed` | cast | draft | CastPinCodeAuthFailedEvent |
| `0x160A` | `cast.windowChanged` | cast | draft | CastWindowChangedEvent |
| `0x160B` | `cast.backendChanged` | cast | draft | CastBackendChangedEvent |
| `0x160C` | `cast.flowControlChanged` | cast | draft | CastFlowControlChangedEvent |
| `0x1701` | `software.configChanged` | software | draft | SoftwareConfigChangedEvent |
| `0x1702` | `software.updatePolicyChanged` | software | draft | SoftwareUpdatePolicyChangedEvent |
