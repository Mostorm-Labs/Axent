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

#include "json.hpp"

#include "axdp_api.h"

#ifdef  WIN32
#include <Windows.h>
#endif
using namespace axdp;

static FILE* rec_file = NULL;

typedef struct DeviceAttr {
    int vid;
    int pid;
    int16_t dst;
    wchar_t* serial_number;
    const char* dfu_file;
    bool enable_ultra;
    uint8_t ultra_dst[33];
}DeviceAttr;

const char* fw_1192 = "/Users/qing/Desktop/gitee/axdp/example/Patus_01_AW20_PRO_V0.2_1.1.9_2_Female_English_20220407_ota.bin";
const char* fw_11132 = "/Users/qing/Desktop/gitee/axdp/example/Patus_01_AW20_PRO_V0.2_1.1.13_2_Female_English_20220530_ota.bin";
const char* fw_v15afl =
    "/Users/qing/Desktop/gitee/axdp/example/General.Hamedal_V15AFL.V1.100.0.1.R.20230327.0.0.6_ota.bin";
const char* fw_v20 =
    "/Users/qing/Desktop/gitee/axdp/example/General_MC_[Hamedal_V20]_V1.100.1.3.R.211008.bin";
const char* fw_a20s = "/Users/qing/Desktop/gitee/axdp/example/Superior_05_A20S_MB_1.1.16_20230525.ota.bin";

DeviceAttr gA20DeviceAttr = { 8137, 33387, 2,  NULL, fw_11132, false };
DeviceAttr gV20DeviceAttr = { 1317, 42156, 2, NULL, fw_v20 , false };
DeviceAttr gV15AFLDeviceAttr = { 1317, 42167, 2, NULL, fw_v15afl, false };
DeviceAttr gA20SDeviceAttr = { 8137, 33389, 2, NULL, fw_a20s, false };

DeviceAttr gTialDeviceAttr = {0x0627, 0xA6A0, 2, NULL, "", false};

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
        std::cout << "Reset device successfully" << std::endl;
    };

    void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) override {
        for (size_t i = 0; i < dev_count; i++) {
            std::cout << "device id: " << i << std::endl;
            std::cout << info[i].phy_version << std::endl;
            std::cout << info[i].product_name << std::endl;
            std::cout << info[i].serial_number << std::endl;
            std::cout << info[i].soft_version << std::endl;
            std::cout << info[i].unique_id << std::endl;
            std::cout << "Index id = " << info[i].index_id << std::endl;
            std::cout << std::endl;
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
        std::cout << "DFU STATE >> " << (int) state << " " << state_str << std::endl;
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        std::cout << "DFU PROGRESS  = " << progress << std::endl;
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
    };

    void onGetWifi(const uint8_t* ssid, uint16_t len) override {

        std::cout << "onGetWifi len = " << len << std::endl;
        for (size_t i = 0; i < len; i++)
        {
            //std::cout << "onGetWifi = " << ssid[i] << std::endl;
            printf("onGetWifi = %x\n", ssid[i]);
        }
    }
};

//设备升级测试
void axdp_test_dfu(const DeviceAttr& attr, UpdateStrategy strategy) {
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
    dev->getDeviceInfo();
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

//获取设备信息的同步接口
static void axdp_syncget_test(const DeviceAttr& attr) {
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
    return;
}


void axdp_sdk_demo_test(const DeviceAttr& attr) {
    nlohmann::json tail_json;
    tail_json["s"] = "washeng-302";
    tail_json["p"] = "Disc1234";
    tail_json["b"] = "80:69:1A:39:EB:20";

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

    std::string tail_json_data = tail_json.dump();
    int tail_json_len = tail_json_data.size() * sizeof(char);
    da->setTailWifiSSID(tail_json_data.c_str(), tail_json_len);

    Wait(2);
    
    /*设备重启*/
    da->reboot();
    std::cout << "Device start reboot" << std::endl;
    //销毁实例
    delete da;
    da = nullptr;
    Wait(5);
    
    //等待设备重启重新获取设备实例
    while(true){
        da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
        if (da) {
            //重新注册回调
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
    Wait(1);

    //设备固件升级
    da->resetDevice();
    //升级策略，不同设备的策略不同，A20设备为Dolphin
    da->setUpgradeStrategy(UpdateStrategy::Dolphin);
    //选择某个A20进行升级，0x00xxxxx0, x处为1表示该台A20需升级，从低至高为A20级联顺序
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

    //解除回调函数代理类，设备内部不维护该类的生命周期，需外部自行管理
    da->unregisterCbDelegate(cb_delegate);
    delete cb_delegate;
    cb_delegate = nullptr;
    //销毁设备实例
    delete da;
    da = nullptr;
    return;
}

int main(int argc, const char** argv) {
    if (argc < 5)
    {
        printf("Please enter to use: ./[demo_name.exe]\n"
            "-v [vid]\n"
            "-p [pid]\n"
            "-f [file_ota.bin]\n"
            "-s [update_strategy]\n"
            "-d [dst]\n"
            "-u [enable ulrtra dst]\n"
        );
        return -1;
    }

    DeviceAttr attr;
    memset(&attr, 0, sizeof(DeviceAttr));
    attr.vid = 0;
    attr.pid = 0;
    attr.dst = 2;
    attr.dfu_file = NULL;
    attr.serial_number = NULL;
    attr.enable_ultra = false;
    UpdateStrategy strategy = UpdateStrategy::Unknown;
    
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            attr.vid = strtol(argv[i + 1], NULL, strstr(argv[i + 1], "0x") == NULL ? 10 : 16);
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            attr.pid = strtol(argv[i + 1], NULL, strstr(argv[i + 1], "0x") == NULL ? 10 : 16);
        }
        /*else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            attr.serial_number = argv[i + 1];
        }*/
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            int st = strtol(argv[i + 1], NULL, 10);
            strategy = UpdateStrategy(st << 20);
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            attr.dfu_file = argv[i + 1];
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
        {
            attr.dst = (1 << strtol(argv[i + 1], NULL, 10));
        }
        else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc)
        {
            attr.enable_ultra = true;
            memcpy(attr.ultra_dst, argv[i + 1], strlen(argv[i + 1]));
        }
        else {
            printf("Invalid command line argument: %s\n", argv[i]);
            printf("Please enter to use: ./[demo_name.exe] -v [vid] -p [pid] -f [file_ota.bin] -s [update_strategy] -d [dst] -u\n");
            return -1;
        }
    }

    if (strategy == UpdateStrategy::Unknown)
    {
        printf("Invalid upgrade strategy value, please choose one from below: \n"
            "1 : V20 like\n"
            "2 : A20 like\n"
            "3 : C20 like\n"
            "4 : C30rk like\n"
            "5 : For prd test\n"
            "6 : A50 like\n"
            "7 : AMX100 like\n"
            "\n"
        );
        return -2;
    }

    printf("Going to be processed Device Info & DFU Info: \n"
        "VID:      %d\n"
        "PID:      %d\n"
        "DST:      1 << %d\n"
        "SUBINDEX: %s\n"
        "DFU_FILE: %s\n"
        "STRATEGY: 0x00%x\n"
        "\n",
        attr.vid, attr.pid, attr.dst, attr.ultra_dst, attr.dfu_file, strategy
    );

    axdp_test_dfu(attr, strategy);

    return 0;
}
