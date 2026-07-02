/*
** Copyright (c) 2021 The TeaInsideSdk project. All rights reserved.
** Created by chunhuwang 2021
*/

#ifndef TEA_INSIDE_SDK_H
#define TEA_INSIDE_SDK_H

#include <stdint.h>
#include "tea_inside_common.h"
#include "tea_inside_event_cb.h"

namespace TeaInsideSdk {

class TEA_INSIDE_SDK_EXPORT TeaInsideApi  {
public:
  /**
    * SDK构造函数
    * @param config_file SDK配置文件
    */
  TeaInsideApi(const char* config_file = nullptr);
  virtual ~TeaInsideApi();

  /**
    * 初始化SDK
    */
  ErrorCode Init(TEASdkParams_t* params = nullptr);

  /**
    * 取消初始化SDK
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
    * 注册设备用户设置状态事件回调类
    * @param delegate 事件回调类
    */
  void RegisterDeviceUserStateUpdateEvent(TeaInsideDeviceUserStateUpdateCbDelegate* delegate);

  /**
    * 注册设备连接状态事件回调类
    * @param delegate 事件回调类
    */
  void RegisterDeviceConnectStateUpdateEvent(TeaInsideDeviceConnectStateUpdateCbDelegate* delegate);

  // API_LEVEL_START_1
  /**
    * 获取SDK状态
    */
  SDKState GetSDKState();

  /**
    * 获取SDK最后一次错误码
    */
  SDKLastErrorCode GetSDKLastErrorCode();

  /**
    * 获取模组日志
    * @param log_storage_dir 日志上传后的存放目录
    */
  ErrorCode GetDeviceLog(const char* log_storage_dir);

  /**
    * 获取音频Dump
    * @param duration_time_s dump持续时间，单位S
    * @param log_storage_dir dump文件上传后的存储目录
    */
  ErrorCode GetAudioDump(long duration_time_s, const char* dump_storage_dir);

  /**
    * 模组工作切换。默认工作在天籁模式。
    * @param OperationMode 0为天籁模式，1为普通模式。
    * @return 错误码
    */
  ErrorCode SetDeviceMode(DeviceOperationMode mode);

  /**
    * 获取设备工作模式信息
    * @return 工作模式
    */
  DeviceOperationMode GetDeviceMode();

  /**
    * 模组Doa结果回调使能。默认不开启。
    * @param is_enable false为不开启，true为开启。
    * @return 错误码
    */
  ErrorCode SetDeviceDoaUploadEnable(bool is_enable);
  
  /**
    * 设置设备系统时间，单位毫秒
    * @param timeMs 系统时间
    * @return 错误码
    */
  ErrorCode SetDeviceTimeMs(int64_t timeMs);

  /**
    * 设置设备mute
    * @param stream_type upstream(采集)/downstream(播放)
    * @param status  true(mute)/false(unmute)
    * @return 错误码
    */
  ErrorCode SetDeviceMute(AvStreamType stream_type, bool is_mute);

  /**
    * 获取设备mute信息
    * @param stream_type upstream(采集)/downstream(播放)
    * @return true:mute/false:unmute
    */
  bool GetDeviceMute(AvStreamType stream_type);

  /**
    * 获取模组设备版本信息
    * @return 返回模组设备版本信息
    */
  const char* GetDeviceVersion();

  /**
    * 获取模组设备状态
    * @return 返回模组设备状态，枚举 DeviceStatus
    */
  DeviceStatus GetDeviceStatus();

  /**
    * 获取模组设备信息
    * @return 返回模组设备信息，Json字符串
    */
  const char* GetDeviceInformation();

  /**
    * 获取所有设备信息
    * @return 返回模组设备信息，Json字符串
    */
  const char* GetAllDeviceInformation();

  /**
    * 获取所有声卡设备参数
    * @return 返回模组声卡设备参数，Json字符串
    */
  const char* GetAllDevicesSoundCardParams();

  /**
    * 获取所有设备数量
    * @param 设备总数量
    * @return 设备数量, 失败返回ErrorCode::FAILED
    */
  int GetDevicesCount();

  /**
    * 重启模组设备
    * @return 错误码
    */
  ErrorCode RebootDevice();

  /**
    * 开始模组设备OTA
    * @param file OTA升级包名
    * @param md5 OTA升级包MD5
    * @return 错误码
    */
  ErrorCode StartDeviceOTA(const char* file, const char* md5);

  /**
    * 主动获取模组设备OTA状态
    * @return 模组设备OTA状态，枚举OTAState
    */
  OTAState DeviceOTAState();

  /**
    * 获取天籁SDK版本信息
    * @return 天籁SDK版本信息
    */
  const char* Version();

  /**
    * 设置SDK日志回调
    * @param cb SDK日志回调函数
    * @note SDK内部日志会通过此回调输出至用户回调函数内
    */
  void SetLogCb(CustomLogCb cb);

  /**
    * 获取Hid设备参数
    * @param hid_params HID设备参数
    * @return
    */
  void GetHidDeviceParams(HidDeviceParams_t* hid_params);

  /**
    * 开始获取模组原始音频PCM数据
    * @return 错误码
    * @note 原始音频数据会通过OnAudioPcmData上报至用户回调内
    */
  ErrorCode StartUploadDeviceAudioPcm();

  /**
    * 停止获取模组原始音频PCM数据
    * @return 错误码
    */
  ErrorCode StopUploadDeviceAudioPcm();
  // API_LEVEL_END_1

  // API_LEVEL_START_1
  /**
    * 设置/获取麦克风全局mute状态
    * @param mute 静音/非静音状态（true:静音/false:非静音）
    */
  ErrorCode SetDeviceMicrophoneOverallMute(bool mute);
  ErrorCode GetDeviceMicrophoneOverallMute(bool& mute);
  // API_LEVEL_END_1

  // API_LEVEL_START_3
  /**
    * 设置/获取指定设备是否选中进行混音输出
    * @param device_sn_str 状态更新的设备sn号
    * @param selected 是否选中（true:选中/false:非选中）
    */
  ErrorCode SetDeviceMicrophoneSelected(const char* device_sn_str, bool selected);
  ErrorCode GetDeviceMicrophoneSelected(const char* device_sn_str, bool& selected);
  // API_LEVEL_END_3

  // API_LEVEL_START_1
  /**
    * 设置/获取麦克风全局前处理模式
    * @param mode 前处理模式（normal:普通模式/false:天籁模式（缺省））
    */
  ErrorCode SetDevicePreprocessOverallMode(DevicePreprocessMode mode);
  ErrorCode GetDevicePreprocessOverallMode(DevicePreprocessMode& mode);

  /**
    * 设置/获取设备全局前处理传统降噪强度
    * @param level 前处理传统降噪强度（0.0f~1.0f）
    */
  ErrorCode SetDevicePreprocessOverallTraditionalNsAggressiveness(float aggr);
  ErrorCode GetDevicePreprocessOverallTraditionalNsAggressiveness(float& aggr);
  
  /**
    * 设置/获取设备全局前处理AI降噪强度
    * @param level 前处理AI降噪强度（0.0f~1.0f）
    */
  ErrorCode SetDevicePreprocessOverallAiNsAggressiveness(float aggr);
  ErrorCode GetDevicePreprocessOverallAiNsAggressiveness(float& aggr);
  // API_LEVEL_END_1

  // API_LEVEL_START_3
  /**
    * 设置/获取指定设备前处理朝向模式
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 设备前处理朝向
    */
  ErrorCode SetDeviceOrientationMode(const char* device_sn_str, DeviceOrientationMode mode);
  ErrorCode GetDeviceOrientationMode(const char* device_sn_str, DeviceOrientationMode& mode);

  /**
    * 设置/获取指定设备前处理AudioZooming工作模式（注意: 该模式必须是在天籁模式下设置才会生效）
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 前处理AudioZooming工作模式
    */
  ErrorCode SetDevicePreprocessZoomingMode(const char* device_sn_str, DevicePreprocessZoomingMode mode);
  ErrorCode GetDevicePreprocessZoomingMode(const char* device_sn_str, DevicePreprocessZoomingMode& mode);

  /**
    * 设置/获取指定设备前处理AudioRange工作模式（注意: 该模式必须是在天籁模式下设置才会生效）
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 前处理AudioRange工作模式
    */
  ErrorCode SetDevicePreprocessRangeMode(const char* device_sn_str, DevicePreprocessRangeMode mode);
  ErrorCode GetDevicePreprocessRangeMode(const char* device_sn_str, DevicePreprocessRangeMode& mode);
  // API_LEVEL_END_3

  // API_LEVEL_START_2
  /**
    * 设置/获取设备全局喇叭播放eq工作模式
    * @param mode 喇叭播放eq工作模式
    */
  ErrorCode SetDeviceSpeakerOverallEqMode(DeviceSpeakerEqMode mode);
  ErrorCode GetDeviceSpeakerOverallEqMode(DeviceSpeakerEqMode& mode);
  // API_LEVEL_END_2

  // API_LEVEL_START_3
  /**
    * 设置/获取全局喇叭播放音量
    * @param volume 喇叭全局音量（0.0f~1.0f）
    */
  ErrorCode SetDeviceSpeakerOverallVolume(float volume);
  ErrorCode GetDeviceSpeakerOverallVolume(float& volume);

  /**
    * 设置/获取指定设备喇叭播放最大音量
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 喇叭播放最大音量（0.0f~1.0f）
    */
  ErrorCode SetDeviceSpeakerMaxVolume(const char* device_sn_str, float volume);
  ErrorCode GetDeviceSpeakerMaxVolume(const char* device_sn_str, float& volume);

  /**
    * 设置/获取指定设备喇叭静音
    * @param device_sn_str 状态更新的设备sn号
    * @param mute 静音状态
    */
  ErrorCode SetDeviceSpeakerMute(const char* device_sn_str, bool mute);
  ErrorCode GetDeviceSpeakerMute(const char* device_sn_str, bool& mute);

  /**
    * 设置/获取全局设备的工作（安全）模式（注：设置模式之后，模组会自动重启，重启后模式设置生效）
    * @param mode 全局设备的工作模式
    */
  ErrorCode SetDeviceOverallWorkingMode(DeviceWorkingMode mode);
  ErrorCode GetDeviceOverallWorkingMode(DeviceWorkingMode& mode);

  /**
    * 设置/获取指定设备的用户态麦克风延时，用于调节在复杂外设连接状态下麦克风采集-参考信号采集延时差异导致的非因果，导致回声消除效果变劣现象
    * @param device_sn_str 状态更新的设备sn号
    * @param delay 用户态麦克风延时，接受延时范围为[20, 400], 单位:ms
    */
  ErrorCode SetDeviceUserMicrophoneDelay(const char* device_sn_str, int delay_ms);
  ErrorCode GetDeviceUserMicrophoneDelay(const char* device_sn_str, int& delay_ms);
  // API_LEVEL_END_3
};

}

#endif //TEA_INSIDE_SDK_H
