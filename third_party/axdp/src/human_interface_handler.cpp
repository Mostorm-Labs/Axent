#include "device_accessor_impl.h"
#include "protocol_defines.h"
#include "axdp_defines.h"
#include "axdp_utils.h"
#include "dfu_helper.h"
#include "log_utils.h"
#include "message_sender.h"
#include "human_interface_handler.h"

#define MAX_REPORT_LENGTH 1024
#define HID_READ_TIMEOUT 1000

#define REPORT_IN_ID 0x01
#define REPORT_IN_LEN 3

#define REPORT_IN_ID_2 0x02
#define REPORT_IN_LEN_2 2

#define REPORT_OUT_ID 0x03
#define REPORT_OUT_LEN 2

#define REPORT_OUT_LEDS_OFFHOOK_OFFSET 0
#define REPORT_OUT_LEDS_MUTE_OFFSET 2

#define REPORT_IN_TELE_HOOKSWITCH_OFFSET 0
#define REPORT_IN_TELE_LINEBUSY_OFFSET 1
#define REPORT_IN_TELE_MUTE_OFFSET 2
#define REPORT_IN_TELE_FLASH_OFFSET 3

namespace axdp {
    int HumanInterfaceEventHandler::onInputReport(const uint8_t* buffer, size_t length) {
        bool hookswitch_high;
        bool linebusy_high;
        bool mute_high;
        bool flash_high;

        if (buffer[0] == REPORT_IN_ID)
        {
            if (buffer[1] & (1 << REPORT_IN_TELE_HOOKSWITCH_OFFSET)) {
                hookswitch_high = true;
            }
            else {
                hookswitch_high = false;
            }
            if (buffer[1] & (1 << REPORT_IN_TELE_LINEBUSY_OFFSET)) {
                linebusy_high = true;
            }
            else {
                linebusy_high = false;
            }
            if (buffer[1] & (1 << REPORT_IN_TELE_MUTE_OFFSET)) {
                mute_high = true;
            }
            else {
                mute_high = false;
            }
            if (buffer[1] & (1 << REPORT_IN_TELE_FLASH_OFFSET)) {
                flash_high = true;
            }
            else {
                flash_high = false;
            }

            if (hookswitch_high == false && (linebusy_high == true || flash_high == true)) {
                if (cb_)
                {
                    cb_->onHumanInterfaceEvent(HIDEvent::HangUp);
                }
            }
            if (hookswitch_high == true && (linebusy_high == false || flash_high == true)) {
                if (cb_)
                {
                    cb_->onHumanInterfaceEvent(HIDEvent::Answer);
                }
            }
            if (mute_high == true) {
                mute_edge_state_ = MUTE_EDGE_HIGH;
            }
            else {
                if (mute_edge_state_ == MUTE_EDGE_HIGH) {
                    if (muted_ == true) {
                        if (cb_)
                        {
                            cb_->onHumanInterfaceEvent(HIDEvent::Unmute);
                        }
                        muted_ = false;
                    }
                    else {
                        if (cb_)
                        {
                            cb_->onHumanInterfaceEvent(HIDEvent::Mute);
                        }
                        muted_ = true;
                    }
                }
                mute_edge_state_ = MUTE_EDGE_LOW;
            }
        }
        else if (buffer[0] == REPORT_IN_ID_2)
        {
            if (buffer[1] == 0x02)
            {
                if (cb_)
                {
                    cb_->onHumanInterfaceEvent(HIDEvent::VolDown);
                }
            }
            else if (buffer[1] == 0x01) {
                if (cb_)
                {
                    cb_->onHumanInterfaceEvent(HIDEvent::VolUp);
                }
            }
            else
            {
                //unknown vloume report event
                return -2;
            }
        }
        else
        {
            //unknown report id data
            return -1;
        }
        return 0;
    }

    int HumanInterfaceEventHandler::setMuted(bool mute)
    {
        muted_ = mute;
        return 0;
    }

    int HumanInterfaceEventHandler::setOffHook(bool off)
    {
        off_hook_ = off;
        return 0;
    }

    int DeviceAccessorImpl::setMuted(bool mute) {
        hid_report_event_handler_->setMuted(mute);
        uint8_t buffer[8] = { 0 };
        buffer[0] = REPORT_OUT_ID;
        if (hid_report_event_handler_->offHook() == true) {
            buffer[1] = 1 << REPORT_OUT_LEDS_OFFHOOK_OFFSET;
        }
        else {
            buffer[1] = 0;
        }
        if (mute)
        {
            buffer[1] |= 1 << REPORT_OUT_LEDS_MUTE_OFFSET;
        }
        int ret = 0;
        if (sender_)
        {
            ret = sender_->sendRaw(buffer, REPORT_OUT_LEN);
        }
        return ret <= 0 ? -1 : 0;
    }

    int DeviceAccessorImpl::setOffHook(bool off) {
        hid_report_event_handler_->setOffHook(off);
        uint8_t buffer[8] = { 0 };
        buffer[0] = REPORT_OUT_ID;
        if (off)
        {
            buffer[1] = 1 << REPORT_OUT_LEDS_OFFHOOK_OFFSET;
        }
        else
        {
            buffer[1] = 0;
        }
        if (hid_report_event_handler_->muted() == true) {
            buffer[1] |= 1 << REPORT_OUT_LEDS_MUTE_OFFSET;
        }
        int ret = 0;
        if (sender_)
        {
            ret = sender_->sendRaw(buffer, REPORT_OUT_LEN);
        }
        return ret <= 0 ? -1 : 0;
    }
}