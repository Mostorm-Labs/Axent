#ifndef AXDP_HUMAN_INTERFACE_HANDLER_H_
#define AXDP_HUMAN_INTERFACE_HANDLER_H_

#include "axdp_api.h"

namespace axdp {
    enum MuteEdgeState {
        MUTE_EDGE_HIGH,
        MUTE_EDGE_LOW,
        MUTE_EDGE_INIT,
    };

    class HumanInterfaceEventHandler {
    public:
        HumanInterfaceEventHandler() = default;
        ~HumanInterfaceEventHandler() = default;
        void setEventCallback(EventCallbackDelegate* cb) { cb_ = cb; }
        int onInputReport(const uint8_t* buffer, size_t length);
        int setMuted(bool mute);
        bool muted() { return muted_; }
        int setOffHook(bool off);
        bool offHook() { return off_hook_; }
    private:
        EventCallbackDelegate* cb_{ nullptr };
        MuteEdgeState mute_edge_state_{ MUTE_EDGE_INIT };
        bool muted_{ false };
        bool off_hook_{ false };
    };
}

#endif // !AXDP_HUMAN_INTERFACE_HANDLER_H_
