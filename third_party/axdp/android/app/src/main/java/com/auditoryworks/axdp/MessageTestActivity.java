package com.auditoryworks.axdp;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

import com.auditoryworks.axdp.beans.WifiHelper;
import com.auditoryworks.axdp.databinding.ActivityMessageTestBinding;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

/**
 * 关于axdp服务接口库aar说明：
 * 1.去除manifest下MessageTestActivity的intent-filter属性，并编译APK提供集成，作为一个服务
 * 2.将三个aidl文件复盖到hidDemo项目下的axdplib对应的aidl文件，实现对应的新接口，重新打包axdplib成aar, 提供给settings来使用。
 * 3.测试：将步骤2的aar复制到hidDemo下的app的libs,编译测试即可。或者直接在axdp项目的MessageTestActivity测试即可。
 * */

public class MessageTestActivity extends AppCompatActivity {
    private final String TAG = "TestActivity";
    private ActivityMessageTestBinding binding;
    private boolean callState = false;
    DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.CHINA);

    AxdpInterface axdpInterface = null;
    ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.i(TAG, "onServiceConnected name:" + name);
            axdpInterface = AxdpInterface.Stub.asInterface(service);
            if (axdpInterface != null) {
                try {
                    Log.e(TAG, "onServiceConnected registerListener");
                    axdpInterface.register(messageListener);
                } catch (Exception e) {
                    e.printStackTrace();
                }
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
            }
        }
    };
    public void initService() {
        Log.e(TAG, "initService");
        //可放在开机启动广播执行
        Intent intent = new Intent();
        intent.setClassName("com.auditoryworks.axdp", "com.auditoryworks.axdp.AxdpService");
        startService(intent);

        //每个第三方应用，要访问A20都要执行的地方
        Intent serviceIntent = new Intent();
        serviceIntent.setAction("com.auditoryworks.axdp.AxdpService");
        serviceIntent.setPackage("com.auditoryworks.axdp");
        bindService(serviceIntent, mConnection, Context.BIND_AUTO_CREATE);
    }
    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.MANAGE_EXTERNAL_STORAGE,
            Manifest.permission.CAMERA,
            "com.android.example.USB_PERMISSION"
    };
    private void verifyStoragePermissions() {
        try {
            //检测是否有写的权限
            int permission = ActivityCompat.checkSelfPermission(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE);
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE, 1);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        verifyStoragePermissions();
        binding = ActivityMessageTestBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        //cjb remark: 按他测试demo逻辑，activity启动时先申请USB访问权限，插入A20时再申请访问camera权限，因为设备有可能是音视频设备
        initService();

        findViewById(R.id.btn_clear_screen).setOnClickListener(v -> binding.etDebug.setText(""));
        findViewById(R.id.btn_call_state).setOnClickListener(v -> {
            try {
                axdpInterface.setHidCallState(callState);
                callState = !callState;
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_device_info).setOnClickListener(v -> {
            try {
                axdpInterface.getDeviceInfo();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_mute).setOnClickListener(v -> {
            try {
                axdpInterface.setDeviceMute(true);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_video_mode).setOnClickListener(v -> {
            try {
                axdpInterface.getVideoMode();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_power_line_freq).setOnClickListener(v -> {
            try {
                axdpInterface.getPowerLineFreq();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_mirror_state).setOnClickListener(v -> {
            try {
                axdpInterface.getMirrorState();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_flip_state).setOnClickListener(v -> {
            try {
                axdpInterface.getFlipState();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_upgrade).setOnClickListener(v -> {
            try {
                axdpInterface.firmwareUpgrade("/sdcard/s55_221112.bin");
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
//        findViewById(R.id.btn_download_dfu).setOnClickListener(v -> {
//            try {
//                String downloadUrl = "https://ota-public.nearhub.co/dfu/C30/General_MC_[Hamedal_C30R]_V1.100.0.1.R.221126.bin";
//                axdpInterface.downloadFirmware(downloadUrl, "/sdcard/", "dfu_fm3.bin", "57F56267593F2C9E0F96F2514F4D7724");
//            } catch (RemoteException e) {
//                e.printStackTrace();
//            }
//        });
//        findViewById(R.id.btn_download_ota).setOnClickListener(v -> {
//            try {
//                String downloadUrl = "https://ota-public.nearhub.co/ota/S55/1055/XMA311D2H.zip";
//                axdpInterface.downloadAndroidOTA(downloadUrl, "/sdcard/", "s55-ota.zip", "57F56267593F2C9E0F96F2514F4D7724");
//            } catch (RemoteException e) {
//                e.printStackTrace();
//            }
//        });
        findViewById(R.id.btn_get_dereveration_alg).setOnClickListener(v -> {
            try {
                axdpInterface.getDereverationAlgParam();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_set_dereveration_alg).setOnClickListener(v -> {
            try {
                EditText et = findViewById(R.id.et_dereveration);
                String value = et.getText().toString();
                if(TextUtils.isEmpty(value)) {
                    Toast.makeText(this, "请在旁边输入框填写值：0～100",Toast.LENGTH_SHORT).show();
                    return;
                }
                int val = Integer.parseInt(value);
                axdpInterface.setDereverationAlgParam(val);
            } catch (Exception e) {
                Log.e(TAG, "setDereverationAlgParam ERROR:" + e.toString());
                Toast.makeText(this, "设置混响失败：只接受数字",Toast.LENGTH_SHORT).show();
            }
        });
        findViewById(R.id.btn_get_video_track_mode).setOnClickListener(v -> {
            try {
                axdpInterface.getVideoTrackMode();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
        findViewById(R.id.btn_set_video_track_mode).setOnClickListener(v -> {
            try {
                EditText et = findViewById(R.id.et_video_track_mode);
                String value = et.getText().toString();
                if(TextUtils.isEmpty(value)) {
                    Toast.makeText(this, "请输入追踪模式值：0～3",Toast.LENGTH_SHORT).show();
                    return;
                }
                axdpInterface.setVideoTrackMode(Integer.parseInt(value));
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        });
    }

    @Override
    protected  void onDestroy(){
        Log.e(TAG, ">>>>>axdp test activity onDestroy");
        unbindService(mConnection);
        super.onDestroy();
    }

    private void appendDebugInfo(String text){
        runOnUiThread(() -> {
            binding.etDebug.append("\n" + text);
            binding.svContainer.post(()->binding.svContainer.smoothScrollTo(0, binding.etDebug.getBottom()));
        });
    }

    private MessageListener messageListener = new MessageListener.Stub() {
        @Override
        public void onDfuUpdateState(int state, int extra) throws RemoteException {
            String info = String.format(Locale.CHINA,"[%s]DFU Update state:%d extra:%d%s",
                    dateFormat.format(new Date()), state, extra, (extra >= 0?"%":""));
            if(state == 0){
                info += "数据准备完成，向设备请求升级";
            }else if(state == 2){
                info += "传输完成，等待数据校验和确认";
            }else if(state == 3){
                info += "设备升级成功，等待重启";
            }else if(state == 4){
                info += "设备升级失败！数据校验错误或其他问题";
            }
            appendDebugInfo(info);
        }

//        @Override
//        public void onDfuDownloadState(int state, int extra) throws RemoteException {
//            String info = String.format(Locale.CHINA,"[%s]DFU download state:%d extra:%d%s",
//                    dateFormat.format(new Date()), state, extra, (extra >= 0?"%":""));
//            if(state == 0){
//                info += " 数据下载完成";
//            }else if(state == 1){
//                info += " 正在下载";
//            }else if(state == 2){
//                info += " 下载失败";
//            }
//            appendDebugInfo(info);
//        }

//        @Override
//        public void onOtaDownloadState(int state, int extra) throws RemoteException {
//            String info = String.format(Locale.CHINA,"[%s]OTA download state:%d extra:%d%s",
//                    dateFormat.format(new Date()), state, extra, (extra >= 0?"%":""));
//            if(state == 0){
//                info += " 数据下载完成";
//            }else if(state == 1){
//                info += " 正在下载";
//            }else if(state == 2){
//                info += " 下载失败";
//            }
//            appendDebugInfo(info);
//        }

        @Override
        public void onGetMessage(MessageData data) throws RemoteException {

        }

        @Override
        public void onDeviceInfo(int device_id, int dev_type, String product_name,
                                 String phy_version, String soft_version,
                                 String serial_number, String unique_id) throws RemoteException {
            String info = "[onDeviceInfo]\nproduct_name:"+product_name+ "\nunique_id:"+unique_id +
                    "\ndevType:"+dev_type + "\nphy_version:"+phy_version + "\nsoft_version:" + soft_version
                    +"\nserial_number:" + serial_number + "\ndevice_id:"+device_id;
            Log.e(TAG, info);
            appendDebugInfo(info);
        }

        @Override
        public void onHidCallState(int state) throws RemoteException {
            appendDebugInfo("HidCallState:" + (state == 0 ? "Disabled":"Enabled"));
        }

        @Override
        public void onDeviceEvent(int event) throws RemoteException {
            for (DeviceEvent.HidEvent hidEvent : DeviceEvent.HidEvent.values()) {
                if(event == hidEvent.ordinal()){
                    appendDebugInfo("GET device event:" + hidEvent.toString());
                    break;
                }
            }
        }

        @Override
        public void onGetVideoMode(int mode) throws RemoteException {
            appendDebugInfo("GET VideoMode:" + mode);
        }

        @Override
        public void onGetPowerLineFreq(int freq) throws RemoteException {
            appendDebugInfo("GET PowerLineFreq:" + freq);
        }

        @Override
        public void onGetMirrorState(int state) throws RemoteException {
            appendDebugInfo("GET MirrorState:" + state);
        }

        @Override
        public void onGetFlipState(int state) throws RemoteException {
            appendDebugInfo("GET FlipState:" + state);
        }

        @Override
        public void onGetDereverationAlgParam(int val) throws RemoteException {
            appendDebugInfo("GET DereverationAlgParam:" + val);
        }

        @Override
        public void onGetVideoTrackMode(int mode) throws RemoteException {
            appendDebugInfo("GET VideoTrackMode:" + mode);
        }

        @Override
        public void onGetTailWifiSSID(String ssid) throws RemoteException {

        }
    };
}