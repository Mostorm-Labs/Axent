#ifndef AXDP_DEVICE_INSTANCE_H
#define AXDP_DEVICE_INSTANCE_H

#include "axdp_api.h"

class DeviceInstance : public axdp::EventCallbackDelegate {
public:
	DeviceInstance() {}
	~DeviceInstance() {}
	void onResetDevice(axdp::ResultState state) override;
	void onGetDeviceInformation(const axdp::DeviceInfo* info, uint16_t dev_count) override;
	void onDfuStateUpdate(axdp::UpgradeState state) override;
	void onDfuProgressUpdate(uint16_t progress) override;
	void onGetVideoMode(axdp::VideoMode mode) override;
private:
	axdp::DeviceAccessor* _dev = nullptr;
};

#endif // !AXDP_DEVICE_INSTANCE_H
