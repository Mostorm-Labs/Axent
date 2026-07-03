#include "device_accessor_impl.h"
#include "protocol_defines.h"
#include "axdp_defines.h"
#include "axdp_utils.h"
#include "dfu_helper.h"
#include "log_utils.h"
#include "hid_device.h"
#include "human_interface_handler.h"
#include <chrono>
#include <thread>

namespace axdp {
#define CMD_RESP_SUCCESS 0

    static int32_t convert(const ProtocolMessage &msg) {
        auto &head = msg.header();
        auto &payload = msg.payload();
        int32_t result = 0;
        if (head.len() == 4) //int resp
        {
            result = utils::ntohl(*(uint32_t *) payload.data());
        } else if (head.len() == 2) {
            result = utils::ntohs(*(uint16_t *) payload.data());
        } else if (head.len() == 1) {
            result = *payload.data();
        }
        return result;
    }

    static TestTaskResult convertResult(int32_t result) {
        TestTaskResult res = TestTaskResult::ResultUnknown;
        switch (result)
        {
        case -1:
            res = TestTaskResult::ResultFailed;
            break;
        case 0:
            res = TestTaskResult::ResultSuccess;
            break;
        default:
            res = TestTaskResult::ResultUnknown;
            break;
        }
        return res;
    }


    int DeviceAccessorImpl::handleReceivedHumanInterfaceInputReport(const uint8_t *buffer,
                                                                    size_t length) {
        if (hid_report_event_handler_.get()) {
            return hid_report_event_handler_->onInputReport(buffer, length);
        }
        return 0;
    }

    int DeviceAccessorImpl::handleReceivedInternalProtocolMessage(const ProtocolMessage &msg) {
        auto &head = msg.header();
        auto &payload = msg.payload();
        auto result = convert(msg);
        UniCmd cmd = addMask(head.cmd(), ProtocolType::Common);
        switch (cmd) {
            case axdp::UniCmd::CommonSetVideoMode:
                break;
            case axdp::UniCmd::CommonGetVideoMode: {
                if (cb_) cb_->onGetVideoMode(VideoMode(result));
                break;
            }
            case axdp::UniCmd::CommonGetPeopleNumber: {
                //if (cb_) cb_->onGetPeopleCount(result);
                break;
            }
            case axdp::UniCmd::CommonSetMicUsed:
                break;
            case axdp::UniCmd::CommonAudioRecord:
                break;
            case axdp::UniCmd::CommonSetTestResult:
                break;
            case axdp::UniCmd::CommonGetTestResult:
                break;
            case axdp::UniCmd::CommonSetEncryptedInfo:
                break;
            case axdp::UniCmd::CommonGetEncryptedInfo:
                break;
            case axdp::UniCmd::CommonGetGyroSlopeAngle: {
                auto *cb = dynamic_cast<EventCallbackDelegateEx *>(cb_);
                if (cb) cb->onGetGyroSlopeAngle(result);
                break;
            }
            case axdp::UniCmd::CommonSetUacState:
                break;
            case axdp::UniCmd::CommonGetUacState: {
                if (cb_) cb_->onGetUacState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonTestAudioConsistency:
                break;
            case axdp::UniCmd::CommonTestNetworkPort:
                break;
            case axdp::UniCmd::CommonSetVideoTrackMode:
                break;
            case axdp::UniCmd::CommonGetVideoTrackMode: {
                if (cb_) cb_->onGetVideoTrackMode(VideoTrackMode(result));
                break;
            }
            case axdp::UniCmd::CommonSetMirrorState:
                break;
            case axdp::UniCmd::CommonGetMirrorState: {
                if (cb_) cb_->onGetMirrorState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetSpeakerTrackDelay:
                break;
            case axdp::UniCmd::CommonGetSpeakerTrackDelay: {
                if (cb_) cb_->onGetSpeakerTrackDelay(result);
                break;
            }
            case axdp::UniCmd::CommonSetSplitScreenNumber:
                break;
            case axdp::UniCmd::CommonGetSplitScreenNumber: {
                if (cb_) cb_->onGetSplitScreenNumber(result);
                break;
            }
            case axdp::UniCmd::CommonSetUsbName:
                break;
            case axdp::UniCmd::CommonGetUsbName:
                break;
            case axdp::UniCmd::CommonSetUsbPid:
                break;
            case axdp::UniCmd::CommonGetUsbPid:
                break;
            case axdp::UniCmd::CommonSetUsbVid:
                break;
            case axdp::UniCmd::CommonGetUsbVid:
                break;
            case axdp::UniCmd::CommonSetReboot: {
                //if (cb_) cb_->onRebootDevice(result);
                break;
            }
            case axdp::UniCmd::CommonSetPowerLineFreq:
                break;
            case axdp::UniCmd::CommonGetPowerLineFreq: {
                if (cb_) cb_->onGetPowerLineFreq(PowerlineFreqType(result));
                break;
            }
            case axdp::UniCmd::CommonGetDeviceUniqueId:
                break;
            case axdp::UniCmd::CommonSetDeviceUniqueId:
                break;
            case axdp::UniCmd::CommonGetAlgAuthContent:
                break;
            case axdp::UniCmd::CommonSetAlgAuthContent:
                break;
            case axdp::UniCmd::CommonSetWdrState:
                break;
            case axdp::UniCmd::CommonGetWdrState: {
                if (cb_) cb_->onGetWdrState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetOsdMirrorState:
                break;
            case axdp::UniCmd::CommonGetOsdMirrorState: {
                if (cb_) cb_->onGetOsdMirrorState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetAFCalibration:
                break;
            case axdp::UniCmd::CommonStartAudioTest:
                break;
            case axdp::UniCmd::CommonSetFlipState:
                break;
            case axdp::UniCmd::CommonGetFlipState: {
                if (cb_) cb_->onGetFlipState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetWatermark: {
                if (cb_) cb_->onGetWatermark(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetPiPMode: {
                if (cb_) cb_->onGetPiPMode(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetNoiseSuppressionLevel: {
                if (cb_ && head.src() == 2){
                    cb_->onGetNoiseSuppressionLevel(DegreeLevel(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetNoiseSuppressionLevel:
                break;
            case axdp::UniCmd::CommonGetBootDetect:
                break;
            case axdp::UniCmd::CommonGetReverberationSuppressionLevel: {
                if (cb_ && head.src() == 2){
                    cb_->onGetReverbrationSuppressionLevel(DegreeLevel(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetReverberationSuppressionLevel:
                break;
            case axdp::UniCmd::CommonGetEchoCancellationLevel: {
                if (cb_ && head.src() == 2)
                    cb_->onGetEchoCancellationLevel(DegreeLevel(result));
                break;
            }
            case axdp::UniCmd::CommonSetEchoCancellationLevel:
                break;
            case axdp::UniCmd::CommonGetRecordEqParams:
                break;
            case axdp::UniCmd::CommonSetRecordEqParams:
                break;
            case axdp::UniCmd::CommonGetDefaultNoiseSuppressionLevel: {
                //if (cb_ && head.src() == 2){
                //  cb_->onGetDefaultNoiseSuppressionLevel(result);
                //}
                break;
            }
            case axdp::UniCmd::CommonSetDefaultNoiseSuppressionLevel:
                break;
            case axdp::UniCmd::CommonGetDefaultReverberationSuppressionLevel: {
                //if (cb_ && head.src() == 2){
                    //cb_->onGetDefaultReverberationEliminationLevel(result);
                //}
                break;
            }
            case axdp::UniCmd::CommonSetDefaultReverberationSuppressionLevel:
                break;
            case axdp::UniCmd::CommonGetDefaultEchoCancellationLevel: {
                //if (cb_ && head.src() == 2){
                    //cb_->onGetDefaultEchoEliminationLevel(result);
                //}
                break;
            }
            case axdp::UniCmd::CommonSetDefaultEchoCancellationLevel:
                break;
            case axdp::UniCmd::CommonGetDefaultRecordEqParams:
                break;
            case axdp::UniCmd::CommonSetDefaultRecordEqParams:
                break;
            case axdp::UniCmd::CommonResetAudioAlgorithmParams: {
                //if (cb_ && head.src() == 2){
                //  cb_->onGetDefaultEchoEliminationLevel(result);
                //}
                break;
            }
            case axdp::UniCmd::CommonGetMuteLightEnhancement: {
                if (cb_ && head.src() == 2)
                    cb_->onGetMuteLightEnhancement(result);
                break;
            }
            case axdp::UniCmd::CommonSetMuteLightEnhancement:
                break;
            case axdp::UniCmd::CommonAudioRecordStart: {
                AudioFormat audio_format = {0};
                const uint8_t *pv = payload.data();
                uint16_t len = head.len();//payload.size();
                uint8_t support_record = pv[0];
                audio_format.audio_type = pv[1];
                audio_format.sample_rate = utils::ntohl(*(uint32_t *) (pv + 2));
                audio_format.channel_num = pv[6];
                audio_format.bit_width = pv[7];
                if (cb_) {
                    cb_->onAudioRecordStart(support_record > 0, &audio_format);
                }
                break;
            }
            case axdp::UniCmd::CommonAudioRecordStop: {
                uint16_t len = head.len();
                uint16_t *pv = (uint16_t *) payload.data();
                uint16_t data_len = utils::ntohs(*pv);
                if (cb_) {
                    cb_->onAudioRecordStopped(data_len);
                }
                break;
            }
            case axdp::UniCmd::CommonAudioRecordData: {
                uint16_t len = head.len();
                if (cb_) {
                    cb_->onAudioRecordData(payload.data(), len);
                }
                break;
            }
            case axdp::UniCmd::CommonGetAFCalibration:
                break;
            case axdp::UniCmd::CommonGetDDRCapacity:
                break;
            case axdp::UniCmd::CommonSetPanTiltZoom:
                break;
            case axdp::UniCmd::CommonGetPanTiltZoom: {
                uint32_t *pv = (uint32_t *) payload.data();
                uint32_t pan = utils::ntohl(pv[0]);
                uint32_t tilt = utils::ntohl(pv[1]);
                uint32_t zoom = utils::ntohl(pv[2]);
                if (cb_)
                {
                    //cb_->onGetPanTiltZoom(pan, tilt, zoom);
                }
                break;
            }
            case axdp::UniCmd::CommonGetDumpInfo: {
                if (cb_)
                {
                    //cb_->onGetDumpInfo(payload.data(), head.len());
                }
                break;
            }
            case axdp::UniCmd::CommonSetAlgoEnable:
                break;
            case axdp::UniCmd::CommonGetAlgoEnable: {
                if (cb_)
                {
                    //cb_->onGetAlgoEnable(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetHidCall: {
                if (cb_){
                    //cb_->onGetHidCallState(result == 1?EnableState::Enabled:EnableState::Disabled);
                }
                break;
            }
            case axdp::UniCmd::CommonGetHidCall: {
                if (cb_){
                    cb_->onGetHidCallState(result == 1?EnableState::Enabled:EnableState::Disabled);
                }
                break;
            }
            case axdp::UniCmd::CommonSetUsbSpeedMode:
                break;
            case axdp::UniCmd::CommonGetUsbSpeedMode: {
                if (cb_) {
                    cb_->onGetUsbSpeedMode(result);
                }
                break;
            }
            case axdp::UniCmd::CommonGetBootInfo: {
                if (cb_) {
                    //cb_->onGetBootInfo(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetBootErase:
                break;
            case axdp::UniCmd::CommonSetPrivacyEnable:
                break;
            case axdp::UniCmd::CommonGetPrivacyEnable: {
                if (cb_) {
                    cb_->onGetPrivacyEnable(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetNDIState:
                break;
            case axdp::UniCmd::CommonGetNDIState: {
                if (cb_) {
                    //cb_->onGetNDIState(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonGetIPConfig: {
                if (cb_) {
                    cb_->onGetIPConfig(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetDHCPState:
                break;
            case axdp::UniCmd::CommonGetDHCPState: {
                uint8_t *pv = (uint8_t *) payload.data();
                uint8_t idx = pv[0];
                uint8_t dhcp_state = pv[1];
                if (cb_) {
                    cb_->onGetDHCPState(idx, EnableState(dhcp_state));
                }
                break;
            }
            case axdp::UniCmd::CommonSetIPAddress:
                break;
            case axdp::UniCmd::CommonGetIPAddress: {
                uint8_t *pv = (uint8_t *) payload.data();
                uint8_t idx = pv[0];
                uint32_t ip_addr = *(uint32_t *) (pv + 1);
                if (cb_) {
                    cb_->onGetIPAddress(idx, ip_addr);
                }
                break;
            }
            case axdp::UniCmd::CommonSetNetMask:
                break;
            case axdp::UniCmd::CommonGetNetMask: {
                uint8_t *pv = (uint8_t *) payload.data();
                uint8_t idx = pv[0];
                uint32_t net_mask = *(uint32_t *) (pv + 1);
                if (cb_) {
                    cb_->onGetNetMask(idx, net_mask);
                }
                break;
            }
            case axdp::UniCmd::CommonSetGateway:
                break;
            case axdp::UniCmd::CommonGetGateway: {
                uint8_t *pv = (uint8_t *) payload.data();
                uint8_t idx = pv[0];
                uint32_t gateway = *(uint32_t *) (pv + 1);
                if (cb_) {
                    cb_->onGetGateway(idx, gateway);
                }
                break;
            }
            case axdp::UniCmd::CommonSetMacAddress:
                break;
            case axdp::UniCmd::CommonGetMacAddress: {
                uint8_t *pv = (uint8_t *) payload.data();
                uint8_t idx = pv[0];
                uint64_t mac_addr = 0;
                memcpy(&mac_addr, pv + 1, 6);
                if (cb_) {
                    cb_->onGetMacAddress(idx, mac_addr);
                }
                break;
            }
            case axdp::UniCmd::CommonSetLensCenter:
                break;
            case axdp::UniCmd::CommonSetConfigJson:{
                if (cb_) cb_->onSetConfigJson(result, head.src());
                break;
            }
            case axdp::UniCmd::CommonGetConfigJson: {
                if (task_state_mgr_.isTaskAsync(UniCmd::CommonGetConfigJson)) {
                    if (cb_) cb_->onGetConfigJson(payload.data(), head.len(), head.src());
                }
                else {
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        config_json_.clear();
                        config_json_.assign((const char*)payload.data(), head.len());
                        task_state_mgr_.setTaskState(UniCmd::CommonGetConfigJson, TaskState::SyncDone);
                    }
                    cond_var_.notify_all();
                }
                break;
            }
            case axdp::UniCmd::CommonSetFbfParamsStart:

            case axdp::UniCmd::CommonSetFbfParamsData: {
                char* str = strstr((char*)payload.data(), "\"errno\":0");
                if (str != nullptr) {
                    if (fbf_helper_.isAllDevicesReady(head.src())) {
                        uint8_t buf[4096];
                        auto derivedPtr = std::dynamic_pointer_cast<HidDevice>(device_);
                        int max_data_len =
                            std::min(derivedPtr->getOutputReportLen(), (int32_t)sizeof(buf)) - 16;
                        int len = fbf_helper_.getCurrentBuffer((char*)buf, max_data_len);
                        if (len) {
                            sendFbfParamData(buf, len, fbf_helper_.getFbfDeviceDst());
                        } else {
                            setFbfParamEnd(fbf_helper_.getFbfDeviceDst());
                        }
                        if (cmd == axdp::UniCmd::CommonSetFbfParamsStart) {
                            if (cb_) cb_->onSetFbfParamState(UpgradeState::Transferring,
                                                          fbf_helper_.getFbfDeviceDst());
                        }
                    }
                } else {
                    if (cmd == axdp::UniCmd::CommonSetFbfParamsData) {
                        if (cb_) cb_->onSetFbfParamState(UpgradeState::Failed,
                                                      fbf_helper_.getFbfDeviceDst());
                    }
                    setFbfParamEnd(fbf_helper_.getFbfDeviceDst());
                    fbf_helper_.fbfEnd();
                }
                break;
            }
            case axdp::UniCmd::CommonSetFbfParamsStop: {
                char* str = strstr((char*)payload.data(), "\"errno\":0");
                if (str != nullptr) {
                    if (fbf_helper_.isAllDevicesReady(head.src())) {
                        if (cb_) {
                            cb_->onSetFbfParamState(UpgradeState::Success, fbf_helper_.getFbfDeviceDst());
                        }
                        fbf_helper_.fbfEnd();
                    }
                } else {
                    if (cb_) cb_->onSetFbfParamState(UpgradeState::Failed,
                                                      fbf_helper_.getFbfDeviceDst());

                    fbf_helper_.fbfEnd();
                }
                break;
            }
            case axdp::UniCmd::CommonGetFbfParams: {
                if (cb_) {
                    cb_->onGetFbfParam(payload.data(), head.len(), head.src());
                }
                break;
            }
            case axdp::UniCmd::CommonGetRegionTracking: {
                if (cb_) {
                    //cb_->onGetConfigJson(head.src(), payload.data(), head.len());
                }
                break;
            }
            case axdp::UniCmd::CommonSetRegionTracking:
                break;
            case axdp::UniCmd::CommonPauseAiAlgThrd:
                break;
            case axdp::UniCmd::CommonContinueAiAlgThrd:
                break;
            case axdp::UniCmd::CommonSetNoTargetStrategyState:
                break;
            case axdp::UniCmd::CommonGetNoTargetStrategyState: {
                if (cb_) {
                    //cb_->onGetNoTargetStrategyState(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetStartupPosition:
                break;
            case axdp::UniCmd::CommonGetStartupPosition: {
                if (cb_) {
                    cb_->onGetStartupPosition(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetMenuBarLanguage:
                break;
            case axdp::UniCmd::CommonGetMenuBarLanguage: {
                if (cb_) {
                    //cb_->onGetMenuBarLanguage(result);
                }
                break;
            }
            case axdp::UniCmd::CommonGetRS232TestResult:
                break;
            case axdp::UniCmd::CommonTestKey:
                break;
            case axdp::UniCmd::CommonTestLed:
                break;
            case axdp::UniCmd::CommonSetDebugJson:
                break;
            case axdp::UniCmd::CommonGetDebugJson:
                break;
            case axdp::UniCmd::CommonSetViscaUdpPort:
                break;
            case axdp::UniCmd::CommonGetViscaUdpPort: {
                if (cb_) {
                    cb_->onGetViscaUdpPort(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetHorTrackingStrategy:
                break;
            case axdp::UniCmd::CommonGetHorTrackingStrategy: {
                if (cb_) {
                    //cb_->onGetHorTrackingStrategy(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetVerTrackingStrategy:
                break;
            case axdp::UniCmd::CommonGetVerTrackingStrategy: {
                if (cb_) {
                    //cb_->onGetVerTrackingStrategy(result);
                }
                break;
            }
            case axdp::UniCmd::CommonGetImageStyle: {
                if (cb_) {
                    cb_->onGetImageStyle(result);
                }
                break;
            }
            case axdp::UniCmd::CommonGetAudioMuteState: {
                if (cb_) {
                    char* str = strstr((char*)payload.data(), "\"mute\":1");
                    cb_->onGetAudioMuteState(str != nullptr, head.src());
                }
                break;
            }
            case axdp::UniCmd::CommonSetDefaultConfigJson: {
                break;
            }
            case axdp::UniCmd::CommonGetDefaultConfigJson: {
                if (cb_) {
                    cb_->onGetDefaultConfigJson(payload.data(), head.len(), head.src());
                }
                break;
            }
            case axdp::UniCmd::CommonSetRTSPStreamURL: {
                break;
            }
            case axdp::UniCmd::CommonGetRTSPStreamURL: {
                if (cb_) cb_->onGetRtspStreamUrl(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetLogData: {
                break;
            }
            case axdp::UniCmd::CommonGetStreamMediaStatus: {
                break;
            }
            case axdp::UniCmd::CommonTestSDCardState: {
                TestTaskResult res = convertResult(result);
                if (cbex_)
                {
                    cbex_->onSDCardTestState(res);
                }
                break;
            }
            case axdp::UniCmd::CommonTestWIFIState: {
                TestTaskResult res = convertResult(result);
                if (cbex_)
                {
                    cbex_->onWifiTestState(res);
                }
                break;
            }
            case axdp::UniCmd::CommonTestBluetoothState: {
                TestTaskResult res = convertResult(result);
                if (cbex_)
                {
                    cbex_->onBluetoothTestState(res);
                }
                break;
            }
            case axdp::UniCmd::CommonTestBadFlashBlock: {
                TestTaskResult res = convertResult(result);
                if (cbex_)
                {
                    cbex_->onBadFlashBlockTestState(res);
                }
                break;
            }
            case axdp::UniCmd::CommonSetVerticalScreenMode: {
                if (cb_) cb_->onSetVerticalScreenMode(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetVerticalScreenMode: {
                if (cb_) cb_->onGetVerticalScreenMode(result);
                break;
            }
            case axdp::UniCmd::CommonSetAutoFocusState: {
                if (cb_) cb_->onSetAutoFocusState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetAutoFocusState: {
                if (cb_) cb_->onGetAutoFocusState(result);
                break;
            }
            case axdp::UniCmd::CommonGetBluetoothMacAddr: {
                break;
            }
            case axdp::UniCmd::CommonGetDereverationAlgParam: {
                if (cb_) {
                    int32_t pram_val = result;
                    cb_->onGetDereverationAlgParam(pram_val);
                }
                break;
            }
            case axdp::UniCmd::CommonGetBatteryCap: {
                if (cb_) {
                    cb_->onGetBatteryCap(result);
                }
                break;
            }
            case axdp::UniCmd::CommonAudioInputDetect: {
                if (cb_) cb_->onAudioInputDetect(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonSetManualFocusPosition: {
                if (cb_) cb_->onSetManualFocusPosition(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetManualFocusPosition: {
                if (cb_) cb_->onGetManualFocusPosition(result);
                break;
            }
            case axdp::UniCmd::CommonGetExternalSpeakerConfigJson: {
                if (cb_) cb_->onGetExternalSpeakerConfigJson(payload.data(), head.len(), head.src());
                break;
            }
            case axdp::UniCmd::CommonSetExternalSpeakerConfigJson: {
                //if (cb_) cb_->onAudioInputDetect(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetConfigJsonSubIndex: {
                if (cb_) cb_->onGetConfigJsonSubIndex(payload.data(), head.len(), head.src());
                break;
            }
            case axdp::UniCmd::CommonSetConfigJsonSubIndex: {
                    //if (cb_) cb_->onAudioInputDetect(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetExternalDeviceInfo: {
                if (cb_) cb_->onGetExternalDeviceInfo(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetTailWiFiSSID: {

                break;
            }
            case axdp::UniCmd::CommonGetPositionNumberJson: {
                if (cb_) cb_->onGetPositionNumberJson(payload.data(), head.len(), head.src());
                break;
            }
            case axdp::UniCmd::CommonGetDanteDevicelic: {
                if (cb_) cb_->onGetDanteDevicelic(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetDanteManufacturer: {
                if (cb_) cb_->onGetDanteManufacturer(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonSetDanteDevicelic: {
                if (cb_) cb_->onSetDanteDevicelic(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetDanteManufacturer: {
                if (cb_) cb_->onSetDanteManufacturer(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetTailWiFiSSID: {
                if (true)
                {
                    uint8_t* pv = (uint8_t*)payload.data();
                    if (cb_) cb_->onGetWifi(pv, head.len());
                }
                break;
            }
            case axdp::UniCmd::CommonSetTipsStatus:{
                if (cb_) {
                    cb_->onSetTipsStatus(EnableState(result));
                }
            }
                break;
            case axdp::UniCmd::CommonGetTipsStatus: {
                if (cb_) {
                    cb_->onGetTipsStatus(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetConfigJsonBySn: {
                if (cb_) cb_->onSetConfigJsonBySn(result);
                break;
            }
            case axdp::UniCmd::CommonGetConfigJsonBySn: {
                if (cb_) cb_->onGetConfigJsonBySn(payload.data(), head.len());
                break;
            }
            case axdp::UniCmd::CommonGetSightAngle: {
                if (cb_) cb_->onGetSightAngle(result);
                break;
            }
            case axdp::UniCmd::CommonSetSightAngle: {
                if (cb_) cb_->onSetSightAngle(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetAllChannelScanState: {
                if (cb_) cb_->onGetAllChannelScanState(result);
                break;
            }
            case axdp::UniCmd::CommonSetAllChannelScanState: {
                if (cb_) cb_->onSetAllChannelScanState(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonAudioBeamReport: {
                uint8_t *pv = (uint8_t *) payload.data();
                if (cb_) cb_->onAudioBeamReport(pv[0], pv[1], pv[2], pv[3]);
                break;
            }
            case axdp::UniCmd::CommonSetFrameRateSwitch:
                break;
            case axdp::UniCmd::CommonGetFrameRateSwitch: {
                if (cb_) {
                    cb_->onGetFrameRateSwitch(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetComboKey: {
                if (cb_) {
                    cb_->onSetComboKey(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonGetComboKey: {
                if (cb_) {
                    cb_->onGetComboKey(result);
                }
                break;
            }
            case axdp::UniCmd::CommonGetAutoShutDown: {
                if (cb_) {
                    cb_->onGetAutoShutDown(result);
                }
                break;
            }
            case axdp::UniCmd::CommonSetAutoShutDown:{
                if (cb_) {
                    cb_->onSetAutoShutDown(EnableState(result));
                }
                break;
            }
            case axdp::UniCmd::CommonSetDefaultVolume: {
                if (cb_) cb_->onSetDefaultVolume(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetDefaultVolume: {
                if (cb_) cb_->onGetDefaultVolume(result);
                break;
            }
            case axdp::UniCmd::CommonGetAudioEqParam: {
                if (cb_) cb_->ongetAudioEqParam(result);
                break;
            }
            case axdp::UniCmd::CommonSetAudioEqParam: {
                if (cb_) cb_->onSetAudioEqParam(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonSetAudioEqMode: {
                if (cb_) cb_->onSetAudioEqMode(EnableState(result));
                break;
            }
            case axdp::UniCmd::CommonGetAudioEqMode: {
                if (cb_) cb_->ongetAudioEqMode(result);
                break;
            }

            default:
                //todo : unsupported protocol here
                break;
        }
        return 0;
    }

    int AlphaDeviceAccessor::handleReceivedInternalProtocolMessage(const ProtocolMessage &msg) {
        auto &head = msg.header();
        auto &payload = msg.payload();
        UniCmd cmd = addMask(head.cmd(), ProtocolType::Alpha);
        switch (cmd) {
            case axdp::UniCmd::AlphaUpgradeInfo: {
                uint32_t data0 = utils::ntohl(*(uint32_t *) payload.data());
                if (data0 == RESULT_SUCCESS) {
                    dfu_helper_.setDowning(true);
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Transferring);
                    while (!dfu_helper_.isSliceEmpty()) {
                        int ret = sendUpgradeData();
                        if (ret < 0) {
                            //onErrorOccured(ErrorCode::UpgradeWriteError);
                            break;
                        }
                        int upgrade_progress = dfu_helper_.progress();
                        if (cb_ && upgrade_progress != upgrade_progress_) {
                          upgrade_progress_ = upgrade_progress;
                          cb_->onDfuProgressUpdate(upgrade_progress_);
                        }
                        dfu_helper_.increaseSliceIndex();
                    }
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Verifying);
                } else {
                    dfu_helper_.setDowning(false);
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Failed);
                }
                break;
            }
            case axdp::UniCmd::AlphaUpgradeData: {
                uint32_t data0 = utils::ntohl(*(uint32_t *) payload.data());
                if (data0 == PROTOCOL_BIN_UPGRADE_FAIL) {
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Failed);
                } else if (data0 == PROTOCOL_BIN_UPGRADE_SUCCESS) {
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Success);
                }
                dfu_helper_.setDowning(false);
                break;
            }
            case axdp::UniCmd::AlphaDeviceInfo: {
                DevInfoSt info{0};
                //DeviceInfo dev_info[1] = {0};
                int dev_count = 1;
                DeviceInfo *dev_info = new DeviceInfo[dev_count];
                memcpy(&info, payload.data(), sizeof(DevInfoSt));
                memcpy(dev_info[0].product_name, info.product_name, 32);
                memcpy(dev_info[0].serial_number, info.serial_number, 32);
                memcpy(dev_info[0].soft_version, info.software_ver, 32);

                if (task_state_mgr_.isTaskAsync(UniCmd::AlphaDeviceInfo)){
                    //onModelChanged(unicmd);
                    //model_.reset_done = true;
                    if(cb_) cb_->onGetDeviceInformation(dev_info, dev_count);
                    //getDeviceType();
                } else{
                    dev_info_list_.clear();
                    for (size_t i = 0; i < dev_count; i++)
                    {
                        //DeviceInfo dev_info_tmp;
                        //memcpy(&dev_info_tmp, &dev_info[i], sizeof(DeviceInfo));
                        //dev_info_list_.push_back(std::move(dev_info_tmp));
                        dev_info_list_.emplace_back(dev_info[i]);
                    }
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        task_state_mgr_.setTaskState(UniCmd::AlphaDeviceInfo, TaskState::SyncDone);
                    }
                    cond_var_.notify_all();
                }
                delete[] dev_info;
                break;
            }
            case axdp::UniCmd::AlphaDeviceType: {
                DevTypeSt type{0};
                memcpy(&type, payload.data(), sizeof(DevTypeSt));
                type.type = utils::ntohs(type.type);
                type.count = utils::ntohs(type.count);
                //onModelChanged(unicmd);
                break;
            }
            default:
                DeviceAccessorImpl::handleReceivedInternalProtocolMessage(msg);
                break;
        }
        return 0;
    }

    int BetaDeviceAccessor::handleReceivedInternalProtocolMessage(const ProtocolMessage &msg) {
        auto &head = msg.header();
        auto &payload = msg.payload();
        auto result = convert(msg);
        UniCmd cmd = addMask(head.cmd(), ProtocolType::Beta);
        switch (cmd) {
            case axdp::UniCmd::BetaDeviceReset: {
                //auto result = utils::ntohl(*(uint32_t*)payload.data());
                auto state = result ? ResultState::Failed : ResultState::Success;
                if (cb_) cb_->onResetDevice(state);
                break;
            }
            case axdp::UniCmd::BetaDeviceInfo: {
                uint8_t *pld = (uint8_t *) payload.data();
                uint16_t *pv = (uint16_t *) payload.data();
                uint16_t dev_count = utils::ntohs(pv[0]);
                if (dev_count > MAX_CASCADE_DEVS) {
                    //onErrorOccured(ErrorCode::DeviceCountError);
                    cb_->onGetDeviceInformation(nullptr, dev_count);
                    break;
                }
                dfu_helper_.setDevCount(dev_count);
                DeviceInfo *dev_info = new DeviceInfo[dev_count];
                int pos = 2;
                for (int i = 0; i < dev_count; ++i) {
                    uint16_t dev_type;
                    char *product_name = dev_info[i].product_name;
                    char *phy_version = dev_info[i].phy_version;
                    char *soft_version = dev_info[i].soft_version;
                    char *serial_number = dev_info[i].serial_number;
                    char *unique_id = dev_info[i].unique_id;
                    int32_t* index_id = &(dev_info[i].index_id);

                    product_name[0] = '\0';
                    phy_version[0] = '\0';
                    soft_version[0] = '\0';
                    serial_number[0] = '\0';
                    unique_id[0] = '\0';
                    *index_id = -1;

                    uint16_t *ptype = (uint16_t *) (pld + pos);
                    pos += 2;
                    dev_type = utils::ntohs(ptype[0]);
                    dev_info[i].dev_type = dev_type;
                    uint32_t *pflag = (uint32_t *) (pld + pos);
                    uint32_t flag = utils::ntohl(pflag[0]);
                    pos += 4;

#define PRODUCT_NAME_SHIFT 0
#define PHY_VERSION_SHIFT 1
#define SOFT_VERSION_SHIFT 2
#define SERIAL_NUMBER_SHIFT 3
#define UNIQUE_ID_SHIFT 4
#define INDEX_ID_SHIFT 5

                    for (int j = 0; j < 32; ++j) {
                        if ((flag & (1 << j)) == 0) {
                            continue;
                        }
                        //aw_print("catch shift %d", j);
                        switch (j) {
                            case PRODUCT_NAME_SHIFT: {
                                uint8_t len = pld[pos++];
                                /*if (dev_type == 0x20) {
                                    memcpy(product_name, pld + pos, len-4);
                                    product_name[len-4] = '\0';
                                } else*/ {
                                    memcpy(product_name, pld + pos, len);
                                    product_name[len] = '\0';
                                }
                                pos += len;
                                break;
                            }
                            case PHY_VERSION_SHIFT: {
                                uint8_t len = pld[pos++];
                                memcpy(phy_version, pld + pos, len);
                                phy_version[len] = '\0';
                                pos += len;
                                break;
                            }
                            case SOFT_VERSION_SHIFT: {
                                uint8_t len = pld[pos++];
                                memcpy(soft_version, pld + pos, len);
                                soft_version[len] = '\0';
                                pos += len;
                                break;
                            }
                            case SERIAL_NUMBER_SHIFT: {
                                uint8_t len = pld[pos++];
                                memcpy(serial_number, pld + pos, len);
                                serial_number[len] = '\0';
                                pos += len;
                                break;
                            }
                            case UNIQUE_ID_SHIFT: {
                                uint8_t len = pld[pos++];
                                memcpy(unique_id, pld + pos, len);
                                unique_id[len] = '\0';
                                pos += len;
                                break;
                            }
                            case INDEX_ID_SHIFT: {
                                uint8_t len = pld[pos++];
                                memcpy(index_id, pld + pos, len);
                                pos += len;
                                break;
                            }
                            default: {
                                uint8_t len = pld[pos++];
                                pos += len;
                                break;
                            }
                        }
                    }
                }
                if (task_state_mgr_.isTaskAsync(UniCmd::BetaDeviceInfo)){
                    if(cb_) cb_->onGetDeviceInformation(dev_info, dev_count);
                } else{
                    dev_info_list_.clear();
                    //dev_info_list_.resize(8);
                    for (size_t i = 0; i < dev_count; i++)
                    {
                        //DeviceInfo dev_info_tmp;
                        //memcpy(&dev_info_tmp, &dev_info[i], sizeof(DeviceInfo));
                        //dev_info_list_.push_back(std::move(dev_info_tmp));
                        dev_info_list_.emplace_back(dev_info[i]);
                    }
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        task_state_mgr_.setTaskState(UniCmd::BetaDeviceInfo, TaskState::SyncDone);
                    }
                    cond_var_.notify_all();
                }
                delete[] dev_info;
                break;
            }
            case axdp::UniCmd::BetaStartUpgrade: {
                uint32_t *data0 = (uint32_t *) payload.data();
                if (head.len() == 4 && utils::ntohl(data0[0]) == 0) {
                    if (dfu_helper_.isAllDevicesReady(head.src())) {
                        //LOGV("[Beta]Upgrade Start!------");
                        downing_ = true;
                        int ret = sendUpgradeInfo();
                        pre_send_slice_index_ = -1;
                        if (cb_) cb_->onDfuStateUpdate(UpgradeState::Transferring);
                    }
                } else {
                    uint16_t dst = dfu_helper_.dst();
                    uint16_t head_dst = head.src();
                    bool isUpgrade = dst & head_dst;
                    if (isUpgrade) {
                        if (cb_) cb_->onDfuStateUpdate(UpgradeState::Failed);
                    }
                }
                break;
            }
            case axdp::UniCmd::BetaStopUpgrade: {
                if (dfu_helper_.isAllDevicesReady(head.src())) {
                    downing_ = false;
                    if (cb_) cb_->onDfuStateUpdate(UpgradeState::Success);
                    //LOGV("[Beta]Upgrade Stop!Upgrade successfully.");
                }
                break;
            }
            case axdp::UniCmd::BetaUpgradeInfo: {
                uint32_t *data0 = (uint32_t *) payload.data();
                if (head.len() == 4 && utils::ntohl(data0[0]) == 0) {
                    if (dfu_helper_.isAllDevicesReady(head.src())) {
                        //LOGV("[Beta]Upgrade Start Block(%d)#########!", dfu_helper_.currentBlockIndex());
                        dfu_helper_.resetBlockStat();
                        dfu_helper_.increaseSliceIndex();
                        int ret = sendUpgradeData();
                    }
                } else {
                    //onErrorOccured(ErrorCode::UpgradeDataError);
                }
                break;
            }
            case axdp::UniCmd::BetaUpgradeData: {
                if (head.len() != 4) {
                    //onErrorOccured(ErrorCode::UpgradeDataError);
                    //LOGV("error when recv slice back with hean.len : %d", head.len());
                    break;
                }
                //std::this_thread::sleep_for(std::chrono::milliseconds(2));
                //1.if data0 == PROTOCOL_BIN_UPGRADE_SUCCESS
                uint32_t *pv = (uint32_t *) payload.data();
                uint32_t data0 = utils::htonl(pv[0]);
                if (data0 == PROTOCOL_BIN_UPGRADE_SUCCESS) {
                    if (dfu_helper_.isAllDevicesReady(head.src())) {
                        //LOGV("[Beta]Upgrade Stop Block(%d)#########!", dfu_helper_.currentBlockIndex());
                        dfu_helper_.increaseBlockIndex();
                        if (dfu_helper_.isBlockEmpty()) {
                            stopUpgrade();
                            if (cb_)cb_->onDfuStateUpdate(UpgradeState::Verifying);
                        } else {
                            sendUpgradeInfo();
                        }
                    }
                } else if (data0 == PROTOCOL_BIN_UPGRADE_FAIL) {
                    //LOGV("[Beta]Upgrade Error when RECV slices INDX : %d", data0);
                    if (cb_)cb_->onDfuStateUpdate(UpgradeState::Failed);
                } else {
                    int requested_slice_index = dfu_helper_.requestedSliceIndex();
                    //LOGV("recv data with requested data0 %d", utils::htonl(pv[0]));
                    if (utils::htonl(pv[0]) == requested_slice_index) {
                        if (dfu_helper_.isAllDevicesReady(head.src())) {
                            dfu_helper_.increaseSliceIndex();
                            sendUpgradeData();
                            int upgrade_progress = dfu_helper_.progress();
                            if (cb_ && upgrade_progress != upgrade_progress_) {
                              upgrade_progress_ = upgrade_progress;
                              cb_->onDfuProgressUpdate(upgrade_progress_);
                            }
                            pre_send_slice_index_ = requested_slice_index;
                            if(dfu_helper_.updatePacketInterval())
                                std::this_thread::sleep_for(std::chrono::milliseconds(dfu_helper_.updatePacketInterval()));
                        }
                    } else {
                        //onErrorOccured(ErrorCode::UpgradeDataError);
                        LOGV("error when send all the slices in slice "
                             " index : %d,"
                             " data0 = 0x%x,"
                             " requested  = %d",
                             utils::ntohl(dfu_helper_.currentSlice().slice_index),
                             utils::htonl(pv[0]),
                             requested_slice_index);
                    }
                }
                break;
            }
            case axdp::UniCmd::BetaUpgradeInfoEx: {
                uint32_t data0 = utils::ntohl(*(uint32_t *) payload.data());
                int ret = 0;
                if (data0 == RESULT_SUCCESS) {
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Transferring);
                    pre_send_slice_index_ = -1;
                    while (!dfu_helper_.isSliceEmpty()) {

                        int upgrade_progress = dfu_helper_.progress();
                        if (cb_ && upgrade_progress != upgrade_progress_) {
                            upgrade_progress_ = upgrade_progress;
                            //std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();

                            cb_->onDfuProgressUpdate(upgrade_progress_);
                            //std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
                            //auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
                            //LOGD("SEND UPGRADE DATA EX DURATION = %ld", delta);
                        }
                        if (dfu_helper_.updatePacketInterval())
                            std::this_thread::sleep_for(std::chrono::milliseconds(dfu_helper_.updatePacketInterval()));

                        ret = sendUpgradeDataEx();

                        if (ret < 0) {
                            //onErrorOccured(ErrorCode::UpgradeWriteError);
                            break;
                        }
                        dfu_helper_.increaseSliceIndex();
                    }
                    if (cb_) {
                        if (ret < 0)
                        {
							cb_->onDfuStateUpdate(UpgradeState::Failed);
                        }
                        else
                        {
                            cb_->onDfuStateUpdate(UpgradeState::Verifying);
                        }
                    }
                } else {
                    //onErrorOccured(ErrorCode::UpgradeStartError);
                    downing_ = false;
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Failed);
                }
                break;
            }
            case axdp::UniCmd::BetaUpgradeDataEx: {
                //aux_print("[Beta Ex]Upgrade Stop!------");
                uint32_t data0 = utils::ntohl(*(uint32_t *) payload.data());
                if (data0 == PROTOCOL_BIN_UPGRADE_FAIL) {
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Failed);
                } else if (data0 == PROTOCOL_BIN_UPGRADE_SUCCESS) {
                    if (cb_)
                        cb_->onDfuStateUpdate(UpgradeState::Success);
                }
                downing_ = false;
                break;
            }
            default:
                DeviceAccessorImpl::handleReceivedInternalProtocolMessage(msg);
                break;
        }
        return 0;
    }

    int BetaDeviceAccessor::onCheckTimeOut() {
        uint32_t curSliceIndex = dfu_helper_.curSliceIndex();
        if (pre_send_slice_index_ == curSliceIndex) {
            sendUpgradeData();
        }
        return 0;
    }
}
