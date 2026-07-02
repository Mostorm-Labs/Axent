#include "axdp_example_common.h"
#include "axdp_api_ex.h"
#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdio.h>
#include <time.h>
#include <atomic>
#include <mutex>
#include <fstream>
#include <condition_variable>
#include <map>

#ifdef  WIN32
#include <Windows.h>
#endif
using namespace axdp;

static FILE* rec_file = NULL;


class EventCbEx : public EventCallbackDelegateEx {
public :
    EventCbEx() = default;

    ~EventCbEx() override = default;

    void onResetDevice(ResultState state) override {
        std::cout << "reset device successfully" << std::endl;
        //Signal();
    };

    void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) override {
        for (size_t i = 0; i < dev_count; i++) {
            std::cout << "device id: " << i << std::endl;
            std::cout << info[i].phy_version << std::endl;
            std::cout << info[i].product_name << std::endl;
            std::cout << info[i].serial_number << std::endl;
            std::cout << info[i].soft_version << std::endl;
            std::cout << info[i].unique_id << std::endl;
        }
        //Signal();
    };

    void onDfuStateUpdate(UpgradeState state) override {
        std::string state_str;
        switch (state) {
            case axdp::UpgradeState::DataReady:
                state_str = "Data is ready, start to transfer";
                break;
            case axdp::UpgradeState::Transferring:
                state_str = "Data is transferring";
                break;
            case axdp::UpgradeState::Verifying:
                state_str = "Data is verifying";
                break;
            case axdp::UpgradeState::Success:
                state_str = "Upgrade is success";
                Wait(5);
                Signal();
                break;
            case axdp::UpgradeState::Failed:
                state_str = "Upgrade is Failed";
                Signal();
                break;
            default:
                break;
        }
        std::cout << "DfuStateUpdate > " << (int) state
                  << " " << state_str << std::endl;
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        std::cout << "DFU UPGRADE PROGRESS  = " << progress << std::endl;
    };

    void onGetNoiseSuppressionLevel(DegreeLevel level) override {
        std::cout << "ANS LEVEL  = " << (int)level << std::endl;
    };

    void onGetReverbrationSuppressionLevel(DegreeLevel level) override {
        std::cout << "Reverb LEVEL  = " << (int)level << std::endl;
    };

    void onGetEchoCancellationLevel(DegreeLevel level) override {
        std::cout << "AEC LEVEL  = " << (int)level << std::endl;
    };

    void onHumanInterfaceEvent(HIDEvent ev) override {
        std::string state_str;
        switch (ev) {
            case axdp::HIDEvent::Mute:
                state_str = "DEVICE_MUTE";
                break;
            case axdp::HIDEvent::Unmute:
                state_str = "DEVICE_UNMUTE";
                break;
            case axdp::HIDEvent::Answer:
                state_str = "DEVICE_ANSWER";
                break;
            case axdp::HIDEvent::HangUp:
                state_str = "DEVICE_HANG_UP";
                break;
            case axdp::HIDEvent::VolDown:
                state_str = "DEVICE_VOL_DOWN";
                break;
            case axdp::HIDEvent::VolUp:
                state_str = "DEVICE_VOL_UP";
                break;
            default:
                break;
        }
        std::cout << "Hid on listen > "
                  << " " << state_str << std::endl;
    }

    void onAudioRecordStart(bool supported, const AudioFormat* format) override {
        std::cout << "Device Supported Record " << supported << std::endl;
        std::cout << "Device Record AudioFormat: " << std::endl;
        std::cout << "   Audio type: " << format->audio_type << std::endl;
        std::cout << "   Samplerate: " << format->sample_rate << std::endl;
        std::cout << "  Channel num: " << format->channel_num << std::endl;
        std::cout << "    Bit Width: " << format->bit_width   << std::endl;
    }

    void onAudioRecordData(const uint8_t* data, int32_t len) override {
        if (rec_file == NULL) {
            rec_file = fopen("rec_file.pcm", "wb");
            std::cout << "Open File To Record Data." << std::endl;
        }
        if (rec_file != NULL) {
            int res = fwrite(data, len, 1, rec_file);
            if (res) {
                //do sth
            }
        }
    }

    void onAudioRecordStopped(int32_t result) override {
        std::cout << "Stop Rec to Close File, result = " << result << std::endl;
    }

    void onGetConfigJson(const uint8_t* payload, uint16_t len, uint16_t src)override {
        std::cout << "config json from src = " << src
            << "\n config json content = \n" << payload << std::endl;
    }


    void onSDCardTestState(TestTaskResult test_res) override {
        std::cout << "SDCARD TEST RESULT = " << test_res << std::endl;
    };

    void onWifiTestState(TestTaskResult test_res) override {
        std::cout << "WIFI TEST RESULT = " << test_res << std::endl;
    };

    void onBluetoothTestState(TestTaskResult test_res)override {
        std::cout << "BLE TEST RESULT = " << test_res << std::endl;
    };

    void onBadFlashBlockTestState(TestTaskResult test_res)override {
        std::cout << "BAD FB TEST RESULT = " << test_res << std::endl;
    };

};

void axdp_vm33_test(const DeviceAttr& attr) {
    //创建设备实例
    DeviceAccessor *da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    DeviceAccessorEx* daex = dynamic_cast<DeviceAccessorEx*>(da);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }
    EventCallbackDelegate* cb_delegate = new EventCbEx();
    da->registerCbDelegate(cb_delegate);
    //重置设备状态，一般生成设备实例后都需要调用一次
    da->resetDevice();
    //获取设备信息，异步接口，由回调函数onGetDeviceInformation返回数据
    da->getDeviceInfo();

    //sd卡测试
    daex->testSDCardState();
    //wifi测试
    daex->testWIFIState(TestTaskCommand::CommandStart);
    int wifi_test_count = 0;
    for (size_t i = 0; i < 5; i++)
    {
        Wait(2);
        daex->testWIFIState(TestTaskCommand::CommandResult);
    }
    //蓝牙测试
    daex->testBluetoothState(TestTaskCommand::CommandStart);
    int ble_test_count = 0;
    for (size_t i = 0; i < 5; i++)
    {
        Wait(2);
        daex->testBluetoothState(TestTaskCommand::CommandResult);
    }
    //flash坏块测试
    daex->testBadFlashBlock();

    Wait(5);
    //解除回调函数代理类，设备内部不维护该类的生命周期，需外部自行管理
    da->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    //销毁设备实例
    delete da;
}
