package com.auditoryworks.axdp;


public class DeviceEvent {

    public enum HidEvent {
        Mute,       //静音
        Unmute,     //非静音
        Answer,     //接听
        HangUp,     //挂断
        VolDown,    //音量减
        VolUp       //音量加
    }

    public enum HidCallState {
        Disabled,
        Enabled
    }
}
