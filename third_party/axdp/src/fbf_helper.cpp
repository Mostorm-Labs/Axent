#include "fbf_helper.h"
#include <fstream>
#include "md5.h"

FbfHelper::FbfHelper()
{

}

int FbfHelper::loadLocalFile(const char *file_name)
{
    if(ifs_)fclose(ifs_);
    ifs_ = fopen(file_name, "rb");
    if (!ifs_) {
        return -1;
    }
    return 0;
}

int64_t FbfHelper::getCurrentBuffer(char *buffer, int64_t size)
{
    if (!ifs_) {
        return -1;
    }
    return fread(buffer, 1, size, ifs_);
}

int64_t FbfHelper::getFileSize()
{
    if (!ifs_) {
        return -1;
    }
    fseek(ifs_, 0, SEEK_END);//定位到文件的最后面
    int64_t len = ftell(ifs_);
    fseek(ifs_, 0, SEEK_SET);
    return len;
}

int FbfHelper::getFileMd5(char *buffer)
{
    if (!ifs_) {
        return -1;
    }
    char data[4096];
    char digest[16];
    int64_t len = 0;
    MD5_CTX context;

    MD5Init(&context);
    fseek(ifs_, 0, SEEK_SET);
    do {
        len = fread(data, 1, sizeof(data), ifs_);
        MD5Update(&context, (unsigned char*)data, len);
    } while (len);

    MD5Final((unsigned char*)digest, &context);

    md52str((unsigned char*)digest, (unsigned char*)buffer);
    return 0;
}

bool FbfHelper::isAllDevicesReady(uint16_t src)
{
    dst_flag_ |= src;
    return dst_flag_ == dst_;
}

void FbfHelper::fbfEnd()
{
    dst_ = 0;
    dst_flag_ = 0;
    if (ifs_) fclose(ifs_);
    ifs_ = nullptr;
}

void FbfHelper::setFbfDeviceDst(uint16_t dst)
{
    dst_ = dst;
}

uint16_t FbfHelper::getFbfDeviceDst()
{
    return dst_;
}


