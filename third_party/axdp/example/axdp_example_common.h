#ifndef AXDP_MAIN_TEST_H_
#define AXDP_MAIN_TEST_H_

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdio.h>
#include <time.h>
#include <atomic>
#include <mutex>
#include <fstream>
#include <condition_variable>
#include <map>

#ifdef  WIN32
#include <Windows.h>
#endif

#include "axdp_api.h"
#include "axdp_defines.h"

typedef struct DeviceAttr {
    int vid;
    int pid;
    int16_t dst;
    wchar_t* serial_number;
    const char* dfu_file;
    bool enable_ultra;
    uint8_t ultra_dst[33];
}DeviceAttr;

extern const char* fw_1192;
extern const char* fw_11132;
extern const char* fw_v15afl;
extern const char* fw_v20;
extern const char* fw_a20s;

extern DeviceAttr gA20DeviceAttr;
extern DeviceAttr gV20DeviceAttr;
extern DeviceAttr gV15AFLDeviceAttr;
extern DeviceAttr gA20SDeviceAttr;
extern DeviceAttr gV415DeviceAttr;
extern DeviceAttr gV520DDeviceAttr;
extern DeviceAttr gAMX100DeviceAttr;
extern DeviceAttr gC30RDeviceAttr;
extern DeviceAttr gAWM10TRDeviceAttr;

void Signal();

void Wait(int timeout_s);

class CommonEventCbEx;

void axdp_dfu_process(const DeviceAttr& attr, axdp::UpdateStrategy strategy);

void axdp_info_process(const DeviceAttr& attr);

void axdp_config_process(const DeviceAttr& attr);

#endif // !AXDP_MAIN_TEST_H_
