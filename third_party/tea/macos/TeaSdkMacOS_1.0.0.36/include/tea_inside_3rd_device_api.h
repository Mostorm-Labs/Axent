/*
** Copyright (c) 2021 The TeaInsideSdk project. All rights reserved.
** Created by chunhuwang 2023
*/

#ifndef TEA_INSIDE_3RD_DEVICE_SDK_H
#define TEA_INSIDE_3RD_DEVICE_SDK_H

#include <stdint.h>
#include "tea_inside_common.h"

namespace TeaInsideSdk {

class TeaInside3rdDeviceEventCbDelegate;

class TEA_INSIDE_SDK_EXPORT TeaInside3rdDeviceApi {
public:
  TeaInside3rdDeviceApi();
  virtual ~TeaInside3rdDeviceApi();

  /**
    * 注册事件回调类
    * @param delegate 事件回调类
    */
  void Register3rdEventCb(TeaInside3rdDeviceEventCbDelegate* delegate);

  /**
    * 控制第三方设备
    * @param special_device 指定预操作设备的名称
    * @param top_event_type 父事件类型
    * @param sub_event_type 子设备类型
    * @param buffer 控制参数
    * @param buffer_len 参数长度
    * @note 异步访问, 结果通过回调返回
    * @return 错误码
    */
  ErrorCode ThirdPartyDeviceControl(const char* special_device,
                                    uint32_t event_type,
                                    const void* buffer,
                                    uint32_t buffer_len);

  /**
    * 升级指定设备
    * @param special_device 设备名
    * @param path_to_file 升级文件, 全路径. 不要包含中文路径.
    * @param file_md5 升级文件MD5
    * @note 异步访问, 结果通过回调返回
    * @return 错误码
    */
  ErrorCode ThirdPartyDeviceUpgrade(const char* special_device,
                                    const char* path_to_file,
                                    const char* file_md5);

};

}

#endif //TEA_INSIDE_3RD_DEVICE_SDK_H