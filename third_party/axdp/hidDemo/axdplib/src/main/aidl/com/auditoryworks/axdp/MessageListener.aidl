// MessageListener.aidl
package com.auditoryworks.axdp;
import com.auditoryworks.axdp.MessageData;
// Declare any non-default types here with import statements


interface MessageListener {

    void onGetMessage(in MessageData data);
    void onDeviceInfo(int device_id, int dev_type, String product_name,
                      String phy_version, String soft_version,
                      String serial_number, String unique_id);
    void onHidCallState(int state);
    void onDeviceEvent(int event);

    void onGetVideoMode(int mode);
    void onGetPowerLineFreq(int freq);
    void onGetMirrorState(int state);
    void onGetFlipState(int state);
    void onDfuUpdateState(int state, int extra);
    void onGetDereverationAlgParam(int val);
    void onGetVideoTrackMode(int mode);
    void onGetTailWifiSSID(String ssid);
}