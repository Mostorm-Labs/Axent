#include "dev_inst.h"
#include "third_party/easyloggingpp/src/easylogging++.h"

void DeviceInstance::onResetDevice(axdp::ResultState state)
{
	std::cout << "Device reset successfully" << std::endl;
}

void DeviceInstance::onGetDeviceInformation(const axdp::DeviceInfo* info, uint16_t dev_count)
{
	std::cout << "Device information received" << std::endl;
	for (size_t i = 0; i < dev_count; i++) {
		std::cout << "Device ID: " << i << std::endl;
		std::cout << "Product Name: " << info[i].product_name << std::endl;
		std::cout << "Serial Number: " << info[i].serial_number << std::endl;
		std::cout << "Software Version: " << info[i].soft_version << std::endl;
	}
}

void DeviceInstance::onDfuStateUpdate(axdp::UpgradeState state)
{
	std::string state_str;
	switch (state) {
	case axdp::UpgradeState::DataReady:
		state_str = "Data is ready, start to transfer";
		break;
	case axdp::UpgradeState::Transferring:
		state_str = "Data is transferring";
		break;
	case axdp::UpgradeState::Verifying:
		state_str = "Data is verifying";
		break;
	case axdp::UpgradeState::Success:
		state_str = "Upgrade success";
		break;
	case axdp::UpgradeState::Failed:
		state_str = "Upgrade Failed";
		break;
	default:
		break;
	}
	std::cout << "DfuStateUpdate > " << (int)state << " " << state_str << std::endl;

}

void DeviceInstance::onDfuProgressUpdate(uint16_t progress)
{
	std::cout << "DFU progress: " << progress << "%" << std::endl;
}

void DeviceInstance::onGetVideoMode(axdp::VideoMode mode)
{
	std::cout << "Video mode: " << (int)mode << std::endl;
}

