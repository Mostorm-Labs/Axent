#ifndef AXDP_API_H_
#define AXDP_API_H_

#include "axdp_defines.h"

namespace axdp {
    class AXDP_API EventCallbackDelegate {
    public:
        EventCallbackDelegate() = default;

        virtual ~EventCallbackDelegate() = default;

        virtual void onHumanInterfaceEvent(HIDEvent event) {};

        virtual void onResetDevice(ResultState state) {};

        virtual void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) {};

        virtual void onDfuStateUpdate(UpgradeState state) {};

        virtual void onDfuProgressUpdate(uint16_t progress) {};

        virtual void onGetNoiseSuppressionLevel(DegreeLevel level) {};

        virtual void onGetReverbrationSuppressionLevel(DegreeLevel level) {};

        virtual void onGetEchoCancellationLevel(DegreeLevel level) {};

        virtual void onAudioRecordStart(bool supported, const AudioFormat *format) {};

        virtual void onAudioRecordData(const uint8_t *data, int32_t len) {};

        virtual void onAudioRecordStopped(int32_t result) {};

        // callback func
        virtual void onGetWifi(const uint8_t* ssid, uint16_t len){}

        virtual void onSetWifi(){}
    };

    class AXDP_API DeviceAccessor {

    public:

#ifdef __ANDROID__

        static DeviceAccessor *Create(int vid, int pid, long file_desc, int interface_num);

#endif // __ANDROID__

        static DeviceAccessor *Create(int vid, int pid, wchar_t *serial_number = nullptr);

    protected:
        DeviceAccessor() = default;

    public:
        DeviceAccessor(const DeviceAccessor &) = delete;

        DeviceAccessor &operator=(const DeviceAccessor &) = delete;

        virtual ~DeviceAccessor() = default;

        virtual void registerCbDelegate(EventCallbackDelegate *cb) {};

        virtual void unregisterCbDelegate(EventCallbackDelegate *cb) {};

        virtual void setUpgradeStrategy(UpdateStrategy strategy) {};

        virtual void setUpgradeDeviceDst(uint16_t dst) {};

        virtual int startFirmwareUpgrade(const char *file_name) = 0;

        virtual int startFirmwareUpgrade(const char *buffer, size_t size) = 0;

        virtual int resetDevice() = 0;

        virtual int getDeviceInfo() = 0;

        virtual int setNoiseSuppressionLevel(DegreeLevel level) = 0;

        virtual int getNoiseSuppressionLevel() = 0;

        virtual int setReverberationSuppressionLevel(DegreeLevel Level) = 0;

        virtual int getReverberationSuppressionLevel() = 0;

        virtual int setEchoCancellationLevel(DegreeLevel Level) = 0;

        virtual int getEchoCancellationLevel() = 0;

        virtual int resetAudioAlgorithmParams() = 0;

        virtual int startAudioRecord(uint16_t max_time_second) = 0;//default = 600s

        virtual int stopAudioRecord() = 0;

        virtual int reboot() = 0;

        // tail
        virtual int setTailWifiSSID(const char* ssid, int length) = 0;

        virtual int getTailWifiSSID() = 0;

        /***For Hunman Interface Settings***/
        virtual int setMuted(bool mute) = 0;

        virtual int setOffHook(bool off) = 0;
    };
}


#endif // !AXDP_API_H_
