#include <mutex>
#include <memory>
#include <iostream>
#include "hid_device.h"

#ifdef __ANDROID__
#include "third_party/hidapi/libusb/hidapi_libusb.h"
#include "log_utils.h"
#endif

#ifdef __APPLE__
#include "third_party/hidapi/mac/hidapi_darwin.h"
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USBSpec.h>
#include <CoreFoundation/CoreFoundation.h>
#include <wchar.h>
#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>
#endif
namespace axdp {
        //dev->max_input_report_len = (CFIndex) get_max_report_length(dev->device_handle);
#ifdef __APPLE__
        static int32_t get_int_property(IOHIDDeviceRef device, CFStringRef key){
            CFTypeRef ref;
            int32_t value;

            ref = IOHIDDeviceGetProperty(device, key);
            if (ref) {
                if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
                    CFNumberGetValue((CFNumberRef) ref, kCFNumberSInt32Type, &value);
                    return value;
                }
            }
            return 0;
        }

        static int32_t get_max_input_report_length(IOHIDDeviceRef device){
            return get_int_property(device, CFSTR(kIOHIDMaxInputReportSizeKey));
        }

        static int32_t get_max_output_report_length(IOHIDDeviceRef device){
            return get_int_property(device, CFSTR(kIOHIDMaxOutputReportSizeKey));
        }
#endif


    ///==================================================================================
    ///=================================HidDevice========================================
    ///==================================================================================
    HidDevice::HidDevice(const char *path) :
            file_desc_(0),
            interface_num_(0),
            handle_(nullptr),
            tx_buffer_(nullptr),
            output_report_len_(0),
            input_report_len_(0),
            path_(path) {
    }

#ifdef __ANDROID__

    HidDevice::HidDevice(long file_desc, int interface_num) :
            file_desc_(file_desc),
            interface_num_(interface_num),
            handle_(nullptr),
            tx_buffer_(nullptr),
            output_report_len_(0),
            input_report_len_(0),
            path_() {

    }

#endif

    HidDevice::~HidDevice() {
    }

    int HidDevice::open() {
#ifdef __APPLE__
        hid_darwin_set_open_exclusive(0);
#endif

#ifdef __ANDROID__
        handle_ = hid_libusb_wrap_sys_device(file_desc_, interface_num_);
#else
        handle_ = hid_open_path(path_.c_str());
#endif

#ifdef __APPLE__
        int ret = hid_darwin_is_device_open_exclusive(handle_);
#endif

        if (handle_ == nullptr) {
            return DeviceCode::DC_ERROR_DEVICE_NOT_FOUND;
        }
        return DeviceCode::DC_OK;
    }

    int HidDevice::close() {
        std::unique_lock<std::shared_mutex> readLock(rwlock_);
        if (handle_ != nullptr) {
            hid_close(handle_);
            handle_ = nullptr;
        }
        if (tx_buffer_ != nullptr) {
            delete[]tx_buffer_;
            tx_buffer_ = nullptr;
        }
        return DeviceCode::DC_OK;
    }

    int32_t HidDevice::read(uint8_t *data, uint32_t max_size, int32_t timeout_ms) {
        std::shared_lock<std::shared_mutex> readLock(rwlock_);
        if (handle_ != nullptr) {
            return hid_read_timeout(handle_, (unsigned char *) data, max_size, timeout_ms);
        }
        return DeviceCode::DC_ERROR_INVALID_HANDLE;
    }

    int32_t HidDevice::write(const uint8_t *data, uint32_t len) {
        std::shared_lock<std::shared_mutex> readLock(rwlock_);
        if (handle_ != nullptr) {
            int res = hid_write(handle_, (const unsigned char *) data, len);
            if (res < 0) {
                //fprintf(stderr, "Error writing to device: %ls\n", hid_error(handle_));
                return DeviceCode::DC_ERROR_INVALID_HANDLE;
            }
            return res;
        }
        return DeviceCode::DC_ERROR_INVALID_HANDLE;
    }

    bool HidDevice::isOpen() const {
        return handle_ != nullptr;
    }


    ///==================================================================================
    ///=================================AwxHidDevice========================================
    ///==================================================================================
    AwxHidDevice::AwxHidDevice(const char *path, uint8_t report_id) :
            HidDevice(path), report_id_(report_id) {
    }

#ifdef __ANDROID__

    AwxHidDevice::AwxHidDevice(long file_desc, int interface_num, uint8_t report_id) :
            HidDevice(file_desc, interface_num), report_id_(report_id) {
    }

#endif

    AwxHidDevice::~AwxHidDevice() {
    }

    int32_t AwxHidDevice::open() {
        int dc = HidDevice::open();
        if (dc != DeviceCode::DC_OK) {
            return dc;
        }
        //Get truely output_report_len_ value
        input_report_len_ = kMaxHidInputReportLen;
        output_report_len_ = kMaxHidOutputReportLen;

        //for max output buffer length detect
        uint8_t test_buffer[kMaxHidOutputReportLen] = {0};
        test_buffer[0] = report_id_;
#if WIN32
        //todo: check if it is suitable for mac and android
        output_report_len_ = HidDevice::write(test_buffer, 8);
#elif __APPLE__
        input_report_len_ = get_max_input_report_length((*(IOHIDDeviceRef*)handle_));
        output_report_len_ = get_max_output_report_length((*(IOHIDDeviceRef*)handle_));
#endif // WIN32
        if (output_report_len_ <= 0 || input_report_len_ <= 0) {
            return DeviceCode::DC_ERROR_INVALID_REPORT_LENGTH;
        }
        tx_buffer_ = new uint8_t[output_report_len_];
        if (tx_buffer_ == nullptr) {
            return DeviceCode::DC_ERROR_INVALID_HANDLE;
        }
        return DeviceCode::DC_OK;
    }

    int32_t AwxHidDevice::read(uint8_t *data, uint32_t max_size, int32_t timeout_ms) {
        int32_t read_len = HidDevice::read(data, max_size, timeout_ms);
        //if (read_len > 0 && report_id_ > 0 && data[0] == report_id_) {
        //    //todo: check if it is ok for mac and android
        //    memmove(data, data + 1, read_len - 1);
        //    read_len -= 1;
        //}
#if defined(WIN32) || defined(__APPLE__)
        if (read_len > 0 && report_id_ == 0)//on windows, we will add this 0 to the head for parse
        {
            memmove(data + 1, data, read_len);
            data[0] = 0;
            read_len = read_len + 1;
        }
#endif // __WIN32__

        return read_len;
    }

    int32_t AwxHidDevice::write(const uint8_t *data, uint32_t len) {
        if (handle_ != nullptr) {
            int writed = 0;
            int pos = 0;
            int ret = 0;

            while (pos < (int) len) {
                int n = (std::min)(output_report_len_ - 1, (int) len - pos);
                memset(tx_buffer_, 0, output_report_len_);
                memcpy(tx_buffer_ + 1, data + pos, n);
                tx_buffer_[0] = report_id_;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
#if defined(__APPLE__)
                    ret = HidDevice::write(tx_buffer_, output_report_len_);
#else
                    ret = HidDevice::write(tx_buffer_, n + 1);
#endif
                    //LOGD("HID WRITE WHEN outpout to %d, %d", n, ret)；
                }
                if (ret == 0) {
                    break;
                }
                if (ret < 0) {
                    return DeviceCode::DC_ERROR_INVALID_HANDLE;
                }
                writed += ret;
                pos += n;
            }
            return writed;
        }
        return DeviceCode::DC_ERROR_INVALID_HANDLE;
    }

    int32_t AwxHidDevice::writeRaw(const uint8_t *data, uint32_t len) {
        return HidDevice::write(data, len);
    }
}
