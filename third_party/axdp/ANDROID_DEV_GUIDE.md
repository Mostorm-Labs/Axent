# Android 项目开发说明

本文档面向首次接触 AXDP Android 项目的开发者，说明项目结构、目标设备参数、设备选择逻辑的代码位置，以及调试环境的配置方式。

---

## 一、项目结构

```
axdp/                               # 仓库根目录（同时也是 native 库源码根）
├── src/                            # AXDP 协议 native 核心实现
│   ├── axdp_api.cpp / .h           # 对外 API 实现
│   ├── hid_device.cpp / .h         # HID 设备封装（调用 hidapi）
│   ├── device_accessor_impl.cpp    # DeviceAccessor 实现，Create() 在此
│   ├── message_engine.cpp          # 消息引擎
│   └── ...
├── include/                        # 公共头文件
│   ├── axdp_api.h                  # Java JNI 调用的 C API 声明
│   └── axdp_defines.h
├── third_party/
│   └── hidapi/libusb/
│       └── hid.c                   # libusb 版 hidapi 实现（Android 使用此版本）
├── android/                        # Android 应用工程
│   ├── app/src/main/
│   │   ├── java/com/auditoryworks/axdp/
│   │   │   ├── AxdpService.java    # 主服务，负责 S55/Tail 设备的连接与 AXDP 协议通信
│   │   │   ├── TailService.java    # Tail 设备独立服务
│   │   │   ├── TailBoxService.java # TailBox 设备服务（级联场景）
│   │   │   ├── MainActivity.java   # 调试入口 Activity
│   │   │   └── beans/
│   │   │       └── DeviceDesc.java # 设备参数静态常量（VID/PID/接口号）★
│   │   └── cpp/
│   │       ├── CMakeLists.txt      # NDK 构建入口
│   │       ├── native_axdp_api.cpp # JNI 接口，Java 调用 native 的入口
│   │       └── axdp -> ../../../../../../  # ★ 软链接，指向仓库根目录
│   └── DEBUG_GUIDE.md              # 调试环境配置与常见问题排查
└── AXDP_Protocol.md                # AXDP 协议文档
```

### 软链接说明

`android/app/src/main/cpp/axdp` 是一个指向**仓库根目录**的软链接：

```
android/app/src/main/cpp/axdp  →  /path/to/axdp/（仓库根）
```

CMakeLists.txt 通过以下方式将根目录的 native 代码引入 Android 构建：

```cmake
# android/app/src/main/cpp/CMakeLists.txt
add_subdirectory("./axdp")          # 等价于 add_subdirectory("仓库根/")
target_include_directories(axdp_android PUBLIC ./axdp)
```

这意味着 `cpp/axdp/src/`、`cpp/axdp/include/`、`cpp/axdp/third_party/` 实际上就是仓库根目录下的 `src/`、`include/`、`third_party/`，**修改根目录的 native 源码会直接影响 Android 构建**，无需复制文件。

**克隆仓库后需手动创建软链接**（git 不跟踪软链接目标的绝对路径）：

```bash
cd android/app/src/main/cpp
ln -s ../../../../../ axdp
```

---

## 二、目标设备参数

所有设备的 USB 参数集中定义在一个文件中：

**[android/app/src/main/java/com/auditoryworks/axdp/beans/DeviceDesc.java](android/app/src/main/java/com/auditoryworks/axdp/beans/DeviceDesc.java)**

```java
// A20 设备
public static final int A20_vendorId    = 0x1fc9;
public static final int A20_productId   = 0x826b;
public static final int A20_interfaceNum = 3;

// S55 声吧
public static final int S55_vendorId    = 0x0627;
public static final int S55_productId   = 0xA6BB;
public static final int S55_interfaceNum = 2;

// Tail 设备（当前激活）
public static final int Tail_vendorId    = 0x1FC9;
public static final int Tail_productId   = 0x826b;
public static final int Tail_interfaceNum = 0;
```

### 参数说明

| 参数 | 含义 | 查看方式 |
|------|------|----------|
| `vendorId` | USB 厂商 ID（VID） | `adb shell lsusb` 或设备管理器 |
| `productId` | USB 产品 ID（PID） | 同上 |
| `interfaceNum` | HID 接口编号 | `adb shell cat /sys/kernel/debug/usb/devices` |

### 切换目标设备

`DeviceDesc.java` 中保留了历史注释版本，切换设备时修改对应常量即可：

```java
// 例：切换 Tail 设备到另一型号，取消注释并修改
// public static final int Tail_vendorId    = 0x1FC9;
// public static final int Tail_productId   = 0x8270;
// public static final int Tail_interfaceNum = 0;
```

---

## 三、设备选择逻辑的代码位置

### 3.1 扫描并连接设备

三个 Service 各自维护一个 `scanUsbDevices()` 方法，逻辑相同：

**AxdpService** — [AxdpService.java:109](android/app/src/main/java/com/auditoryworks/axdp/AxdpService.java#L109)
```java
synchronized private void scanUsbDevices() {
    // ★ 在此切换目标设备类型
    // DeviceDesc desc = DeviceDesc.getS55Device();
    // DeviceDesc desc = DeviceDesc.getA20Device();
    DeviceDesc desc = DeviceDesc.getTailDevice();   // 当前激活

    UsbDevice usbDevice = desc.getFirstMatchedDevice(usbHelper.getUsbDevices());
    if (usbDevice != null) {
        openUsbDevice(usbDevice, desc.getInterfaceNum());
    }
}
```

**TailService** — [TailService.java:131](android/app/src/main/java/com/auditoryworks/axdp/TailService.java#L131)

**TailBoxService** — [TailBoxService.java:135](android/app/src/main/java/com/auditoryworks/axdp/TailBoxService.java#L135)

> 切换调试设备时，修改 `scanUsbDevices()` 中的 `desc` 赋值行，或直接修改 `DeviceDesc.java` 中的常量。

### 3.2 USB 热插拔监听

设备插入/拔出时触发 `BroadcastReceiver`，判断逻辑：

**AxdpService** — [AxdpService.java:72](android/app/src/main/java/com/auditoryworks/axdp/AxdpService.java#L72)
```java
if (DeviceDesc.isS55Device(usbDevice)) { scanUsbDevices(); }
if (DeviceDesc.isTailDevice(usbDevice)) { ... }
```

### 3.3 打开设备并传递 fd 给 native 层

**AxdpService** — [AxdpService.java:135](android/app/src/main/java/com/auditoryworks/axdp/AxdpService.java#L135)

```java
private boolean openUsbDevice(UsbDevice usbDevice, int interfaceNum) {
    UsbDeviceConnection conn = usbHelper.getUsbManager().openDevice(usbDevice);
    // claimInterface(true) 必须在传 fd 给 native 前调用，否则 libusb_claim_interface 返回 -6
    conn.claimInterface(usbDevice.getInterface(interfaceNum), true);
    int fileDescriptor = conn.getFileDescriptor();
    deviceHandle = nativeCreateDevice(fileDescriptor, vendorId, productId, interfaceNum);
}
```

### 3.4 Native 层入口

**[android/app/src/main/cpp/native_axdp_api.cpp:251](android/app/src/main/cpp/native_axdp_api.cpp#L251)**

```cpp
Java_com_auditoryworks_axdp_AxdpService_nativeCreateDevice(
    JNIEnv *env, jobject thiz, jint file_desc, jint vid, jint pid, jint interface_num)
{
    DeviceAccessor *handle = DeviceAccessor::Create(vid, pid, file_desc, interface_num);
    ...
}
```

内部调用 `hid_libusb_wrap_sys_device(file_desc, interface_num)` 打开 HID 设备。

---

## 四、调试环境配置

### 4.1 快速配置步骤

1. 用 USB 或 TCP/IP（`adb connect <ip>:5555`）连接设备
2. 确认设备为 `userdebug` 版本：`adb shell getprop ro.build.type`
3. Android Studio → Run → Edit Configurations → Debugger：
   - **Debug type**：`Dual`（同时调试 Java 和 Native）
   - **Symbol Directories**：添加 `android/app/build/intermediates/cmake/debug/obj`
4. Build → Run

### 4.2 注意事项

- 本项目使用 `android:sharedUserId="android.uid.system"`，以 system UID 运行。**LLDB native debugger 无法 attach 到 system UID 进程**，若需调试 native 代码，需临时注释该属性并重新安装（详见 [android/DEBUG_GUIDE.md](android/DEBUG_GUIDE.md)）。
- 切换 `sharedUserId` 状态后必须先卸载旧包再安装，否则报 `INSTALL_FAILED_SHARED_USER_INCOMPATIBLE`。
- 签名文件：`android/xbh.jks`，密码 `android`，alias `android`。

### 4.3 查看运行日志

```bash
# 查看所有 AXDP 相关日志
adb logcat -s AxdpService:V TailService:V axdp_native:V

# 查看 USB 设备连接情况
adb shell dumpsys usb
```

---

## 五、常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| native debugger 报 Unsupported device | `sharedUserId=android.uid.system` 限制 | 临时注释该属性，见 DEBUG_GUIDE.md §1 |
| `nativeCreateDevice` 返回 0 | `libusb_claim_interface` 失败（-6 BUSY） | 确认 Java 层 `claimInterface(true)` 在传 fd 前已调用 |
| `INSTALL_FAILED_SHARED_USER_INCOMPATIBLE` | 新旧 APK 的 `sharedUserId` 不一致 | 卸载后重装，见 DEBUG_GUIDE.md §3 |
| 设备扫描不到 | VID/PID 不匹配 | 用 `adb shell lsusb` 确认实际 VID/PID，对照 `DeviceDesc.java` |
