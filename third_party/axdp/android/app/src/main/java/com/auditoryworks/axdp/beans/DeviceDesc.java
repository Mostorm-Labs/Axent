package com.auditoryworks.axdp.beans;

import android.hardware.usb.UsbDevice;

import java.util.List;

/**
 * Created by cjb on 2022/11/23
 * Desc :设备描述符特征，VID及PID，设备例如：A20，A15
 */
public final class DeviceDesc {
    public static final int A20_vendorId = 0x1fc9;
    public static final int A20_productId = 0x826b;
    public static final int A20_interfaceNum = 3;

    public static final int S55_vendorId = 0x0627;
    public static final int S55_productId = 0xA6BB;
    public static final int S55_interfaceNum = 2;

//    public static final int Tail_vendorId = 0x1FC9;
//    public static final int Tail_productId = 0x8270;
//    public static final int Tail_interfaceNum = 0;

    public static final int Tail_vendorId = 0x1FC9;
    public static final int Tail_productId = 0x826b;
    public static final int Tail_interfaceNum = 0;

    private int vendorId;
    private int productId;
    private int interfaceNum;

    private DeviceDesc(int vendorId, int productId, int interfaceNum){
        this.vendorId = vendorId;
        this.productId = productId;
        this.interfaceNum = interfaceNum;
    }

    public static DeviceDesc getA20Device(){
        return new DeviceDesc(A20_vendorId, A20_productId, A20_interfaceNum);
    }
    public static boolean isA20Device(UsbDevice device){
        return (device.getVendorId() == A20_vendorId) && (device.getProductId() == A20_productId);
    }

    public static DeviceDesc getS55Device(){
        return new DeviceDesc(S55_vendorId, S55_productId, S55_interfaceNum);
        //return new DeviceDesc(0x1fc9,0x8270, 5);
    }
    public static boolean isS55Device(UsbDevice device){
        return (device.getVendorId() == S55_vendorId) && (device.getProductId() == S55_productId);
    }

    public static DeviceDesc getTailDevice(){
        return new DeviceDesc(Tail_vendorId, Tail_productId, Tail_interfaceNum);
    }
    public static boolean isTailDevice(UsbDevice device){
        return (device.getVendorId() == Tail_vendorId) && (device.getProductId() == Tail_productId);
    }

    public UsbDevice getFirstMatchedDevice(List<UsbDevice> devices){
        UsbDevice dest = null;
        for(UsbDevice device : devices){
            if(device.getVendorId() == vendorId && device.getProductId() == productId){
                dest = device;
                break;
            }
        }
        return dest;
    }

    public int getVendorId() {
        return vendorId;
    }

    public int getProductId() {
        return productId;
    }

    public int getInterfaceNum() {
        return interfaceNum;
    }

    public String getDeviceId(){
        return vendorId+"_"+productId;
    }
}
