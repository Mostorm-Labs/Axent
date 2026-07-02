#include <tuple>
#include <map>
#include <set>
#include "axdp_api_ex.h"
#include "device_accessor_impl.h"
#include "message_sender.h"
#include "message_receiver.h"
#include "hid_device.h"
#include "protocol_defines.h"

namespace axdp {

    typedef std::tuple<std::string, int, int, int, ProtocolType> DeviceAttribute;
    static std::set<DeviceAttribute> DeviceTypeMatchTbl =
            {
                    {"v20",    1317, 42156, 0, ProtocolType::Alpha},
                    {"c30",    1574, 42699, 0, ProtocolType::Alpha},
                    {"c30new", 1575, 42670, 0, ProtocolType::Alpha},
            };

    static ProtocolType FindDeviceProtocol(int vid, int pid) {
        ProtocolType result_type = ProtocolType::Beta;
        for (const auto &it: DeviceTypeMatchTbl) {
            int vendor_id = std::get<1>(it);
            int product_id = std::get<2>(it);
            if (vendor_id == vid && product_id == pid) {
                result_type = ProtocolType::Alpha;
            } else {
                continue;
            }
        }
        return result_type;
    }

    static std::string
    FindDevicePath(int vid, int pid, int usage_page, int usage, const wchar_t *serial_number) {
        struct hid_device_info *devs, *cur_dev;
        std::string path;
        devs = hid_enumerate(vid, pid);
        cur_dev = devs;
        while (cur_dev) {
            if (vid == cur_dev->vendor_id && pid == cur_dev->product_id &&
                usage == cur_dev->usage && usage_page == cur_dev->usage_page) {
                if (serial_number != nullptr) {
                    if (cur_dev->serial_number != nullptr) {
                        int res = wcscmp(serial_number, cur_dev->serial_number);
                        if (res == 0) {
                            path = cur_dev->path;
                            break;
                        }
                    }
                } else {
                    path = cur_dev->path;
                    break;
                }
            }
            cur_dev = cur_dev->next;
        }
        hid_free_enumeration(devs);

        return path;
    }

    /*Todo: Create specific handler for every report_id to build Device handle*/
#ifdef __ANDROID__

    DeviceAccessor *DeviceAccessor::Create(
            int vid,
            int pid,
            long file_desc,
            int interface_num) {
        ProtocolType pt = FindDeviceProtocol(vid, pid);
        int report_id = pt == ProtocolType::Alpha ? 0 : 5;
        IODevice *device = new AwxHidDevice(file_desc, interface_num, report_id);
        MessageSender *sender = new CommonSender(device);
        MessageReceiver *recver = new CommonReceiver(device);
        DeviceAccessor *da = nullptr;
        switch (pt) {
            case axdp::ProtocolType::Alpha:
                da = new AlphaDeviceAccessor(device, sender, recver);
                break;
            case axdp::ProtocolType::Beta:
                da = new BetaDeviceAccessor(device, sender, recver);
                break;
            case axdp::ProtocolType::Common:
            default:
                if (recver != nullptr) delete recver;
                if (sender != nullptr) delete sender;
                if (device != nullptr) delete device;
                break;
        }
        auto dc = dynamic_cast<DeviceAccessorImpl *>(da)->start();
        if (dc != IODevice::DC_OK) {
            delete da;
            da = nullptr;
        }
        return da;
    }

#endif    // __ANDROID__

    DeviceAccessor *DeviceAccessor::Create(int vid, int pid, const wchar_t *serial_number) {
        std::string path = FindDevicePath(vid, pid, kDefaultUsagePage, kDefaultUsage,
                                          serial_number);
        if (path.empty()) {
            return nullptr;
        }
        ProtocolType pt = FindDeviceProtocol(vid, pid);
        int report_id = pt == ProtocolType::Alpha ? 0 : 5;
        IODevice *device = new AwxHidDevice(path.c_str(), report_id);
        MessageSender *sender = new CommonSender(device);
        MessageReceiver *recver = new CommonReceiver(device);
        DeviceAccessor *da = nullptr;
        switch (pt) {
            case axdp::ProtocolType::Alpha:
                da = new AlphaDeviceAccessor(device, sender, recver);
                break;
            case axdp::ProtocolType::Beta:
                da = new BetaDeviceAccessor(device, sender, recver);
                break;
            case axdp::ProtocolType::Common:
            default:
                if (recver != nullptr) delete recver;
                if (sender != nullptr) delete sender;
                if (device != nullptr) delete device;
                break;
        }
        auto dc = dynamic_cast<DeviceAccessorImpl *>(da)->start();
        if (dc != IODevice::DC_OK) {
            delete da;
            da = nullptr;
        }
        return da;
    }
}
