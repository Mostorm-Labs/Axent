#include "axdp_main_test.h"
#include "axdp_api.h"
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

DeviceAttr g_A20_device_attr = { 8137, 33387,  NULL, "../example/a20.bin" };
DeviceAttr g_V520D_device_attr = { 1575, 42688, NULL, NULL };
DeviceAttr g_vm33_attr = { 1317, 42195, NULL, NULL };

std::mutex g_mt;
std::condition_variable g_cv;
long g_task_count;

void Signal() {
    std::unique_lock<std::mutex> unique(g_mt);
    ++g_task_count;
    if (g_task_count <= 0)
        g_cv.notify_one();
}

void Wait(int timeout_s) {
    std::unique_lock<std::mutex> unique(g_mt);
    --g_task_count;
    if (g_task_count < 0)
        g_cv.wait_for(unique, std::chrono::seconds(timeout_s));
}

class EventCb : public EventCallbackDelegate {
public :
    EventCb() = default;

    ~EventCb() = default;

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

    void onGetConfigJson(const uint8_t* payload, uint16_t len, uint16_t src) {
        std::cout << "config json from src = " << src
            << "\n config json content = \n" << payload << std::endl;
    }
};

void axdp_test_dfu(const DeviceAttr& attr) {
    //Create device instance
    DeviceAccessor* dev = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (dev == nullptr)
    {
        std::cout << "Create device instance failed, EXITED." << std::endl;
        return;
    }
    EventCallbackDelegate* cb_delegate = new EventCb();
    dev->registerCbDelegate(cb_delegate);
    dev->resetDevice();
    dev->setUpgradeStrategy(UpdateStrategy::Dolphin);
    dev->setUpgradeDeviceDst(0x2);
    int ret = dev->startFirmwareUpgrade(attr.dfu_file);
    if (ret != 0) {
        std::cout << "Prepare data for DFU failed! ret = " << ret << std::endl;
        return;
    }
    else {
        Wait(1000);
    }
    Wait(1);
    dev->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    delete dev;
}

void axdp_sdk_demo_test(const DeviceAttr& attr) {
    //创建设备实例
    DeviceAccessor *da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }
    EventCallbackDelegate* cb_delegate = new EventCb();
    da->registerCbDelegate(cb_delegate);
    //重置设备状态，一般生成设备实例后都需要调用一次
    da->resetDevice();
    //获取设备信息，异步接口，由回调函数onGetDeviceInformation返回数据
    da->getDeviceInfo();

    /*设备重启*/
    Wait(5);
    //重启设备
    da->reboot();
    std::cout << "Device start reboot" << std::endl;
    //销毁实例
    delete da;
    da = nullptr;
    //等待设备重启重新获取设备实例
    while(true){
        da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
        if (da) {
            da->registerCbDelegate(cb_delegate);
            da->resetDevice();
            std::cout << "Device restarted" << std::endl;
            break;
        }
        else {
            std::cout << "Device is still off, wait for 1s" << std::endl;
            Wait(1);
        }
    }
    /*参数设置*/
    //获取A20 ANS降噪等级
    da->getNoiseSuppressionLevel();
    //获取A20 AEC降噪等级
    da->getEchoCancellationLevel();
    //获取A20 混响抑制等级
    da->getReverberationSuppressionLevel();
    //设置A20降噪等级参数
    da->setNoiseSuppressionLevel(DegreeLevel::UltraHigh);
    std::cout << "setNoiseSuppressionLevel 4" << std::endl;
    //获取A20降噪等级参数
    da->getNoiseSuppressionLevel();
    //设置A20回声抑制等级参数
    da->setEchoCancellationLevel(DegreeLevel::UltraHigh);
    std::cout << "setEchoCancellationLevel 4" << std::endl;
    //获取A20回声抑制等级参数
    da->getEchoCancellationLevel();
    //设置混响抑制等级
    da->setReverberationSuppressionLevel(DegreeLevel::UltraHigh);
    std::cout << "setReverberationSuppressionLevel 4" << std::endl;
    //获取混响抑制等级
    da->getReverberationSuppressionLevel();
    //重置音频算法参数
    da->resetAudioAlgorithmParams();
    std::cout << "Reset Audio Alg Params" << std::endl;
    //再次获取这些参数
    da->getNoiseSuppressionLevel();
    da->getEchoCancellationLevel();
    da->getReverberationSuppressionLevel();

    /*设备录音*/
    Wait(3);
    //开始录音后，会由回调函数onAudioRecordStart告知设备是否支持录音
    //允许录音情况下，由回调函数onAudioRecordData返回录音数据
    da->startAudioRecord(5);
    Wait(5);
    //停止录音，可以提前停止
    da->stopAudioRecord();

    Wait(1);

    //设备固件升级
    da->resetDevice();
    //升级策略，不同设备的策略不同，A20设备为Dolphin
    da->setUpgradeStrategy(UpdateStrategy::Dolphin);
    da->setUpgradeDeviceDst(0x2);
    int ret = da->startFirmwareUpgrade(attr.dfu_file);
    if (ret != 0) {
        std::cout << "Prepare data for DFU failed! ret = " << ret << std::endl;
        return;
    }
    else {
        //成功开始升级，等待回调onDfuStateUpdate显示状态变更
        //以及回调onDfuProgressUpdate升级进度变更
        Wait(1000);
    }
    Wait(1);

    //解除回调函数代理类，设备内部不维护该类的生命周期，需外部自行管理
    da->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    //销毁设备实例
    delete da;
}

static void sync_interface_test(const DeviceAttr& attr) {
    //创建设备实例
    DeviceAccessor* da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }
    EventCallbackDelegate* callback = new EventCb();
    da->registerCbDelegate(callback);
    da->resetDevice();
    da->getDeviceInfo();
    const DeviceInfoList& device_info_list = da->syncGetDeviceInfo();
    if (device_info_list.empty())
    {
        std::cout << "error get device number" << std::endl;
    }
    else
    {
        std::cout << "Get device info successfully , device count = " 
            << device_info_list.size() << std::endl;
        for (size_t i = 0; i < device_info_list.size(); i++)
        {
            std::cout << "device id: " << i << std::endl;
            std::cout << device_info_list[i].phy_version << std::endl;
            std::cout << device_info_list[i].product_name << std::endl;
            std::cout << device_info_list[i].serial_number << std::endl;
            std::cout << device_info_list[i].soft_version << std::endl;
            std::cout << device_info_list[i].unique_id << std::endl;
        }
    }
    Wait(10);
    da->unregisterCbDelegate(callback);
    delete callback;
    delete da;
    
}

//config demo test
static void config_json_demo(int argc, const char** argv) {
    //创建设备实例


    if (argc < 2)
    {
        std::cout << "use ./*.exe i/j/s/a/g/x enter" << std::endl;
        return ;
    }

    std::cout << argv[0] << " " << argv[1] << std::endl;

    const DeviceAttr& attr = g_V520D_device_attr;
    DeviceAccessor* da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }
    EventCallbackDelegate* callback = new EventCb();
    da->registerCbDelegate(callback);
    da->resetDevice();

    if (strcmp(argv[1], "i") == 0)
    {
        std::cout << "on get device info " << argv[1] << std::endl;
        da->getDeviceInfo();
    }
    else if (strcmp(argv[1], "j") == 0) {
        std::cout << "on get json info " << argv[1] << std::endl;
        da->getConfigJson(2);
    }
    Wait(1);
    goto end;

    while (true)
    {
        char c = getchar();
        char n = getchar();
        if (n != '\n')
        {
            continue;
        }
        switch (c)
        {
        case 'i'://async get info frome device
        {
            da->getDeviceInfo();
            break;
        }
        case 'j'://async get json config
        {
            da->getConfigJson(2);
            break;
        }
        case 's'://set get device info and json config to device
        {
            const char* json = "{\"a\":\"b\"}";
            da->setConfigJson(json, strlen(json), 2);
            break;
        }
        case 'a'://sync get json config
        {
            const DeviceInfoList& device_info_list = da->syncGetDeviceInfo();
            if (device_info_list.empty())
            {
                std::cout << "error get device number" << std::endl;
            }
            else
            {
                std::cout << "Get device info successfully , device count = "
                    << device_info_list.size() << std::endl;
                for (size_t i = 0; i < device_info_list.size(); i++)
                {
                    std::cout << "device id: " << i << std::endl;
                    std::cout << device_info_list[i].phy_version << std::endl;
                    std::cout << device_info_list[i].product_name << std::endl;
                    std::cout << device_info_list[i].serial_number << std::endl;
                    std::cout << device_info_list[i].soft_version << std::endl;
                    std::cout << device_info_list[i].unique_id << std::endl;
                }
            }

            const std::string& config_json = da->syncGetConfigJson(2);
            if (device_info_list.empty())
            {
                std::cout << "error get device number" << std::endl;
            }
            else
            {
                std::cout << "Get device info successfully , device count = "
                    << device_info_list.size() << std::endl;
                for (size_t i = 0; i < device_info_list.size(); i++)
                {
                    std::cout << "device id: " << i << std::endl;
                    std::cout << device_info_list[i].phy_version << std::endl;
                    std::cout << device_info_list[i].product_name << std::endl;
                    std::cout << device_info_list[i].serial_number << std::endl;
                    std::cout << device_info_list[i].soft_version << std::endl;
                    std::cout << device_info_list[i].unique_id << std::endl;
                }
            }
            break;
        }
        case 'g'://get json config syncly
        {
            da->getConfigJson(2);
            break;
        }
        case 'x':
        {
            std::cout << "Program Exited" << std::endl;
            goto end;
        }
        default:
            std::cout << "use i/j/s/a/g/x enter" << std::endl;
            break;
        }
    }

    Wait(10);
end:
    da->unregisterCbDelegate(callback);
    delete callback;
    delete da;
}

