#ifndef AXDP_API_H_
#define AXDP_API_H_

#include "axdp_defines.h"

namespace axdp {
    class AXDP_API EventCallbackDelegate {
    public:
        EventCallbackDelegate() = default;

        virtual ~EventCallbackDelegate() = default;

        virtual void onHumanInterfaceEvent(HIDEvent event) {};

        virtual void onResetDevice(ResultState state) {};

        virtual void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) {};

        virtual void onDfuStateUpdate(UpgradeState state) {};

        virtual void onDfuProgressUpdate(uint16_t progress) {};

        virtual void onGetNoiseSuppressionLevel(DegreeLevel level) {};

        virtual void onGetReverbrationSuppressionLevel(DegreeLevel level) {};

        virtual void onGetEchoCancellationLevel(DegreeLevel level) {};

        virtual void onAudioRecordStart(bool supported, const AudioFormat *format) {};

        virtual void onAudioRecordData(const uint8_t *data, int32_t len) {};

        virtual void onAudioRecordStopped(int32_t result) {};

        virtual void onGetHidCallState(EnableState state) {};

        virtual void onGetVideoMode(VideoMode mode) {};

        virtual void onGetUacState(EnableState state) {};

        virtual void onGetVideoTrackMode(VideoTrackMode mode) {};

        virtual void onGetMirrorState(EnableState state) {};

        virtual void onGetSpeakerTrackDelay(int32_t delay) {};

        virtual void onGetSplitScreenNumber(int32_t screen_num) {};

        virtual void onGetPowerLineFreq(PowerlineFreqType freq) {};

        virtual void onGetFlipState(EnableState state) {};

        virtual void onGetDefaultConfigJson(const uint8_t* payload, uint16_t len, uint16_t src) {};

        virtual void onGetConfigJson(const uint8_t* payload, uint16_t len, uint16_t src) {};

        virtual void onSetConfigJson(int result, uint16_t src) {};

        virtual void onGetAudioMuteState(bool mute, uint16_t src) {};

        virtual void onGetDereverationAlgParam(int32_t val) {}

        virtual void onGetUsbSpeedMode(int32_t val) {}

        virtual void onGetMuteLightEnhancement(int32_t val) {}

        virtual void onGetPrivacyEnable(EnableState state) {}

        virtual void onGetOsdMirrorState(EnableState state) {}

        virtual void onGetWdrState(EnableState state) {}

        virtual void onAudioInputDetect(const uint8_t* payload, uint16_t len) {}

        virtual void onAudioBeamReport(uint8_t device_type, uint8_t device_index,
                                       uint8_t beam_index, uint8_t is_speech) {}

        virtual void onSetManualFocusPosition(EnableState state) {}

        virtual void onGetManualFocusPosition(int32_t result) {}

        virtual void onGetBatteryCap(uint8_t power) {}

        virtual void onGetExternalSpeakerConfigJson(const uint8_t* payload, uint16_t len, uint16_t src) {}

        virtual void onGetImageStyle(int32_t style) {}

        virtual void onGetConfigJsonSubIndex(const uint8_t* payload, uint16_t len, uint16_t src) {}

        virtual void onFbfParamError(const uint8_t* payload, uint16_t len, uint16_t src) {}

        virtual void onGetFbfParam(const uint8_t* payload, uint16_t len, uint16_t src) {}

        virtual void onSetFbfParamState(UpgradeState state, uint16_t src) {}

        virtual void onGetExternalDeviceInfo(const uint8_t* payload, uint16_t len) {}

        virtual void onSetDanteDevicelic(EnableState state){}

        virtual void onGetDanteDevicelic(const uint8_t* str, size_t len){}

        virtual void onSetDanteManufacturer(EnableState state){}

        virtual void onGetDanteManufacturer(const uint8_t* str, size_t len){}

        virtual void onGetConfigJsonBySn(const uint8_t* payload, uint16_t len) {}

        virtual void onSetConfigJsonBySn(int result) {}

        virtual void onGetSightAngle(int angle) {}

        virtual void onSetSightAngle(EnableState result) {}

        virtual void onGetAllChannelScanState(int state) {}

        virtual void onSetAllChannelScanState(EnableState result) {}

        virtual void onGetVerticalScreenMode(int state) {}

        virtual void onSetVerticalScreenMode(EnableState result) {}

        virtual void onGetAutoFocusState(int state) {}

        virtual void onSetAutoFocusState(EnableState result) {}

        virtual void onGetStartupPosition(int state) {}

        virtual void onSetStartupPosition(EnableState result) {}

        virtual void onGetFrameRateSwitch(int state) {}

        virtual void onSetFrameRateSwitch(EnableState result) {}

        virtual void onGetComboKey(int state) {}

        virtual void onSetComboKey(EnableState result) {}

        virtual void onGetAutoShutDown(int state) {}

        virtual void onSetAutoShutDown(EnableState result) {}

        virtual void onGetTipsStatus(int state) {}

        virtual void onSetTipsStatus(EnableState result) {}

        virtual void onGetDefaultVolume(int angle) {}

        virtual void onGetWatermark(EnableState result) {}

        virtual void onGetPiPMode(EnableState result) {}

        virtual void onSetDefaultVolume(EnableState result) {}

        virtual void ongetAudioEqParam(unsigned int angle) {}

        virtual void onSetAudioEqParam(EnableState result) {}

        virtual void onSetAudioEqMode(EnableState result) {}

        virtual void ongetAudioEqMode(unsigned int angle) {}


        //network
        virtual void onGetRtspStreamUrl(const uint8_t* payload, uint16_t len) {}

        virtual void onGetIPConfig(uint32_t) {}

        virtual void onGetDHCPState(uint8_t idx, EnableState state) {}

        virtual void onGetIPAddress(uint8_t idx, uint32_t addr) {}

        virtual void onGetNetMask(uint8_t idx, uint32_t mak) {}

        virtual void onGetGateway(uint8_t idx, uint32_t gw) {}

        virtual void onGetMacAddress(uint8_t idx, uint64_t mac) {}

        virtual void onGetViscaUdpPort(uint32_t port) {}

        virtual void onGetPositionNumberJson(const uint8_t* payload, uint16_t len, uint16_t src) {}

        // tail
        virtual void onGetWifi(const uint8_t* ssid, uint16_t len){}
        virtual void onSetWifi(){}
    };

    class AXDP_API DeviceAccessor {

    public:

#ifdef __ANDROID__

        static DeviceAccessor *Create(int vid, int pid, long file_desc, int interface_num);

#endif // __ANDROID__

        static DeviceAccessor *Create(int vid, int pid, const wchar_t *serial_number = nullptr);

    protected:
        DeviceAccessor() = default;

    public:
        DeviceAccessor(const DeviceAccessor &) = delete;

        DeviceAccessor &operator=(const DeviceAccessor &) = delete;

        virtual ~DeviceAccessor() = default;

        virtual void setQueueMode(bool enable) {};

        virtual void registerCbDelegate(EventCallbackDelegate *cb) {};

        virtual void unregisterCbDelegate(EventCallbackDelegate *cb) {};

        virtual void setUpgradeStrategy(UpdateStrategy strategy) {};

        virtual void setUpgradeDeviceDst(uint16_t dst) {};

        virtual void setUpdatePacketInterval(uint16_t interval) {};

        virtual void setUpgradeUltraDstEnabled(EnableState st) {};

        virtual void setUpgradeDeviceUltraDst(const uint8_t* sub_index, int32_t size) {};

        virtual int startFirmwareUpgrade(const char *file_name) = 0;

        virtual int startFirmwareUpgrade(const char *buffer, size_t size) = 0;

        virtual int resetDevice() = 0;

        virtual int getDeviceInfo() = 0;

        virtual const DeviceInfoList& syncGetDeviceInfo() = 0;

        virtual int setNoiseSuppressionLevel(DegreeLevel level) = 0;

        virtual int getNoiseSuppressionLevel() = 0;

        virtual int setReverberationSuppressionLevel(DegreeLevel Level) = 0;

        virtual int getReverberationSuppressionLevel() = 0;

        virtual int setEchoCancellationLevel(DegreeLevel Level) = 0;

        virtual int getEchoCancellationLevel() = 0;

        virtual int resetAudioAlgorithmParams() = 0;

        virtual int startAudioRecord(uint16_t max_time_second) = 0;//default = 600s

        virtual int stopAudioRecord() = 0;

        virtual int getHidCallState() = 0;

        virtual int setHidCallState(EnableState state) = 0;

        virtual int setVideoMode(VideoMode mode) = 0;

        virtual int getVideoMode() = 0;

        virtual int setVideoTrackMode(VideoTrackMode mode) = 0;

        virtual int getVideoTrackMode()  = 0;

        virtual int getUacState() = 0;

        virtual int setUacState(EnableState state) = 0;

        virtual int setMirrorState(EnableState state) = 0;

        virtual int getMirrorState() = 0;

        virtual int setSpeakerTrackDelay(uint32_t second) = 0;

        virtual int getSpeakerTrackDelay() = 0;

        virtual int setSplitScreenNumber(uint32_t number) = 0;

        virtual int getSplitScreenNumber() = 0;

        virtual int setPowerLineFreq(PowerlineFreqType freq) = 0;

        virtual int getPowerLineFreq() = 0;

        virtual int setFlipState(EnableState state) = 0;

        virtual int getFlipState() = 0;

        virtual int setWatermark(EnableState state) = 0;
        virtual int getWatermark() = 0;
        virtual int setPiPMode(EnableState mode) = 0;
        virtual int getPiPMode() = 0;

        virtual int setConfigJson(const char* cfg, size_t len, uint16_t dst) = 0;

        virtual int getConfigJson(uint16_t dst) = 0;

        virtual const std::string& syncGetConfigJson(uint16_t dst) = 0;

        virtual int setDefaultConfigJson(uint16_t dst) = 0;

        virtual int getDefaultConfigJson(uint16_t dst) = 0;

        virtual int setAudioMuteState(bool mute, uint16_t dst) = 0;

        virtual int getAudioMuteState(uint16_t dst) = 0;

        virtual int getDereverationAlgParam() = 0;

        virtual int setDereverationAlgParam(int32_t val) = 0;//val : [0~100]

        virtual int reboot() = 0;

        virtual int getUsbSpeedMode() = 0;

        virtual int setUsbSpeedMode(int32_t val) = 0;

        virtual int getMuteLightEnhancement() = 0;

        virtual int setMuteLightEnhancement(int32_t val) = 0;

        virtual int getOsdMirrorState() = 0;

        virtual int setOsdMirrorState(EnableState state) = 0;

        virtual int getPrivacyEnable() = 0;

        virtual int setPrivacyEnable(EnableState state) = 0;

        virtual int getWdrState() = 0;

        virtual int setWdrState(EnableState state) = 0;

        //for network setting
        virtual int setRtspStreamUrl(const char* url, size_t len) = 0;

        virtual int getRtspStreamUrl() = 0;

        virtual int getIPConfig() = 0;

        virtual int setDHCPState(uint8_t idx, EnableState flag) = 0;

        virtual int getDHCPState(uint8_t idx) = 0;

        virtual int setIPAddress(uint8_t idx, uint32_t ip_addr) = 0;

        virtual int getIPAddress(uint8_t idx) = 0;

        virtual int setNetmask(uint8_t idx, uint32_t netmask) = 0;

        virtual int getNetmask(uint8_t idx) = 0;

        virtual int setGateway(uint8_t idx, uint32_t gateway) = 0;

        virtual int getGateway(uint8_t idx) = 0;

        virtual int setMacAddress(uint8_t idx, uint64_t mac_addr) = 0;

        virtual int getMacAddress(uint8_t idx) = 0;

        virtual int setViscaUdpPort(uint32_t port) = 0;// port：[1024,65535]

        virtual int getViscaUdpPort() = 0;

        virtual int setManualFocusPosition(uint32_t port) = 0;// port：[1024,65535]

        virtual int getManualFocusPosition() = 0;

        virtual int audioInputDetect(EnableState state) = 0;

        virtual int audioBeamReport(EnableState state) = 0;

        virtual int setBTRestore() = 0;

        virtual int getBatteryCap() = 0;

        virtual int getExternalSpeakerConfigJson(uint16_t dst) = 0;

        virtual int setExternalSpeakerConfigJson(const char* cfg, size_t len, uint16_t dst) = 0;

        virtual int setImageStyle(uint32_t style)= 0;

        virtual int getImageStyle()= 0;

        virtual int getConfigJsonSubIndex(const uint8_t* sub_idx, size_t idx_len)= 0;

        virtual int setConfigJsonSubIndex(const char* cfg, size_t len, const uint8_t* sub_idx, size_t idx_len)= 0;

        virtual int getExternalDeviceInfo() = 0;

        virtual int getPositionNumberJson(uint16_t dst) = 0;

        virtual int setPositionNumberJson(const char* cfg, size_t len, uint16_t dst) = 0;
        // tail
        virtual int setTailWifiSSID(const char* ssid, int length) = 0;

        virtual int getTailWifiSSID() = 0;

        virtual int setFbfParam(const char* file, uint16_t dst) = 0;

        virtual int getFbfParam(uint16_t dst) = 0;

        virtual int setDanteDevicelic(const char* str, size_t len) = 0;

        virtual int getDanteDevicelic() = 0;

        virtual int setDanteManufacturer(const char* str, size_t len) = 0;

        virtual int getDanteManufacturer() = 0;

        virtual int setConfigJsonBySn(const char* cfg, size_t len) = 0;

        virtual int getConfigJsonBySn(const char* cfg, size_t len) = 0;

        virtual int getSightAngle() = 0;

        virtual int setSightAngle(int32_t angle) = 0;

        virtual int getAllChannelScanState() = 0;

        virtual int setAllChannelScanState(EnableState state) = 0;

        virtual int getVerticalScreenMode() = 0;

        virtual int setVerticalScreenMode(EnableState state) = 0;

        virtual int getAutoFocusState() = 0;

        virtual int setAutoFocusState(EnableState state) = 0;

        virtual int getStartupPosition() = 0;

        virtual int setStartupPosition(EnableState state) = 0;

        virtual int getFrameRateSwitch() = 0;

        virtual int setFrameRateSwitch(EnableState state) = 0;

        virtual int getComboKey() = 0;

        virtual int setComboKey(EnableState state) = 0;

        virtual int getAutoShutDown() = 0;

        virtual int setAutoShutDown(int32_t state) = 0;

        virtual int getTipsStatus() = 0;

        virtual int setTipsStatus(EnableState state) = 0;

        virtual int getDefaultVolume() = 0;

        virtual int setDefaultVolume(int32_t gain) = 0;

        virtual int getAudioEqParam() = 0;

        virtual int setAudioEqParam(uint32_t gain) = 0;

        virtual int setAudioEqMode(int32_t gain) = 0;

        virtual int getAudioEqMode() = 0;


        /***For Hunman Interface Settings***/
        virtual int setMuted(bool mute) = 0;

        virtual int setOffHook(bool off) = 0;
    };
}


#endif // !AXDP_API_H_
