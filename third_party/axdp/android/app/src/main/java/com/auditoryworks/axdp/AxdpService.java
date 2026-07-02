package com.auditoryworks.axdp;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.util.Log;

import com.auditoryworks.axdp.beans.DeviceDesc;
import com.auditoryworks.axdp.utils.USBHelper;
import com.xbh.sdk3.client.UserAPI;

/**
 * Created by cjb on 2022/11/21
 * Desc :与多个USB设备通讯的主服务类
 */
public class AxdpService extends Service {
    static {
        System.loadLibrary("axdp_android");
    }

    //USB设备连接开关，1：连接USB设备  0：断开USB设备连接
    private final String ACTION_USB_CONNECTION_TRIGGER = "action_usb_conn_trigger";

    private final String TAG = "AxdpService";
    private final Object broadcastSync = new Object();
    private Handler mHandler;
    private USBHelper usbHelper;
    //连接到此服务的client列表
    private RemoteCallbackList<MessageListener> clientList = new RemoteCallbackList<>();

    private TailService mTailHelper;
    @Override
    public void onCreate() {
        super.onCreate();
        UserAPI.getInstance().init(this);
        Log.v(TAG, "========>>>AXDP Service onCreate");
        try {
            mBinder.linkToDeath(deathRecipient, 0);
        } catch (Exception e) {
            e.printStackTrace();
        }
        mHandler = new Handler(Looper.myLooper());
        usbHelper = new USBHelper(this);
        usbHelper.setOnFindUsbListener(findUsbListener);
        mTailHelper = new TailService(this,usbHelper);
        mTailHelper.start();

        scanUsbDevices();

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_CONNECTION_TRIGGER);
        registerReceiver(deviceConnectionTriggerReceiver, filter);
    }

    //USB插拨通知
    private final USBHelper.OnFindUsbListener findUsbListener = new USBHelper.OnFindUsbListener() {
        @Override
        public void attached(Intent intent) {
            UsbDevice usbDevice = intent.getExtras().getParcelable(UsbManager.EXTRA_DEVICE);
            Log.v(TAG, "====>>>>>>NEW usb device attached:" + usbDevice);
            Log.v(TAG, "Is a20:" + DeviceDesc.isS55Device(usbDevice));
            if(DeviceDesc.isS55Device(usbDevice)) {
                scanUsbDevices();
            }
            if (DeviceDesc.isTailDevice(usbDevice)){
                mTailHelper.onDeviceAttached(usbDevice);
            }

        }

        @Override
        public void detached(Intent intent) {
            UsbDevice usbDevice = intent.getExtras().getParcelable(UsbManager.EXTRA_DEVICE);
            Log.v(TAG, "usb device detached<<<<<======"+usbDevice);
            if(DeviceDesc.isS55Device(usbDevice)) {
                closeUsbDevice();
            }
            if (DeviceDesc.isTailDevice(usbDevice)){
                mTailHelper.onDeviceDetached(usbDevice);
            }
        }

        @Override
        public void onUsbPermission(UsbDevice usbDevice, boolean granted) {
            if (granted) {
                if (DeviceDesc.isA20Device(usbDevice)) {
                    //            openUsbDevice(usbDevice, DeviceDesc.getA20Device().getInterfaceNum());
                }
            }
        }
    };

    Runnable reopenRunnable = () -> {
        Log.v(TAG, "rescan Usb Devices.....");
        scanUsbDevices();
    };

    synchronized private void scanUsbDevices() {
        Log.v(TAG, "scan Usb Devices.....");
        //DeviceDesc desc = DeviceDesc.getS55Device();
        //DeviceDesc desc = DeviceDesc.getA20Device();
        DeviceDesc desc = DeviceDesc.getTailDevice();
        UsbDevice usbDevice = desc.getFirstMatchedDevice(usbHelper.getUsbDevices());
        if (usbDevice != null) {
            Log.v(TAG, ">>>>>>find S55, OPEN it:" + usbDevice);
            if (usbHelper.checkAndRequestPermission(usbDevice)) {
                if (!openUsbDevice(usbDevice, desc.getInterfaceNum())) {
                    mHandler.postDelayed(reopenRunnable, 8000);
                }
            }
        } else {
            Log.e(TAG, "NO S55 exist!!!");
        }
    }

    synchronized private void closeUsbDevice() {
        if (deviceHandle != 0) {
            Log.v(TAG, "close Usb Device:" + deviceHandle);
            nativeDestroyDevice(deviceHandle);
            deviceHandle = 0;
        }
    }

    private boolean openUsbDevice(UsbDevice usbDevice, int interfaceNum) {
        UsbDeviceConnection conn = usbHelper.getUsbManager().openDevice(usbDevice);
        if (conn == null) {
            Log.e(TAG, "openDevice failed for " + usbDevice.getDeviceName());
            return false;
        }
        // Claim the interface in Java first (force=true detaches kernel driver).
        // This is required before libusb_wrap_sys_device can claim it in native code.
        UsbInterface usbInterface = usbDevice.getInterface(interfaceNum);
        if (!conn.claimInterface(usbInterface, true)) {
            Log.e(TAG, "claimInterface failed for interface " + interfaceNum);
            conn.close();
            return false;
        }
        int fileDescriptor = conn.getFileDescriptor();
        deviceHandle = nativeCreateDevice(fileDescriptor, usbDevice.getVendorId(), usbDevice.getProductId(), interfaceNum);
        Log.v(TAG, "打开 Usb Device:filedesc: " + fileDescriptor + " " + usbDevice.getVendorId() + " " + usbDevice.getProductId() + (deviceHandle != 0 ? "成功" : "失败"));
        return deviceHandle != 0;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.v(TAG, "AXDP Service onDestroy<<<========");
        unregisterReceiver(deviceConnectionTriggerReceiver);
        closeUsbDevice();
        usbHelper.close();
        mTailHelper.stop();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return super.onStartCommand(intent, flags, startId);
    }

    private final BroadcastReceiver deviceConnectionTriggerReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            try {
                if (ACTION_USB_CONNECTION_TRIGGER.equals(intent.getAction())) {
                    int trigger = intent.getIntExtra("trigger", 0);
                    Log.v(TAG, "收到USB连接[开/关]消息：" + trigger);
                    mHandler.removeCallbacks(reopenRunnable);
                    if (trigger == 1) {
                        usbHelper.setOnFindUsbListener(null);
                        closeUsbDevice();
                    } else {
                        usbHelper.setOnFindUsbListener(findUsbListener);
                        scanUsbDevices();
                    }
                    mTailHelper.onUsbConnectClosed(trigger == 1);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    };

//    private void downloadDfuFile(String url, String savePath, String saveFileName,String md5) {
//        // url：下载地址，savePath：下载保存路径，fileName：保存的文件名
//        DownloadUtil downloadUtil = new DownloadUtil();
//        downloadUtil.download(this, url, savePath, saveFileName, new DownloadUtil.OnDownloadListener() {
//            @Override
//            public void onDownloadSuccess() {
//                Log.i(TAG, savePath + saveFileName + "下载完成"+" deviceHandle"+deviceHandle+"  md5="+md5);
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onDfuDownloadState(DownloadUtil.STATE_SUCCESS, 100);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//                //添加下载完成后的升级逻辑,其中DownloadUtil是石工做的
//                File dfuFile = new File(savePath + "/" + saveFileName);
//                String dfuMd5 = DownloadUtil.file2MD5(dfuFile);
//                Log.i(TAG, "dfu md5:" + dfuMd5);
//                if (md5.equals(dfuMd5)) {
//                    if (deviceHandle != 0) {
//                        Log.i(TAG, "nativeFirmwareUpgrade :" + dfuFile.getAbsolutePath());
//                        nativeFirmwareUpgrade(deviceHandle, dfuFile.getAbsolutePath());
//                    }
//                } else {
//                    synchronized (broadcastSync) {
//                        try {
//                            int size = clientList.beginBroadcast();
//                            for (int i = 0; i < size; i++) {
//                                MessageListener listener = clientList.getBroadcastItem(i);
//                                // 某些客户端因异常退出可能会触发异常
//                                listener.onDfuDownloadState(DownloadUtil.STATE_FAILED, 0);
//                            }
//                        } catch (Exception e) {
//                            e.printStackTrace();
//                        }finally {
//                            clientList.finishBroadcast();
//                        }
//                    }
//                }
//            }
//
//            int prevProgress = -1;
//            @Override
//            public void onDownloading(int progress, float download, float total) {
//                if(prevProgress == progress)return;
//                prevProgress = progress;
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onDfuDownloadState(DownloadUtil.STATE_DOWNLOADING, progress);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//
//            }
//
//            @Override
//            public void onDownloadFailed(int status, int time) {
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onDfuDownloadState(DownloadUtil.STATE_FAILED, 0);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//            }
//
//            @Override
//            public void onDownloadSpeed(long speedCount, long download, long total) {
//
//            }
//        });
//    }

//    private void downloadOtaFile(String url, String savePath, String saveFileName,String md5) {
//        // url：下载地址，savePath：下载保存路径，fileName：保存的文件名
//        DownloadUtil downloadUtil = new DownloadUtil();
//        downloadUtil.download(this, url, savePath, saveFileName, new DownloadUtil.OnDownloadListener() {
//            @Override
//            public void onDownloadSuccess() {
//                Log.i(TAG, savePath + saveFileName + "下载完成"+" deviceHandle"+deviceHandle+"  md5="+md5);
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onOtaDownloadState(DownloadUtil.STATE_SUCCESS, 100);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//                //添加下载完成后的升级逻辑,其中DownloadUtil是石工做的
////                File dfuFile = new File(savePath + "/" + saveFileName);
////                String dfuMd5 = DownloadUtil.file2MD5(dfuFile);
////                Log.i(TAG, "OTA md5:" + dfuMd5);
//            }
//
//            float downloadSize = 0.0f;
//            @Override
//            public void onDownloading(int progress, float download, float total) {
//             //   Log.i(TAG, "download:" + download + " total:"+total);
//                if(downloadSize != 0.0f && (download - downloadSize < (2<<16)) ){
//                    return;
//                }
//                downloadSize = download;
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onOtaDownloadState(DownloadUtil.STATE_DOWNLOADING, progress);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//            }
//
//            @Override
//            public void onDownloadFailed(int status, int time) {
//                synchronized (broadcastSync) {
//                    try {
//                        int size = clientList.beginBroadcast();
//                        for (int i = 0; i < size; i++) {
//                            MessageListener listener = clientList.getBroadcastItem(i);
//                            // 某些客户端因异常退出可能会触发异常
//                            listener.onOtaDownloadState(DownloadUtil.STATE_FAILED, 0);
//                        }
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }finally {
//                        clientList.finishBroadcast();
//                    }
//                }
//            }
//
//            @Override
//            public void onDownloadSpeed(long speedCount, long download, long total) {
//
//            }
//        });
//    }

    /******************************************************************************
     * 从native回调数据上来的接口
     ******************************************************************************/
    public void jni_onDeviceInfo(int device_id, int dev_type, String product_name,
                                 String phy_version, String soft_version,
                                 String serial_number, String unique_id) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    // 某些客户端因异常退出可能会触发异常
                    listener.onDeviceInfo(device_id, dev_type, product_name, phy_version,
                            soft_version, serial_number, unique_id);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
    }

    public void jni_onHidCallState(int state) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onHidCallState(state);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
    }

    public void jni_onDeviceEvent(int event) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onDeviceEvent(event);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }

    }

    public void jni_onGetVideoMode(int mode) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetVideoMode(mode);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
        Log.d(TAG,"jni_onGetVideoMode "+mode);

    }

    public void jni_onGetPowerLineFreq(int freq) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetPowerLineFreq(freq);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }

    }

    public void jni_onGetMirrorState(int state) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetMirrorState(state);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }

    }

    public void jni_onGetFlipState(int state) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetFlipState(state);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }

    }

    public void jni_onDfuUpdateState(int state, int extra) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onDfuUpdateState(state, extra);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
    }

    public void jni_onGetDereverationAlgParam(int val) {//val范围:0～100
        Log.d(TAG,"jni_onGetDereverationAlgParam->"+val);
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetDereverationAlgParam(val);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
    }

    public void jni_onGetVideoTrackMode(int mode) {
        synchronized (broadcastSync) {
            try {
                int size = clientList.beginBroadcast();
                for (int i = 0; i < size; i++) {
                    MessageListener listener = clientList.getBroadcastItem(i);
                    listener.onGetVideoTrackMode(mode);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }finally {
                clientList.finishBroadcast();
            }
        }
        Log.d(TAG,"jni_onGetVideoTrackMode="+mode);
    }

    public void jni_onGetTailWifiSSID(String ssid) {
        Log.d(TAG,"jni_onGetTailWifiSSID: ssid="+ssid);

        try {
            int size = clientList.beginBroadcast();
            for (int i = 0; i < size; i++) {
                MessageListener listener = clientList.getBroadcastItem(i);
                listener.onGetTailWifiSSID(ssid);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }finally {
            clientList.finishBroadcast();
        }
    }

    /******************************************************************************
     * AIDL接口
     ******************************************************************************/
    private final IBinder mBinder = new AxdpStub();

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    IBinder.DeathRecipient deathRecipient = new IBinder.DeathRecipient() {
        @Override
        public void binderDied() {
            mBinder.unlinkToDeath(this,0);
            Log.e(TAG, "binderDied");
        }
    };

    public class AxdpStub extends AxdpInterface.Stub {
        @Override
        public boolean hasSoundbar() throws RemoteException {
            DeviceDesc desc = DeviceDesc.getS55Device();
            UsbDevice usbDevice = desc.getFirstMatchedDevice(usbHelper.getUsbDevices());
            return usbDevice!=null;
        }

        @Override
        public boolean isActive() throws RemoteException {
            return deviceHandle!=0;
        }

        @Override
        public void register(MessageListener listener) throws RemoteException {
            clientList.register(listener);
            Log.e(TAG, "[register]client count:" + clientList.getRegisteredCallbackCount());
        }

        @Override
        public void unregister(MessageListener listener) throws RemoteException {
            clientList.unregister(listener);
            Log.e(TAG, "[unregister]client count:" + clientList.getRegisteredCallbackCount());
        }

        @Override
        public void getDeviceInfo() throws RemoteException {
            if (deviceHandle != 0){
                nativeGetDeviceInfo(deviceHandle);
            }
        }

        @Override
        public void setHidCallState(boolean enabled) throws RemoteException {
            if (deviceHandle != 0) nativeSetHidCallState(deviceHandle, enabled);
        }

        @Override
        public void getHidCallState() throws RemoteException {
            if (deviceHandle != 0) nativeGetHidCallState(deviceHandle);
        }

        @Override
        public void answer() throws RemoteException {
            if (deviceHandle != 0) nativeSetDeviceHookOff(deviceHandle, true);
        }

        @Override
        public void hangUp() throws RemoteException {
            if (deviceHandle != 0) nativeSetDeviceHookOff(deviceHandle, false);
        }

        @Override
        public void setDeviceMute(boolean mute) throws RemoteException {
            if (deviceHandle != 0) nativeSetDeviceMute(deviceHandle, mute);
        }

        @Override
        public void getPowerLineFreq() throws RemoteException {
            if (deviceHandle != 0) nativeGetPowerLineFreq(deviceHandle);
        }

        @Override
        public void setPowerLineFreq(int freq_type) throws RemoteException {
            if (deviceHandle != 0) nativeSetPowerLineFreq(deviceHandle, freq_type);
        }

        @Override
        public void getVideoMode() throws RemoteException {
            if (deviceHandle != 0) nativeGetVideoMode(deviceHandle);
        }

        @Override
        public void setVideoMode(int video_mode) throws RemoteException {
            if (deviceHandle != 0) nativeSetVideoMode(deviceHandle, video_mode);
        }

        @Override
        public void getReverberationSuppressionLevel() throws RemoteException {
            if (deviceHandle != 0) nativeGetReverberationSuppressionLevel(deviceHandle);
        }

        @Override
        public void setReverberationSuppressionLevel(int degree_level) throws RemoteException {
            if (deviceHandle != 0)
                nativeSetReverberationSuppressionLevel(deviceHandle, degree_level);
        }

        @Override
        public void getMirrorState() throws RemoteException {
            if (deviceHandle != 0) nativeGetMirrorState(deviceHandle);
        }

        @Override
        public void setMirrorState(int enable_state) throws RemoteException {
            if (deviceHandle != 0) nativeSetMirrorState(deviceHandle, enable_state);
        }

        @Override
        public void getFlipState() throws RemoteException {
            if (deviceHandle != 0) nativeGetFlipState(deviceHandle);
        }

        @Override
        public void setFlipState(int enable_state) throws RemoteException {
            if (deviceHandle != 0) nativeSetFlipState(deviceHandle, enable_state);
        }

        @Override
        public void firmwareUpgrade(String dfu_file) throws RemoteException {
            if (deviceHandle != 0) nativeFirmwareUpgrade(deviceHandle, dfu_file);
        }

//        @Override
//        public void downloadFirmware(String download_url, String save_path, String save_file_name, String md5) throws RemoteException {
//            downloadDfuFile(download_url, save_path, save_file_name,md5);
//        }
//
//        @Override
//        public void downloadAndroidOTA(String download_url, String save_path, String save_file_name, String md5) throws RemoteException {
//            downloadOtaFile(download_url, save_path, save_file_name,md5);
//        }

        @Override
        public void getDereverationAlgParam() throws RemoteException {
            if (deviceHandle != 0) nativeGetDereverationAlgParam(deviceHandle);
        }

        @Override
        public void setDereverationAlgParam(int val) throws RemoteException {
            if (deviceHandle != 0) nativeSetDereverationAlgParam(deviceHandle, val);
        }

        @Override
        public void getVideoTrackMode() throws RemoteException {
            if (deviceHandle != 0) nativeGetVideoTrackMode(deviceHandle);
        }

        @Override
        public void setVideoTrackMode(int video_mode) throws RemoteException {
            if (deviceHandle!=0) nativeSetVideoTrackMode(deviceHandle,video_mode);
        }
    }

    /******************************************************************************
     * Native接口
     ******************************************************************************/
    private long deviceHandle = 0;

    public native long nativeCreateDevice(int file_desc, int vid, int pid, int interface_num);

    public native int nativeDestroyDevice(long device_handle);

    private native int nativeGetDeviceInfo(long device_handle);

    private native int nativeSyncGetDeviceInfo(long device_handle);

    private native int nativeSetHidCallState(long device_handle, boolean enabled);

    private native int nativeGetHidCallState(long device_handle);

    private native int nativeSetDeviceHookOff(long device_handle, boolean off);

    private native int nativeSetDeviceMute(long device_handle, boolean mute);

    private native int nativeGetPowerLineFreq(long device_handle);

    private native int nativeSetPowerLineFreq(long device_handle, int freq_type);

    private native int nativeGetVideoMode(long device_handle);

    private native int nativeSetVideoMode(long device_handle, int video_mode);

    private native int nativeGetReverberationSuppressionLevel(long device_handle);

    private native int nativeSetReverberationSuppressionLevel(long device_handle, int degree_level);

    private native int nativeGetMirrorState(long device_handle);

    private native int nativeSetMirrorState(long device_handle, int enable_state);

    private native int nativeGetFlipState(long device_handle);

    private native int nativeSetFlipState(long device_handle, int enable_state);

    private native int nativeFirmwareUpgrade(long device_handle, String dfu_file);

    private native int nativeGetDereverationAlgParam(long device_handle);

    private native int nativeSetDereverationAlgParam(long device_handle, int val_0_100);

    private native int nativeGetVideoTrackMode(long device_handle);

    private native int nativeSetVideoTrackMode(long device_handle, int val_0_100);

}
