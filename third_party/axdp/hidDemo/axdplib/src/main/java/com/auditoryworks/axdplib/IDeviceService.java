package com.auditoryworks.axdplib;

/**
 * created by cjb 2022-11-28 11:26
 * Desc :接口类，控制设备接口
 */
public interface IDeviceService {
    boolean hasSoundbar();
    boolean isActive();
    boolean connect();
    void disconnect();
    boolean isConnected();
    void setMessageListener(IDeviceMessageListener listener);

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
    void getDereverationAlgParam();
    void setDereverationAlgParam(int val);
    void getVideoTrackMode();
    void setVideoTrackMode(int mode);

    void firmwareUpgrade(String dfu_file);
}
