package com.auditoryworks.axdplib;


import com.auditoryworks.axdp.DeviceEvent;

public interface IDeviceMessageListener {
    void onServiceConnected();
    void onServiceDisconnected();
    void onDeviceInfo(int deviceId, int devType, String productName,
                      String phyVersion, String softVersion,
                      String serialNumber, String uniqueId);
    void onHidCallState(DeviceEvent.HidCallState state);
    void onDeviceEvent(DeviceEvent.HidEvent event);
    void onVideoMode(DeviceEvent.VideoMode mode);
    void onGetPowerLineFreq(int freq);
    void onGetMirrorState(int state);
    void onGetFlipState(int state);
    void onDfuUpdateState(DeviceEvent.UpgradeState state, int progress);
    void onGetDereverationAlgParam(int val);
    void onGetVideoTrackMode(int mode);
}
