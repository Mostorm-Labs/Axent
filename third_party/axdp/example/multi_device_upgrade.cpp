#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <memory>
#include <map>
#include <thread>
#include <chrono>
#include <future>
#include "axdp_api.h"
#include "axdp_defines.h"
#include "axdp_example_common.h"
#include "hidapi.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace axdp;

// 自定义回调类，用于接收升级进度
class UpgradeProgressCallback : public EventCallbackDelegate {
public:
    explicit UpgradeProgressCallback(const std::wstring& serialNumber) : serialNumber_(serialNumber) {}

    void onDfuProgressUpdate(uint16_t progress) override {
        lastProgress = progress; // 更新进度
        std::wcout << L"Device " << serialNumber_ << L" upgrade progress: " << progress << L"%" << std::endl;
    }
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
        try {
            devCountPromise_.set_value(dev_count);
        } catch (...) {}
    }

    std::future<uint16_t> getDevCountFuture() {
        return devCountPromise_.get_future();
    }

    void onDfuStateUpdate(UpgradeState state) override {
        upgradeState = state; // 更新状态
        std::wstring state_str;
        switch (state) {
        case UpgradeState::DataReady:
            state_str = L"Data is ready, start to transfer";
            break;
        case UpgradeState::Transferring:
            state_str = L"Data is transferring";
            break;
        case UpgradeState::Verifying:
            state_str = L"Data is verifying";
            break;
        case UpgradeState::Success:
            state_str = L"Upgrade success";
            break;
        case UpgradeState::Failed:
            state_str = L"Upgrade Failed";
            break;
        default:
            break;
        }
        std::wcout << L"Device " << serialNumber_ << L" upgrade state: " << (int)state << L" " << state_str << std::endl;
    }

public:
    uint16_t getLastProgress() const { return lastProgress; }
    UpgradeState getUpgradeState() const { return upgradeState; }

private:
    std::wstring serialNumber_; // 设备序列号
    uint16_t lastProgress = 0; // 上次报告的进度
    UpgradeState upgradeState = UpgradeState::DataReady; // 当前升级状态
    std::promise<uint16_t> devCountPromise_;
};

// 获取设备序列号（使用std::wstring处理）
bool getDeviceSerialNumber(const std::string& devicePath, std::wstring& serialNumber, uint16_t vid, uint16_t pid) {
    struct hid_device_info *devs, *cur_dev;
    devs = hid_enumerate(vid, pid);
    cur_dev = devs;

    while (cur_dev) {
        if (devicePath == cur_dev->path) {
            // 使用std::wstring存储序列号
            serialNumber = cur_dev->serial_number ? std::wstring(cur_dev->serial_number) : std::wstring(L"Unknown");
            hid_free_enumeration(devs);
            return true;
        }
        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);
    return false;
}

// 枚举所有匹配vid和pid的设备
std::vector<std::string> enumerateDevices(uint16_t vid, uint16_t pid) {
    std::vector<std::string> devicePaths;
    struct hid_device_info *devs, *cur_dev;

    devs = hid_enumerate(vid, pid);
    cur_dev = devs;

    while (cur_dev) {
        // 只升级usage == 130 && usage_page == 129的设备
        if (vid == cur_dev->vendor_id && pid == cur_dev->product_id && cur_dev->usage == 130 && cur_dev->usage_page == 129) {
            devicePaths.push_back(cur_dev->path);
            std::cout << "Found device: " << cur_dev->path << std::endl;
            // 使用std::wstring存储序列号
            std::wstring serialNum = cur_dev->serial_number ? std::wstring(cur_dev->serial_number) : std::wstring(L"Unknown");

#ifdef _WIN32
            // Windows平台下使用wcout打印宽字符
            std::wcout << L"  Serial Number: " << serialNum << std::endl;
#else
            // 非Windows平台下的处理方式
            std::cout << "  Serial Number: " << (cur_dev->serial_number ? reinterpret_cast<const char*>(serialNum.c_str()) : "Unknown") << std::endl;
#endif
        }
        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);
    return devicePaths;
}

// 将UpdateStrategyId转换为UpdateStrategy枚举
UpdateStrategy getUpdateStrategy(int strategyId) {
    switch (strategyId) {
        case 1: return UpdateStrategy::Falcon;
        case 2: return UpdateStrategy::Dolphin;
        case 3: return UpdateStrategy::Gopher;
        case 4: return UpdateStrategy::Viper;
        case 5: return UpdateStrategy::Cobra;
        case 6: return UpdateStrategy::Gecko;
        case 7: return UpdateStrategy::Discus;
        case 8: return UpdateStrategy::Camel;
        case 9: return UpdateStrategy::Jagger;
        default: return UpdateStrategy::Unknown;
    }
}

// 升级单个设备
bool upgradeDevice(const std::string& devicePath, uint16_t vid, uint16_t pid, UpdateStrategy strategy, const std::string& softwarePath) {
    std::cout << "Upgrading device: " << devicePath << std::endl;

    // 获取设备序列号
    std::wstring serialNumber;
    if (!getDeviceSerialNumber(devicePath, serialNumber, vid, pid)) {
        std::cerr << "Failed to get serial number for device: " << devicePath << std::endl;
        serialNumber = L"Unknown";
    }
    std::wcout << L"Device serial number: " << serialNumber << std::endl;

    DeviceAccessor* devAccessor = DeviceAccessor::Create(vid, pid, serialNumber.c_str());

    if (!devAccessor) {
        std::cerr << "Failed to create DeviceAccessor" << std::endl;
        return false;
    }

    // 创建升级进度回调
    UpgradeProgressCallback* callback = new UpgradeProgressCallback(serialNumber.c_str());
    devAccessor->registerCbDelegate(callback);

    // 重置设备
    devAccessor->resetDevice();
    Wait(2); // 等待设备重置完成

    // 设置升级策略
    devAccessor->setUpgradeStrategy(strategy);

    // Get device info to calculate dst
    auto devCountFuture = callback->getDevCountFuture();
    devAccessor->getDeviceInfo();

    int dst = 0x2; // Default fallback
    if (devCountFuture.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        uint16_t dev_count = devCountFuture.get();
        dst = 0;
        int base = 0x02;
        for (int i = 0; i < dev_count; ++i) {
            dst += base << i;
        }
        std::cout << "Calculated upgrade dst: " << dst << " for dev_count: " << dev_count << std::endl;
    } else {
        std::cerr << "Timeout waiting for device info, using default dst: " << dst << std::endl;
    }

    // 设置升级目标设备
    devAccessor->setUpgradeDeviceDst(dst);

    // 开始固件升级
    int ret = devAccessor->startFirmwareUpgrade(softwarePath.c_str());
    if (ret != 0) {
        std::cerr << "Failed to start firmware upgrade for: " << devicePath << ", ret = " << ret << std::endl;
        devAccessor->unregisterCbDelegate(callback);
        delete callback;
        delete devAccessor;
        return false;
    }

    std::cout << "Firmware upgrade started for: " << devicePath << std::endl;

    // 等待升级完成
    bool upgradeComplete = false;
    bool upgradeSuccess = false;
    int maxWaitTime = 300; // 最大等待时间（秒）
    int waitTime = 0;
    // lastProgress 是私有成员，不可直接访问，需要添加访问方法
    // 由于当前代码结构限制，这里可通过修改类设计或添加公共方法来访问
    // 假设添加一个 getLastProgress() 方法，这里先模拟实现
    // 实际应在 UpgradeProgressCallback 类中添加 public: uint16_t getLastProgress() const { return lastProgress; }
    int lastProgressCheck = callback->getLastProgress(); // 初始化进度检查值
    int noProgressCount = 0;

    while (!upgradeComplete && waitTime < maxWaitTime) {
        Wait(10); // 每10秒检查一次
        waitTime += 10;
        if (callback->getLastProgress() == lastProgressCheck && callback->getLastProgress() < 100) {
            noProgressCount++;
            if (noProgressCount >= 3) { // 连续3次(30秒)进度不变，认为升级失败
                std::cerr << "Upgrade stalled at " << callback->getLastProgress() << "% for device: " << devicePath << std::endl;
                upgradeComplete = true;
                upgradeSuccess = false;
            }
        } else {
            lastProgressCheck = callback->getLastProgress();
            noProgressCount = 0;
        }

        // 检查升级状态
        if (callback->getUpgradeState() == UpgradeState::Success) {
            upgradeComplete = true;
            upgradeSuccess = true;
            break;
        } else if (callback->getUpgradeState() == UpgradeState::Failed) {
            upgradeComplete = true;
            upgradeSuccess = false;
            break;
        }
    }

    if (waitTime >= maxWaitTime) {
        std::cerr << "Upgrade timed out for device: " << devicePath << std::endl;
        upgradeSuccess = false;
    }

    // 清理资源
    devAccessor->unregisterCbDelegate(callback);
    delete callback;
    delete devAccessor;

    std::cout << "Upgrade process completed for: " << devicePath << std::endl;
    return upgradeSuccess;
}

int main(int argc, const char** argv) {
    // 检查命令行参数数量是否正确
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [parameters]" << std::endl;
        std::cout << "Commands: " << std::endl;
        std::cout << "  upgrade <vid> <pid> <UpdateStrategyId> <softwarePath>" << std::endl;
        std::cout << "    UpdateStrategyId values: " << std::endl;
        std::cout << "      1: Falcon   2: Dolphin  3: Gopher  4: Viper   5: Cobra" << std::endl;
        std::cout << "      6: Gecko    7: Discus   8: Camel   9: Jagger" << std::endl;
        std::cout << "  getdeviceinfo" << std::endl;
        return -1;
    }

    std::string command = argv[1];

    // 处理不同的命令
    if (command == "upgrade") {
        // 检查升级命令的参数数量
        if (argc != 6) {
            std::cout << "Usage: " << argv[0] << " upgrade <vid> <pid> <UpdateStrategyId> <softwarePath>" << std::endl;
            return -1;
        }

        int vid = std::stoi(argv[2]);
        int pid = std::stoi(argv[3]);
        int strategyId = std::stoi(argv[4]);
        std::string softwarePath = argv[5];

        std::cout << "Parameters: " << std::endl;
        std::cout << "  Command:          " << command << std::endl;
        std::cout << "  PID:              " << pid << std::endl;
        std::cout << "  VID:              " << vid << std::endl;
        std::cout << "  UpdateStrategyId: " << strategyId << std::endl;
        std::cout << "  SoftwarePath:     " << softwarePath << std::endl;

        // 获取升级策略
        UpdateStrategy strategy = getUpdateStrategy(strategyId);
        if (strategy == UpdateStrategy::Unknown) {
            std::cerr << "Invalid UpdateStrategyId: " << strategyId << std::endl;
            return -2;
        }

        // 枚举设备
        std::vector<std::string> devicePaths = enumerateDevices(vid, pid);
        if (devicePaths.empty()) {
            std::cerr << "No devices found with VID: " << vid << " and PID: " << pid << std::endl;
            return -3;
        }

        // 创建线程向量用于管理多线程升级
        std::vector<std::thread> upgradeThreads;
        std::vector<bool> upgradeResults(devicePaths.size(), false);

        std::cout << "Starting multi-threaded upgrade for " << devicePaths.size() << " devices..." << std::endl;

        // 为每个设备创建一个线程进行升级
        for (size_t i = 0; i < devicePaths.size(); ++i) {
            upgradeThreads.emplace_back([&, i]() {
                upgradeResults[i] = upgradeDevice(devicePaths[i], vid, pid, strategy, softwarePath);
            });
        }

        // 等待所有线程完成
        for (auto& t : upgradeThreads) {
            if (t.joinable()) {
                t.join();
            }
        }

        // 统计升级结果
        int successCount = 0;
        for (bool result : upgradeResults) {
            if (result) {
                successCount++;
            }
        }

        std::cout << "Multi-threaded upgrade completed. " <<
            successCount << " out of " << devicePaths.size() << " devices upgraded successfully." << std::endl;
    }
    else if (command == "getdeviceinfo") {

        std::cout << "Parameters: " << std::endl;
        std::cout << "  Command:          " << command << std::endl;

        struct hid_device_info *devs, *cur_dev;
        devs = hid_enumerate(0, 0);
        cur_dev = devs;

        while (cur_dev) {
        // 只升级usage == 130 && usage_page == 129的设备
            if (cur_dev->usage == 130 && cur_dev->usage_page == 129) {
                DeviceAccessor* devAccessor =
                    DeviceAccessor::Create(cur_dev->vendor_id, cur_dev->product_id, cur_dev->serial_number);
                if (!devAccessor) {
                    std::cerr << "Failed to create DeviceAccessor" << std::endl;
                    return -1;
                }

                // 创建升级进度回调
                UpgradeProgressCallback* callback = new UpgradeProgressCallback(L"");
                devAccessor->registerCbDelegate(callback);

                // 重置设备
                devAccessor->getDeviceInfo();
            }

            cur_dev = cur_dev->next;
        }
        Wait(1);
        std::cout << "Device information retrieval completed." << std::endl;
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        std::cout << "Usage: " << argv[0] << " <command> [parameters]" << std::endl;
        std::cout << "Commands: " << std::endl;
        std::cout << "  upgrade <vid> <pid> <UpdateStrategyId> <softwarePath>" << std::endl;
        std::cout << "  getdeviceinfo" << std::endl;
        return -1;
    }

    return 0;
}
