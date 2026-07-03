package com.auditoryworks.axdp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Created by cjb on 2022/11/29
 * Desc :开机广播
 */
public class BootReceiver extends BroadcastReceiver {
    final static String TAG = "[cjb]BootReceiver";
    private static final String ACTION_BOOT = "android.intent.action.BOOT_COMPLETED";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(ACTION_BOOT)) { //开机启动完成
            Log.i(TAG, "收到开机广播，启动AXDP设备服务。。。");
            try{
                Intent axdpIntent = new Intent();
                axdpIntent.setClassName("com.auditoryworks.axdp",
                        "com.auditoryworks.axdp.AxdpService");
                context.startService(axdpIntent);
            }catch (Exception e){
                Log.e(TAG, "启动AXDP设备服务异常："+e.toString());
            }
        }
    }
}
