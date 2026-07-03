//
// Created by Staney on 23/11/2022.
//

#ifndef AXDP_AXDP_SERVICE_METHOD_H
#define AXDP_AXDP_SERVICE_METHOD_H


#include <jni.h>

class AxdpServiceMethod {
public:
    AxdpServiceMethod();
    ~AxdpServiceMethod();
    void create(JavaVM *jvm, jobject service);
    void destroy();
    void onDeviceInfo(int device_id, int dev_type, const char *product_name, const char *phy_version,
                      const char *soft_version, const char *serial_number, const char *unique_id);
    void onHidCallState(int state);
    void onDeviceEvent(int event);
    void onGetVideoMode(int mode);
    void onGetPowerLineFreq(int freq);
    void onGetMirrorState(int state);
    void onGetFlipState(int state);
    void onDfuUpdateState(int state, int extra);
    void onGetDereverationAlgParam(int val);
    void onGetVideoTrackMode(int mode);
    void onGetTailWifiSSId(const uint8_t *string, uint16_t i);
private:
    void callVoidMethodParamInt1(jmethodID &method, int param);

    JavaVM *_jvm;
    jobject _serviceObj;
    jmethodID _method_on_device_info;
    jmethodID _method_on_hid_call_state;
    jmethodID _method_on_device_event;
    jmethodID _method_on_get_video_mode;
    jmethodID _method_on_get_power_line_freq;
    jmethodID _method_on_get_mirror_state;
    jmethodID _method_on_get_flip_state;
    jmethodID _method_on_dfu_update_state;
    jmethodID _method_on_dereveration_alg_param;
    jmethodID _method_on_get_video_track_mode;
    jmethodID _method_on_get_tail_ssid;
};


#endif //AXDP_AXDP_SERVICE_METHOD_H
