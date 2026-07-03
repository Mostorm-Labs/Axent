#ifndef AXDP_TRANSFER_CHANNEL_H_
#define AXDP_TRANSFER_CHANNEL_H_

#include <memory>

namespace axdp {
    //using ParamList = std::initializer_list<uint16_t>;
    /**
     * 消息引擎类，一个协议引擎对应多种设备
     * 该类管理消息的传输（transfer message）和消息的处理（process message）
     * 消息传输有三个必要属性：
     * 1. 协议类型，需要协议自身匹配相应的编码、解码函数，T为协议类型
     * 2. 协议传输介质，可以是不同类型的信道，在这里是IO设备，可以是
     *    USB设备、串口设备或者是网络传输端口等
     * 3. 传输工人，即Writer、Reader，这些工人拿到数据后，需要借助协议
     *    编解码器和然后把数据放到流水线上进行传输
     * 该类可以认为是一个统筹、协调消息传输的引擎类
     * */
    class IODevice;

    class MessageSender;

    class MessageReceiver;

    class MessageEngine {
    public:
        MessageEngine();

        MessageEngine(IODevice *device, MessageSender *sender, MessageReceiver *receiver);

        virtual ~MessageEngine();

        virtual int start();

        virtual int stop();

    protected:
        virtual int onRxData(const void *ptr, uint16_t len) = 0;

        virtual int onTxData(const uint8_t *data, uint16_t len) = 0;

        std::shared_ptr<IODevice> device_;
        std::shared_ptr<MessageSender> sender_;
        std::shared_ptr<MessageReceiver> recver_;
    };
}


#endif // AXDP_TRANSFER_CHANNEL_H_
