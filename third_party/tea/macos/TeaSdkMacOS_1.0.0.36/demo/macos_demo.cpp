/*
** Copyright (c) 2025 The TeaInsideSdk project. All rights reserved.
** Created by qoroliang 2025
*/

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <unistd.h>

#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdio.h>
#include <time.h>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include <condition_variable>
#include <openssl/md5.h>
#include "third_party/libjsoncpp/json/json.h"

#include "tea_inside_event_cb.h"
#include "tea_inside_api.h"
#include "tea_inside_production_api.h"

std::mutex g_mt;
std::condition_variable g_cv;
long g_task_count;
FILE* f_log;
static std::mutex log_mutex;

static char s_dump_config_type[10][100] = {
  PREPROCESS_DUMP_TAG,
  PREPROCESS_MIX_DUMP_TAG,
  PREPROCESS_MUTE_DUMP_TAG,
  PREPROCESS_FILTER_DUMP_TAG,
  UPSTREAM_CAPTURE_DUMP_TAG,
  UPSTREAM_PLAYBACK_DUMP_TAG,
  DOWNSTREAM_CAPTURE_DUMP_TAG,
  DOWNSTREAM_PLAYBACK_DUMP_TAG,
  NET_DUMP_TAG,
  AZ_INPUT_DUMP_TAG,
};

void Signal() {
    std::unique_lock<std::mutex> unique(g_mt);
    ++g_task_count;
    if (g_task_count <= 0)
        g_cv.notify_one();
}

void Wait() {
    std::unique_lock<std::mutex> unique(g_mt);
    --g_task_count;
    if (g_task_count < 0)
        g_cv.wait(unique);
}

std::string GetTimeString() {
    struct timeval tv;
    struct tm ptm;
    uint32_t milliseconds;
    gettimeofday(&tv, nullptr);
    localtime_r(&tv.tv_sec, &ptm);

    milliseconds = (uint32_t)(tv.tv_usec / 1000);

    static char tmp_buff[128] = {'\0'};
    memset(tmp_buff, '\0', 128);
    snprintf(tmp_buff, 128, "%04u.%02u.%02u %02u:%02u:%02u.%04u",
             (uint16_t)(ptm.tm_year + 1900),
             (uint16_t)(ptm.tm_mon + 1),
             (uint16_t)ptm.tm_mday,
             (uint16_t)ptm.tm_hour,
             (uint16_t)ptm.tm_min,
             (uint16_t)ptm.tm_sec,
             (uint16_t)milliseconds);

    return tmp_buff;
}

class CustomEventCbDelegate : public TeaInsideSdk::TeaInsideEventCbDelegate {
public:
  CustomEventCbDelegate() : TeaInsideEventCbDelegate() {};
  virtual ~CustomEventCbDelegate() {};

  void OnOTAStateChange(OTAState state) {
    std::cout << GetTimeString() << " OTA State = " << state << std::endl;
  }

  void OnOTAResultUpdate(OTAResult result) {
    std::cout << GetTimeString() << " OTA Result = " << result << std::endl;
    Signal();
  }

  void OnDOAUpdate(float angle) {
    std::cout << GetTimeString() << " DOA =" << angle << std::endl;
  }

  virtual void OnDeviceLog(ErrorCode result) {
    Signal();
  }

  virtual void OnAudioDump(ErrorCode result) {
    Signal();
  }

  virtual void OnMuteStatusUpdate(AvStreamType stream_type, bool is_mute) {
  }

  virtual void OnModeStatusUpdate(DeviceOperationMode mode) {
  }

  virtual void OnDeviceControlResult(InsideDeviceControlCommand cmd, const void* arg, int arg_size) {
    std::cout << GetTimeString() << " Inside device control cmd = " << cmd << ", result = " << (char*)arg << std::endl;
    Signal();
  }

  virtual void OnInsideDeviceReboot(ErrorCode result) {
    std::cout << GetTimeString() << " OTA reboot device done" << std::endl;
  }
};

class CustomUserStateUpdateCbDelegate : public TeaInsideSdk::TeaInsideDeviceUserStateUpdateCbDelegate {
public:
  CustomUserStateUpdateCbDelegate() : TeaInsideDeviceUserStateUpdateCbDelegate() {};
  virtual ~CustomUserStateUpdateCbDelegate() {}

  virtual void OnDeviceMicrophoneOverallMuteStateUpdate(bool mute, ErrorCode err) {
    std::cout << GetTimeString() << " Callback mic overall mute state change to " << mute;
    std::cout << (mute ? "-mute" : "-unmute") << std::endl;
  }
  virtual void OnDeviceMicrophoneSelectedStateUpdate(const char* device_sn_str, bool selected, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " mic selected state change to " << selected;
    std::cout << (selected ? "-selected" : "-unselected") << std::endl;
  }
  virtual void OnDevicePreprocessOverallModeStateUpdate(DevicePreprocessMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback preprocess overall mode change to " << mode;
    std::cout << ((mode == DEVICE_PREPROCESS_MODE_TEA) ? "-tea" : "-normal") << std::endl;
  }
  virtual void OnDevicePreprocessTraditionalNsAggressivenessUpdate(float aggr, ErrorCode err) {
    std::cout << GetTimeString() << " Callback preprocess overall tradition_ns_aggr change to " << aggr << std::endl;
  }
  virtual void OnDevicePreprocessAiNsAggressivenessUpdate(float aggr, ErrorCode err) {
    std::cout << GetTimeString() << " Callback preprocess overall ai_ns_aggr change to " << aggr << std::endl;
  }
  virtual void OnDevicePreprocessZoomingModeStateUpdate(const char* device_sn_str, DevicePreprocessZoomingMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " zooming mode change to " << mode << std::endl;
  }
  virtual void OnDevicePreprocessRangeModeStateUpdate(const char* device_sn_str, DevicePreprocessRangeMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " range mode change to " << mode << std::endl;
  }
  virtual void OnDeviceSpeakerOverallEqModeStateUpdate(DeviceSpeakerEqMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback spk overall eq mode change to " << mode << std::endl;
  }
  virtual void OnDeviceSpeakerOverallVolumeUpdate(float overall_volume, ErrorCode err) {
    std::cout << GetTimeString() << " Callback spk overall volume change to " << overall_volume << std::endl;
  }
  virtual void OnDeviceSpeakerMaxVolumeStateUpdate(const char* device_sn_str, float max_volume, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " spk max volume change to " << max_volume << std::endl;
  }
  virtual void OnDeviceSpeakerMuteStateUpdate(const char* device_sn_str, bool mute, ErrorCode err) {
    std::cout << GetTimeString() << "Ccallback poe device " << device_sn_str << " spk mute change to " << mute;
    std::cout << (mute ? "-mute" : "-unmute") << std::endl;
  }
  virtual void OnDeviceOrientationModeStateUpdate(const char* device_sn_str, DeviceOrientationMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " orientation mode change to " << mode << std::endl;
  }
  virtual void OnDeviceOverallWorkingModeStateUpdate(DeviceWorkingMode mode, ErrorCode err) {
    std::cout << GetTimeString() << " Callback overall security mode change to " << mode << std::endl;
  }
  virtual void OnDevicePreprocessUserMicrophoneDelayUpdate(const char* device_sn_str, int16_t delay_ms, ErrorCode err) {
    std::cout << GetTimeString() << " Callback poe device " << device_sn_str << " user mic delay change to " << delay_ms << "ms" << std::endl;
  }
};

std::vector<std::string> SplitString(std::string src_str, std::string delim_str, bool repeat_char_ignore) {
  std::vector<std::string> res;
  std::replace_if(src_str.begin(), 
                  src_str.end(), 
                  [&](const char& c) {
                    if (delim_str.find(c) != std::string::npos) {
                      return true;
                    } else {
                      return false;
                    }
                  }, delim_str.at(0));
  size_t pos = src_str.find(delim_str.at(0));
  std::string added_str = "";
  while (pos != std::string::npos) {
    added_str = src_str.substr(0, pos);
    if (!added_str.empty() || !repeat_char_ignore) {
      res.push_back(added_str);
    }
    src_str.erase(src_str.begin(), src_str.begin() + pos + 1);
    pos = src_str.find(delim_str.at(0));
  }
  added_str = src_str;
  if (!added_str.empty() || !repeat_char_ignore) {
    res.push_back(added_str);
  }
  return res;
}

std::vector<std::string> GetSnListFromJsonString(std::string json_string) {
  // 解析json string
  Json::Value root;
  JSONCPP_STRING err;
  Json::CharReaderBuilder builder;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(json_string.c_str(), json_string.c_str() + json_string.length(), &root, &err);
  Json::Value all_device_info = root["all_device_info"];

  std::vector<std::string> res;
  for (int i = 0; i < all_device_info.size(); i++) {
    if (all_device_info[i]["sn"].asString() == "") {
      res.push_back("NULL_SN");
    } else {
      res.push_back(all_device_info[i]["sn"].asString());
    }
  }
  return res;
}

std::vector<int> GetInputIndex() {
  char input_msg[32] = {'\0'};
  int id = 0;
  std::string str_msg;
  while ((input_msg[id++] = getchar()) != '\n') { // 回车结束
  }
  input_msg[id - 1] = '\0';
  str_msg.assign(input_msg);

  std::vector<std::string> res_str = SplitString(str_msg, " ", true);
  std::vector<int> res_id;
  for (int i = 0; i < res_str.size(); i++) {
    res_id.push_back(atoi(res_str[i].c_str()));
  }
  return res_id;
}

std::string GetFileMd5(const char* file_name) {
  MD5_CTX ctx;
  char buf[1024] = {'\0'};
  char hex[35] = {'\0'};
  unsigned char digest[MD5_DIGEST_LENGTH];
  int len = 0;
  FILE *fp = nullptr;
  fp = fopen(file_name, "rb");
  if (!fp) {
    printf("Md5 calculate cannot open file: %s\n", file_name);
    return "";
  }
  MD5_Init(&ctx);
  while ((len = fread(buf, 1, 1024, fp)) > 0) {
    MD5_Update(&ctx, buf, len);
    memset(buf, '\0', sizeof(buf));
  }
  MD5_Final(digest, &ctx);
  
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    sprintf(hex + i * 2, "%02x", digest[i]);
  }
  
  std::string md5;
  md5 = std::string(hex);
  return md5;
}

std::vector<std::string> EnumOtaFilePath(const char* path) {
  std::vector<std::string> ota_file_list;
  DIR* dir;
  struct dirent* file_info;
  struct stat statbuf;

  dir = opendir(path);
  if (!dir) {
    printf("Cannot open path:%s\n", path);
    return ota_file_list;
  }

  while ((file_info = readdir(dir)) != nullptr) {
    lstat(file_info->d_name, &statbuf);
    if (!S_ISDIR(statbuf.st_mode)) {
      std::string file_name(path);
      file_name.append("/");
      file_name.append(file_info->d_name);
      std::size_t seek_end = file_name.find(".zip");
      if (seek_end == std::string::npos) {
        continue;
      }
      ota_file_list.push_back(file_name);
    }
  }
  closedir(dir);

  return ota_file_list;
}

std::vector<std::string> EnumVspNodePath(const char* path) {
  std::vector<std::string> vsp_node_list;
  DIR* dir;
  struct dirent* file_info;
  struct stat statbuf;

  dir = opendir(path);
  if (!dir) {
    printf("Cannot open path:%s\n", path);
    return vsp_node_list;
  }

  while ((file_info = readdir(dir)) != nullptr) {
    std::string file_name(path);
    file_name.append("/");
    file_name.append(file_info->d_name);
    std::size_t seek_end = file_name.find("ttyACM");
    if (seek_end == std::string::npos) {
      continue;
    }
    vsp_node_list.push_back(file_name);
  }
  closedir(dir);

  return vsp_node_list;
}

void PrintBasicFuncHelpMessage() {
  std::cout << "Basic Functions[天籁SDK基础功能]， cmd:TEASdkDemo -b" << std::endl;
  std::cout << "    Master device infomations     [获取主麦相关信息]                    cmd: mi" << std::endl;
  std::cout << "    Master device roomix version  [获取主麦固件版本]                    cmd: fv" << std::endl;
  std::cout << "    Master device log zip file    [获取主麦的日志文件(deprecated)]      cmd: ml" << std::endl;
  std::cout << "    Master device hid params      [获取主麦的hid参数]                   cmd: hp" << std::endl;
  std::cout << "    Master device mute/unmute     [设置主麦静音:1 / 非静音:0]           cmd: mute 1/mute 0" << std::endl;
  std::cout << "    Master device tea/normal mode [设置主麦天籁模式:1 / 普通模式:0]     cmd: mode 1/mode 0" << std::endl;
  std::cout << "    Reboot master device          [重启主麦设备]                        cmd: reboot" << std::endl;
  std::cout << "    TEA SDK version               [获取天籁SDK版本]                     cmd: tv" << std::endl;
  std::cout << "    OTA device(s)                 [设备OTA]                             cmd: ota" << std::endl;
  std::cout << "    Reinit SDK                    [重新初始化SDK]                       cmd: reinit" << std::endl;
  std::cout << "    Quit                          [退出基础功能页]                      cmd: quit" << std::endl << std::endl;
}

void PrintProductionFuncHelpMessage() {
  std::cout << "Production Functions[天籁SDK产测/调试功能]， cmd:TEASdkDemo -p" << std::endl;
  std::cout << "    All devices informations      [获取主副麦所有相关信息]              cmd: ai" << std::endl;
  std::cout << "    M/S devices log zip sync      [同步主麦或副麦日志文件到主麦]        cmd: msls" << std::endl;
  std::cout << "    M/S devices log zip get       [获取主麦或副麦日志文件到Host PC]     cmd: mslg" << std::endl;
  std::cout << "    M/S devices dump config       [配置主麦或副麦音频dump]              cmd: msdc" << std::endl;
  std::cout << "    M/S devices dump zip sync     [同步主麦或副麦音频dump文件到主麦]    cmd: msds" << std::endl;
  std::cout << "    M/S devices dump zip get      [获取主麦或副麦音频dump文件到Host PC] cmd: msdg" << std::endl;
  std::cout << "    Reinit SDK                    [重新初始化SDK]                      cmd: reinit" << std::endl;
  std::cout << "    Quit                          [退出产测/调试功能页]                 cmd: quit" << std::endl << std::endl;
}

void PrintHelpMessage() {
  std::cout << "天籁SDK相关功能:" << std::endl << std::endl;
  PrintBasicFuncHelpMessage();
  PrintProductionFuncHelpMessage();
}

void CustomLog(const char* msg) {
  std::unique_lock<std::mutex> lock(log_mutex);
  if (f_log) {
    int msg_len = std::strlen(msg);
    fwrite(msg, sizeof(char), msg_len, f_log);
   std::cout << "Sdk| " << msg;
  }
}

void HandleBasicFuncCmd() {
  CustomEventCbDelegate* basic_cb = new CustomEventCbDelegate();
  CustomUserStateUpdateCbDelegate* user_state_cb = new CustomUserStateUpdateCbDelegate();
  const char* config_file = "/home/chunhuwang/Desktop/Work/Roomix/tea_sdk/tea_sdk_config.json";
  TeaInsideSdk::TeaInsideApi* api = new TeaInsideSdk::TeaInsideApi(config_file);
  api->RegisterEvent(basic_cb);
  api->RegisterDeviceUserStateUpdateEvent(user_state_cb);
  api->SetLogCb(CustomLog);
  bool quit = false;

  if (api->Init() == ErrorCode::FAILED) {
    printf("Basic sdk init failed, maybe device unattach\n");
    delete api;
    delete basic_cb;
    delete user_state_cb;
    api = nullptr;
    basic_cb = nullptr;
    user_state_cb = nullptr;
    return;
  }
  
  do {
    char input_msg[32] = {'\0'};
    int id = 0;
    std::string str_msg;
    while ((input_msg[id++] = getchar()) != '\n') { // 回车结束
    }
    input_msg[id - 1] = '\0';
    str_msg.assign(input_msg);
    
    std::vector<std::string> res = SplitString(str_msg, " ", true);
    size_t res_size = res.size();
    // printf("%s %s\n", res[0].c_str(), res[1].c_str());
    if (res_size > 0) {
      std::string cmd = res[0];
      std::string param;
      if (!cmd.compare("mute")) {
        if (res_size > 1) {
          param.assign(res[1]);
          api->SetDeviceMute(AUDIO_CAPTURE_STREAM, param.compare("1") ? false : true);
          printf("Set master device mute: %s\n", (param.compare("1") ? "unmute" : "mute"));
        } else {
          printf("Invalid param: lack of mute param!\n");
        }
      } else if (!cmd.compare("mode")) {
        if (res_size > 1) {
          param.assign(res[1]);
          api->SetDeviceMode(param.compare("1") ? DEVICE_OPERATION_MODE_NORMAL : DEVICE_OPERATION_MODE_TEA);
          printf("Set master device mode: %s\n", (param.compare("1") ? "normal mode" : "tea mode"));
        } else {
          printf("Invalid param: lack of mode param!\n");
        }
      } else if (!cmd.compare("reboot")) {
        printf("Set master device reboot: %s\n", (api->RebootDevice() == ErrorCode::OK) ? "success" : "failed");
      } else if (!cmd.compare("reinit")) {
        printf("Set TEASdk deinit: %s\n", (api->UnInit() == ErrorCode::OK) ? "success" : "failed");
        printf("Set TEASdk init: %s\n", (api->Init() == ErrorCode::OK) ? "success" : "failed");
      } else if (!cmd.compare("mi")) {
        const char* info = api->GetDeviceInformation();
        printf("Get master device infomation:\n%s\n", (info ? info : "Invliad infomation"));
      } else if (!cmd.compare("fv")) {
        const char* fv = api->GetDeviceVersion();
        printf("Get master device firmware version: %s\n", (fv ? fv : "Invalid firmware version"));
      } else if (!cmd.compare("tv")) {
        const char* tv = api->Version();
        printf("Get TEASdk version: %s\n", (tv ? tv : "Invalid sdk version"));
      } else if (!cmd.compare("ml")) {
        char cp[256] = {'\0'};
        char* o = getcwd(cp, sizeof(cp));
        if (api->GetDeviceLog(cp) == ErrorCode::OK) {
          printf("Get master device log success(path:%s), please wait...\n", cp);
          Wait();
          printf("Get master device log done\n");
        } else {
          printf("Get master device log failed(path:%s)\n", cp);
        }
      } else if (!cmd.compare("hp")) {
        TeaInsideSdk::HidDeviceParams_t hid_params;
        api->GetHidDeviceParams(&hid_params);
        printf("Get master device hid params, vid=%d, pid=%d\n", hid_params.vendor_id, hid_params.product_id);
      } else if (!cmd.compare("ota")) {
        char cp[256] = {'\0'};
        char* o = getcwd(cp, sizeof(cp));
        std::vector<std::string> ota_file_list = EnumOtaFilePath(cp);
        if (ota_file_list.size()) {
          printf("Please input ONLY one file index of your wanted ota file: (ex: 0 or 1)\n");
          for (int idx = 0; idx < ota_file_list.size(); idx++) {
            printf("file %d: %s\n", idx, ota_file_list[idx].c_str());
          }
          std::vector<int> file_ids = GetInputIndex();
          if (file_ids.size()) {
            // calculate file md5:
            const char* ota_file = ota_file_list[file_ids[0]].c_str();
            std::string ota_file_md5 = GetFileMd5(ota_file);
            printf("Select ota file:%s\nmd5:%s\n", ota_file, ota_file_md5.c_str());
            if (api->StartDeviceOTA(ota_file, ota_file_md5.c_str()) == ErrorCode::OK) {
              printf("Ota device call success, please wait...\n");
              Wait();
              printf("Ota device done\n");
            } else {
              printf("Ota device call failed\n");
            }
          }
        } else {
          printf("No ota zip file in current path:%s\n", cp);
        }
      } else if (!cmd.compare("quit")) {
        quit = true;
        printf("Quit basic function\n");
      } else {
        printf("Unknown cmd\n");
      }
    }
  } while (!quit);

  api->UnInit();
  api->UnRegisterEvent(basic_cb);
  delete api;
  api = nullptr;
  delete basic_cb;
  basic_cb = nullptr;
  delete user_state_cb;
  user_state_cb = nullptr;
}

void HandleProductionFuncCmd() {
  CustomEventCbDelegate* delegate = new CustomEventCbDelegate();
  TeaInsideSdk::TeaInsideApi* base_api = new TeaInsideSdk::TeaInsideApi();
  TeaInsideSdk::TeaInsideProductionApi* api = new TeaInsideSdk::TeaInsideProductionApi();
  base_api->RegisterEvent(delegate);
  base_api->SetLogCb(CustomLog);
  bool quit = false;

  if (base_api->Init() == ErrorCode::FAILED) {
    printf("Production sdk init failed, maybe device unattach\n");
    delete api;
    api = nullptr;
    return;
  }

  do {
    char input_msg[32] = {'\0'};
    int id = 0;
    std::string str_msg;
    while ((input_msg[id++] = getchar()) != '\n') { // 回车结束
    }
    input_msg[id - 1] = '\0';
    str_msg.assign(input_msg);
    
    std::vector<std::string> res = SplitString(str_msg, " ", true);
    size_t res_size = res.size();
    // printf("%s %s\n", res[0].c_str(), res[1].c_str());
    if (res_size > 0) {
      std::string cmd = res[0];
      std::string param;
      if (!cmd.compare("ai")) {
        const char *ainfo = api->GetAllDeviceInformation();
        printf("Get all device(s) information:\n%s\n", (ainfo ? ainfo : "Invalid information"));
      } else if (!cmd.compare("msls")) {
        std::string ainfo;
        ainfo.assign(api->GetAllDeviceInformation());
        std::vector<std::string> sns = GetSnListFromJsonString(ainfo);
        printf("M/S device sn list:(Input the device index if you want to get the log of specific device, ex: 0 1 or 1 2)\n");
        for (int i = 0; i < sns.size(); i++) {
          printf("id:%d sn:%s\n", i, sns[i].c_str());
        }
        std::vector<int> ids = GetInputIndex();
        int expect_sn_size = ids.size();
        for (int i = 0; i < expect_sn_size; i++) {
          char *sn = new char[100];
          memset(sn, '\0', 100);
          memcpy(sn, sns[ids[i]].c_str(), sns[ids[i]].length());
          api->SyncSpecificDevicesLogToMasterDevice(1, const_cast<const char**>(&sn), false);
          Wait();
          printf("Sync device:%s log to master device done\n", sns[i].c_str());
          delete [] sn;
        }
      } else if (!cmd.compare("msds")) {
        std::string ainfo;
        ainfo.assign(api->GetAllDeviceInformation());
        std::vector<std::string> sns = GetSnListFromJsonString(ainfo);
        printf("M/S device sn list:(Input the device index if you want to get the dump of specific device , ex: \"0 1\" or \"1 2\")\n");
        for (int i = 0; i < sns.size(); i++) {
          printf("id:%d sn:%s\n", i, sns[i].c_str());
        }
        std::vector<int> ids = GetInputIndex();
        int expect_sn_size = ids.size();
        for (int i = 0; i < expect_sn_size; i++) {
          char *sn = new char[100];
          memset(sn, '\0', 100);
          memcpy(sn, sns[ids[i]].c_str(), sns[ids[i]].length());
          api->SyncSpecificDevicesDumpToMasterDevice(1, const_cast<const char**>(&sn), 0, nullptr);
          Wait();
          printf("Sync device:%s dump to master device done\n", sns[i].c_str());
          delete [] sn;
        }
      } else if (!cmd.compare("mslg")) {
        char cp[256] = {'\0'};
        char* o = getcwd(cp, sizeof(cp));
        if (api->SyncMasterDeviceDebugFilesToHost(cp,
                                                  InsideDeviceDebugFileType::INSIDE_DEVICE_DBG_FILE_LOG,
                                                  InsideDeviceTransportType::INSIDE_DEVICE_TRANSPORT_VSP) == ErrorCode::OK) {
          printf("Get master device debug log success(path:%s), please wait...\n", cp);
          Wait();
          printf("Get master device debug log done\n");
        } else {
          printf("Get master device debug log failed(path:%s)\n", cp);
        }
      } else if (!cmd.compare("msdc")) {
        std::string ainfo;
        ainfo.assign(api->GetAllDeviceInformation());
        std::vector<std::string> sns = GetSnListFromJsonString(ainfo);
        printf("M/S device sn list:(Input the device index if you want to config dump of specific device , ex: \"0 1\" or \"1 2\")\n");
        for (int i = 0; i < sns.size(); i++) {
          printf("sn_id:%d sn:%s\n", i, sns[i].c_str());
        }
        std::vector<int> sn_ids = GetInputIndex();
        int expect_sn_size = sn_ids.size();
        
        printf("Dump config list:(Input the config index if you want to config dump of specific type), ex: \"0 1\" or \"0 1 2 8\"\n");
        printf("If want to clear the dump config and dump file, you can input: \"-1\"\n");
        for (int i = 0; i < 9; i++) {
          printf("config_id:%d config:%s\n", i, s_dump_config_type[i]);
        }
        std::vector<int> cfg_ids = GetInputIndex();
        int expect_cfg_size = cfg_ids.size();
        if (expect_cfg_size) {
          for (int i = 0; i < expect_sn_size; i++) {
            char *sn = new char[100];
            char **cfg = new char*[expect_cfg_size];
            memset(sn, '\0', 100);
            memcpy(sn, sns[sn_ids[i]].c_str(), sns[sn_ids[i]].length());
            if (expect_cfg_size == 1 && cfg_ids[0] == -1) {
              // Clear dump config and dump file
              api->SetSpecificDevicesDumpConfigs(1,
                                                 const_cast<const char**>(&sn),
                                                 0,
                                                 nullptr);
              delete [] cfg;
            } else {
              for (int k = 0; k < expect_cfg_size; k++) {
                cfg[k] = new char[100];
                memset(cfg[k], '\0', 100);
                memcpy(cfg[k], s_dump_config_type[cfg_ids[k]], strlen(s_dump_config_type[cfg_ids[k]]));
              }
              api->SetSpecificDevicesDumpConfigs(1,
                                                 const_cast<const char**>(&sn),
                                                 expect_cfg_size,
                                                 const_cast<const char**>(cfg));
              for (int k = 0; k < expect_cfg_size; k++) {
                delete [] cfg[k];
              }
              delete [] cfg;
            }
            delete [] sn;
          }
        }
      } else if (!cmd.compare("msdg")) {
        char cp[256] = {'\0'};
        char* o = getcwd(cp, sizeof(cp));
        if (api->SyncMasterDeviceDebugFilesToHost(cp,
                                                  InsideDeviceDebugFileType::INSIDE_DEVICE_DBG_FILE_DUMP,
                                                  InsideDeviceTransportType::INSIDE_DEVICE_TRANSPORT_VSP) == ErrorCode::OK) {
          printf("Get master device debug dump success(path:%s), please wait...\n", cp);
          Wait();
          printf("Get master device debug dump done\n");
        } else {
          printf("Get master device debug dump failed(path:%s)\n", cp);
        }
      } else if (!cmd.compare("reinit")) {
        printf("Set TEASdk deinit: %s\n", (api->UnInit() == ErrorCode::OK) ? "success" : "failed");
        printf("Set TEASdk init: %s\n", (api->Init() == ErrorCode::OK) ? "success" : "failed");
      } else if (!cmd.compare("quit")) {
        quit = true;
        printf("Quit production function\n");
      } else {
        printf("Unknown cmd\n");
      }
    }
  } while (!quit);

  base_api->UnInit();
  base_api->UnRegisterEvent(delegate);
  delete api;
  api = nullptr;
  delete base_api;
  base_api = nullptr;
  delete delegate;
  delegate = nullptr;
}

int main(int argc, char *argv[]) {
  int oc = 0;     // 选项字符
  char ec;        // 无效选项字符
  bool print_help = false;
  f_log = fopen("./TEASdkDemo.log", "wb+");
  if(f_log == NULL) {
    printf("open TEASdkDemo.log file error.");
  }

  while ((oc = getopt(argc, argv, "hbp")) != -1) {
    switch (oc) {
      case 'h':
        PrintHelpMessage();
        print_help = true;
        break;
      case 'b':
        PrintBasicFuncHelpMessage();
        print_help = true;
        HandleBasicFuncCmd();
        break;
      case 'p':
        PrintProductionFuncHelpMessage();
        print_help = true;
        HandleProductionFuncCmd();
        break;
      case '?':
        ec = (char)optopt;
        break;
      case ':':
        printf("lack option argv!\n");
        break;
      default:
        break;
    }
  }

  if (oc == -1 && !print_help) {
    PrintHelpMessage();
  }
  if(f_log) {
    fclose(f_log);
  }

  return 0;
}

