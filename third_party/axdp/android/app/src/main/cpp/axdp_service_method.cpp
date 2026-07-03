//
// Created by Staney on 23/11/2022.
//
#include <android/log.h>
#include <cstring>
#include <string>
#include "axdp_service_method.h"

#define TAG "ServiceMethod"
#define LOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);

AxdpServiceMethod::AxdpServiceMethod() :_jvm(nullptr){

}

AxdpServiceMethod::~AxdpServiceMethod() {

}

void AxdpServiceMethod::create(JavaVM *jvm, jobject service) {
    this->_jvm = jvm;
    JNIEnv *env = nullptr;
    jvm->GetEnv((void**)(&env), JNI_VERSION_1_6);
    if(env){
        _serviceObj = env->NewGlobalRef(service);
        jclass serviceClz = env->GetObjectClass(service);
        _method_on_device_info = env->GetMethodID(serviceClz, "jni_onDeviceInfo",
                                                  "(IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
        LOGV("method address:%08x", _method_on_device_info);
        _method_on_hid_call_state = env->GetMethodID(serviceClz, "jni_onHidCallState", "(I)V");
        _method_on_device_event = env->GetMethodID(serviceClz, "jni_onDeviceEvent", "(I)V");
        _method_on_get_video_mode = env->GetMethodID(serviceClz, "jni_onGetVideoMode", "(I)V");
        _method_on_get_power_line_freq = env->GetMethodID(serviceClz, "jni_onGetPowerLineFreq", "(I)V");
        _method_on_get_mirror_state = env->GetMethodID(serviceClz, "jni_onGetMirrorState", "(I)V");
        _method_on_get_flip_state = env->GetMethodID(serviceClz, "jni_onGetFlipState", "(I)V");
        _method_on_dfu_update_state = env->GetMethodID(serviceClz, "jni_onDfuUpdateState", "(II)V");
        _method_on_dereveration_alg_param = env->GetMethodID(serviceClz, "jni_onGetDereverationAlgParam", "(I)V");
        _method_on_get_video_track_mode = env->GetMethodID(serviceClz, "jni_onGetVideoTrackMode", "(I)V");
        _method_on_get_tail_ssid = env->GetMethodID(serviceClz, "jni_onGetTailWifiSSID", "(Ljava/lang/String;)V");
    }
}

void AxdpServiceMethod::destroy() {
    LOGV("###### AxdpServiceMethod destroy ######");
    if(_jvm){
        JNIEnv *env = nullptr;
        int getEnvStat = _jvm->GetEnv((void**)(&env), JNI_VERSION_1_6);
        if (getEnvStat == JNI_EDETACHED) {
            _jvm->AttachCurrentThread(&env, nullptr);
        }
        if(env){
            env->DeleteGlobalRef(_serviceObj);
            LOGV(">>>>>AxdpServiceMethod delete axdp service object OK");
        }
        if (getEnvStat == JNI_EDETACHED) {
            _jvm->DetachCurrentThread();
        }
    }
    this->_jvm = nullptr;
}

void AxdpServiceMethod::callVoidMethodParamInt1(jmethodID &method, int param) {
    if(_jvm){
        JNIEnv *env = nullptr;
        _jvm->AttachCurrentThread(&env, nullptr);
        if(env){
            env->CallVoidMethod(_serviceObj, method, param);
        }
        _jvm->DetachCurrentThread();
    }
}

void AxdpServiceMethod::onDeviceInfo(int device_id, int dev_type, const char *product_name,
                                     const char *phy_version, const char *soft_version,
                                     const char *serial_number, const char *unique_id) {
    LOGV("onDeviceInfo called:%08x", _jvm);
    if(_jvm){
        JNIEnv *env = nullptr;
        _jvm->AttachCurrentThread(&env, nullptr);
        LOGV("onDeviceInfo env:%08x", env);
        if(env){
            jstring jName = env->NewStringUTF(product_name);
            jstring jPhyVersion = env->NewStringUTF(phy_version);
            jstring jSoftVersion = env->NewStringUTF(soft_version);
            jstring jSerial = env->NewStringUTF(serial_number);
            jstring jId = env->NewStringUTF(unique_id);
            LOGV("onDeviceInfo ready to call _method_on_device_info");
            env->CallVoidMethod(_serviceObj, _method_on_device_info,
                                device_id, dev_type, jName, jPhyVersion, jSoftVersion, jSerial, jId);
        }
        _jvm->DetachCurrentThread();
    }
}

void AxdpServiceMethod::onHidCallState(int state) {
    callVoidMethodParamInt1(_method_on_hid_call_state, state);
}

void AxdpServiceMethod::onDeviceEvent(int event) {
    callVoidMethodParamInt1(_method_on_device_event, event);
}

void AxdpServiceMethod::onGetVideoMode(int mode) {
    callVoidMethodParamInt1(_method_on_get_video_mode, mode);
}

void AxdpServiceMethod::onGetVideoTrackMode(int mode) {
    callVoidMethodParamInt1(_method_on_get_video_track_mode, mode);
}

void AxdpServiceMethod::onGetPowerLineFreq(int freq) {
    callVoidMethodParamInt1(_method_on_get_power_line_freq, freq);
}

void AxdpServiceMethod::onGetMirrorState(int state) {
    callVoidMethodParamInt1(_method_on_get_mirror_state, state);
}

void AxdpServiceMethod::onGetFlipState(int state) {
    callVoidMethodParamInt1(_method_on_get_flip_state, state);
}

void AxdpServiceMethod::onDfuUpdateState(int state, int extra) {
    if(_jvm){
        JNIEnv *env = nullptr;
        int getEnvStat = _jvm->GetEnv((void**)(&env), JNI_VERSION_1_6);
        if (getEnvStat == JNI_EDETACHED) {
            _jvm->AttachCurrentThread(&env, nullptr);
        }
        if(env){
            env->CallVoidMethod(_serviceObj, _method_on_dfu_update_state, state, extra);
        }
        if (getEnvStat == JNI_EDETACHED) {
            _jvm->DetachCurrentThread();
        }
    }
}

void AxdpServiceMethod::onGetDereverationAlgParam(int val) {
    callVoidMethodParamInt1(_method_on_dereveration_alg_param, val);
}

void AxdpServiceMethod::onGetTailWifiSSId(const uint8_t *ssid, uint16_t len)  {
    LOGV("onGetTailWifiSSId called:%08x", _jvm);
    if(_jvm){
        JNIEnv *env = nullptr;
        _jvm->AttachCurrentThread(&env, nullptr);
        LOGV("onGetTailWifiSSId env:%08x", env)
        if(env){
//            std::string s(ssid,ssid+len);
//            jstring jssid = env->NewStringUTF(s.c_str());
//            LOGV("onGetTailWifiSSId ready to call %s",jssid)
//            env->CallVoidMethod(_serviceObj, _method_on_get_tail_ssid,jssid);
        }
        _jvm->DetachCurrentThread();
    }
}




