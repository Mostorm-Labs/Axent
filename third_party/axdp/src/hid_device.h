#ifndef AXDP_HID_DEVICE_H_
#define AXDP_HID_DEVICE_H_

#include <string>
#include <mutex>
#include <shared_mutex>
#include "io_device.h"

#include "third_party/hidapi/hidapi/hidapi.h"

namespace axdp {
    constexpr uint32_t kMaxHidOutputReportLen = 1024;
    constexpr uint32_t kMaxHidInputReportLen = 1024;
    constexpr uint16_t kDefaultHidUsage = 130;
    constexpr uint16_t kDefaultHidUsagePage = 830;
    constexpr uint16_t kDefaultHidReportId = 5;

    //class for common hid device
    class HidDevice : public IODevice {
    public:
        explicit HidDevice(const char *path);

#ifdef __ANDROID__
        HidDevice(long file_desc, int interface_num);
#endif

        virtual ~HidDevice();

        int open() override;

        int close() override;

        int32_t read(uint8_t *data, uint32_t max_size, int32_t timeout_ms) override;

        int32_t write(const uint8_t *data, uint32_t len) override;

        bool isOpen() const override;

        int32_t getOutputReportLen() override { return output_report_len_; }

    protected:
        long file_desc_;
        int interface_num_;
        std::string path_;
        hid_device *handle_;
        uint8_t *tx_buffer_;

        std::shared_mutex rwlock_;

        int output_report_len_; //HOST TO DEVICE, DOWN STREAM; nomatter what it is, but bigger than 256
        int input_report_len_;// DEVICE TO HOST, UP STREAM; it's not matter
    };

    class AwxHidDevice : public HidDevice {
    public:
        AwxHidDevice(const char *path, uint8_t report_id);

#ifdef __ANDROID__
        AwxHidDevice(long file_desc, int interface_num, uint8_t report_id);
#endif

        ~AwxHidDevice() override;

        int32_t open() override;

        int32_t read(uint8_t *data, uint32_t max_size, int32_t timeout_ms) override;

        int32_t write(const uint8_t *data, uint32_t len) override;

        int32_t writeRaw(const uint8_t* data, uint32_t len);
    protected:
        uint8_t report_id_;
        std::mutex mutex_;
    };
}

#endif // !AXDP_HID_DEVICE_H_
