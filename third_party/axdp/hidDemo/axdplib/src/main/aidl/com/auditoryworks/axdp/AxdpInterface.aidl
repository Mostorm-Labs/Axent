// AxdpInterface.aidl
package com.auditoryworks.axdp;
import com.auditoryworks.axdp.MessageData;
import com.auditoryworks.axdp.MessageListener;
// Declare any non-default types here with import statements

interface AxdpInterface {
    boolean hasSoundbar();
    boolean isActive();
    void register(MessageListener listener);//注册回调
    void unregister(MessageListener listener);//销毁回调
    void getDeviceInfo();
    void setHidCallState(boolean enabled);
    void getHidCallState();
    void answer();
    void hangUp();
    void setDeviceMute(boolean mute);

    void getPowerLineFreq();
    void setPowerLineFreq(int freq_type);
    void getVideoMode();
    void setVideoMode(int video_mode);
    void getReverberationSuppressionLevel();
    void setReverberationSuppressionLevel(int degree_level);
    void getMirrorState();
    void setMirrorState(int enable_state);
    void getFlipState();
    void setFlipState(int enable_state);

    void firmwareUpgrade(String dfu_file);

    void getDereverationAlgParam();
    void setDereverationAlgParam(int val);

    void getVideoTrackMode();
    void setVideoTrackMode(int video_mode);
}