/*
** Copyright (c) 2021 The TeaInsideSdk project. All rights reserved.
** Created by chunhuwang 2022
*/

#ifndef TEA_INSIDE_COMMON_H
#define TEA_INSIDE_COMMON_H

#if defined(_WIN32)
#define TEA_INSIDE_SDK_EXPORT __declspec(dllexport)
#else
#define TEA_INSIDE_SDK_EXPORT __attribute__((visibility("default")))
#endif

typedef void(*CustomLogCb)(const char* msg);

enum ErrorCode : int {
  /**
    * 失败
    */
  FAILED = -1,
  /**
    * 成功
    */
  OK = 0,
};

enum SDKLastErrorCode : int {
  /**
    * 成功
    */
  LASTERR_SUCCESS = 0X00000000,
  /**
    * 未初始化
    */
  LASTERR_UNINIT = 0X10000000,
  /**
    * 初始化失败
    */
  LASTERR_INIT_FAILED = 0X10000001,
  /**
  * 调用参数异常
  */
  LASTERR_INPUT_PARAMS_ABNORMAL = 0X10000002,
  /**
    * 任务已在执行
    */
  LASTERR_TASK_IS_RUNNING = 0X10000003,
  /**
    * 文件不存在
    */
  LASTERR_FILE_IS_NOT_EXIST = 0X10000004,
  /**
    * HID设备打开异常
    */
  LASTERR_HID_DEVICE_ABNORMAL = 0X20000000,
  /**
    * HID 读异常
    */
  LASTERR_HID_READ_ABNORMAL = 0X20000001,
  /**
    * HID 写异常
    */
  LASTERR_HID_WRITE_ABNORMAL = 0X20000002,
  /**
    * HID 超时
    */
  LASTERR_HID_TIMEOUT_ABNORMAL = 0X20000003,
};

enum DeviceOperationMode : int {
  /**
    * 错误模式
    */
  DEVICE_OPERATION_MODE_UNKNOWN = -1,
  /**
    * 普通模式
    */
  DEVICE_OPERATION_MODE_NORMAL = 0,
  /**
    * 天籁模式
    */
  DEVICE_OPERATION_MODE_TEA = 1,
};

enum DeviceStatus : int {
  /**
    * 模组设备工作异常
    */
  DEV_STATUS_ABNORMAL = -1,
  /**
    * 模组设备工作正常
    */
  DEV_STATUS_NORMAL = 0,
};

enum SDKState : int {
  /**
    * SDK Engine未初始化
    */
  SDK_STATE_IDLE = 0,
  /**
    * SDK Engine初始化完成
    */
  SDK_STATE_INITIALIZED,
  /**
    * SDK Engine工作中
    */
  SDK_STATE_WORKING,
  /**
    * SDK Engine已停止
    */
  SDK_STATE_STOPPED,
};

enum OTAState : int {
  /**
    * 未开始
    */
  OTA_STATE_IDLE = 0,
  /**
    * OTA 升级开始
    */
  OTA_STATE_STARTED = 1,
  /**
    * OTA 升级校验
    */
  OTA_STATE_VERIFYING,
  /**
    * OTA 升级包传输
    */
  OTA_STATE_TRANSFERRING,
  /**
    * OTA 升级进行中
    */
  OTA_STATE_UPGRADING,
  /**
    * OTA 升级结束
    */
  OTA_STATE_END,
};

enum OTAResult : int {
  /**
    * 失败
    */
  OTA_RESULT_FAILED = -1,
  /**
    * 成功
    */
  OTA_RESULT_SUCCESS = 0,
  /**
    * 升级文件异常
    */
  OTA_RESULT_FAILED_UPDATE_FILE_ABNORMAL,
  /**
    * 升级文件校验失败
    */
  OTA_RESULT_FAILED_UPDATE_FILE_VERIFY,
  /**
    * 通信异常
    */
  OTA_RESULT_FAILED_HID_TRANSPORT,
  /**
    * 设备没有足够空间
    */
  OTA_RESULT_FAILED_INSIDE_NO_FREE_SPACE,
  /**
    * 设备内部异常
    */
  OTA_RESULT_FAILED_INSIDE_ABNORMAL,
  /**
   * 文件操作异常
   */
  OTA_RESULT_FAILED_FILE_OPERATION,
};

enum AvStreamType : int {
  /**
    * 采集，上行音频流
    */
  AUDIO_CAPTURE_STREAM = 0,
  /**
    * 播放，下行音频流
    */
  AUDIO_PLAYBACK_STREAM = 1,
};

enum InsideDeviceType : int {
  /**
    * 未知设备
    */
  UNKNOWN_DEVICE = 0,
  /**
    * 专款设备(TC65\86)
    */
  SPECIAL_DEVICE = 1,
  /**
    * 非专款设备(TC6586POE)
    */
  GENERAL_DEVICE = 2,
};

enum InsideDeviceControlCommand : int {
  /**
    * 未知命令
    */
  INSIDE_DEVICE_CMD_UNKNOWN = -1,
  /**
    * mic 自检
    */
  INSIDE_DEVICE_CMD_INSPECT_MIC = 0,
  /**
    * 点亮 mic
    */
  INSIDE_DEVICE_CMD_LIGHT_UP_MIC,
  /**
    * 复位 mic led
    */
  INSIDE_DEVICE_CMD_RESET_MIC,
  /**
    * mute mic
    */
  INSIDE_DEVICE_CMD_MUTE_MIC,
  /**
    * unmute mic
    */
  INSIDE_DEVICE_CMD_UNMUTE_MIC,
  // Overall Mic mute
  INSIDE_DEVICE_CMD_MICROPHONE_OVERALL_MUTE,
  // Mic selected
  INSIDE_DEVICE_CMD_MICROPHONE_SELECTED,
  // Overall Preprocess mode
  INSIDE_DEVICE_CMD_PREPROCESS_OVERALL_MODE,
  // Overall Transditional Noise Suppress Aggressiveness
  INSIDE_DEVICE_CMD_PREPROCESS_OVERALL_TRNS_AGGR,
  // Overall AI noise Supress Aggressiveness
  INSIDE_DEVICE_CMD_PREPROCESS_OVERALL_AINS_AGGR,
  // Zooming mode
  INSIDE_DEVICE_CMD_PREPROCESS_ZOOMING_MODE = 10,
  // Range mode
  INSIDE_DEVICE_CMD_PREPROCESS_RANGE_MODE,
  // Speaker overall EQ mode
  INSIDE_DEVICE_CMD_SPEAKER_OVERALL_EQ_MODE,
  // Speaker overall volume
  INSIDE_DEVICE_CMD_SPEAKER_OVERALL_VOLUME,
  // Speaker max volume
  INSIDE_DEVICE_CMD_SPEAKER_MAX_VOLUME,
  // Speaker mute
  INSIDE_DEVICE_CMD_SPEAKER_MUTE,
  // Orientation mode
  INSIDE_DEVICE_CMD_ORIENTATION_MODE,
  // Overall working mode
  INSIDE_DEVICE_CMD_OVERALL_WORKING_MODE,
  // User microphone delay
  INSIDE_DEVICE_CMD_PREPROCESS_USER_MICROPHONE_DELAY = 30,

  INSIDE_DEVICE_NB
};

enum InsideDeviceDebugFileType : int {
  /**
    * 未知类型
    */
  INSIDE_DEVICE_DBG_FILE_UNKNOWN = -1,
  /**
    * log zip
    */
  INSIDE_DEVICE_DBG_FILE_LOG = 0,
  /**
    * dump zip
    */
  INSIDE_DEVICE_DBG_FILE_DUMP = 1,
};

enum InsideDeviceTransportType : int {
  /**
    * 未知类型
    */
  INSIDE_DEVICE_TRANSPORT_UNKNOWN = -1,
  /**
    * 使用hid传输
    */
  INSIDE_DEVICE_TRANSPORT_HID = 0,
  /**
    * 使用vsp虚拟串口传输
    */
  INSIDE_DEVICE_TRANSPORT_VSP = 1,
};

enum DevicePreprocessMode : int {
  /**
    * 未知模式
    */
  DEVICE_PREPROCESS_MODE_UNKNOWN = -1,
  /**
    * 普通模式
    */
  DEVICE_PREPROCESS_MODE_NORMAL = 0,
  /**
    * 天籁模式（缺省）
    */
  DEVICE_PREPROCESS_MODE_TEA = 1,
};

enum DeviceSpeakerEqMode : int {
  /**
    * 未知喇叭eq模式类型
    */
  INSIDE_DEVICE_SPEAKER_EQ_MODE_UNKNOWN = -1,
  /**
    * 会议模式
    */
  INSIDE_DEVICE_SPEAKER_EQ_MODE_MEETING = 0,
  /**
    * 音乐模式
    */
  INSIDE_DEVICE_SPEAKER_EQ_MODE_MUSIC = 1,
};

enum DeviceOrientationMode: int {
  /**
    * 未知设备Orientation
    */
  INSIDE_DEVICE_ORIENTATION_MODE_UNKNOWN = -1,
  /**
    * 横向朝向模式
    */
  INSIDE_DEVICE_ORIENTATION_MODE_CROSSWISE = 0,
  /**
    * 纵向朝向模式
    */
  INSIDE_DEVICE_ORIENTATION_MODE_LENGTHWISE = 1,
  /**
    * 桌面（朝上）模式
    */
  INSIDE_DEVICE_ORIENTATION_MODE_UPWARD = 2,
};

enum DevicePreprocessZoomingMode : int {
  /**
    * 未知前处理zooming模式类型
    */
  INSIDE_DEVICE_PREPROCESS_ZOOMING_MODE_UNKNOWN = -1,
  /**
    * 全向拾音
    */
  INSIDE_DEVICE_PREPROCESS_ZOOMING_MODE_OMNIDIRECTIONAL = 0,
  /**
    * 30度拾音
    */
  INSIDE_DEVICE_PREPROCESS_ZOOMING_MODE_THIRTY_DEGREE = 1,
  /**
    * 45度拾音
    */
  INSIDE_DEVICE_PREPROCESS_ZOOMING_MODE_FORTYFIVE_DEGREE = 2,
  /**
    * 60度拾音
    */
  INSIDE_DEVICE_PREPROCESS_ZOOMING_MODE_SIXTY_DEGREE = 3,
};

enum DevicePreprocessRangeMode : int {
  /**
    * 未知前处理range模式类型
    */
  INSIDE_DEVICE_PREPROCESS_RANGE_MODE_UNKNOWN = -1,
  /**
    * 近场模式
    */
  INSIDE_DEVICE_PREPROCESS_RANGE_MODE_ALL_FIELD = 0,
  /**
    * 远场模式
    */
  INSIDE_DEVICE_PREPROCESS_RANGE_MODE_NEAR_FIELD = 1,
};

enum DeviceWorkingMode : int {
  /**
    * 未知工作模式
    */
  INSIDE_DEVICE_WORKING_MODE_UNKNOWN = -1,
  /**
    * 普通工作模式（缺省，启动缺省是非静音）
    */
  INSIDE_DEVICE_WORKING_MODE_DEFAULT = 0,
  /**
    * 安全工作模式（启动是静音）
    */
  INSIDE_DEVICE_WORKING_MODE_SECURITY = 1,
};

namespace TeaInsideSdk {

typedef struct HidDeviceParams {
  unsigned short vendor_id;
  unsigned short product_id;
  unsigned short usage_page;
  unsigned short usage;

#if defined(__ANDROID__)
  unsigned int   fd;
  unsigned int   bus_num;
  unsigned int   dev_num;
  unsigned int   in_ep_addr;
  unsigned int   out_ep_addr;
#endif

  char description[256];
} HidDeviceParams_t;

typedef struct TEASdkParams {
  HidDeviceParams_t hid_device_params;
} TEASdkParams_t;

enum TEASdkPcmFormat : int {
  TEA_PCM_FORMAT_UNKNOWN = -1, /** Unknown */\
  TEA_PCM_FORMAT_S8,  /** Signed 8 bit */\
  TEA_PCM_FORMAT_U8,  /** Unsigned 8 bit */\
  TEA_PCM_FORMAT_S16_LE,  /** Signed 16 bit Little Endian */\
  TEA_PCM_FORMAT_S16_BE,  /** Signed 16 bit Big Endian */\
};

enum TEASdkSampleRate : int {
  TEA_AUDIO_SAMPLE_RATE_UNKNOWN = -1,
  TEA_AUDIO_SAMPLE_RATE_16K = 16000,
  TEA_AUDIO_SAMPLE_RATE_48K = 48000,
  TEA_AUDIO_SAMPLE_RATE_NB, /*useless type as counter*/\
};

typedef struct TEASdkAudioFormat {
  TEASdkPcmFormat            format;
  TEASdkSampleRate           sample_rate;
  uint32_t                   period_size;
  uint32_t                   channels;
} TEASdkAudioFormat_t;

enum TEASdk3rdDeviceEventType : uint32_t {
  EVENT_UNKNOWN = 0,
  EVENT_DEVICE_VERSION = 1,
  EVENT_DEVICE_INFO,
  EVENT_CONFIG_DONGLE_NAME,
  EVENT_DEVICE_UPGRADE,
  EVENT_ALL_DEVICES_VERSION,
};

};

#endif //TEA_INSIDE_COMMON_H
