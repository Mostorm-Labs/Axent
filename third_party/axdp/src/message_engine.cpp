#include <memory>

#include "message_engine.h"
#include "io_device.h"
#include "message_receiver.h"
#include "message_sender.h"
#include "observer.h"

namespace axdp {
    MessageEngine::MessageEngine() :
            device_(nullptr), sender_(nullptr), recver_(nullptr) {
    }

    MessageEngine::MessageEngine(IODevice *device, MessageSender *sender, MessageReceiver *receiver)
            : device_(device), sender_(sender), recver_(receiver) {
        if (recver_ != nullptr) {
            recver_->setCallback(std::bind(
                    &MessageEngine::onRxData,
                    this, std::placeholders::_1, std::placeholders::_2));
        }
    }

    MessageEngine::~MessageEngine() {
        if (sender_ != nullptr)
            sender_.reset();
        if (recver_ != nullptr)
            recver_.reset();
        if (device_ != nullptr)
            device_.reset();
    }

    int MessageEngine::start() {
        if (device_ != nullptr && !device_->isOpen()) {
            if (device_->open() == IODevice::DC_OK) {
                recver_->start();
                return IODevice::DC_OK;
            }
        }
        return device_ == nullptr ? 
            IODevice::DC_ERROR_INVALID_HANDLE : IODevice::DC_ERROR_DEVICE_NOT_OPEN;
    }

    int MessageEngine::stop() {
        if (device_ != nullptr && device_->isOpen()) {
            recver_->stop();
            device_->close();
            //return device_->close();
        }
        return IODevice::DC_OK;
    }

}