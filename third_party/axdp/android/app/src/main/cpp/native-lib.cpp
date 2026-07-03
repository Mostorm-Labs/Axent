#include <jni.h>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <android/log.h>
#include "include/axdp_api.h"
#include "include/axdp_defines.h"

#define TAG "AXDP_JNI"
#define LOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG,\
        "[%s][%d]: " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);

using namespace axdp;

static JavaVM *jvm;
static std::mutex g_mutex;
static std::map<long, EventCallbackDelegate *> g_dev_mgr;

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

class Callbacks : public EventCallbackDelegate {
public:
    explicit Callbacks(jobject obj) : obj_(obj){

    }

    ~Callbacks() override {

    };

    void setJObj(jobject obj) {
        obj_ = obj;
    }

    void onHumanInterfaceEvent(HIDEvent event) override {
        notify_device_interface_on_event(obj_, (int) event);
    };

    void onResetDevice(ResultState state) override {
        LOGD("reset device successfully");
    };

    void onGetDeviceInformation(const DeviceInfo *info, uint16_t dev_count) override {
        for (size_t i = 0; i < dev_count; i++) {
            LOGD("device id: %d", i);
            LOGD("phy_version: %s", info[i].phy_version);
            LOGD("product_name: %s", info[i].product_name);
            LOGD("serial_number: %s", info[i].serial_number);
            LOGD("soft_version: %s", info[i].soft_version);
            LOGD("unique_id: %s", info[i].unique_id);
        }
    };

    void onDfuStateUpdate(UpgradeState state) override {
        std::string state_str;
        switch (state) {
            case axdp::UpgradeState::DataReady:
                LOGD("Data is ready, start to transfer");
                break;
            case axdp::UpgradeState::Transferring:
                LOGD("Data is transferring");
                break;
            case axdp::UpgradeState::Verifying:
                LOGD("Data is verifying");
                break;
            case axdp::UpgradeState::Success:
                LOGD("Upgrade is success");
                break;
            case axdp::UpgradeState::Failed:
                LOGD("Upgrade is Failed");
                break;
            default:
                break;
        }
    };

    void onDfuProgressUpdate(uint16_t progress) override {
        LOGD("DFU PROGRESS  = %d", progress);
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
    }

    void onGetWifi(const uint8_t* ssid, uint16_t len) override {

        LOGD("onGetWifi len ======== ", int(len));
        for (size_t i = 0; i < len; i++)
        {
            //std::cout << "onGetWifi = " << ssid[i] << std::endl;
            LOGD("onGetWifi = %x", ssid[i]);
        }

    }

private:
    jobject obj_;
};

extern "C" JNIEXPORT jstring JNICALL
Java_com_auditoryworks_axdp_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from axdp";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_auditoryworks_axdp_MainActivity_createDeviceInterface(JNIEnv *env, jobject thiz,
                                                               jint file_desc) {
    DeviceAccessor *handle = DeviceAccessor::Create(0x0627, 0xA6A0, file_desc, 2);
    if (NULL == handle) {
        LOGE("Failed to Create DeviceAccessor");
        return 0;
    }
    //jvm对象保存，为后面找到每个线程对应的env
    env->GetJavaVM(&jvm);

    //保存jobj对象，其目的是在回调线程中能够调用java方法
    jobject obj = env->NewGlobalRef(thiz);

    //生成回调代理对象，并保存obj对象引用
    EventCallbackDelegate *cb = new Callbacks(obj);
    handle->registerCbDelegate(cb);

    //保存设备及回调类
    long device = (long) handle;
    AddDevInst(device, cb);
    return (long) handle;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_setDeviceHookOff(JNIEnv *env, jobject thiz,
                                                          jlong device_handle, jboolean off) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setOffHook(off);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_setDeviceMute(JNIEnv *env, jobject thiz,
                                                       jlong device_handle, jboolean mute) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setMuted(mute);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_destroyDeviceInterface(JNIEnv *env, jobject thiz,
                                                                jlong device_handle) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    auto* cb = GetCallbackInst(device_handle);
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
Java_com_auditoryworks_axdp_MainActivity_getDeviceInfo(JNIEnv *env, jobject thiz,
                                                       jlong device_handle) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getDeviceInfo();
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_setHidCallState(JNIEnv *env, jobject thiz,
                                                         jlong device_handle, jboolean enabled) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->setHidCallState(enabled?EnableState::Enabled:EnableState::Disabled);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_getHidCallState(JNIEnv *env, jobject thiz,
                                                         jlong device_handle) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getHidCallState();
    return ret;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_syncGetDeviceInfo(JNIEnv *env, jobject thiz,
                                                           jlong device_handle) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    auto& dev_info_list = device->syncGetDeviceInfo();
    for (int i = 0; i < dev_info_list.size(); ++i) {
        LOGD("device id: %d", i);
        LOGD("phy_version: %s", dev_info_list[i].phy_version);
        LOGD("product_name: %s", dev_info_list[i].product_name);
        LOGD("serial_number: %s", dev_info_list[i].serial_number);
        LOGD("soft_version: %s", dev_info_list[i].soft_version);
        LOGD("unique_id: %s", dev_info_list[i].unique_id);
    }
    if (dev_info_list.empty())
        return -1;
    return 0;
}

// tail
extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_setTailWiFiSSID(JNIEnv *env, jobject thiz,
                                                         jlong device_handle, jstring ssid, jint len) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    const char *c_ssid = env->GetStringUTFChars(ssid, JNI_FALSE);
    int ret = device->setTailWifiSSID(c_ssid, len);
    return ret;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_auditoryworks_axdp_MainActivity_getTailWiFiSSID(JNIEnv *env, jobject thiz,
                                                         jlong device_handle) {
    auto* device =  GetDevInst(device_handle);
    if (device == nullptr)
        return -1;
    int ret = device->getTailWifiSSID();
    return ret;
}