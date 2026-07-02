/*
** Copyright (c) 2021 The Roomix project. All rights reserved.
** Created by chunhuwang 2022
*/

#ifndef TEA_INSIDE_SDK_PRODUCTION_API_H
#define TEA_INSIDE_SDK_PRODUCTION_API_H

#include "tea_inside_common.h"
#include "tea_inside_event_cb.h"

#define PREPROCESS_DUMP_TAG                     "preprocess"
#define PREPROCESS_MIX_DUMP_TAG                 "preprocess_mix"
#define PREPROCESS_MUTE_DUMP_TAG                "preprocess_mute"
#define PREPROCESS_FILTER_DUMP_TAG              "preprocess_filter"
#define UPSTREAM_CAPTURE_DUMP_TAG               "upstream_capture"
#define UPSTREAM_PLAYBACK_DUMP_TAG              "upstream_playback"     //"uac"
#define DOWNSTREAM_CAPTURE_DUMP_TAG             "downstream_capture"    //"uac"
#define DOWNSTREAM_PLAYBACK_DUMP_TAG            "downstream_playback"
#define NET_DUMP_TAG                            "net"
#define AZ_INPUT_DUMP_TAG                       "az_input"

namespace TeaInsideSdk {

class TEA_INSIDE_SDK_EXPORT TeaInsideProductionApi {
public:
  TeaInsideProductionApi(const char* config_file = nullptr);
  virtual ~TeaInsideProductionApi();

  /**
    * 初始化
    * @param 配置文件
    */
  ErrorCode Init(TEASdkParams_t* params = nullptr);

  /**
    * 反初始化
    */
  ErrorCode UnInit();

  /**
    * 重新初始化SDK
    */
  ErrorCode ReInit(TEASdkParams_t* params = nullptr);

  /**
    * 注册事件回调类
    * @param delegate 事件回调类
    */
  void RegisterEvent(TeaInsideEventCbDelegate* delegate);

  /**
    * 销毁事件回调类
    * @param delegate 事件回调类
    */
  void UnRegisterEvent(TeaInsideEventCbDelegate* delegate);

  /**
    * 进入刷机模式
    */
  ErrorCode EnterBootloaderMode();

  /**
    * 获取序列号
    */
  const char* GetDeviceSerialNumber();

  /**
    * 写入序列号
    */
  ErrorCode SetDeviceSerialNumber(const char* serial_number);

  /**
    * 获取所有设备信息
    * @return 返回模组设备信息，Json字符串
    */
  const char* GetAllDeviceInformation();

  /**
    * 获取所有设备数量
    * @param 设备总数量
    * @return 设备数量, 失败返回ErrorCode::FAILED
    */
  int GetDevicesCount();

  /**
    * 同步指定主麦或扩展麦日志到主设备
    * @param devices_num      指定的设备数(=0为全部设备，devices_sn=null)
    * @param devices_sn       指定的设备数sn号
    * @param is_all_log       是否拉取全量rtc日志
    * @return ErrorCode       返回错误码
    */
  ErrorCode SyncSpecificDevicesLogToMasterDevice(const int    devices_num,
                                                 const char**  devices_sn,
                                                 bool          is_all_log);
  
  /**
    * 删除指定主麦或扩展麦日志
    * @param devices_num      指定的设备数(=0为全部设备，devices_sn=null)
    * @param devices_sn       指定的设备数sn号
    * @return ErrorCode       返回错误码
    */
  ErrorCode DeleteSpecificDevicesLog(const int devices_num, const char** devices_sn);

  /**
    * 同步指定主麦或扩展麦dump文件到主设备
    * @param devices_num      指定的设备数(=0为全部设备，devices_sn=null)
    * @param devices_sn       指定的设备sn号
    * @param configs_num      指定的dump config数(=0为全部dump config, dump_config=null)
    * @param configs          指定的dump config(如"preprocess", "net", 可参考宏定义)
    * @return ErrorCode       返回错误码
    */
  ErrorCode SyncSpecificDevicesDumpToMasterDevice(const int     devices_num,
                                                  const char**  devices_sn,
                                                  const int     configs_num,
                                                  const char**  configs);

  /**
    * 配置指定主麦或扩展麦dump/删除配置和dump文件
    * @param devices_num      指定的设备数(=0为全部设备，devices_sn=null)
    * @param devices_sn       指定的设备sn号
    * @param configs_num      指定的dump config数
    * @param configs          指定的configs (如preprocess/mixer/capture/playback)，对设备不指定configs就是删除config和dump操作
    * @return ErrorCode       返回错误码
    */
  ErrorCode SetSpecificDevicesDumpConfigs(const int     devices_num,
                                          const char**  devices_sn,
                                          const int     configs_num,
                                          const char**  configs);
  
  /**
    * 同步主麦/userdata/roomix/debug目录下的日志或者dump文件zip包到Host PC
    * @param file_storage_dir       上传后的Host PC存放目录
    * @param zip_file_type          debug zip file类型，是日志还是音频dump
    * @param expect_transport_type  期望的传输类型，虚拟串口，hid  
    * @return ErrorCode             返回错误码
    */
  ErrorCode SyncMasterDeviceDebugFilesToHost(const char* file_storage_dir, 
                                             InsideDeviceDebugFileType zip_file_type,
                                             InsideDeviceTransportType expect_transport_type);

  /**
    * 设备控制
    * @param cmd          操作指令
    * @param devices_num  指定的设备数(=0为全部设备，devices_sn=null)
    * @param devices_sn   指定的设备sn号
    * @param arg          自定义参数
    * @param arg_size     自定义参数长度
    * @note 所有结果通过OnDeviceControlResult回调上报
    * @return 失败返回ErrorCode::FAILED
    */
  ErrorCode SpecificDevicesControl(InsideDeviceControlCommand cmd,
                                   int devices_num,
                                   const char** devices_sn,
                                   const void* user_arg = nullptr,
                                   int user_arg_size = 0);

};

}

#endif //TEA_INSIDE_SDK_PRODUCTION_API_H
