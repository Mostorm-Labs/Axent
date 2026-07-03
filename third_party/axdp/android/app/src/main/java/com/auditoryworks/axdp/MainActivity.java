package com.auditoryworks.axdp;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.hardware.usb.UsbManager;
import android.util.Log;

import com.auditoryworks.axdp.databinding.ActivityMainBinding;
import com.auditoryworks.axdp.utils.USBHelper;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'a20hid' library on application startup.
    static {
        System.loadLibrary("axdp_android");
    }

    private ActivityMainBinding binding;
    private Button btn_connect;
    private long device_handle = 0;
    private UsbManager usbManager;
    private USBHelper usbHelper;
    private UsbDeviceConnection usbDeviceConnection;
    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private Handler handler;
    private boolean mute_state = true;
    private boolean hook_state = true;

    private static String[] PERMISSIONS_STORAGE = {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE",
            "android.permission.MANAGE_EXTERNAL_STORAGE",
            Manifest.permission.CAMERA,
            "com.android.example.USB_PERMISSION"
    };
    public static final int DEVICE_MUTE = 0;
    public static final int DEVICE_UNMUTE = 1;
    public static final int DEVICE_ANSWER = 2;
    public static final int DEVICE_HANG_UP = 3;
    public static final int DEVICE_VOL_DOWN = 4;
    public static final int DEVICE_VOL_UP = 5;

    enum EventType{
        DEVICE_MUTE,
        DEVICE_UNMUTE,
        DEVICE_ANSWER,
        DEVICE_HANG_UP,
        DEVICE_VOL_DOWN,
        DEVICE_VOL_UP
    }

    public static void verifyStoragePermissions(Activity activity) {
        try {
            //检测是否有写的权限
            int permission = ActivityCompat.checkSelfPermission(activity,
                    "android.permission.WRITE_EXTERNAL_STORAGE");
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE,REQUEST_EXTERNAL_STORAGE);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        verifyStoragePermissions(this);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        usbHelper=new USBHelper(this);
        // Example of a call to a native method
        TextView tv = binding.sampleText;
        tv.setText(stringFromJNI());

        btn_connect= findViewById(R.id.button);

        handler = new Handler(getMainLooper()){
            @Override
            public void handleMessage(Message msg){
              switch (msg.what){
                  case DEVICE_MUTE://0:
                  {
                      tv.setText("DEVICE_MUTE");
                      break;
                  }
                  case DEVICE_UNMUTE://1:
                  {
                      tv.setText("DEVICE_UNMUTE");
                      break;
                  }
                  case DEVICE_ANSWER://2:
                  {
                      tv.setText("DEVICE_ANSWER");
                      break;
                  }
                  case DEVICE_HANG_UP://3:
                  {
                      tv.setText("DEVICE_HANG_UP");
                      break;
                  }
                  case DEVICE_VOL_DOWN://4
                  {
                      tv.setText("DEVICE_VOL_DOWN");
                      break;
                  }
                  case DEVICE_VOL_UP://5
                  {
                      tv.setText("DEVICE_VOL_UP");
                      break;
                  }
              }
          }
        };

        //open device when open app
        findViewById(R.id.button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //confirm device is closed and reopen
                if (device_handle != 0){
                    closeDeviceInterface();
                }
                int ret = openDeviceInterface();
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Open Device Successfully");
                    getDeviceInfo(device_handle);
                    getHidCallState(device_handle);
                }else {
                    tv.setText("Open Device Failed, ret = " + ret);
                }
            }
        });
        findViewById(R.id.button2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int ret = setDeviceHookOff(device_handle, false);
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Set device hook off success.");
                }else {
                    tv.setText("Set device hook off failed.");
                }
            }
        });
        findViewById(R.id.button3).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int ret = setDeviceHookOff(device_handle, true);
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Set device hook off success.");
                }else {
                    tv.setText("Set device hook off failed.");
                }
            }
        });
        findViewById(R.id.button7).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int ret = setDeviceMute(device_handle, true);
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Set device mute success.");
                }else {
                    tv.setText("Set device mute failed.");
                }
            }
        });
        findViewById(R.id.button8).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int ret = setDeviceMute(device_handle, false);
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Set device unmute success.");
                }else {
                    tv.setText("Set device unmute failed.");
                }
            }
        });
        findViewById(R.id.button9).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                closeDeviceInterface();
                TextView tv = binding.sampleText;
                tv.setText("Device Closed");
            }
        });
        findViewById(R.id.button4).setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                int ret  = getDeviceInfo(device_handle);
                ret = syncGetDeviceInfo(device_handle);
                TextView tv = binding.sampleText;
                if (ret == 0){
                    tv.setText("Get Device Info...Printed in logcat");
                }else {
                    tv.setText("Get Device Info Failed, please Connect device or check device usb connection");
                }
            }
        });
        findViewById(R.id.button6).setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                int ret = setHidCallState(device_handle, true);
                TextView tv = binding.sampleText;
                if (ret == 0) {
                    tv.setText("Set Device Hid Call Enabled successfully");
                } else {
                    tv.setText("Set Device Hid Call Enabled failed, please Connect device or check device usb connection");
                }
            }
        });
        findViewById(R.id.button10).setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                int ret = setHidCallState(device_handle, false);
                TextView tv = binding.sampleText;
                if (ret == 0) {
                    tv.setText("Set Device Hid Call Disabled successfully");
                } else {
                    tv.setText("Set Device Hid Call Disabled failed, please Connect device or check device usb connection");
                }
            }
        });
        findViewById(R.id.button11).setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                String ssid = "\"{\\\"b\\\":\\\"80:69:1A:39:EB:20\\\",\\\"p\\\":\\\"Disc1234\\\",\\\"s\\\":\\\"washeng-302\\\"}\"";
                int ret = setTailWiFiSSID(device_handle, ssid, ssid.length());
                TextView tv = binding.sampleText;
                if (ret == 0) {
                    tv.setText("Set Device TAil SSID successfully");
                } else {
                    tv.setText("Set Device Device TAil SSID failed, please Connect device or check device usb connection");
                }
            }
        });
    }

    @Override
    protected  void onDestroy(){
        closeDeviceInterface();
        super.onDestroy();
    }

    public int openDeviceInterface() {
        //--> Obtain USB permissions over the android.hardware.usb.UsbManager class
        UsbDevice usbDevice = usbHelper.getUsbDevice(0x0627, 0xA6A0);
        usbManager = usbHelper.getUsbManager(); //(UsbManager) getSystemService(Context.USB_SERVICE);
        int ret = usbHelper.requestPermission(usbDevice);
        if (ret > 0) {
            usbDeviceConnection = usbManager.openDevice(usbDevice);
            int fileDescriptor = usbDeviceConnection.getFileDescriptor();
            device_handle = createDeviceInterface(fileDescriptor);
        } else {
            return -3;
        }
        return device_handle == 0 ? -1 : 0;
    }


    protected void closeDeviceInterface(){
        if (device_handle == 0)
            return;
        destroyDeviceInterface(device_handle);
        //usbHelper.close();
        device_handle = 0;
    }

    public int onDeviceEventNotify(int event){
        Message msg = new Message();
        msg.what = event;
        handler.sendMessage(msg);
        switch(event) {
            case 0://DEVICE_MUTE:
            {
                Log.d("[HIDNotify]", "DEVICE_MUTE\r\n");
                break;
            }
            case 1://DEVICE_UNMUTE:
            {
                Log.d("[HIDNotify]", "DEVICE_UNMUTE\r\n");
                break;
            }
            case 2://DEVICE_ANSWER:
            {
                Log.d("[HIDNotify]", "DEVICE_ANSWER\r\n");
                break;
            }
            case 3://DEVICE_HANG_UP:
            {
                Log.d("[HIDNotify]", "DEVICE_HANG_UP\r\n");
                break;
            }
            case 4://DEVICE_VOL_DOWN
            {
                Log.d("[HIDNotify]", "DEVICE_VOL_DOWN\r\n");
                break;
            }
            case 5://DEVICE_VOL_UP
            {
                Log.d("[HIDNotify]", "DEVICE_VOL_UP\r\n");
                break;
            }
        }
        return 0;
    }

    /**
     * A native method that is implemented by the 'a20hid' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native long createDeviceInterface(int file_desc);

    public native int getDeviceInfo(long device_handle);

    public native int syncGetDeviceInfo(long device_handle);

    public native int setHidCallState(long device_handle, boolean enabled);

    public native int getHidCallState(long device_handle);

    public native int setDeviceHookOff(long device_handle, boolean off);

    public native int setDeviceMute(long device_handle, boolean mute);

    public native int destroyDeviceInterface(long device_handle);

//    tail
    public native int setTailWiFiSSID(long device_handle, String ssid, int len);

    public native int getTailWiFiSSID(long device_handle);


}