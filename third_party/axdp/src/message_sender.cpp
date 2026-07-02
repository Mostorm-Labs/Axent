#include "message_sender.h"
#include "io_device.h"
#include "hid_device.h"
namespace axdp {
    MessageSender::MessageSender(IODevice *device) : device_(device) {

    }

    MessageSender::~MessageSender() {

    }

    int MessageSender::send(const uint8_t* msg_buf, uint32_t length)
    {
        //uint8_t data[1025] = {0};
        //memcpy(data, msg_buf, length);
        if (device_->isOpen()) {
            int res = device_->write(msg_buf, length);
            if (res < 0) {
                device_->close();
            }
            return res;
        }
        return 0;
    }

    int MessageSender::sendRaw(const uint8_t* msg_buf, uint32_t length)
    {
        if (device_->isOpen()) {
            AwxHidDevice* hdev = dynamic_cast<AwxHidDevice*>(device_);
            if (hdev)
            {
                return hdev->writeRaw(msg_buf, length);
            }
            //return device_->write(msg_buf, length);
        }
        return 0;
    }


    ///==================================================================================
    ///=============================HID CommonWriter=====================================
    ///==================================================================================
    CommonSender::CommonSender(IODevice *device) : MessageSender(device) {

    }

    CommonSender::~CommonSender() {

    }

    HidSender::HidSender(IODevice* device):MessageSender(device)
    {
    }

    HidSender::~HidSender()
    {
    }

}

