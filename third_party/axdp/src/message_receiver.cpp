#include "message_receiver.h"
#include "log_utils.h"
#include "io_device.h"
#include "axdp_defines.h"

#ifdef _WIN32
#include <windows.h>
#define AWX_SLEEP(ms)  Sleep(ms)
#else
#define AWX_SLEEP(ms)  std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace axdp {

    MessageReceiver::MessageReceiver(IODevice *device) : device_(device), read_thread_(nullptr) {

    }

    MessageReceiver::~MessageReceiver() {

    }

    int MessageReceiver::start() {
        read_thread_ = new std::thread(&MessageReceiver::recv, this, timeout_ms_);
        return read_thread_ != nullptr;
    }

    void MessageReceiver::stop() {
        //todo : this can't be stoped
        LOGV("MessageReader is Stopping...\n");
        if (read_thread_ != nullptr) {
            if (read_thread_->joinable()) {
                read_thread_->join();
            }
            delete read_thread_;
            read_thread_ = nullptr;
        }
        LOGV("MessageReader is Stopped...\n");
    }


    ///==================================================================================
    ///=================================CommonReader=====================================
    ///==================================================================================
    CommonReceiver::CommonReceiver(IODevice *device) :
            MessageReceiver(device), stop_read_(false) {

    }

    CommonReceiver::~CommonReceiver() {

    }

    void CommonReceiver::onData(std::shared_ptr<RawBytes> data) {
        TaskQueue::onData(data);
        if (callback_ != nullptr) {
            callback_(data->buf(), data->len());
        }
    }

    void CommonReceiver::recv(int timeout_ms) {
        int32_t res = 0;
        int32_t read_timeout = 0;
        int32_t read_failed = 0;
        constexpr int32_t max_read_buf_size = 4096;
        LOGV("CommonReader start read thread loop\n");
//        std::shared_ptr<uint8_t> data(new uint8_t[max_read_buf_size],
//                                      std::default_delete<uint8_t[]>());
        uint8_t data[max_read_buf_size] = {0};
        //    remark:A)如果设备是中断阻塞，按理close会触发read直接返回。
        //           B)轮询式应会有个小delay循环read
        while (!stop_read_) {
            try {
                res = device_->read(data/*.get()*/, max_read_buf_size, timeout_ms);
                if (res > 0 && res <= max_read_buf_size) {
                    TaskQueue::push(data/*.get()*/, res);
                    //LOGD("CommonReader read bytes = %d", res);
                } else if (res == 0) {
                    //device read timeout, do sth here
                    read_timeout++;
                } 
                else if (res == -1) {
                    LOGV("CommonReader read thread ERROR\n");
                    if (++read_failed > 3) stop_read_ = true;
                } else {
                    LOGV("CommonReader read thread unknown ERROR\n");
                    if (++read_failed > 3) stop_read_ = true;
                }
            }
            catch (std::exception &e) {
                //Android won't come here
                //But windows will throw exception when device plug out
                LOGV("CommonReader read thread ERROR:%s\n", e.what());
                if (++read_failed > 3)stop_read_ = true;
            }
        }
        LOGV("Common Reader read thread loop EXITED\n");
    }

    void CommonReceiver::stop() {
        stop_read_ = true;
        MessageReceiver::stop();
    }

}
