/*
** Copyright (c) 2021 The TeaInsideSdk project. All rights reserved.
** Created by qoroliang 2021
*/

#pragma once

#include "tea_inside_common.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace TeaInsideSdk {

class TEA_INSIDE_SDK_EXPORT TeaInsideEventCbDelegate {
public:
  TeaInsideEventCbDelegate() {};
  virtual ~TeaInsideEventCbDelegate() {};

  /**
    * OTA 升级过程中状态更新回调
    * @param state 表示更新的状态，对应枚举 OTAState
    */
  virtual void OnOTAStateChange(OTAState state) = 0;

  /**
    * OTA 升级最终结果回调
    * @param result 表示OTA升级是否成功，对应枚举 OTAResult
    */
  virtual void OnOTAResultUpdate(OTAResult result) = 0;

  /**
    * 说话人 DOA 角度更新
    * @param angle 说话人角度（0.0f~180.0f）
    */
  virtual void OnDOAUpdate(float angle) = 0;

  /**
    * 模组日志上报结果
    * @param result 成功/失败
    */
  virtual void OnDeviceLog(ErrorCode result) = 0;

  /**
    * audio dump上报结果
    * @param result 成功/失败
    */
  virtual void OnAudioDump(ErrorCode result) = 0;

  /**
    * 天籁模组 mute/unmute 状态上报
    * @param stream_type upstream(采集)/downstream(播放)
    * @param status  true(mute)/false(unmute)
    */
  virtual void OnMuteStatusUpdate(AvStreamType stream_type, bool is_mute) = 0;

  /**
    * 天籁模组 天籁模式/普通模式 状态上报
    * @param status  true(mute)/false(unmute)
    */
  virtual void OnModeStatusUpdate(DeviceOperationMode mode) = 0;

  /**
    * 上报天籁模组控制指令执行结果
    * @param cmd 控制指令
    * @param arg 上报参数
    * @param arg 上报参数长度
    */
  virtual void OnDeviceControlResult(InsideDeviceControlCommand cmd, const void* arg, int arg_size) = 0;

#if defined(WIN32) || defined(TARGET_OS_OSX)
	/**
    * 重启结束上报Host端
    * @param result 成功/失败
    * @note 固件升级后，模组会重启
    */
	virtual void OnInsideDeviceReboot(ErrorCode result) = 0;
#endif

#if defined (WIN32)
	/**
    * 上报模组PCM
    * @param format 音频描述
    * @param buffer pcm缓冲
    * @param buffer_size pcm缓冲长度
    * @note 固件升级后，模组会重启
    */
	virtual void OnAudioPcmData(TEASdkAudioFormat_t* format, uint8_t* buffer, size_t buffer_size) = 0;
#endif
};

class TEA_INSIDE_SDK_EXPORT TeaInside3rdDeviceEventCbDelegate {
public:
  TeaInside3rdDeviceEventCbDelegate() {};
  virtual ~TeaInside3rdDeviceEventCbDelegate() {};

  /**
    * 升级过程中状态更新回调
    * @param special_device 正在升级的设备名
    * @param state 表示更新的状态，对应枚举 OTAState
    */
  virtual void OnUpgradeStateChange(const char* special_device, OTAState state) = 0;

  /**
    * 升级最终结果回调
    * @param special_device 正在升级的设备名
    * @param result 表示OTA升级是否成功，对应枚举 OTAResult
    */
  virtual void OnUpgradeResult(const char* special_device, OTAResult result) = 0;

  /**
    * 第三方设备指令执行结果回调
    * @param special_device 正在执行指令的设备名
    * @param event_type 事件类型
    * @param result 执行结果
    * @param buffer 返回内容
    * @param buffer_len 返回内容长度
    */
  virtual void OnControlCmdResult(const char* special_device,
                                  uint32_t event_type,
                                  int result,
                                  const void* buffer,
                                  uint32_t buffer_len) = 0;

};

class TEA_INSIDE_SDK_EXPORT TeaInsideDeviceUserStateUpdateCbDelegate {
public:
  virtual ~TeaInsideDeviceUserStateUpdateCbDelegate() {}

  // API_LEVEL_START_1
  /**
    * 设备麦克风全局静音状态更新回调
    * @param mute 静音/非静音状态（true:静音/false:非静音）
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceMicrophoneOverallMuteStateUpdate(bool mute, ErrorCode err) = 0;
  // API_LEVEL_END_1

  // API_LEVEL_START_3
  /**
    * 设备是否选中进行混音输出状态更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param selected 是否选中
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceMicrophoneSelectedStateUpdate(const char* device_sn_str, bool selected, ErrorCode err) = 0;
  // API_LEVEL_END_3

  // API_LEVEL_START_1
  /**
    * 设备前处理全局工作模式状态更新回调
    * @param mode 前处理模式（normal:普通模式/false:天籁模式）
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessOverallModeStateUpdate(DevicePreprocessMode mode, ErrorCode err) = 0;

  /**
    * 设备前处理全局传统降噪强度状态更新回调
    * @param aggr 表示更新的前处理传统降噪强度
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessTraditionalNsAggressivenessUpdate(float aggr, ErrorCode err) = 0;

  /**
    * 设备前处理全局AI降噪强度状态更新回调
    * @param aggr 表示更新的前处理AI降噪强度
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessAiNsAggressivenessUpdate(float aggr, ErrorCode err) = 0;
  // API_LEVEL_END_1

  // API_LEVEL_START_3
  /**
    * 设备前处理拾音角度（audiozooming）状态更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 表示更新的拾音角度模式
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessZoomingModeStateUpdate(const char* device_sn_str, DevicePreprocessZoomingMode mode, ErrorCode err) = 0;

  /**
    * 设备前处理拾音范围状态更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 表示更新的拾音范围
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessRangeModeStateUpdate(const char* device_sn_str, DevicePreprocessRangeMode mode, ErrorCode err) = 0;
  // API_LEVEL_END_3

  // API_LEVEL_START_2
  /**
    * 设备喇叭全局播放eq模式状态更新回调
    * @param mode 表示更新的播放eq模式
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceSpeakerOverallEqModeStateUpdate(DeviceSpeakerEqMode mode, ErrorCode err) = 0;
  // API_LEVEL_END_2

  // API_LEVEL_START_3
  /**
    * 设备喇叭播放全局音量更新回调
    * @param overall_volume 表示更新的最大播放音量
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceSpeakerOverallVolumeUpdate(float overall_volume, ErrorCode err) = 0;

  /**
    * 设备喇叭播放最大音量更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param max_volume 表示更新的最大播放音量
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceSpeakerMaxVolumeStateUpdate(const char* device_sn_str, float max_volume, ErrorCode err) = 0;

  /**
    * 设备喇叭播放静音更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param mute 表示更新的静音状态
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceSpeakerMuteStateUpdate(const char* device_sn_str, bool mute, ErrorCode err) = 0;

  /**
    * 设备摆放朝向状态更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param mode 表示更新的朝向状态
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceOrientationModeStateUpdate(const char* device_sn_str, DeviceOrientationMode mode, ErrorCode err) = 0;

  /**
    * 设备全局工作模式更新回调
    * @param mode 表示更新的工作模式
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDeviceOverallWorkingModeStateUpdate(DeviceWorkingMode mode, ErrorCode err) = 0;

  /**
    * 设备用户态麦克风延时状态更新回调
    * @param device_sn_str 状态更新的设备sn号
    * @param delay_ms 表示更新的麦克风延时,单位: 毫秒
    * @param err 错误码，只有当ErrorCode::OK的时候才可用（OK,FAILED,TIMEOUT...）
    */
  virtual void OnDevicePreprocessUserMicrophoneDelayUpdate(const char* device_sn_str, int16_t delay_ms, ErrorCode err) = 0;
  // API_LEVEL_END_3
};

class TEA_INSIDE_SDK_EXPORT TeaInsideDeviceConnectStateUpdateCbDelegate {
public:
  virtual ~TeaInsideDeviceConnectStateUpdateCbDelegate() {}

  /**
    * 设备连接网络/移出网络回调
    * @param device_sn_str 状态更新的设备sn号
    * @param connect 表示连接网络/断开网络
    */
  virtual void OnDeviceConnectStateUpdate(const char* device_sn_str, bool connect, const char* connect_message) = 0;
};

class TEA_INSIDE_SDK_EXPORT TeaInsideDeviceExceptionCbDelegate {
public:
  virtual ~TeaInsideDeviceExceptionCbDelegate() {}

  /**
    * 设备运行出错回调
    * @param device_sn_str 出错设备sn号
    * @param exception_message 错误描述字段，json string
    * @param message_length 错误描述字段长度
    */
  virtual void OnDeviceExceptionUpdate(const char* device_sn_str, const char* exception_message) = 0;
};

}