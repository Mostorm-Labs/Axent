package com.auditoryworks.axdp;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbInterface;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.auditoryworks.axdp.beans.DeviceDesc;
import com.auditoryworks.axdp.beans.WifiHelper;
import com.auditoryworks.axdp.utils.USBHelper;

import java.nio.charset.StandardCharsets;

public class TailService {
    private static final String TAG = TailService.class.getSimpleName();
    private Context context;
    private USBHelper usbHelper;
    private Handler mHandler;
    private WifiHelper mWifiHelper;
    public TailService(Context context, USBHelper usbHelper) {
        this.context = context;
        this.usbHelper = usbHelper;
        mWifiHelper = new WifiHelper(context);
        mHandler = new Handler(Looper.myLooper());
    }
    private long deviceHandle = 0;
    private int sendWifiSSId() {
        //如果未打开app，则先打开app
        if (!WifiHelper.isApOn(context)){
            mWifiHelper.openAPWithName();
        }
        //发送tail wifi info
        String tailWifiJson = mWifiHelper.getTailWifiJson();
        if (TextUtils.isEmpty(tailWifiJson)){
            Log.d(TAG,"setTailWifiSSId: tailWifiJson empty");
            return -1;
        }
        byte[] tailWifiJsonData = tailWifiJson.getBytes(StandardCharsets.US_ASCII);
        Log.d(TAG,"setTailWifiSSId: tailWifiJson="+tailWifiJson);

        StringBuilder builder = new StringBuilder();
        for (byte b : tailWifiJsonData) {
            builder.append(" ").append(Integer.toHexString(b));
        }
        Log.d(TAG,"setTailWifiSSId: tailWifiJsonData="+builder.toString());

        if (deviceHandle!=0){
            return nativeSetTailWifiSsid(deviceHandle, tailWifiJsonData, tailWifiJsonData.length);
        }else {
            Log.d(TAG,"setTailWifiSSId: device handle 0");
        }
        return -1;
    }

    public void onDeviceAttached(UsbDevice usbDevice) {
        if (DeviceDesc.isTailDevice(usbDevice)){
            openUsbDevice(usbDevice);
        }
    }

    private void openUsbDevice(UsbDevice usbDevice) {
        if (usbDevice != null) {
            Log.v(TAG, ">>>>>>find Tail device, OPEN it:vId=" + usbDevice.getVendorId()+" pId="+usbDevice.getProductId());
            if (usbHelper.checkAndRequestPermission(usbDevice)) {
                if (!openUsbDevice(usbDevice, DeviceDesc.Tail_interfaceNum)) {
                    mHandler.postDelayed(reopenRunnable, 8000);
                }else {
                    int result = sendWifiSSId();
                    if (result!=-1){
                        mHandler.postDelayed(() -> Toast.makeText(context.getApplicationContext(), R.string.tail_connected, Toast.LENGTH_SHORT).show(),1000);
                    }
                    Log.e(TAG,"setTailWifiSSId result="+result);
                }
            }
        } else {
            Log.e(TAG, "NO Tail device exist!!!");
        }
    }
    Runnable reopenRunnable = () -> {
        Log.v(TAG, "rescan Usb Devices.....");
        scanUsbDevices();
    };

    synchronized private boolean openUsbDevice(UsbDevice usbDevice, int interfaceNum) {
        UsbDeviceConnection conn = usbHelper.getUsbManager().openDevice(usbDevice);
        if (conn == null) {
            Log.e(TAG, "openDevice failed for " + usbDevice.getDeviceName());
            return false;
        }
        UsbInterface usbInterface = usbDevice.getInterface(interfaceNum);
        if (!conn.claimInterface(usbInterface, true)) {
            Log.e(TAG, "claimInterface failed for interface " + interfaceNum);
            conn.close();
            return false;
        }
        int fileDescriptor = conn.getFileDescriptor();
        deviceHandle = nativeCreateDevice(fileDescriptor, usbDevice.getVendorId(), usbDevice.getProductId(), interfaceNum);
        Log.v(TAG, "打开 Usb Device:" + (deviceHandle != 0 ? "成功" : "失败"));
        return deviceHandle != 0;
    }

    synchronized private void closeUsbDevice() {
        if (deviceHandle != 0) {
            Log.v(TAG, "close Usb Device:" + deviceHandle);
            nativeDestroyDevice(deviceHandle);
            deviceHandle = 0;
            mHandler.post(() -> Toast.makeText(context.getApplicationContext(), R.string.tail_disconnected, Toast.LENGTH_SHORT).show());
        }
    }

    public void onDeviceDetached(UsbDevice usbDevice) {
        if (DeviceDesc.isTailDevice(usbDevice)){
            closeUsbDevice();
        }
    }

    public void onUsbConnectClosed(boolean closed) {
        mHandler.removeCallbacksAndMessages(null);
        if (closed){
            closeUsbDevice();
        }else {
            scanUsbDevices();
        }
    }

    synchronized private void scanUsbDevices() {
        Log.v(TAG, "scan Usb Devices.....");
        DeviceDesc desc = DeviceDesc.getTailDevice();
        UsbDevice usbDevice = desc.getFirstMatchedDevice(usbHelper.getUsbDevices());
        if (usbDevice != null) {
            Log.v(TAG, ">>>>>>find Tail device, OPEN it:vId=" + usbDevice.getVendorId()+" pId="+usbDevice.getProductId());
            if (usbHelper.checkAndRequestPermission(usbDevice)) {
                if (!openUsbDevice(usbDevice, desc.getInterfaceNum())) {
                    mHandler.postDelayed(reopenRunnable, 8000);
                }else {
                    int result = sendWifiSSId();
                    if (result!=-1){
                        mHandler.postDelayed(() -> Toast.makeText(context.getApplicationContext(), R.string.tail_connected, Toast.LENGTH_SHORT).show(),1000);
                    }
                    Log.e(TAG,"setTailWifiSSId result="+result);
                }
            }
        } else {
            Log.e(TAG, "NO Tail device exist!!!");
        }
    }

    public void start() {
        scanUsbDevices();
    }
    public void stop(){
        closeUsbDevice();
        mHandler.removeCallbacksAndMessages(null);
    }

    public native long nativeCreateDevice(int file_desc, int vid, int pid, int interface_num);
    public native int nativeDestroyDevice(long device_handle);

    public native int nativeSetTailWifiSsid(long device_handle, byte[] ssid, int length);
    private native int nativeGetTailWiFiSsid(long device_handle);
}
