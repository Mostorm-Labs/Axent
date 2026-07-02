#include <vector>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include "third_party/cxxopts/include/cxxopts.hpp"
#include "third_party/easyloggingpp/src/easylogging++.h"
#include "third_party/json/single_include/nlohmann/json.hpp"
#include "include/axdp_api.h"
#include "dev_inst.h"
#include "axdp_cli.h"

using namespace axdp;
using namespace axtool;
using json = nlohmann::json;

INITIALIZE_EASYLOGGINGPP

void configureEasyLogging() {
    // 获取默认 Logger
    el::Logger* defaultLogger = el::Loggers::getLogger("default");
    el::Configurations defaultConf;

    // 1. 禁用输出日志到控制台 STDOUT
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::ToStandardOutput, "false");

    // 2. 启用日志输出到文件
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::ToFile, "true");

    // 3. 设置日志文件路径（可以自定义），这里注释了文件路径
    //defaultLogger->configurations()->setGlobally(
    //    el::ConfigurationType::Filename, "logs/my_app.log");

    // 4. 设置滚动日志（30MB切割）
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::MaxLogFileSize, "30971520");  // 30MB = 30 * 1024 * 1024

    // 5. 设置日志文件刷新策略，保留多个滚动日志文件
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::LogFlushThreshold, "1");  // 每条日志刷新到文件
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::LogFlushThreshold, "3");  // 或者保留 10 个日志文件

    // 6. 可选：设置日志格式（显示日期时间 + 级别 + 消息）
    defaultLogger->configurations()->setGlobally(
        el::ConfigurationType::Format, "%datetime %level %msg");

    // 应用配置
    el::Loggers::reconfigureLogger(defaultLogger, defaultConf);
}
static int vmode(-1);

std::condition_variable g_cv;
long g_task_count = 0;  // 必须初始化
std::mutex g_mt;

void Signal() {
    std::unique_lock<std::mutex> unique(g_mt);
    ++g_task_count;
    if (g_task_count <= 0)
        g_cv.notify_one();
}

bool Wait(int timeout_ms) {
    std::unique_lock<std::mutex> unique(g_mt);
    --g_task_count;

    // 如果计数器 >= 0，说明已经有信号，无需等待
    if (g_task_count >= 0)
        return true;

    // 等待信号或超时，使用谓词避免虚假唤醒
    bool signaled = g_cv.wait_for(unique, std::chrono::milliseconds(timeout_ms),
                                   [&] { return g_task_count >= 0; });

    // 如果超时，恢复计数器状态
    if (!signaled) {
        ++g_task_count;
        return false;
    }

    return true;
}

class Message {
public:
    Message(const std::string& method) {
		_method = method;
    };
	~Message() = default;

    static json Event(const std::string& event_name, json data) {
        json event;
        event["op"] = 6;
        event["d"]["event"] = event_name;
        if (data != nullptr) {
            event["d"]["data"] = data;
        }
        return event;
    }
    static json Failure(const std::string& method_name, json data, int error_code) {
        json result;
        result["op"] = 8;
        result["d"]["method"] = method_name;

        if (data != nullptr) {
            result["d"]["result"] = data;
        }
        result["d"]["status"]["code"] = error_code;
        result["d"]["status"]["result"] = false;

        return result;
    }
	static json Error(const std::string& comment, int error_code) {
		json result;
		result["op"] = 8;
		result["d"]["status"]["code"] = error_code;
		result["d"]["status"]["result"] = false;
		result["d"]["status"]["comment"] = comment;
		return result;
	}
	static json Success(const std::string& method_name, json data = nullptr) {
        json result;
        result["op"] = 8;
        result["d"]["method"] = method_name;

        if (data != nullptr) {
            result["d"]["result"] = data;
        }
        result["d"]["status"]["code"] = 100;
        result["d"]["status"]["result"] = true;

        return result;
	}
private:
    std::string _method = "";
};

void output(const std::string& content) {
    std::cout << content << std::endl;
}

std::string formatMacAddress(const char* macBytes, int len) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(macBytes[i]));
        if (i < 5) ss << ":";
    }
    return ss.str();
}

bool isValidTrackMode(int value) {
    switch (static_cast<VideoTrackMode>(value)) {
    case VideoTrackMode::NoTrack:
    case VideoTrackMode::SingleSpeaker:
    case VideoTrackMode::SplitScreens:
    case VideoTrackMode::ZoneFollowing:
    case VideoTrackMode::Manual:
    case VideoTrackMode::PanoramaShot:
    case VideoTrackMode::AutoFraming:
        return true;
    default:
        return false;
    }
}


class DeviceEventCallback : public EventCallbackDelegate {
public:
    DeviceEventCallback() = default;

    ~DeviceEventCallback() override = default;

    void onResetDevice(ResultState state) override {
        //std::cout << "Open device successfully" << std::endl;
        json result = Message::Success("onRestDevice");
        output(result.dump());
        //Signal();
    };

    void onGetDeviceInformation(const DeviceInfo* info, uint16_t dev_count) override {
        //std::cout << "Async get device info as below:" << std::endl;
        json devlist;
        for (size_t i = 0; i < dev_count; i++) {
            json devinfo;
            devinfo["phy_version"] = info[i].phy_version;
            devinfo["product_name"] = info[i].product_name;
            devinfo["serial_number"] = info[i].serial_number;
            devinfo["version"] = info[i].soft_version;
            
			devlist["devs"].push_back(devinfo);
        }
        json result = Message::Success("getDeviceInfo", devlist);
        output(result.dump());
        Signal();
    };

    void onDfuStateUpdate(UpgradeState state) override {
        std::string state_str;
        switch (state) {
        case axdp::UpgradeState::DataReady:
            state_str = "DataReady";
            break;
        case axdp::UpgradeState::Transferring:
            state_str = "DataTransferring";
            break;
        case axdp::UpgradeState::Verifying:
            state_str = "DataVerifying";
            break;
        case axdp::UpgradeState::Success:
            state_str = "UpgradeSuccess";
            Signal();
            break;
        case axdp::UpgradeState::Failed:
            state_str = "UpgradeFailed";
            Signal();
            break;
        default:
            break;
        }
        json data;
		data["state"] = state_str;
		json result = Message::Event("onDfuStateUpdate", data);
        output(result.dump());
        //std::cout << "DfuStateUpdate > " << (int)state << " " << state_str << std::endl;
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        json data;
		data["progress"] = progress;
		json result = Message::Event("onDfuProgressUpdate", data);
		output(result.dump());
    };

    void onGetWifi(const uint8_t* ssid, uint16_t len)override {
        std::string macStr = formatMacAddress((const char*)ssid, len);
        json data;
		data["macAddr"] = macStr;
		json result = Message::Success("settailwifissid", data);
        output(result.dump());
		Signal();
    }
    
    void onGetVideoMode(axdp::VideoMode mode)override {
        vmode = (int)mode;
    }
    void onGetVideoTrackMode(axdp::VideoTrackMode mode)override {
        json data;
        data["videomode"] = vmode;
        data["videotrackmode"] = (int)mode;
        json result = Message::Success("getvideomode", data);
        output(result.dump());
        Signal();
    }

    void onGetMirrorState(axdp::EnableState state)override {
        json data;
        data["mirrorstate"] = (int)state;
        json result = Message::Success("getmirrorstate", data);
        output(result.dump());
        Signal();
    }

    void onGetWatermark(axdp::EnableState state)override {
        json data;
        data["watermark"] = (int)state;
        json result = Message::Success("getwatermark", data);
        output(result.dump());
        Signal();
    }

    void onGetPiPMode(axdp::EnableState state)override {
        json data;
        data["pipmode"] = (int)state;
        json result = Message::Success("getpipmode", data);
        output(result.dump());
        Signal();
    }

    void onGetSplitScreenNumber(int32_t number)override {
        json data;
        data["splitscreennumber"] = (int)number;
        json result = Message::Success("getsplitscreennumber", data);
        output(result.dump());
        Signal();
    }
};

bool caseInsensitiveCompare(const std::string& str1, const std::string& str2) {
    std::string lowerStr1 = str1;
    std::string lowerStr2 = str2;

    std::transform(lowerStr1.begin(), lowerStr1.end(), lowerStr1.begin(), ::tolower);
    std::transform(lowerStr2.begin(), lowerStr2.end(), lowerStr2.begin(), ::tolower);

    return lowerStr1 == lowerStr2;
}

int str2int(const std::string& str) {
    try {
        size_t pos;
        int num = std::stoi(str, &pos);
        if (pos != str.length()) {  // 检查是否完全转换
            LOG(ERROR) << "Non-num char in string";
            return -1;
        }
        return num;
    }
    catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return -1;
    }
}

int main(int argc, char** argv)
{
    configureEasyLogging();
    LOG(INFO) << "Program Start";
    cxxopts::Options options("axdptool", "A brief description");
    std::vector<std::string> input_params;
    options.add_options()
        ("v,vid", "Vender id", cxxopts::value<int>())
        ("p,pid", "Product id", cxxopts::value<int>())
        ("m,method", "Choose a method to execute", cxxopts::value<std::string>())
        ("a,params", "Input arguments for method", cxxopts::value<std::vector<std::string>>())
        ("h,help", "Print usage");
    options.parse_positional({"params"});
    cxxopts::ParseResult result;
    try {
		result = options.parse(argc, argv);
	}
    catch (const std::exception& e) {
        json data = Message::Error(e.what(), InvalidArguments);
        output(data.dump());
        LOG(ERROR) << e.what();
        exit(0);
    }
    
    if (result.count("help"))
    {
        output(options.help());
        exit(0);
    }

    int vid = 0, pid = 0;
    try {
		if (result.count("vid") == 0 || result.count("pid") == 0)
		{
			throw std::runtime_error("Vender id and Product id are required");
		}
        vid = result["vid"].as<int>();
        pid = result["pid"].as<int>();
	}
    catch (const std::exception& e) {
        json result = Message::Error(e.what(), InvalidArguments);
        output(result.dump());
        LOG(ERROR) << e.what();
        exit(0);
    }

    std::string method;
    if (result.count("method"))
    {
        method = result["method"].as<std::string>();
    }
    else
    {
        json data = Message::Error("Method type is required", MissingMethodType);
        output(data.dump());
		LOG(ERROR) << "Method type is required";
		exit(0);
    }
    std::vector<std::string> params;
    if (result.count("params"))
    {
        params = result["params"].as<std::vector<std::string>>();
    }

    //Create Device Instance
	// EventCallbackDelegate* cb_delegate = nullptr;
    // DeviceAccessor* dev = DeviceAccessor::Create(vid, pid, nullptr);
    // if (dev == nullptr)
    // {
	// 	json data = Message::Error("Create device instance failed", InvalidDeviceInstance);
    //     output(data.dump());
    //     LOG(ERROR) << "Create device instance failed, EXITED.";
    //     goto end;
    // }
    // cb_delegate = new DeviceEventCallback();
    // dev->registerCbDelegate(cb_delegate);
    std::shared_ptr<EventCallbackDelegate> cb_delegate(nullptr);
    std::shared_ptr<DeviceAccessor> dev(DeviceAccessor::Create(vid, pid));
    if (dev == nullptr)
    {
		json data = Message::Error("Create device instance failed", InvalidDeviceInstance);
        output(data.dump());
        LOG(ERROR) << "Create device instance failed, EXITED.";
        return -1;
    }
    cb_delegate.reset(new DeviceEventCallback());
    dev->registerCbDelegate(cb_delegate.get());

    int res = 0;
    if (caseInsensitiveCompare(method, "getdeviceinfo"))
    {
        res = dev->getDeviceInfo();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getDeviceInfo timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "settailwifiinfo"))
    {
		if (params.size() < 5)
		{
			json data = Message::Error("Not enough parameters", InsufficientArguments);
			output(data.dump());
			LOG(ERROR) << "Not enough parameters";
			//goto end;
            return -1;
		}
        json wifi_info;
        wifi_info["s"] = params[0];
        wifi_info["k"] = params[1];
        wifi_info["b"] = params[2];
        wifi_info["ip"] = params[3];
        try {
            size_t pos;
            int num = std::stoi(params[4], &pos);
            if (pos != params[4].length()) {  // 检查是否完全转换
                throw std::runtime_error("Parameter Error:tail cast destnation type should be 1/2/3/4");
            }
			wifi_info["t"] = num;
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            //goto end;
            return -1;
        }

		std::string json_str = wifi_info.dump();
        //std::cout << json_str << std::endl;
        res = dev->setTailWifiSSID(json_str.c_str(), json_str.length());
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "setTailWifiSSID timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "dfu"))
    {
        if (params.size() < 2)
        {
            json data = Message::Error("Not enough arguments", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough arguments";
            return -1;
        }
		dev->setUpgradeDeviceDst(0xff);
		UpdateStrategy strategy = UpdateStrategy::Viper;
		int strategy_type = 4;
        if (params.size() > 1)
        {
            int strategy_type = str2int(params[1]);
			strategy = strategy_type == 1 ? UpdateStrategy::Falcon :
				strategy_type == 2 ? UpdateStrategy::Dolphin :
				strategy_type == 3 ? UpdateStrategy::Gopher :
				strategy_type == 4 ? UpdateStrategy::Viper :
				strategy_type == 5 ? UpdateStrategy::Cobra :
				strategy_type == 6 ? UpdateStrategy::Gecko :
				strategy_type == 7 ? UpdateStrategy::Discus :
				strategy_type == 8 ? UpdateStrategy::Camel : UpdateStrategy::Viper;
        }
		
        dev->setUpgradeStrategy(strategy);
        res = dev->startFirmwareUpgrade(params[0].c_str());
		if (res == 0)
		{
			LOG(INFO) << "Start upgrade successfully";
		}
		else
		{
			json data = Message::Error("Start upgrade failed", 207);
			output(data.dump());
			LOG(ERROR) << "Start upgrade failed";
			//goto end;
            return -1;
		}
		if (!Wait(500000)) {
            json data = Message::Error("Firmware upgrade timeout", 209);
            output(data.dump());
            LOG(ERROR) << "Firmware upgrade timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "getvideomode"))
    {
        res = dev->getVideoMode();
        res = dev->getVideoTrackMode();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getVideoMode timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "setvideomode"))
    {
        if (params.size() < 1)
        {
            json data = Message::Error("Not enough parameters", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough parameters";
            //goto end;
            return -1;
        }
		int vmode = -1;
        int tmode = -1;
        try {
            size_t pos;
            vmode = std::stoi(params[0], &pos);
            
            if (pos != params[0].length()) {
                throw std::runtime_error("Parameter Error:Video Mode type should be 0/1");
            }

            tmode = std::stoi(params[1], &pos);

            if (pos != params[1].length()) {
                throw std::runtime_error("Parameter Error:Video track Mode type should between 0 and 6");
            }
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            //goto end;
            return -1;
        }
        if (vmode == 1) //turn on autoframing
        {
            res = dev->setVideoMode(axdp::VideoMode(vmode));
            //res = dev->setVideoTrackMode(axdp::VideoTrackMode(0));
        }
        else if (vmode == 0) {
            res = dev->setVideoMode(axdp::VideoMode(vmode));
            //res = dev->setVideoTrackMode(axdp::VideoTrackMode(5));
        }
        else
        {
            json data = Message::Error("Unsupported video mode input", 207);
            output(data.dump());
            LOG(ERROR) << "Unsupported video mode input";
            return -1;
        }

        if (isValidTrackMode(tmode)) {
            res = dev->setVideoTrackMode(axdp::VideoTrackMode(tmode));
        }
        else {
            json data = Message::Error("Unsupported video track mode input", 207);
            output(data.dump());
            LOG(ERROR) << "Unsupported video track mode input";
            return -1;
        }
       
        json data;
        json result = Message::Success("setvideomode", data);
        output(result.dump());

        //Wait(50);
    }
    else if (caseInsensitiveCompare(method, "getwatermark"))
    {
        res = dev->getWatermark();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getWatermark timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "setwatermark"))
    {
        if (params.size() < 1)
        {
            json data = Message::Error("Not enough parameters", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough parameters";
            return -1;
        }
        int state = -1;
        try {
            size_t pos;
            state = std::stoi(params[0], &pos);

            if (pos != params[0].length()) {
                throw std::runtime_error("Parameter Error: Watermark state should be 0/1");
            }

            if (state != 0 && state != 1) {
                throw std::runtime_error("Parameter Error: Watermark state should be 0/1");
            }
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            return -1;
        }

        res = dev->setWatermark(axdp::EnableState(state));

        json data;
        json result = Message::Success("setwatermark", data);
        output(result.dump());

        /*if (!Wait(50)) {
            LOG(WARNING) << "setWatermark confirmation timeout";
        }*/
    }
    else if (caseInsensitiveCompare(method, "getmirrorstate"))
    {
        res = dev->getMirrorState();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getMirrorState timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "setmirrorstate"))
    {
        if (params.size() < 1)
        {
            json data = Message::Error("Not enough parameters", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough parameters";
            return -1;
        }
        int state = -1;
        try {
            size_t pos;
            state = std::stoi(params[0], &pos);

            if (pos != params[0].length()) {
                throw std::runtime_error("Parameter Error: Mirror state should be 0/1");
            }

            if (state != 0 && state != 1) {
                throw std::runtime_error("Parameter Error: Mirror state should be 0/1");
            }
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            return -1;
        }

        res = dev->setMirrorState(axdp::EnableState(state));

        json data;
        json result = Message::Success("setmirrorstate", data);
        output(result.dump());
    }
    else if (caseInsensitiveCompare(method, "getpipmode"))
    {
        res = dev->getPiPMode();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getPiPMode timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "setpipmode"))
    {
        if (params.size() < 1)
        {
            json data = Message::Error("Not enough parameters", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough parameters";
            return -1;
        }
        int mode = -1;
        try {
            size_t pos;
            mode = std::stoi(params[0], &pos);

            if (pos != params[0].length()) {
                throw std::runtime_error("Parameter Error: PiP mode should be 0/1");
            }

            if (mode != 0 && mode != 1) {
                throw std::runtime_error("Parameter Error: PiP mode should be 0/1");
            }
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            return -1;
        }

        res = dev->setPiPMode(axdp::EnableState(mode));

        json data;
        json result = Message::Success("setpipmode", data);
        output(result.dump());

        /*if (!Wait(50)) {
            LOG(WARNING) << "setPiPMode confirmation timeout";
        }*/
    }
    else if (caseInsensitiveCompare(method, "getsplitscreennumber"))
    {
        res = dev->getSplitScreenNumber();
        if (!Wait(500)) {
            json data = Message::Error("Operation timeout", 209);
            output(data.dump());
            LOG(ERROR) << "getSplitScreenNumber timeout";
            return -1;
        }
    }
    else if (caseInsensitiveCompare(method, "setsplitscreennumber"))
    {
        if (params.size() < 1)
        {
            json data = Message::Error("Not enough parameters", InsufficientArguments);
            output(data.dump());
            LOG(ERROR) << "Not enough parameters";
            return -1;
        }
        int number = -1;
        try {
            size_t pos;
            number = std::stoi(params[0], &pos);

            if (pos != params[0].length()) {
                throw std::runtime_error("Parameter Error: Split screen number should be 2-8 (C30R: 2-6, W65P: 6 or 8)");
            }

            if (number < 2 || number > 8) {
                throw std::runtime_error("Parameter Error: Split screen number should be 2-8 (C30R: 2-6, W65P: 6 or 8)");
            }
        }
        catch (const std::exception& e) {
            json data = Message::Error(e.what(), InvalidArguments);
            output(data.dump());
            LOG(ERROR) << e.what();
            return -1;
        }

        res = dev->setSplitScreenNumber(number);

        json data;
        json result = Message::Success("setsplitscreennumber", data);
        output(result.dump());

        /*if (!Wait(50)) {
            LOG(WARNING) << "setSplitScreenNumber confirmation timeout";
        }*/
    }
    else
    {
        json data = Message::Error("Unknown method", 208);
        output(data.dump());
        LOG(ERROR) << "Unknown method";
        //goto end;
        return -1;
    }

//    end:
// 	if(cb_delegate) 
//         delete cb_delegate;
//     if (dev) {
//         dev->unregisterCbDelegate(cb_delegate);
//         delete dev;
//         dev = nullptr;
//     }
    return 0;
}
