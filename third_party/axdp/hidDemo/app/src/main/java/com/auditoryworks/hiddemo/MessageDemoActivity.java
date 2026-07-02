package com.auditoryworks.hiddemo;

import android.Manifest;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;


import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import com.auditoryworks.axdp.DeviceEvent;
import com.auditoryworks.axdplib.DeviceServiceImpl;
import com.auditoryworks.axdplib.IDeviceMessageListener;
import com.auditoryworks.axdplib.IDeviceService;


public class MessageDemoActivity extends AppCompatActivity {
    private final String TAG = "MessageDemoActivity";
    private TextView etDebug;
    private ScrollView svContainer;

    private boolean callState = false;

    IDeviceService deviceService = null;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_message_test);
        etDebug = (TextView)findViewById(R.id.et_debug);
        svContainer= (ScrollView)findViewById(R.id.sv_container);


        findViewById(R.id.btn_connect).setOnClickListener(v -> {
            if(deviceService.isConnected()){
                Log.v(TAG, "click disconnect<<<<");
                deviceService.disconnect();
                ((Button)findViewById(R.id.btn_connect)).setText("connect(X)");
            }else{
                Log.v(TAG, "click connecting>>>>");
                deviceService.connect();
                ((Button)findViewById(R.id.btn_connect)).setText("connect(√)");
            }
        });
        findViewById(R.id.btn_call_state).setOnClickListener(v -> {
            deviceService.setHidCallState(callState);
            callState = !callState;
        });
        findViewById(R.id.btn_device_info).setOnClickListener(v -> {
            deviceService.getDeviceInfo();
        });
        findViewById(R.id.btn_mute).setOnClickListener(v -> {
            deviceService.setDeviceMute(true);
        });
        findViewById(R.id.btn_video_mode).setOnClickListener(v -> {
            deviceService.getVideoMode();
        });
        findViewById(R.id.btn_get_video_track_mode).setOnClickListener(v -> {
            deviceService.getVideoTrackMode();
        });
        findViewById(R.id.btn_video_mode).setOnClickListener(v -> {
            deviceService.getVideoMode();
        });
        findViewById(R.id.btn_get_power_line_freq).setOnClickListener(v -> {
            deviceService.getPowerLineFreq();
        });
        findViewById(R.id.btn_get_mirror_state).setOnClickListener(v -> {
            deviceService.getMirrorState();
        });
        findViewById(R.id.btn_get_flip_state).setOnClickListener(v -> {
            deviceService.getFlipState();
        });
        findViewById(R.id.btn_upgrade).setOnClickListener(v -> {
//            registerForActivityResult(new ActivityResultContracts.RequestPermission(), new ActivityResultCallback<Boolean>() {
//                @Override
//                public void onActivityResult(Boolean result) {
//                    Log.v("cjb", "RequestPermission result:" + result);
//                }
//            }).launch(Manifest.permission_group.STORAGE);
            deviceService.firmwareUpgrade("/sdcard/s55_221112.bin");
        });
        findViewById(R.id.btn_download_bin).setOnClickListener(v -> {
            try {
                String downloadUrl = "https://ota-public.nearhub.co/dfu/C30/General_MC_[Hamedal_C30R]_V1.100.0.1.R.221126.bin";
                deviceService.downloadFirmware(downloadUrl, "/sdcard/", "dfu_fm_test.bin", "57F56267593F2C9E0F96F2514F4D7724");
            } catch (Exception e) {
                e.printStackTrace();
            }
        });

        deviceService = new DeviceServiceImpl(this);
        deviceService.setMessageListener(new IDeviceMessageListener() {
            @Override
            public void onDeviceInfo(int deviceId, int devType, String productName, String phyVersion,
                                     String softVersion, String serialNumber, String uniqueId) {
                String info = "[onDeviceInfo]\nproduct_name:"+productName+ "\nunique_id:"+uniqueId +
                        "\ndevType:"+devType + "\nphy_version:"+phyVersion + "\nsoft_version:" + softVersion
                        +"\nserial_number:" + serialNumber + "\ndevice_id:"+deviceId;
                Log.e(TAG, info);
                appendDebugInfo(info);
            }

            @Override
            public void onHidCallState(DeviceEvent.HidCallState state) {
                appendDebugInfo("HidCallState:" + state.toString());
            }

            @Override
            public void onDeviceEvent(DeviceEvent.HidEvent event) {
                appendDebugInfo("HidCallState:" + event.toString());
            }

            @Override
            public void onVideoMode(DeviceEvent.VideoMode mode) {
                appendDebugInfo("VideoMode:" + mode.toString());
            }

            @Override
            public void onGetPowerLineFreq(int freq) {
                appendDebugInfo("onGetPowerLineFreq:" + freq);
            }

            @Override
            public void onGetMirrorState(int state) {
                appendDebugInfo("onGetMirrorState:" + state);
            }

            @Override
            public void onGetFlipState(int state) {
                appendDebugInfo("onGetFlipState:" + state);
            }

            @Override
            public void onDfuUpdateState(DeviceEvent.UpgradeState state, int progress) {
                appendDebugInfo("onDfuUpdateState:" + state.toString() + " progress:" + progress + "%");
            }

            @Override
            public void onDfuDownloadState(int state, int progress){
                appendDebugInfo("download dfu State:" + state + " progress:" + progress + "%");
            }

            @Override
            public void onOtaDownloadState(int state, int progress) {
                appendDebugInfo("download OTA State:" + state + " progress:" + progress + "%");
            }

            @Override
            public void onGetDereverationAlgParam(int val) {
                appendDebugInfo("get DereverationAlgParam:" + val);
            }

            @Override
            public void onGetVideoTrackMode(int mode) {
                appendDebugInfo("get VideoTrackMode:" + mode);
            }
        });

    }

    @Override
    protected  void onDestroy(){
        deviceService.disconnect();
        super.onDestroy();
    }

    private void appendDebugInfo(String text){
        runOnUiThread(() -> {
            etDebug.append("\n" + text);
            svContainer.post(()->svContainer.smoothScrollTo(0, etDebug.getBottom()));
        });
    }

}