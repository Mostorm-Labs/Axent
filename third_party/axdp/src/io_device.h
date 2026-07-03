#ifndef AWX_IO_DEVICE_H_
#define AWX_IO_DEVICE_H_

#include "axdp_defines.h"

namespace axdp {
    //消息通道传输信道基类
    //
    class IODevice {
    public:
        enum DeviceCode : int32_t{
            DC_OK = 0,
            DC_ERROR_INVALID_HANDLE = -1,
            DC_ERROR_DEVICE_NOT_FOUND = -2,
            DC_ERROR_ACCESS_DENIED = -3,
            DC_ERROR_INVALID_REPORT_LENGTH = -4,
            DC_ERROR_DEVICE_NOT_OPEN = -5,
        };

        IODevice() = default;

        virtual ~IODevice() = default;

        virtual int32_t open() = 0;

        virtual int32_t close() = 0;

        virtual int32_t read(uint8_t *data, uint32_t max_size, int32_t timeout_ms) = 0;

        virtual int32_t write(const uint8_t *data, uint32_t len) = 0;

        virtual bool isOpen() const = 0;

        virtual int32_t getOutputReportLen() { return 0;}
    };
}

#endif // !AWX_IO_DEVICE_H_
