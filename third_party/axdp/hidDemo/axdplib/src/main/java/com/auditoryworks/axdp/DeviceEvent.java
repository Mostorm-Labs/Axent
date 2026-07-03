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

    public enum UpgradeState{
        DataReady,      //数据准备完成，向设备请求升级
        Transferring,   //数据传输中，此时开始progress变化
        Verifying,      //传输完成，等待数据确认
        Success,        //设备升级成功，等待重启动
        Failed          //设备升级失败
    }

    public enum VideoMode {
        PanoramicView,
        SmartTracking   //auto framing
    }
}
