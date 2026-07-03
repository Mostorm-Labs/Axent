package com.auditoryworks.axdplib;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.auditoryworks.axdp.AxdpInterface;
import com.auditoryworks.axdp.DeviceEvent;
import com.auditoryworks.axdp.MessageData;
import com.auditoryworks.axdp.MessageListener;

/**
 * Created by cjb 2022-11-28
 * Desc :
 */
public class DeviceServiceImpl implements IDeviceService, IBinder.DeathRecipient {
    private final String TAG = "AxdpServiceImpl";
    private AxdpInterface axdpInterface = null;
    private Context mContext;
    private IDeviceMessageListener devMessageListener = null;

    public DeviceServiceImpl(Context context){
        mContext = context;
    }

    private final ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.i(TAG, "onServiceConnected name:" + name);
            axdpInterface = AxdpInterface.Stub.asInterface(service);
            if (axdpInterface != null) {
                try {
                    Log.e(TAG, "onServiceConnected registerListener");
                    axdpInterface.asBinder().linkToDeath(DeviceServiceImpl.this,0);
                    axdpInterface.register(messageListener);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            if (devMessageListener!=null){
                devMessageListener.onServiceConnected();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.i(TAG, "onServiceDisconnected name:" + name);
            if (axdpInterface != null) {
                try {
                    axdpInterface.unregister(messageListener);
                } catch (Exception e) {
                    e.printStackTrace();
                }
                axdpInterface.asBinder().unlinkToDeath(DeviceServiceImpl.this,0);
                axdpInterface = null;
            }
            if (devMessageListener!=null){
                devMessageListener.onServiceDisconnected();
            }
        }
    };

    private final MessageListener messageListener = new MessageListener.Stub() {
        @Override
        public void onGetMessage(MessageData data) throws RemoteException {

        }

        @Override
        public void onDeviceInfo(int device_id, int dev_type, String product_name,
                                 String phy_version, String soft_version,
                                 String serial_number, String unique_id) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onDeviceInfo(device_id,dev_type,product_name,
                        phy_version,soft_version,serial_number,unique_id);
            }
        }

        @Override
        public void onHidCallState(int state) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onHidCallState(state == 0 ?
                        DeviceEvent.HidCallState.Disabled : DeviceEvent.HidCallState.Enabled);
            }
        }

        @Override
        public void onDeviceEvent(int event) throws RemoteException {
            for (DeviceEvent.HidEvent hidEvent : DeviceEvent.HidEvent.values()) {
                if(event == hidEvent.ordinal()){
                    if(devMessageListener != null){
                        devMessageListener.onDeviceEvent(hidEvent);
                    }
                    break;
                }
            }
        }

        @Override
        public void onGetVideoMode(int mode) throws RemoteException {
            for (DeviceEvent.VideoMode videoMode : DeviceEvent.VideoMode.values()) {
                if(mode == videoMode.ordinal()){
                    if(devMessageListener != null){
                        devMessageListener.onVideoMode(videoMode);
                    }
                    break;
                }
            }
        }

        @Override
        public void onGetPowerLineFreq(int freq) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onGetPowerLineFreq(freq);
            }
        }

        @Override
        public void onGetMirrorState(int state) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onGetMirrorState(state);
            }
        }

        @Override
        public void onGetFlipState(int state) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onGetFlipState(state);
            }
        }

        @Override
        public void onDfuUpdateState(int state, int extra) throws RemoteException {
            for (DeviceEvent.UpgradeState upgradeState : DeviceEvent.UpgradeState.values()) {
                if(state == upgradeState.ordinal()){
                    if(devMessageListener != null){
                        devMessageListener.onDfuUpdateState(upgradeState, extra);
                    }
                    break;
                }
            }
        }

        @Override
        public void onGetDereverationAlgParam(int val) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onGetDereverationAlgParam(val);
            }
        }

        @Override
        public void onGetVideoTrackMode(int mode) throws RemoteException {
            if(devMessageListener != null){
                devMessageListener.onGetVideoTrackMode(mode);
            }
        }

        @Override
        public void onGetTailWifiSSID(String ssid) throws RemoteException {

        }
    };

    @Override
    public boolean connect() {
        try{
            Intent serviceIntent = new Intent();
            serviceIntent.setClassName("com.auditoryworks.axdp", "com.auditoryworks.axdp.AxdpService");
            return mContext.bindService(serviceIntent, mConnection, Context.BIND_AUTO_CREATE);
        }catch (Exception e){
            Log.e(TAG, "[cjb]ERROR connect:" + e.toString());
        }
        return false;
    }

    @Override
    public void disconnect() {
        try {
            if (axdpInterface!=null){
                axdpInterface.unregister(messageListener);
            }
            if (mConnection!=null){
                mContext.unbindService(mConnection);
            }
        }catch (Exception e){
            Log.e(TAG, "[cjb]ERROR disconnect:" + e.toString());
        }
    }

    @Override
    public boolean isConnected() {
        return axdpInterface != null;
    }

    @Override
    public void setMessageListener(IDeviceMessageListener listener) {
        this.devMessageListener = listener;
    }

    @Override
    public boolean hasSoundbar() {
        try{
            if (isConnected()){
                return axdpInterface.hasSoundbar();
            }
        }catch (Exception e){
            e.printStackTrace();
        }
        return false;
    }

    @Override
    public boolean isActive() {
        try{
            if (isConnected()){
                return axdpInterface.isActive();
            }
        }catch (Exception e){
            e.printStackTrace();
        }
        return false;
    }

    @Override
    public void getDeviceInfo() {
        try {
            if(isConnected())axdpInterface.getDeviceInfo();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setHidCallState(boolean enabled) {
        try {
            if(isConnected())axdpInterface.setHidCallState(enabled);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getHidCallState() {
        try {
            if(isConnected())axdpInterface.getHidCallState();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void answer() {
        try {
            if(isConnected())axdpInterface.answer();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void hangUp() {
        try {
            if(isConnected())axdpInterface.hangUp();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setDeviceMute(boolean mute) {
        try {
            if(isConnected())axdpInterface.setDeviceMute(mute);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getPowerLineFreq() {
        try {
            if(isConnected())axdpInterface.getPowerLineFreq();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setPowerLineFreq(int freq_type) {
        try {
            if(isConnected())axdpInterface.setPowerLineFreq(freq_type);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getVideoMode() {
        try {
            if(isConnected())axdpInterface.getVideoMode();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setVideoMode(int video_mode) {
        try {
            if(isConnected())axdpInterface.setVideoMode(video_mode);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getReverberationSuppressionLevel() {
        try {
            if(isConnected())axdpInterface.getReverberationSuppressionLevel();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setReverberationSuppressionLevel(int degree_level) {
        try {
            if(isConnected())axdpInterface.setReverberationSuppressionLevel(degree_level);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getMirrorState() {
        try {
            if(isConnected())axdpInterface.getMirrorState();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setMirrorState(int enable_state) {
        try {
            if(isConnected())axdpInterface.setMirrorState(enable_state);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getFlipState() {
        try {
            if(isConnected())axdpInterface.getFlipState();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setFlipState(int enable_state) {
        try {
            if(isConnected())axdpInterface.setFlipState(enable_state);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getDereverationAlgParam() {
        try {
            if(isConnected())axdpInterface.getDereverationAlgParam();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setDereverationAlgParam(int val) {
        try {
            if(isConnected())axdpInterface.setDereverationAlgParam(val);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void getVideoTrackMode() {
        try {
            if(isConnected())axdpInterface.getVideoTrackMode();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void setVideoTrackMode(int mode) {
        try {
            if(isConnected())axdpInterface.setVideoTrackMode(mode);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void firmwareUpgrade(String dfu_file) {
        try {
            if(isConnected())axdpInterface.firmwareUpgrade(dfu_file);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void binderDied() {
        axdpInterface = null;
        axdpInterface.asBinder().unlinkToDeath(this,0);
        connect();
        Log.d(TAG,"binderDied-> try to connect");
    }
}
