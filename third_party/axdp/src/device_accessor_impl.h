#ifndef AXDP_COMMON_MESSAGE_ENGINE_H_
#define AXDP_COMMON_MESSAGE_ENGINE_H_

#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include "axdp_api_ex.h"
#include "message_engine.h"
#include "protocol_message.h"
#include "dfu_helper.h"
#include "fbf_helper.h"
#include "bin_parser.h"

namespace axdp {
    /*
    * 设备数据读写类，封装了一系列设备的Set和Get方法
    */
    class HumanInterfaceEventHandler;

    class InternalProtocolMessageHandler;

    class DeviceAccessorImpl : public MessageEngine,
                               public DeviceAccessorEx {
    public:
        DeviceAccessorImpl();

        DeviceAccessorImpl(IODevice *device, MessageSender *sender, MessageReceiver *receiver);

        virtual ~DeviceAccessorImpl() override;

        void setQueueMode(bool enable) override;;

        void registerCbDelegate(EventCallbackDelegate *cb) override;

        void unregisterCbDelegate(EventCallbackDelegate *cb) override;

        virtual int resetDevice() override = 0;

        virtual int getDeviceInfo() override = 0;

        virtual int getDeviceType() = 0;

        void setUpgradeStrategy(UpdateStrategy strategy) override;

        void setUpdatePacketInterval(uint16_t interval) override;

        void setUpgradeDeviceDst(uint16_t dst) override;

        void setUpgradeUltraDstEnabled(EnableState st)override;

        /*max buffer size for sub_index is 32 bytes*/
        void setUpgradeDeviceUltraDst(const uint8_t* sub_index, int32_t size)override;

        int startFirmwareUpgrade(const char *file_name) override;

        int startFirmwareUpgrade(const char *buffer, size_t size) override;

        int setNoiseSuppressionLevel(DegreeLevel level) override;

        int getNoiseSuppressionLevel() override;

        int setReverberationSuppressionLevel(DegreeLevel level) override;

        int getReverberationSuppressionLevel() override;

        int setEchoCancellationLevel(DegreeLevel level) override;

        int getEchoCancellationLevel() override;

        int resetAudioAlgorithmParams() override;

        int startAudioRecord(uint16_t time_second) override;

        int stopAudioRecord() override;

        int getHidCallState() override;

        int setHidCallState(EnableState state) override;

        int setVideoMode(VideoMode mode) override;

        int getVideoMode() override;

        int getUacState() override;

        int setUacState(EnableState state) override;

        int setVideoTrackMode(VideoTrackMode mode) override;

        int getVideoTrackMode() override;

        int setMirrorState(EnableState state) override;

        int getMirrorState() override;

        int setSpeakerTrackDelay(uint32_t second) override;

        int getSpeakerTrackDelay() override;

        int setSplitScreenNumber(uint32_t number) override;

        int getSplitScreenNumber() override;

        int setPowerLineFreq(PowerlineFreqType freq) override;

        int getPowerLineFreq() override;

        int setFlipState(EnableState state) override;

        int getFlipState() override;

        int setWatermark(EnableState state) override;

        int getWatermark() override;

        int setPiPMode(EnableState mode) override;

        int getPiPMode() override;

        int setConfigJson(const char* cfg, size_t len, uint16_t dst) override;

        int getConfigJson(uint16_t dst) override;

        const std::string& syncGetConfigJson(uint16_t dst) override;

        int setDefaultConfigJson(uint16_t dst) override;

        int getDefaultConfigJson(uint16_t dst) override;

        int setAudioMuteState(bool mute, uint16_t dst) override;

        int getAudioMuteState(uint16_t dst) override;

        int getDereverationAlgParam() override;

        int setDereverationAlgParam(int32_t val) override;//val : [0~100]

        int reboot() override;

        int getUsbSpeedMode() override;

        int setUsbSpeedMode(int32_t val) override;

        int getMuteLightEnhancement() override;

        int setMuteLightEnhancement(int32_t val) override;

        int getOsdMirrorState() override;

        int setOsdMirrorState(EnableState state) override;

        int getPrivacyEnable() override;

        int setPrivacyEnable(EnableState state) override;

        int getWdrState()  override;

        int setWdrState(EnableState state)  override;

        //for network setting
        int setRtspStreamUrl(const char* url, size_t len)override;

        int getRtspStreamUrl()override;

        int getIPConfig()override;

        int setDHCPState(uint8_t idx, EnableState flag)override;

        int getDHCPState(uint8_t idx)override;

        int setIPAddress(uint8_t idx, uint32_t ip_addr)override;

        int getIPAddress(uint8_t idx)override;

        int setNetmask(uint8_t idx, uint32_t netmask)override;

        int getNetmask(uint8_t idx)override;

        int setGateway(uint8_t idx, uint32_t gateway)override;

        int getGateway(uint8_t idx)override;

        int setMacAddress(uint8_t idx, uint64_t mac_addr)override;

        int getMacAddress(uint8_t idx)override;

        int setViscaUdpPort(uint32_t port)override;// port：[1024,65535]

        int getViscaUdpPort()override;

        int setManualFocusPosition(uint32_t port)override;// port：[1024,65535]

        int getManualFocusPosition()override;

        int audioInputDetect(EnableState state)override;

        int audioBeamReport(EnableState state)override;

        int setBTRestore()override;

        int getBatteryCap()override;

        int getExternalSpeakerConfigJson(uint16_t dst)override;

        int setExternalSpeakerConfigJson(const char* cfg, size_t len, uint16_t dst)override;

        int setImageStyle(uint32_t style)override;

        int getImageStyle()override;

        int getConfigJsonSubIndex(const uint8_t* sub_idx, size_t idx_len)override;

        int setConfigJsonSubIndex(const char* cfg, size_t len, const uint8_t* sub_idx, size_t idx_len)override;

        int setFbfParam(const char* file, uint16_t dst)override;

        int getFbfParam(uint16_t dst)override;

        int sendFbfParamData(const uint8_t* buf, size_t len, uint16_t dst);

        int setFbfParamEnd(uint16_t dst);

        /*AMX100S GET DEVCIE INFO*/
        int getExternalDeviceInfo()override;

        int getPositionNumberJson(uint16_t dst)override;

        int setPositionNumberJson(const char* cfg, size_t len, uint16_t dst)override;

        int setDanteDevicelic(const char* str, size_t len)override;

        int getDanteDevicelic()override;

        int setDanteManufacturer(const char* str, size_t len)override;

        int getDanteManufacturer()override;

        int setConfigJsonBySn(const char* cfg, size_t len)override;

        int getConfigJsonBySn(const char* cfg, size_t len)override;

        int getSightAngle()override;

        int setSightAngle(int32_t angle)override;

        int getAllChannelScanState()override;

        int setAllChannelScanState(EnableState state)override;

        int getVerticalScreenMode()override;

        int setVerticalScreenMode(EnableState state)override;

        int getAutoFocusState()override;

        int setAutoFocusState(EnableState state)override;

        int getStartupPosition()override;

        int setStartupPosition(EnableState state)override;

        int getFrameRateSwitch()override;

        int setFrameRateSwitch(EnableState state)override;

        int getComboKey()override;

        int setComboKey(EnableState state)override;

        int getAutoShutDown()override;

        int setAutoShutDown(int32_t state)override;

        int getTipsStatus()override;

        int setTipsStatus(EnableState state)override;

        int getDefaultVolume()override;

        int setDefaultVolume(int32_t gain)override;

        int getAudioEqParam()override;

        int setAudioEqParam(uint32_t gain)override;

        int setAudioEqMode(int32_t gain)override;

        int getAudioEqMode()override;



        /*For standard hid use*/
        int setMuted(bool mute) override;

        int setOffHook(bool off) override;

        /*For factory test api*/
        int setMicUsed(uint32_t mic_index) override;
        int startAudioRecord(uint32_t mic_mask) override;//use mic 0~3, mic_mask = (1<<0)|(1<<1)|(1<<2)|(1<<3)
        int setTestResult(uint32_t test_index, uint32_t test_result) override;
        int getTestResult(uint32_t test_index) override;
        int setEncryptedInfoHardwareId(const char* hwid, uint32_t bytes) override;
        int setEncryptedInfoSerialNumber(const char* sn, uint32_t bytes) override;
        int getEncryptedInfo(uint32_t state) override;
        int testAudioConsistency(TestTaskCommand cmd) override;
        int testNetworkPort(TestTaskCommand cmd) override;
        int getDeviceUniqueId(const char* lic_content, uint32_t bytes) override;
        int setDeviceUniqueId() override;
        int getAlgAuthContent() override;
        int setAlgAuthContent(const char* auth_content, uint32_t bytes) override;
        int setAFCalibration() override;
        int getAFCalibrationResult() override;
        int getDDRCapacity() override;
        int setLensCenter(uint32_t value) override;
        int getRS232TestResult() override;
        int testKey(TestTaskCommand cmd) override;//0 for start, 1 for end
        int testLed(TestTaskCommand cmd) override;//0 for start, 1 for end
        int testSDCardState() override;
        int testWIFIState(TestTaskCommand cmd) override;//1 for start, -1 for get result
        int testBluetoothState(TestTaskCommand cmd) override;//1for start, -1for get result
        int testBadFlashBlock() override;
        // tail
        int setTailWifiSSID(const char* ssid, int length) override;
        int getTailWifiSSID() override;
    protected:
        virtual int startUpgrade() = 0;

        virtual int startUpgradeEx() = 0;

        virtual int sendUpgradeInfo() = 0;

        virtual int sendUpgradeInfoEx() = 0;

        virtual int sendUpgradeData() = 0;

        virtual int sendUpgradeDataEx() = 0;

        virtual int stopUpgrade() = 0;

        int sendUpgradeBlockInfo(UniCmd cmd);

        int sendUpgradeSliceData(UniCmd cmd);

        int onRxData(const void *ptr, uint16_t len) override;

        int onTxData(const uint8_t *buf, uint16_t len) override;

        int send(std::shared_ptr<ProtocolMessage> msg);

        void sendThreadFunc();

        int syncGetTask(UniCmd task_cmd, uint16_t dst, int32_t timeout_ms);

        virtual int handleReceivedInternalProtocolMessage(const ProtocolMessage &msg);

        int handleReceivedHumanInterfaceInputReport(const uint8_t *buffer, size_t length);

        MessageParser parser_;
        MessageBuilder builder_;
        EventCallbackDelegate* cb_{ nullptr };
        EventCallbackDelegateEx* cbex_{ nullptr };
        DfuHelper dfu_helper_;
        FbfHelper fbf_helper_;
        int upgrade_progress_{0};
        BinParser bin_parser_;
        std::unique_ptr<HumanInterfaceEventHandler> hid_report_event_handler_;
        bool downing_;
        std::condition_variable cond_var_;
        std::mutex mutex_;
        ProtocolTaskStateManager task_state_mgr_;
        uint32_t default_timeout_ms_{2000};
        DeviceInfoList dev_info_list_;
        std::string config_json_;
        int report_len_{64};

        std::mutex queue_mutex_;
        std::thread send_thread_;
        bool running_ = true;
        bool use_queue_mode_ = false;
        std::queue<std::shared_ptr<ProtocolMessage>> send_queue_;
    };

    class AlphaDeviceAccessor : public DeviceAccessorImpl {
    public:
        AlphaDeviceAccessor();

        AlphaDeviceAccessor(IODevice *device, MessageSender *sender, MessageReceiver *receiver);

        ~AlphaDeviceAccessor() override;

        int resetDevice() override;

        int getDeviceInfo() override;

        const DeviceInfoList& syncGetDeviceInfo() override;

        int getDeviceType() override;

        int startUpgrade() override;

        int startUpgradeEx() override;

        int sendUpgradeInfo() override;

        int sendUpgradeInfoEx() override;

        int sendUpgradeData() override;

        int sendUpgradeDataEx() override;

        int stopUpgrade() override;

    private:
        int handleReceivedInternalProtocolMessage(const ProtocolMessage &msg) override;
    };

    class BetaDeviceAccessor : public DeviceAccessorImpl {
    public:
        BetaDeviceAccessor();

        BetaDeviceAccessor(IODevice *device, MessageSender *sender, MessageReceiver *receiver);

        ~BetaDeviceAccessor() override;

        int resetDevice() override;

        int getDeviceInfo() override;

        const DeviceInfoList& syncGetDeviceInfo() override;

        int getDeviceType() override;

        int startUpgrade() override;

        int startUpgradeEx() override;

        int sendUpgradeInfo() override;

        int sendUpgradeInfoEx() override;

        int sendUpgradeData() override;

        int sendUpgradeDataEx() override;

        int stopUpgrade() override;

    private:
        int pre_send_slice_index_ = -1;

        int onCheckTimeOut();

        int handleReceivedInternalProtocolMessage(const ProtocolMessage &msg) override;
    };

}


#endif // !AXDP_COMMON_MESSAGE_ENGINE_H_
