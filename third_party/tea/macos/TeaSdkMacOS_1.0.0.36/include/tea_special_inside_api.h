/*
** Copyright (c) 2021 The TeaInsideSdk project. All rights reserved.
** Created by chunhuwang 2021
*/

#ifndef TEA_SPECIAL_INSIDE_SDK_H
#define TEA_SPECIAL_INSIDE_SDK_H

#include <stdint.h>
#include "tea_inside_common.h"

namespace TeaInsideSdk {

class TEA_INSIDE_SDK_EXPORT TeaSpecialInsideApi {
public:
  TeaSpecialInsideApi();
  virtual ~TeaSpecialInsideApi();

  ErrorCode Init(const char* config_file = nullptr, TEASdkParams_t* params = nullptr);
  void UnInit();

  /**
    * 获取设备信息
    * @return 返回设备信息完整消息。需要二次解析，提取设备信息。
    */
  const char* GetSpecialDeviceInformation();

  /**
    * 获取所有设备信息
    * @return 返回所有设备信息完整消息。需要二次解析，提取设备信息。
    */
  const char* GetAllSpecialDeviceInformation();

  /**
    * TC65、TC86专款设备升级
    * @param file 升级文件
    * @param file_size 升级文件大小
    * @param md5 升级文件MD5
    * @param version  升级文件版本
    * @param private_key  密钥
    * @return 返回升级结果
    */
  int SpecialDeviceNormalUpgrade(const char* file, int file_size, const char* md5, const char* version, const char* private_key);

  /**
    * TC6586POE设备升级
    * @param file 升级文件
    * @param md5 升级文件MD5
    * @return 返回升级结果
    */
  int SpecialDeviceStandardUpgrade(const char* file, const char* md5);

  /**
    * 设置日志回调函数
    * @param cb 日志回调函数
    * @note SDK内部日志会通过此回调输出至用户回调函数内
    */
  void SetSpecialDeviceLogCb(CustomLogCb cb);

  /**
    * 返回设备类型
    * @return 设备类型
    */
  InsideDeviceType GetInsideDeviceType();
 };

}

#endif //TEA_SPECIAL_INSIDE_SDK_H
