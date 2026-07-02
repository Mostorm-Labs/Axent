#ifndef AXDP_MESSAGE_RECEIVER_H_
#define AXDP_MESSAGE_RECEIVER_H_

#include <memory>
#include "async_queue.h"

//多个uint16_t是为向下兼容, 以支持基本数据类型, 如byte*, char*等等
#define RX_CALLBACK_FUNC      std::function<int(const void*, uint16_t)>

namespace axdp {
    class IODevice;

    /**
     * 读 USB,串口 数据类
     * */
    class MessageReceiver {
    public:
        explicit MessageReceiver(IODevice* device);
        virtual ~MessageReceiver();
        void setCallback(RX_CALLBACK_FUNC cbk) { callback_ = std::move(cbk); }
        virtual int start();
        virtual void stop();
    protected:
        /**
         * read() 由读线程触发，子类必需实现相关读业务逻辑
         * */
        virtual void recv(int timeout_ms) = 0;
        IODevice* device_;
        RX_CALLBACK_FUNC      callback_;

    private:
        std::thread* read_thread_;
#ifdef WIN32
        int timeout_ms_ { 1000 }; //-1 for block, 0 for unblock, others for certain time
#else
        int timeout_ms_ { 1000 };
#endif
    };


    class CommonReceiver : public MessageReceiver, TaskQueue {
    public:
        explicit CommonReceiver(IODevice* device);
        ~CommonReceiver() override;

        void stop() override;
    protected:
        void recv(int timeout_ms) override;

    private:
        void onData(std::shared_ptr<RawBytes> data) override;
        std::atomic<bool>              stop_read_;
        //   std::mutex                     mutex_;
        //   std::condition_variable        cond_var_;
    };
}
#endif // !AXDP_MESSAGE_RECEIVER_H_
