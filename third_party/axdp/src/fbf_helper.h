#ifndef FBFHELPER_H
#define FBFHELPER_H

#include <cstdio>
#include <cstdint>

class FbfHelper
{
public:
    FbfHelper();

    void setFbfDeviceDst(uint16_t dst);

    uint16_t getFbfDeviceDst();

    int loadLocalFile(const char *file_name);

    int64_t getCurrentBuffer(char *buffer, int64_t size);

    int64_t getFileSize();

    int getFileMd5(char *buffer);

    void resetReadyFlag() { dst_flag_ = 0;}

    bool isAllDevicesReady(uint16_t src);

    void fbfEnd();

private:
    FILE* ifs_ = nullptr;
    //所有级联设备地址位
    uint16_t dst_ = 0;
    //级联设备传输标志位
    uint16_t dst_flag_ = 0;
};

#endif // FBFHELPER_H
