#ifndef AXDP_MESSAGE_SENDER_
#define AXDP_MESSAGE_SENDER_

#include <memory>

namespace axdp {
    class IODevice;

    /**
     * 写 USB,串口 数据类
     * */
    class MessageSender {
    public:
        explicit MessageSender(IODevice *device);

        virtual ~MessageSender();

        virtual int send(const uint8_t *msg_buf, uint32_t length);

        virtual int sendRaw(const uint8_t* msg_buf, uint32_t length);

    protected:
        IODevice *device_;
    };

    class CommonSender : public MessageSender {
    public:
        explicit CommonSender(IODevice *device);

        ~CommonSender() override;

        //如果要异步写入则继承AuxQueue实现类似于reader
        //int send(const uint8_t *msg_buf, uint32_t length) override;

    protected:

    };

    class HidSender : public MessageSender {
    public:
        explicit HidSender(IODevice* device);

        ~HidSender() override;

        //如果要异步写入则继承AuxQueue实现类似于reader
        //int send(const uint8_t* msg_buf, uint32_t length) override;

    protected:

    };

}

#endif // !AXDP_MESSAGE_SENDER_
