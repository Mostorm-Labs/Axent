#ifndef AXDP_API_EX_H_
#define AXDP_API_EX_H_

#include "axdp_api.h"

namespace axdp {

    enum TestTaskCommand : int32_t {
        CommandStartEx = 2,
        CommandStart = 1,
        CommandUnknown = 0,
        CommandResult = -1
    };

    enum TestTaskResult : int32_t {
        ResultFailed = -1,
        ResultSuccess = 0,
        ResultUnknown = 1,
    };

    class AXDP_API EventCallbackDelegateEx : public EventCallbackDelegate {
    public:
        EventCallbackDelegateEx() = default;

        virtual ~EventCallbackDelegateEx() override = default;

        /*for device control implenmented*/
        virtual void onRebootDevice(int32_t res) {
            /*
            * 很多设备没有该命令的回传消息
            * 考虑通过内部的设备热插拔监听机制来进行回调
            * TODO: 热插拔模块
            */
        };

        /*for manufacture test*/
        virtual void onSetUsedMics(uint32_t mic_index){ };

        virtual void onAudioRecord(bool success){ };

        virtual void onSetTestResult(bool success) {};

        virtual void onGetTestResult(bool success) {};

        virtual void onGetTestResult() {};

        virtual void onSetEncryptedInfo(bool success) {};

        virtual void onGetEncryptedInfo(bool success) {};

        virtual void onGetGyroSlopeAngle(bool success) {};

        virtual void onTestAudioConsistency(bool success) {};

        virtual void onTestNetworkPort(bool success) {};

        virtual void onGetDeviceUniqueId(bool success) {};

        virtual void onSetDeviceUniqueId(bool success) {};

        virtual void onGetAFCalibration(bool success) {};

        virtual void onGetDDRCapacity(bool success) {};

        virtual void onGetAlgAuthContent(bool success) {};

        virtual void onSetAlgAuthContent(bool success) {};

        virtual void onSetAFCalibration(bool success) {};

        virtual void onStartAudioTest(bool success) {};

        virtual void onGetEncryptedInfoHwid() {};

        virtual void onGetEncryptedInfoSn() {};

        virtual void onStartAudioConsistencyTest() {};

        virtual void onStopAudioConsistencyTest() {};

        virtual void onStartNetworkPortTest() {};

        virtual void onStopNetworkPortTest() {};

        virtual void onGetDeviceUid() {};

        virtual void onGetAlgAuthContent() {};

        virtual void onGetAFCalibrationResult() {};

        virtual void onGetDDRCapacity() {};

        virtual void onGetRS232TestResult() {};

        virtual void onDeviceKeyTest() {};

        virtual void onDeviceLedTest() {};

        virtual void onSDCardTestState(TestTaskResult test_res) {};

        virtual void onWifiTestState(TestTaskResult test_res) {};

        virtual void onBluetoothTestState(TestTaskResult test_res) {};

        virtual void onBadFlashBlockTestState(TestTaskResult test_res) {};
    };

    class AXDP_API DeviceAccessorEx : public DeviceAccessor {
    public:
        DeviceAccessorEx() = default;

        virtual ~DeviceAccessorEx() noexcept = default;

        /*Todo:api for manufacture test*/
        virtual int setMicUsed(uint32_t mic_index) = 0;
        virtual int startAudioRecord(uint32_t mic_mask) = 0;//use mic 0~3, mic_mask = (1<<0)|(1<<1)|(1<<2)|(1<<3)
        virtual int setTestResult(uint32_t test_index, uint32_t test_result) = 0;
        virtual int getTestResult(uint32_t test_index) = 0;
        virtual int setEncryptedInfoHardwareId(const char* hwid, uint32_t bytes) = 0;
        virtual int setEncryptedInfoSerialNumber(const char* sn, uint32_t bytes) = 0;
        virtual int getEncryptedInfo(uint32_t state) = 0;
        virtual int testAudioConsistency(TestTaskCommand cmd) = 0;
        virtual int testNetworkPort(TestTaskCommand cmd) = 0;
        virtual int getDeviceUniqueId(const char* lic_content, uint32_t bytes) = 0;
        virtual int setDeviceUniqueId() = 0;
        virtual int getAlgAuthContent() = 0;
        virtual int setAlgAuthContent(const char* auth_content, uint32_t bytes) = 0;
        virtual int setAFCalibration() = 0;
        virtual int getAFCalibrationResult() = 0;
        virtual int getDDRCapacity() = 0;
        virtual int setLensCenter(uint32_t value) = 0;
        virtual int getRS232TestResult() = 0;
        virtual int testKey(TestTaskCommand st) = 0;//0 for start, 1 for end
        virtual int testLed(TestTaskCommand st) = 0;//0 for start, 1 for end
        virtual int testSDCardState() = 0;
        virtual int testWIFIState(TestTaskCommand st) = 0;//1 for start, -1 for get result
        virtual int testBluetoothState(TestTaskCommand st) = 0;//1for start, -1for get result
        virtual int testBadFlashBlock() = 0;
        
        /*Video settings*/
        virtual int setWatermark(EnableState state) = 0;
        virtual int getWatermark() = 0;
        virtual int setPiPMode(EnableState mode) = 0;
        virtual int getPiPMode() = 0;
    };
}

#endif // !AXDP_API_EX_H_
