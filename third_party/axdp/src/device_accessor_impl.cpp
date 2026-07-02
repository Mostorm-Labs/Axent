#include "device_accessor_impl.h"
#include "message_sender.h"
#include "log_utils.h"
#include "human_interface_handler.h"

namespace axdp {
    // 修改构造函数实现
    DeviceAccessorImpl::DeviceAccessorImpl()
        : MessageEngine(nullptr, nullptr, nullptr) {  // 显式调用基类构造函数
        send_thread_ = std::thread([this] { sendThreadFunc(); });  // 在构造函数体内初始化线程
    }

    DeviceAccessorImpl::DeviceAccessorImpl(
            IODevice *device,
            MessageSender *sender,
            MessageReceiver *receiver) :
            MessageEngine(device, sender, receiver),
            hid_report_event_handler_(new HumanInterfaceEventHandler()) {
        send_thread_ = std::thread([this] { sendThreadFunc(); });  // 在构造函数体内初始化线程
    }

    // 修改析构函数
    DeviceAccessorImpl::~DeviceAccessorImpl() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            running_ = false;
        }
        cond_var_.notify_all();
        if (send_thread_.joinable()) {
            send_thread_.join();
        }
        MessageEngine::stop();
    }

    void DeviceAccessorImpl::registerCbDelegate(EventCallbackDelegate *cb) {
        cb_ = (cb);
        cbex_ = dynamic_cast<EventCallbackDelegateEx*>(cb);
        if (hid_report_event_handler_.get() != nullptr) {
            hid_report_event_handler_->setEventCallback(cb);
        }
    }

    void DeviceAccessorImpl::unregisterCbDelegate(EventCallbackDelegate *cb) {
        if (cb_ == cb) {
            cb_ = NULL;
        }
        cb_ = NULL;
        cbex_ = NULL;
    }

    constexpr char kPrivateHidReportId = 5;
    constexpr char kPublicHidReportId = 1;
    //constexpr char kPublicHidReportLen = 3;
    constexpr char kPublicHidReportId2 = 2;
    //constexpr char kPublicHidReportLen2 = 2;
    constexpr char kUnusedHidReportId = 0;

    int DeviceAccessorImpl::onRxData(const void *ptr, uint16_t len) {
        const char *pchar = (const char *) ptr;
        char rx_report_id = pchar[0];
        //LOGD("CommonReader read bytes = %d, report_id = %d", len, rx_report_id);
        switch (rx_report_id) {
            case kUnusedHidReportId:
            case kPrivateHidReportId: {
                //LOGV("AuxMessageProtocol onRxData data len:[%d]%s\n", len, (char*)ptr);
                MessageParser::ParseState state = parser_.parse((const uint8_t *) ptr + 1, len - 1);
                if (state == MessageParser::ParseState::Done) {
                    std::shared_ptr<ProtocolMessage> msg = parser_.message();
                    handleReceivedInternalProtocolMessage(*msg);
                } else if (state < MessageParser::ParseState::Done) {
                    //todo: history why is parse failed;
                } else {
                    //Need more data for feeding to parse, read one more time;
                }
                break;
            }
            case kPublicHidReportId:
            case kPublicHidReportId2: {
                int ret = 0;
                ret = handleReceivedHumanInterfaceInputReport((const uint8_t *) ptr, len);
                if (ret) {

                } else {

                }
                break;
            }
            default:
                break;
        }
        return 0;
    }

    int DeviceAccessorImpl::onTxData(const uint8_t *buf, uint16_t len) {
        if (sender_ != nullptr) {
            return sender_->send(buf, len);
        }
        return 0;
    }

    int DeviceAccessorImpl::send(std::shared_ptr<ProtocolMessage> msg) {
        if (use_queue_mode_) {  // 新增队列模式判断
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                send_queue_.push(msg);
            }
            cond_var_.notify_one();
            return 0;
        }
        // 保持原有直接发送逻辑
        if (sender_ != nullptr) {
            int ret = sender_->send(msg->data(), msg->length());
            return ret > 0 ? 0 : -1;
        }
        return -1;
    }

    // 新增队列模式设置方法
    void DeviceAccessorImpl::setQueueMode(bool enable) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        use_queue_mode_ = enable;
        if (!enable) {
            // 清空队列当关闭模式时
            while (!send_queue_.empty()) {
                send_queue_.pop();
            }
        }
    }

    // 修改线程函数增加模式检查
    void DeviceAccessorImpl::sendThreadFunc() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this] {
                return !send_queue_.empty() || !running_;
            });

            if (!running_) break;
            if (!use_queue_mode_) continue;  // 新增模式检查

            auto msg = send_queue_.front();
            send_queue_.pop();
            lock.unlock();

            if (sender_) {
                sender_->send(msg->data(), msg->length());
            }
        }
    }


    AlphaDeviceAccessor::AlphaDeviceAccessor() {
    }

    AlphaDeviceAccessor::AlphaDeviceAccessor(
            IODevice *device,
            MessageSender *sender,
            MessageReceiver *receiver) :
            DeviceAccessorImpl(device, sender, receiver) {
    }

    AlphaDeviceAccessor::~AlphaDeviceAccessor() {
    }

    BetaDeviceAccessor::BetaDeviceAccessor() {
    }

    BetaDeviceAccessor::BetaDeviceAccessor(
            IODevice *device,
            MessageSender *sender,
            MessageReceiver *receiver) :
            DeviceAccessorImpl(device, sender, receiver) {

    }

    BetaDeviceAccessor::~BetaDeviceAccessor() {
    }

    void DeviceAccessorImpl::setUpgradeStrategy(UpdateStrategy strategy) {
        dfu_helper_.setUpdateStrategy(strategy);
    }

    void DeviceAccessorImpl::setUpdatePacketInterval(uint16_t interval){
        dfu_helper_.setUpdatePacketInterval(interval);
    }


    void DeviceAccessorImpl::setUpgradeDeviceDst(uint16_t dst) {
        dfu_helper_.setUpdateDeviceDst(dst);
        //int dev_count = 0;
        //for (size_t i = 1; i < 16; i++) {
        //    if (dst & (1 << i)) {
        //        dev_count++;
        //    }
        //}
        //dfu_helper_.setDevCount(dev_count);
    }

    void DeviceAccessorImpl::setUpgradeUltraDstEnabled(EnableState st)
    {
        dfu_helper_.setUpdateUltraDstEnabled(st);
    }

    void DeviceAccessorImpl::setUpgradeDeviceUltraDst(const uint8_t* sub_index, int32_t size)
    {
        dfu_helper_.setUpdateDeviceUltraDst(sub_index, size);
    }

    int DeviceAccessorImpl::startFirmwareUpgrade(const char *file_name) {
        int ret = 0;
        ret = bin_parser_.loadLocalFile(file_name);
        if (ret <= 0) {
            return ErrorCode::FileAbnormal;
        }
        ret = dfu_helper_.config(bin_parser_);
        if (ret != 0) {
            return ErrorCode::FileAbnormal;
        }
        if (cb_) cb_->onDfuStateUpdate(UpgradeState::DataReady);
        switch (dfu_helper_.updateStrategy()) {
            case UpdateStrategy::Falcon:
            case UpdateStrategy::Dolphin:
            case UpdateStrategy::Gopher:
                return startUpgrade();
            case UpdateStrategy::Viper:
            case UpdateStrategy::Cobra:
                return startUpgradeEx();
            case UpdateStrategy::Gecko:
            case UpdateStrategy::Discus:
            case UpdateStrategy::Camel:
                return startUpgrade();
            case UpdateStrategy::Jagger:{
                uint8_t cmd_endpoint[3] = {0xff, 0, 0};
                sender_->send(cmd_endpoint, 3);
                return startUpgrade();
            }
            default:
                break;
        }
        return -1;
    }

    int DeviceAccessorImpl::startFirmwareUpgrade(const char *buffer, size_t size) {
        int ret = 0;
        ret = bin_parser_.loadMemBuffer(buffer, size);
        if (ret <= 0) {
            return ErrorCode::FileAbnormal;
        }
        ret = dfu_helper_.config(bin_parser_);
        if (ret != 0) {
            return ErrorCode::FileAbnormal;
        }
        switch (dfu_helper_.updateStrategy()) {
            case UpdateStrategy::Falcon:
            case UpdateStrategy::Dolphin:
            case UpdateStrategy::Gopher:
                return startUpgrade();
            case UpdateStrategy::Viper:
            case UpdateStrategy::Cobra:
                return startUpgradeEx();
            case UpdateStrategy::Gecko:
            case UpdateStrategy::Discus:
            case UpdateStrategy::Camel:
                return startUpgrade();
            default:
                break;
        }
        return -1;
    }

}
