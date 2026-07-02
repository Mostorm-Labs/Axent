#include "axdp_example_common.h"

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

const char* fw_1192 = "/Users/qing/Desktop/gitee/axdp/resources/Patus_01_AW20_PRO_V0.2_1.1.9_2_Female_English_20220407_ota.bin";
const char* fw_11132 = "D:/GiteeRepos/axdp/resources/Patus_01_AW20_PRO_V0.2_1.1.13_2_Female_English_20220530_ota.bin";
const char* fw_v15afl = "/Users/qing/Desktop/gitee/axdp/resources/General.Hamedal_V15AFL.V1.100.0.1.R.20230327.0.0.6_ota.bin";
const char* fw_v20 = "/Users/qing/Desktop/gitee/axdp/resources/General_MC_[Hamedal_V20]_V1.100.1.3.R.211008.bin";
const char* fw_a20s = "/Users/qing/Desktop/gitee/axdp/resources/Superior_05_A20S_MB_1.1.16_20230525.ota.bin";
const char* fw_c30r = "D:/GiteeRepos/axdp/resources/General_MC_[Hamedal_C30R]_V1.100.1.4.R.230421.bin";
const char* fw_awm10t = "General_MC_[AWM10T]_V2.100.1.0.R.20240531RX_V2.100.1.0.R.20240531TX_ota.bin";
const char* fw_amx100 = "";
DeviceAttr gA20DeviceAttr = { 8137, 33387, 6,  NULL, fw_11132, false };
DeviceAttr gV20DeviceAttr = { 1317, 42156, 2, NULL, fw_v20 , false };
DeviceAttr gV15AFLDeviceAttr = { 1317, 42167, 2, NULL, fw_v15afl, false };
DeviceAttr gA20SDeviceAttr = { 8137, 33389, 2, NULL, fw_a20s, false };
DeviceAttr gAMX100DeviceAttr = {8137, 33388, 2, NULL, fw_amx100, false};
DeviceAttr gV520DDeviceAttr = {1575,42688,2,NULL,fw_amx100,false};
DeviceAttr gC30RDeviceAttr = { 1575, 42671,2,NULL, fw_c30r, false };
DeviceAttr gAWM10TRDeviceAttr = { 6675, 9282, 2, NULL, fw_awm10t, false };
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

class CommonEventCbEx : public EventCallbackDelegate {
public:
    CommonEventCbEx() = default;

    ~CommonEventCbEx() override = default;

    void onResetDevice(ResultState state) override {
        std::cout << "reset device successfully" << std::endl;
        //Signal();
    };

    void onGetDeviceInformation(const DeviceInfo* info, uint16_t dev_count) override {
        std::cout << "Async get device info as below:" << std::endl;
        for (size_t i = 0; i < dev_count; i++) {
            std::cout << "device id: " << i << std::endl;
            std::cout << info[i].phy_version << std::endl;
            std::cout << info[i].product_name << std::endl;
            std::cout << info[i].serial_number << std::endl;
            std::cout << info[i].soft_version << std::endl;
            std::cout << info[i].unique_id << std::endl;
        }
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
            state_str = "Upgrade success";
            Wait(5);
            Signal();
            break;
        case axdp::UpgradeState::Failed:
            state_str = "Upgrade Failed";
            Signal();
            break;
        default:
            break;
        }
        std::cout << "DfuStateUpdate > " << (int)state << " " << state_str << std::endl;
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        std::cout << "DFU progress  = " << progress << std::endl;
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
        std::cout << "Hid on listen > " << " " << state_str << std::endl;
    };

    void onGetConfigJson(const uint8_t* payload, uint16_t len, uint16_t src)override {
        std::cout << "config json from src = " << src
            << "\n config json content = \n" << payload << std::endl;
    }
};

//设备升级测试
void axdp_dfu_process(const DeviceAttr& attr, UpdateStrategy strategy) {
    //Create device instance
    DeviceAccessor* dev = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (dev == nullptr)
    {
        std::cout << "Create device instance failed, EXITED." << std::endl;
        return;
    }
    EventCallbackDelegate* cb_delegate = new CommonEventCbEx();
    dev->registerCbDelegate(cb_delegate);
    dev->resetDevice();
    //dev->getDeviceInfo();
    Wait(2);
    //std::cout << "device info = " << deviceinfo[0].soft_version << std::endl;
    dev->setUpgradeStrategy(strategy);//A20的升级策略为Dophine选项
    dev->setUpgradeDeviceDst(attr.dst);
    if (attr.enable_ultra)
    {
        dev->setUpgradeUltraDstEnabled(EnableState::Enabled);
        dev->setUpgradeDeviceUltraDst(&attr.ultra_dst[0], 32);
        printf("Sub index is enabled and subindex = %s\n", &attr.ultra_dst[0]);
    }
    int ret = dev->startFirmwareUpgrade(attr.dfu_file);
    if (ret != 0) {
        std::cout << "prepare data for dfu failed! ret = " << ret << std::endl;
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


//获取设备信息及配置
void axdp_info_process(const DeviceAttr& attr){
    //Create device Instance pointer
    DeviceAccessor* da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }

    //Register callbacks
    EventCallbackDelegate* cb_delegate = new CommonEventCbEx();
    da->registerCbDelegate(cb_delegate);

    //Reset device
    da->resetDevice();

    std::cout << "getDeviceInfo: begin"<< std::endl;
    da->getDeviceInfo();

    Wait(7);

    //ULONGLONG tickCount = GetTickCount64();
    //std::cout << "json:" << tickCount << std::endl;
    da->getConfigJson(2);


    //Get device basic info
    auto info =  da->syncGetDeviceInfo();
    if (info.empty())
    {
        std::cout << "Device info failed" << std::endl;
    } else{
        std::cout << "Sync get device info as below:" << std::endl;
        for (size_t i = 0; i < info.size(); i++) {
            std::cout << "device id: " << i << std::endl;
            std::cout << info[i].phy_version << std::endl;
            std::cout << info[i].product_name << std::endl;
            std::cout << info[i].serial_number << std::endl;
            std::cout << info[i].soft_version << std::endl;
            std::cout << info[i].unique_id << std::endl;
        }
    }
    da->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    delete da;
    return;
}

//Json配置文件读写
void axdp_config_process(const DeviceAttr& attr){

}

void axdp_v520d_sdk_demo(const DeviceAttr& attr) {
    //Create device Instance pointer
    DeviceAccessor* da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (da == nullptr)
    {
        std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        return;
    }

    //Register callbacks
    EventCallbackDelegate* cb_delegate = new CommonEventCbEx();
    da->registerCbDelegate(cb_delegate);
    
    //Reset device
    da->resetDevice();

    std::string config_json;

    //Get device basic info 
    auto device_info =  da->syncGetDeviceInfo();
    if (device_info.empty())
    {
        std::cout << "Device info failed" << std::endl;
    }

    //Get Device config json info
    auto device_config = da->syncGetConfigJson(2);
    if (device_config.empty())
    {
        std::cout << "Device config failed" << std::endl;
    }

    //AI Mode select��
    //   int :
    //   0-->Manual mode
    //   1-->Panorama View mode
    //   2-->Speaker tracking mode
    //   3-->Area Tracking 
    //   4-->Auto-Framing 

    //Set device to 4-->Auto-Framing mode
    config_json = 
        "{"
            "\"ai\":{"
            "\"aiMode\" : 2}"
        "}";
    da->setConfigJson(config_json.c_str(), config_json.length(), 2);
    Wait(5);

    //switch device to 1-->Panorama View mode
    config_json.assign(
        "{"
        "\"ai\":{"
        "\"aiMode\" : 1}"
        "}");
    da->setConfigJson(config_json.c_str(), config_json.length(), 2);
    Wait(5);
    
    da->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    delete da;
}
