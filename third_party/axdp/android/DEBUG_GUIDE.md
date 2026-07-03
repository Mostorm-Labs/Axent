# Android 调试指南

本文档记录在 Android Studio 中调试 AXDP 项目时遇到的问题及解决方案，重点覆盖 Native (JNI/NDK) 调试流程。

---

## 一、Native 调试环境配置

### 1.1 错误现象

```
Error running 'app': Unsupported device. This device cannot be debugged using the native debugger.
```

### 1.2 排查流程

**Step 1：先用 Java Only 模式验证基础环境**

Run → Edit Configurations → Debugger → Debug type 改为 `Java Only`，能正常运行说明问题仅在 native debugger。

**Step 2：检查 `sharedUserId`**

查看 `AndroidManifest.xml`：

```xml
<manifest android:sharedUserId="android.uid.system">
```

若存在 `sharedUserId="android.uid.system"`，LLDB 无法 attach 到 system UID 进程，这是 Android 安全限制，与设备无关。

**Step 3：临时移除 `sharedUserId` 以启用 native 调试**

注释掉该属性，重新 Build，先卸载旧版本再安装：

```bash
adb uninstall com.auditoryworks.axdp
```

> 注意：若设备上存在以 system UID 预装的同包名 APK，普通 uninstall 只删用户层，系统层仍在。
> 此时需确认签名一致后直接覆盖安装（见 1.3）。

**Step 4：配置 Symbol Directories**

Run → Edit Configurations → Debugger → Symbol Directories，添加：

```
<project>/android/app/build/intermediates/cmake/debug/obj
```

此路径包含带完整调试符号的 `.so`（未 strip 版本），LLDB 需要它来解析断点和变量。

**Step 5：将 Debug type 改为 `Dual`**

同时调试 Java 和 Native 代码。

---

### 1.3 签名与覆盖安装

若设备上已有 system UID 版本的 APK，需确认签名一致才能覆盖：

```bash
# 检查设备上已安装 APK 的签名
adb pull /system/app/<AppDir>/<App>.apk /tmp/app.apk
keytool -printcert -jarfile /tmp/app.apk

# 检查本地 keystore 签名
keytool -list -v -keystore android/xbh.jks -storepass android
```

两者 SHA1/SHA256 一致则可覆盖安装：

```bash
adb install -r android/app/build/outputs/apk/debug/axdp-service-debug-V*.apk
```

---

### 1.4 设备调试能力检查

```bash
# 查看设备型号和 Android 版本
adb shell getprop ro.product.model
adb shell getprop ro.build.version.release

# 确认是否为 userdebug/eng 版本（支持 native 调试）
adb shell getprop ro.build.type       # 期望: userdebug 或 eng
adb shell getprop ro.debuggable       # 期望: 1

# 检查 ptrace 限制（0=允许，1+=受限）
adb shell cat /proc/sys/kernel/yama/ptrace_scope
```

---

## 二、Native 层 USB/HID 设备打开失败

### 2.1 错误现象

`libusb_claim_interface` 返回 `-6`（`LIBUSB_ERROR_BUSY`），设备打开失败，`nativeCreateDevice` 返回 0。

### 2.2 根本原因

Java 层调用 `UsbManager.openDevice()` 获取 fd 后直接传给 native，但**没有先调用 `claimInterface()`**。内核 HID 驱动仍持有接口，导致 native 层 `libusb_claim_interface` 失败。

### 2.3 排查流程

**Step 1：在 `hidapi_initialize_device` 处打断点**

文件：`third_party/hidapi/libusb/hid.c`，函数 `hidapi_initialize_device`。

观察 `libusb_claim_interface` 的返回值 `res`：

| 返回值 | 含义 |
|--------|------|
| `0` | 成功 |
| `-3` | `LIBUSB_ERROR_ACCESS`：无访问权限 |
| `-6` | `LIBUSB_ERROR_BUSY`：接口被内核驱动占用 |
| `-4` | `LIBUSB_ERROR_NO_DEVICE`：设备已断开 |

**Step 2：确认 Java 层是否在传 fd 前 claim 了接口**

检查 `openUsbDevice` 方法，正确写法：

```java
synchronized private boolean openUsbDevice(UsbDevice usbDevice, int interfaceNum) {
    UsbDeviceConnection conn = usbHelper.getUsbManager().openDevice(usbDevice);
    if (conn == null) {
        Log.e(TAG, "openDevice failed");
        return false;
    }
    // 必须先 claim，force=true 会强制 detach 内核驱动
    UsbInterface usbInterface = usbDevice.getInterface(interfaceNum);
    if (!conn.claimInterface(usbInterface, true)) {
        Log.e(TAG, "claimInterface failed for interface " + interfaceNum);
        conn.close();
        return false;
    }
    int fileDescriptor = conn.getFileDescriptor();
    deviceHandle = nativeCreateDevice(fileDescriptor, usbDevice.getVendorId(),
            usbDevice.getProductId(), interfaceNum);
    return deviceHandle != 0;
}
```

**Step 3：检查 hidapi 的 detach 逻辑（Android 特殊路径）**

`libusb_wrap_sys_device` 打开的设备，`libusb_kernel_driver_active` 会返回 `LIBUSB_ERROR_NOT_SUPPORTED(-12)`，导致原始代码跳过 detach。

已在 `hid.c` 中修复，增加对 `-12` 的处理，强制尝试 detach：

```c
} else if (res == LIBUSB_ERROR_NOT_SUPPORTED) {
    // Android wrap_sys_device 路径：unconditionally try detach
    res = libusb_detach_kernel_driver(dev->device_handle, intf_desc->bInterfaceNumber);
    if (res == 0) {
        dev->is_driver_detached = 1;
    }
    // 忽略失败，由后续 claim_interface 决定最终结果
}
```

---

## 三、安装失败：INSTALL_FAILED_SHARED_USER_INCOMPATIBLE

### 3.1 错误现象

```
INSTALL_FAILED_SHARED_USER_INCOMPATIBLE: Package tried to change user android.uid.system
```

### 3.2 原因

APK 的 `sharedUserId` 与设备上已安装版本不一致（一个有，一个没有）。

### 3.3 解决方案

确保 `AndroidManifest.xml` 中的 `sharedUserId` 状态与当前要安装的 APK 一致，重新 Build 后安装。

调试时的操作顺序：

1. 注释 `sharedUserId` → Build → 卸载旧包 → 安装新包 → 调试
2. 调试完成 → 恢复 `sharedUserId` → Build → 覆盖安装（签名需一致）

---

## 四、常用调试命令速查

```bash
# 查看已连接设备
adb devices

# 查看包安装位置和 UID
adb shell dumpsys package com.auditoryworks.axdp | grep -E "uid|codePath|install"

# 卸载应用
adb uninstall com.auditoryworks.axdp

# 覆盖安装（保留数据）
adb install -r <apk路径>

# 实时查看 native 日志（过滤 axdp 相关）
adb logcat -s axdp_native:* AxdpService:* TailService:*

# 查看 USB 设备列表
adb shell lsusb 2>/dev/null || adb shell cat /sys/kernel/debug/usb/devices

# 检查接口是否被 claim
adb shell cat /proc/bus/usb/devices
```

---

## 五、Native 日志调试（无法使用 native debugger 时的替代方案）

当设备限制导致 native debugger 无法使用时，在 C++ 代码中加入日志：

```cpp
#include <android/log.h>
#define LOG_TAG "axdp_native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
```

Android Studio Logcat 过滤：`tag:axdp_native`
