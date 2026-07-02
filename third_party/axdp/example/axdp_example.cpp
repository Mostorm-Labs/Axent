#include "axdp_example_common.h"

#ifdef  WIN32
#include <Windows.h>
#endif

int main(int argc, const char** argv) {
//    axdp_info_process(gV20DeviceAttr);
//    axdp_dfu_process(gV20DeviceAttr, axdp::UpdateStrategy::Falcon);
    //axdp_info_process(gA20DeviceAttr);
    //axdp_dfu_process(gA20DeviceAttr,axdp::UpdateStrategy::Dolphin);
    //axdp_info_process(gAMX100DeviceAttr);
    //std::cout << "axdp_info_process begin" << std::endl;
    //axdp_info_process(gC30RDeviceAttr);
    axdp_dfu_process(gAWM10TRDeviceAttr, axdp::UpdateStrategy::Discus);
    return 0;
}
