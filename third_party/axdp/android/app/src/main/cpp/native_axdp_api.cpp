#include <jni.h>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <android/log.h>
#include "include/axdp_api.h"
#include "include/axdp_defines.h"
#include "axdp_service_method.h"

#define TAG "JNI_AXDP_API"
#define LOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,\
        "[line:%d]: " format, __LINE__, ##__VA_ARGS__);
#define LOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG,\
        "[line:%d]: " format, __LINE__, ##__VA_ARGS__);
#define LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG,\
        "[line:%d]: " format, __LINE__, ##__VA_ARGS__);
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN, TAG,\
        "[line:%d]: " format, __LINE__, ##__VA_ARGS__);
#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG,\
        "[line:%d]: " format, __LINE__, ##__VA_ARGS__);

using namespace axdp;

static JavaVM *jvm;
static std::mutex g_mutex;
static std::map<long, EventCallbackDelegate *> g_dev_mgr;
static AxdpServiceMethod axdpJavaService;

static DeviceAccessor* GetDevInst(jlong dev_handle){
    std::unique_lock<std::mutex> lock(g_mutex);
    if (dev_handle == 0)
        return nullptr;
    auto it = g_dev_mgr.find(dev_handle);
    if (it == g_dev_mgr.end()) {
        LOGE("Not found device in map, dev id = %ld", dev_handle);
        return nullptr;
    }
    return reinterpret_cast<DeviceAccessor*>(dev_handle);
}

static EventCallbackDelegate* GetCallbackInst(jlong dev_handle){
    std::unique_lock<std::mutex> lock(g_mutex);
    if (dev_handle == 0)
        return nullptr;
    auto it = g_dev_mgr.find(dev_handle);
    if (it == g_dev_mgr.end()) {
        LOGE("Not found device in map, dev id = %ld", dev_handle);
        return nullptr;
    }
    return it->second;
}

static void AddDevInst(jlong dev_handle, EventCallbackDelegate* cb){
    std::unique_lock<std::mutex> lock(g_mutex);
    g_dev_mgr.emplace(std::make_pair(dev_handle, cb));
}

static void RemoveDevInst(jlong dev_handle){
    std::unique_lock<std::mutex> lock(g_mutex);
    auto it = g_dev_mgr.find(dev_handle);
    if (it == g_dev_mgr.end()) {
        LOGE("Not found device in map, dev id = %ld", dev_handle);
        return;
    }
    g_dev_mgr.erase(dev_handle);
}

static void notify_device_interface_on_event(jobject obj, int event_type) {
    JNIEnv *env;
    //获取当前native线程是否有没有被附加到jvm环境中
    int getEnvStat = jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        //如果没有， 主动附加到jvm环境中，获取到env
        if (jvm->AttachCurrentThread(&env, NULL) != 0) {
            return;
        }
    }
    //通过存储的变量obj 获取到要回调的类
    jclass javaClass = env->GetObjectClass(obj);

    //获取要回调的方法ID
    jmethodID javaCallbackId = (env)->GetMethodID(javaClass, "onDeviceEventNotify", "(I)I");
    if (javaCallbackId == NULL) {
        LOGW("Unable to find method:onProgressCallBack");
        return;
    }

    env->CallIntMethod(obj, javaCallbackId, event_type);

    //todo : check if it is needed
    //env->DeleteGlobalRef(g_obj);

    //jvm->DetachCurrentThread();
}

class CallbackImpl : public EventCallbackDelegate {
public:
    explicit CallbackImpl(jobject obj) : obj_(obj){

    }

    ~CallbackImpl() override {

    };

    void setJObj(jobject obj) {
        obj_ = obj;
    }

    void onHumanInterfaceEvent(HIDEvent event) override {
      //  notify_device_interface_on_event(obj_, (int) event);
        axdpJavaService.onDeviceEvent((int) event);
    };

    void onResetDevice(ResultState state) override {
        LOGD("reset device successfully");
    };

    void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) override {
        for (size_t i = 0; i < dev_count; i++) {
            LOGD("11device id: %d", i);
            LOGD("11phy_version: %s", info[i].phy_version);
            LOGD("11product_name: %s", info[i].product_name);
            LOGD("11serial_number: %s", info[i].serial_number);
            LOGD("11soft_version: %s", info[i].soft_version);
            LOGD("11unique_id: %s", info[i].unique_id);
            axdpJavaService.onDeviceInfo(i, info[i].dev_type, info[i].product_name,
                                          info[i].phy_version,info[i].soft_version,
                                         info[i].serial_number, info[i].unique_id);

        }
    };

    void onDfuStateUpdate(UpgradeState state) override {
        std::string state_str;
        switch (state) {
            case axdp::UpgradeState::DataReady:
                LOGD("Data is ready, start to transfer");
                update_progress_ = 0;
                break;
            case axdp::UpgradeState::Transferring:
                LOGD("Data is transferring");
                break;
            case axdp::UpgradeState::Verifying:
                LOGD("Data is verifying");
                break;
            case axdp::UpgradeState::Success:
                LOGD("Upgrade is success");
                update_progress_ = 0;
                break;
            case axdp::UpgradeState::Failed:
                LOGD("Upgrade is Failed");
                update_progress_ = 0;
                break;
            default:
                break;
        }
        axdpJavaService.onDfuUpdateState((int)state, -1);
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        if(update_progress_ != progress){
            LOGD("DFU PROGRESS  = %d", progress);
            axdpJavaService.onDfuUpdateState((int)axdp::UpgradeState::Transferring, progress);
            update_progress_ = progress;
        }
    };

    void onGetNoiseSuppressionLevel(DegreeLevel level) override {
        LOGD("ANS LEVEL  = %d", int(level));
    };

    void onGetReverbrationSuppressionLevel(DegreeLevel level) override {
        LOGD("Reverb suppression LEVEL  = %d", int(level));
    };

    void onGetEchoCancellationLevel(DegreeLevel level) override {
        LOGD("AEC LEVEL  = %d", int(level));
    };

    void onAudioRecordStart(bool supported, const AudioFormat *format) override {
        LOGD("Device Supported Record ", supported);
        LOGD("Device Record AudioFormat: ");
        LOGD("   Audio type: ", format->audio_type);
        LOGD("   Samplerate: ", format->sample_rate);
        LOGD("  Channel num: ", format->channel_num);
        LOGD("    Bit Width: ", format->bit_width);
    }

    void onAudioRecordData(const uint8_t *data, int32_t len) override {
        //On RECV RECORD DATA HERE, CAN BE DOWNLOAD
    }

    void onAudioRecordStopped(int32_t result) override {
        LOGD("Device Supported Record, result = ", result);
    }

    void onGetHidCallState(EnableState state) override{
        LOGD("Device hid call state, result = ", int(state));
        axdpJavaService.onHidCallState((int)state);
    }

    void onGetVideoMode(VideoMode mode) override {
        LOGD("Get VideoMode, result = ", int(mode));
        axdpJavaService.onGetVideoMode((int)mode);
    }

    void onGetPowerLineFreq(PowerlineFreqType freq) override {
        LOGD("Get PowerLineFreq, result = ", int(freq));
        axdpJavaService.onGetPowerLineFreq((int)freq);
    }

    void onGetMirrorState(EnableState state) override {
        LOGD("Get MirrorState, result = ", int(state));
        axdpJavaService.onGetMirrorState((int)state);
    }

    void onGetFlipState(EnableState state) override {
        LOGD("Get FlipState, result = ", int(state));
        axdpJavaService.onGetFlipState((int)state);
    }

    void onGetDereverationAlgParam(int32_t val) override {
        LOGD("Get Dereveration Alg, result = ", val);
        axdpJavaService.onGetDereverationAlgParam(val);
    }

    void onGetVideoTrackMode(VideoTrackMode mode) override {
        LOGD("Get VideoTrackMode, result = ", (int)mode);
        axdpJavaService.onGetVideoTrackMode((int)mode);
    }

    void onGetWifi(const uint8_t* ssid, uint16_t len) override {
        LOGD("onGetWifi len ======== %x", int(len))
        for (size_t i = 0; i < len; i++)
        {
            //std::cout << "onGetWifi = " << ssid[i] << std::endl;
            LOGD("onGetWifi = %x", ssid[i]);
        }
        axdpJavaService.onGetTailWifiSSId(ssid, len);
    }

private:
    jobject obj_;
    int update_progress_ = 0;
};

extern "C"
JNIEXPORT jlong JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeCreateDevice(JNIEnv *env, jobject thiz, jint file_desc,
                                                           jint vid, jint pid, jint interface_num) {
    DeviceAccessor *handle = DeviceAccessor::Create(vid, pid, file_desc, interface_num);
    if (NULL == handle) {
        LOGE("Failed to Create DeviceAccessor");
        return 0;
    }
    //jvm对象保存，为后面找到每个线程对应的env
    env->GetJavaVM(&jvm);

    //保存jobj对象，其目的是在回调线程中能够调用java方法
    jobject obj = env->NewGlobalRef(thiz);

    axdpJavaService.create(jvm, thiz);

    //生成回调代理对象，并保存obj对象引用
    EventCallbackDelegate *cb = new CallbackImpl(obj);
    handle->registerCbDelegate(cb);

    //保存设备及回调类
    long device = (long) handle;
    AddDevInst(device, cb);
    return (long) handle;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeDestroyDevice(JNIEnv *env, jobject thiz,
                                                                       jlong device_handle) {
    axdpJavaService.destroy();
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    auto cb = GetCallbackInst(device_handle);
    if (cb == nullptr)
        return -2;
    RemoveDevInst(device_handle);
    device->unregisterCbDelegate(cb);
    delete cb;
    delete device;
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetDeviceHookOff(JNIEnv *env, jobject thiz,
                                                          jlong device_handle, jboolean off) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setOffHook(off);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetDeviceMute(JNIEnv *env, jobject thiz,
                                                       jlong device_handle, jboolean mute) {
    auto device =  GetDevInst(device_handle);
    LOGW("jni device mute called:%d", mute);
    if (device == nullptr)
        return -1;
    int ret = device->setMuted(mute);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetDeviceInfo(JNIEnv *env, jobject thiz,
                                                       jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getDeviceInfo();
    return ret;
}

char* ConvertJByteaArrayToChars(JNIEnv *env, jbyteArray bytearray){
    jbyte *bytes = env->GetByteArrayElements(bytearray, 0);
    int arrayLength = env->GetArrayLength(bytearray);
    char *chars = new char[arrayLength];
    memset(chars, 0, arrayLength);
    memcpy(chars, bytes, arrayLength);
    env->ReleaseByteArrayElements(bytearray, bytes, 0);
    return chars;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_TailService_nativeSetTailWifiSsid(JNIEnv *env, jobject thiz,
                                                              jlong device_handle,
                                                              jbyteArray data, jint length) {
    auto device =  GetDevInst(device_handle);
    const char *ssid = ConvertJByteaArrayToChars(env,data);
    LOGW("jni device tail wifi ssid called:%d", ssid)
    if (device == nullptr)
        return -1;

    int ret = device->setTailWifiSSID(ssid,length);
    if (ssid){
        delete[] ssid;
    }
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_TailService_nativeGetTailWiFiSsid(JNIEnv *env, jobject thiz, jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getTailWifiSSID();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetHidCallState(JNIEnv *env, jobject thiz,
                                                         jlong device_handle, jboolean enabled) {
    LOGD("nativeSetHidCallState = %d", enabled);
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setHidCallState(enabled?EnableState::Enabled:EnableState::Disabled);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetHidCallState(JNIEnv *env, jobject thiz,
                                                         jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getHidCallState();
    return ret;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSyncGetDeviceInfo(JNIEnv *env, jobject thiz,
                                                           jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    auto& dev_info_list = device->syncGetDeviceInfo();
    for (int i = 0; i < dev_info_list.size(); ++i) {
        LOGD("22device id: %d", i);
        LOGD("22phy_version: %s", dev_info_list[i].phy_version);
        LOGD("22product_name: %s", dev_info_list[i].product_name);
        LOGD("22serial_number: %s", dev_info_list[i].serial_number);
        LOGD("22soft_version: %s", dev_info_list[i].soft_version);
        LOGD("22unique_id: %s", dev_info_list[i].unique_id);
    }
    if (dev_info_list.empty())
        return -1;
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetPowerLineFreq(JNIEnv *env, jobject thiz,
                                                               jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getPowerLineFreq();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetPowerLineFreq(JNIEnv *env, jobject thiz,
                                                               jlong device_handle, jint freq_type) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setPowerLineFreq(static_cast<PowerlineFreqType>(freq_type));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetVideoMode(JNIEnv *env, jobject thiz,
                                                               jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getVideoMode();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetVideoMode(JNIEnv *env, jobject thiz,
                                                           jlong device_handle, jint video_mode) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setVideoMode(static_cast<VideoMode>(video_mode));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetReverberationSuppressionLevel(JNIEnv *env, jobject thiz,
                                                           jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getReverberationSuppressionLevel();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetReverberationSuppressionLevel(JNIEnv *env, jobject thiz,
                                                           jlong device_handle, jint degree_level) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setReverberationSuppressionLevel(static_cast<DegreeLevel>(degree_level));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetMirrorState(JNIEnv *env, jobject thiz,
                                                                               jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getMirrorState();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetMirrorState(JNIEnv *env, jobject thiz,
                                                             jlong device_handle, jint enable_state) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setMirrorState(static_cast<EnableState>(enable_state));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetFlipState(JNIEnv *env, jobject thiz,
                                                             jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getFlipState();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetFlipState(JNIEnv *env, jobject thiz,
                                                             jlong device_handle, jint enable_state) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setFlipState(static_cast<EnableState>(enable_state));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeFirmwareUpgrade(JNIEnv *env, jobject thiz,
                                                           jlong device_handle, jstring dfu_file) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    device->resetDevice();
    device->setUpgradeStrategy(UpdateStrategy::Viper);
    device->setUpgradeDeviceDst(2);
    const char* jni_dfu_file = env->GetStringUTFChars(dfu_file, 0);
    LOGD("升级固件:%s", jni_dfu_file);
    int ret = device->startFirmwareUpgrade(jni_dfu_file);
    env->ReleaseStringUTFChars(dfu_file, jni_dfu_file);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetDereverationAlgParam(JNIEnv *env, jobject thiz,
                                                           jlong device_handle, jint val_0_100) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    if(val_0_100 < 0)val_0_100 = 0;
    if(val_0_100 > 100)val_0_100 = 100;
    int ret = device->setDereverationAlgParam(val_0_100);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetDereverationAlgParam(JNIEnv *env, jobject thiz,
                                                           jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getDereverationAlgParam();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeSetVideoTrackMode(JNIEnv *env, jobject thiz,
                                                                      jlong device_handle, jint value) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setVideoTrackMode(static_cast<VideoTrackMode>(value));
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_AxdpService_nativeGetVideoTrackMode(JNIEnv *env, jobject thiz,
                                                                      jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getVideoTrackMode();
    return ret;
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_auditoryworks_axdp_TailService_nativeCreateDevice(JNIEnv *env, jobject thiz, jint file_desc,
                                                           jint vid, jint pid, jint interface_num) {
    DeviceAccessor *handle = DeviceAccessor::Create(vid, pid, file_desc, interface_num);
    if (NULL == handle) {
        LOGE("Failed to Create DeviceAccessor");
        return 0;
    }
    //jvm对象保存，为后面找到每个线程对应的env
    env->GetJavaVM(&jvm);

    //保存jobj对象，其目的是在回调线程中能够调用java方法
    jobject obj = env->NewGlobalRef(thiz);

    //生成回调代理对象，并保存obj对象引用
    EventCallbackDelegate *cb = new CallbackImpl(obj);
    handle->registerCbDelegate(cb);

    //保存设备及回调类
    long device = (long) handle;
    AddDevInst(device, cb);
    return (long) handle;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_TailService_nativeDestroyDevice(JNIEnv *env, jobject thiz,
                                                            jlong device_handle) {
    auto device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    auto cb = GetCallbackInst(device_handle);
    if (cb == nullptr)
        return -2;
    RemoveDevInst(device_handle);
    device->unregisterCbDelegate(cb);
    delete cb;
    delete device;
    return 0;
}