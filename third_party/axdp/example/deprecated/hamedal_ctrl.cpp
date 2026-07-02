#include "axdp_api.h"
#include "third_party/json/single_include/nlohmann/json.hpp"
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
using json = nlohmann::json;
static FILE* rec_file = NULL;


typedef struct DeviceAttr {
    const char* name;
    int vid;
    int pid;
    wchar_t* serial_number;
    const char* dfu_file;
}DeviceAttr;

DeviceAttr gA20DevAttr = { "A20", 8137, 33387,  NULL, "../example/a20.bin" };
DeviceAttr gV520DDevAttr = { "V520D", 1575, 42688, NULL, NULL };
DeviceAttr gV20DevAttr = { "V20", 1317, 42156, NULL, "../example/v20.bin"};

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
    explicit EventCb(DeviceAttr attr):attr_(attr){};

    ~EventCb() = default;

    void onResetDevice(ResultState state) override {
        //std::cout << "reset device successfully" << std::endl;
        //Signal();
    };

    void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) override {
        dev_info_list_.clear();
        dev_info_list_.resize(dev_count);
//        for (size_t i = 0; i < dev_count; i++) {
//            std::cout << "device id: " << i << std::endl;
//            std::cout << info[i].phy_version << std::endl;
//            std::cout << info[i].product_name << std::endl;
//            std::cout << info[i].serial_number << std::endl;
//            std::cout << info[i].soft_version << std::endl;
//            std::cout << info[i].unique_id << std::endl;
//        }
        for (size_t i = 0; i < dev_count; i++) {
            dev_info_list_[i].dev_type = info[i].dev_type;
            memcpy(dev_info_list_[i].phy_version, info[i].phy_version, 64);
            memcpy(dev_info_list_[i].product_name, info[i].product_name, 64);
            memcpy(dev_info_list_[i].soft_version, info[i].soft_version, 64);
            memcpy(dev_info_list_[i].serial_number, info[i].serial_number, 64);
            memcpy(dev_info_list_[i].unique_id, info[i].unique_id, 64);
        }
        Signal();
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
        ///std::cout << "DfuStateUpdate > " << (int) state
        //          << " " << state_str << std::endl;
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        //std::cout << "DFU UPGRADE PROGRESS  = " << progress << std::endl;
    };

    void onGetNoiseSuppressionLevel(DegreeLevel level) override {
        //std::cout << "ANS LEVEL  = " << (int)level << std::endl;
    };

    void onGetReverbrationSuppressionLevel(DegreeLevel level) override {
        //std::cout << "Reverb LEVEL  = " << (int)level << std::endl;
    };

    void onGetEchoCancellationLevel(DegreeLevel level) override {
        //std::cout << "AEC LEVEL  = " << (int)level << std::endl;
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
        //std::cout << "Hid on listen > "
        //          << " " << state_str << std::endl;
    }

    void onAudioRecordStart(bool supported, const AudioFormat* format) override {
        //std::cout << "Device Supported Record " << supported << std::endl;
        //std::cout << "Device Record AudioFormat: " << std::endl;
        //std::cout << "   Audio type: " << format->audio_type << std::endl;
        //std::cout << "   Samplerate: " << format->sample_rate << std::endl;
        //std::cout << "  Channel num: " << format->channel_num << std::endl;
        //std::cout << "    Bit Width: " << format->bit_width   << std::endl;
    }

    void onAudioRecordData(const uint8_t* data, int32_t len) override {
        if (rec_file == NULL) {
            rec_file = fopen("rec_file.pcm", "wb");
            //std::cout << "Open File To Record Data." << std::endl;
        }
        if (rec_file != NULL) {
            size_t res = fwrite(data, len, 1, rec_file);
            if (res) {
                //do sth
            }
        }
    }

    void onAudioRecordStopped(int32_t result) override {
        //std::cout << "Stop Rec to Close File, result = " << result << std::endl;
    }

    void onGetConfigJson(const uint8_t* payload, uint16_t len, uint16_t src) override{
        //std::cout << "config json from src = " << src
        //    << "\n config json content = \n" << payload << std::endl;
    }
    
    void onGetUacState(EnableState state) override {
        //std::cout << "Get device uac state : " << int32_t(state) << std::endl;
        mic_enabled_ = state;
    }
    
    void onGetVideoMode(VideoMode mode) override {
        //std::cout << "Get video mode : " << int32_t(mode) << std::endl;
        video_mode_ = mode;
    }
    
    std::string returnDeviceInfo() {
        std::string result;
        json obj;
        obj["count"] = dev_info_list_.size();
        obj["list"];
        for (int i = 0; i < dev_info_list_.size(); i++) {
            json element;
            element["index"] = i;
            element["serial_number"] = dev_info_list_[i].serial_number;
            element["version"] = dev_info_list_[i].soft_version;
            if (strcmp(attr_.name, "V20") == 0) {
                element["smart_track_video_enabled"] = video_mode_;
                element["mic_enabled"] = mic_enabled_;
            }
            obj["list"].push_back(element);
        }
        json res;
        res[attr_.name] = obj;
        result.assign(res.dump());
        return result;
    }
    
private:
    DeviceInfoList dev_info_list_;
    VideoMode video_mode_;
    EnableState mic_enabled_;
    DeviceAttr attr_;
};

/*
* 设备升级测试
* DFU Test
*/
void axdp_test_dfu(const DeviceAttr& attr) {
    //Create device instance
    DeviceAccessor* dev = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    if (dev == nullptr)
    {
        std::cout << "Create device instance failed, EXITED." << std::endl;
        return;
    }
    EventCallbackDelegate* cb_delegate = new EventCb(attr);
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

enum class DeviceErrorCode {
    
};

enum class OperationCode {
    GetDeviceInfo = 0,
    SetMicState,
    SetVideoState,
    Reboot,
    DFU
};

static OperationCode g_operation_code;
static DeviceAttr g_dev_attr;

static int parseConfigParams(int argc, const char** argv){
    if (argc < 2) {
        return -1;
    }
    return 0;
}

int DeviceMainCtrl(DeviceAttr dev_attr, OperationCode oc, int val) {
    const DeviceAttr& attr = dev_attr;
    DeviceAccessor* da = DeviceAccessor::Create(attr.vid, attr.pid, attr.serial_number);
    EventCallbackDelegate* callback = new EventCb(attr);
    if (da == nullptr)
    {
        //std::cout << "Device Accessor Create failed, Program EXITED!" << std::endl;
        EventCb* ecb = dynamic_cast<EventCb*>(callback);
        std::string infodata = ecb->returnDeviceInfo();
        std::cout << infodata << std::endl;
        return -1;
    }
    
    da->registerCbDelegate(callback);
    switch (oc) {
        case OperationCode::GetDeviceInfo:{
            if (strcmp(attr.name, "V20") == 0) {
                da->getVideoMode();
                da->getUacState();
            }
            da->getDeviceInfo();
            Wait(5);
            EventCb* ecb = dynamic_cast<EventCb*>(callback);
            std::string infodata = ecb->returnDeviceInfo();
            std::cout << infodata << std::endl;
            //Wait(50000);
            break;
        }
        case OperationCode::SetVideoState:{
            if (da) {
                if (strcmp(attr.name, "V20") == 0)  {
                    da->setVideoMode(VideoMode(val));
                }
            }
            break;
        }
        case OperationCode::SetMicState:{
            if (da) {
                if (strcmp(attr.name, "V20") == 0) {
                    da->setUacState(axdp::EnableState(val));
                }
            }
            break;
        }
        case OperationCode::Reboot:{
            if (da) {
                da->reboot();
            }
            break;
        }
        default:
            break;
    }
    
    da->unregisterCbDelegate(callback);
    delete da;
    delete callback;
    da = nullptr;
    callback = nullptr;
    return 0;
}

typedef struct CmdInfo{
    DeviceAttr attr;
    OperationCode oc;
    int val;
}CmdInfoSt;

static int parseAndExecCmd(int argc, const char** argv, CmdInfo& ci){
    if (argc <= 2) {
        return -1;
    }
    int i = 0;
    while (i < argc) {
        
        i++;
        //printf("arg %d is %s", i, argv[i]);
        if (strcmp(argv[i], "-info") == 0) {
            ci.oc = OperationCode::GetDeviceInfo;
            i++;
            if (i<argc) {
                if (strcmp(argv[i], "a20") == 0){
                    ci.attr = gA20DevAttr;
                }else if (strcmp(argv[i], "v20") == 0){
                    ci.attr = gV20DevAttr;
                }else{
                    return -1;
                }
            }
            return 0;
        }else if (strcmp(argv[i], "-mic") == 0){
            ci.oc = OperationCode::SetMicState;
            ci.attr = gV20DevAttr;
            i++;
            if (i < argc) {
                if (strcmp(argv[i], "1") == 0) {
                    ci.val = 1;
                }else if (strcmp(argv[i], "0") == 0){
                    ci.val = 0;
                }else{
                    return -1;
                }
            }
            return 0;
        }else if (strcmp(argv[i], "-video") == 0){
            ci.oc = OperationCode::SetVideoState;
            ci.attr = gV20DevAttr;
            i++;
            if (i < argc) {
                if (strcmp(argv[i], "1") == 0) {
                    ci.val = 1;
                }else if (strcmp(argv[i], "0") == 0){
                    ci.val = 0;
                }else{
                    return -1;
                }
                return 0;
            }
        }else if (strcmp(argv[i], "-reboot") == 0){
            ci.oc = OperationCode::Reboot;
            i++;
            if (i<argc) {
                if (strcmp(argv[i], "a20") == 0){
                    ci.attr = gA20DevAttr;
                }else if (strcmp(argv[i], "v20") == 0){
                    ci.attr = gV20DevAttr;
                }else{
                    return -1;
                }
                return 0;
            }
        }else if (strcmp(argv[i], "-dfu") == 0){
            ci.oc = OperationCode::DFU;
            return -1;
        }else{
            return -1;
        }
    }
    return 0;
}

int main(int argc, const char** argv) {
    CmdInfo ci;
    memset(&ci, 0, sizeof(CmdInfo));
    int res = parseAndExecCmd(argc, argv, ci);
    if (res == 0) {
        DeviceMainCtrl(ci.attr, ci.oc, ci.val);
    }else{
        return -1;
    }
    //if (strcmp(argv[2], "a20") == 0) {
        //DeviceMainCtrl(gA20DevAttr, OperationCode::GetDeviceInfo, 0);
    //}
   
    //DeviceMainCtrl(gA20DevAttr, OperationCode::Reboot, 0);
    //DeviceMainCtrl(gV20DevAttr, OperationCode::SetVideoState, 0);
    //DeviceMainCtrl(gV20DevAttr, OperationCode::SetMicState, 0);
    //Wait(5);
    //if (strcmp(argv[2], "v20") == 0) {
    //    DeviceMainCtrl(gV20DevAttr, OperationCode::GetDeviceInfo, 0);
    //}
    
    //DeviceMainCtrl(gV20DevAttr, OperationCode::Reboot, 0);
    return 0;
}

