package com.auditoryworks.axdp.beans;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.Uri;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import com.auditoryworks.axdp.utils.SPUtil;
import com.auditoryworks.axdp.utils.SystemPropertyUtil;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonDeserializer;
import com.xbh.sdk3.Network.NetworkHelper;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Random;

public class WifiHelper {

    private String TAG = WifiHelper.class.getSimpleName();
    private WifiManager mWifiManager;
    private NetworkHelper networkHelper;
    private Context context;
    private final Gson mGson = (new GsonBuilder()).registerTypeAdapter(Uri.class, (JsonDeserializer<Uri>) (jsonElement, type, jsonDeserializationContext) -> Uri.parse((String) jsonElement.getAsString())).create();
    public WifiHelper(Context context) {
        this.context = context;
        mWifiManager = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        networkHelper = new NetworkHelper();
    }

    public String getTailWifiJson() {
        try {
            if (isApOn(context)){
                String ssid = networkHelper.getWifiAp2SSID().replace("\"", "");
                String macAddress = networkHelper.getHotspotMacAddress();
                String wifiAp2Password = networkHelper.getWifiAp2Password();
                TailWifiSSidInfo tailWifiSSidInfo = new TailWifiSSidInfo(macAddress, wifiAp2Password,ssid);
                String tailWifiJson = mGson.toJson(tailWifiSSidInfo);
                Log.d(TAG,"setTailWifiSSId:ap tailWifiJson="+tailWifiJson);
                return tailWifiJson;
            }else {
                WifiInfo wifiInfo = getWifiInfo();
                if (wifiInfo!=null){
                    String ssid = wifiInfo.getSSID().replace("\"","");
                    int networkId = wifiInfo.getNetworkId();
                    WifiConfig config = getWifiConfig(ssid, networkId);
                    @SuppressLint("MissingPermission")
                    TailWifiSSidInfo tailWifiInfo = new TailWifiSSidInfo(wifiInfo.getMacAddress(), (config == null)?"":config.getPassword(),ssid);
                    String tailWifiJson = mGson.toJson(tailWifiInfo);
                    Log.d(TAG,"setTailWifiSSId:wifi tailWifiJson="+tailWifiJson);
                    return tailWifiJson;
                }else {
                    Log.d(TAG,"setTailWifiSSId:getWifiInfo null");
                    return "";
                }
            }
        }catch (Exception e){
            e.printStackTrace();
            return "";
        }
    }

    public static boolean isApOn(Context context) {
        WifiManager wifiManager = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifiManager == null)
            return false;
        try {
            Method method = wifiManager.getClass().getDeclaredMethod("isWifiApEnabled");
            method.setAccessible(true);
            return (Boolean) method.invoke(wifiManager, new Object[0]);
        } catch (NoSuchMethodException | InvocationTargetException | IllegalAccessException noSuchMethodException) {
            noSuchMethodException.printStackTrace();
            return false;
        }
    }

    public WifiConfig getWifiConfig(String ssid, int networkId) {
        Log.i(TAG, "ssid-->"+ssid +" networkId-->"+networkId);
        String wifiConfig = networkHelper.getWifiConfig(ssid, networkId);
        Log.i(TAG, "wifiConfig-->"+wifiConfig);
        return TextUtils.isEmpty(wifiConfig) ? null : mGson.fromJson(wifiConfig, WifiConfig.class);
    }

    public WifiInfo getWifiInfo() {
        return !mWifiManager.isWifiEnabled() ? null : mWifiManager.getConnectionInfo();
    }

    public static final String DEFAULT_AP_SSID = "NearHub_AP_";
    public static final String DEFAULT_AP_PASS = "12345678";
    public static final String SAVED_AP_NAME_KEY = "hot_spot_name";
    public static final String SAVED_AP_PASS_KEY = "hot_spot_pass";

    private static final String FIRST_INIT_HOT_SPOT_COMPLETE = "persist.init.hotspot.complete";

    public void openAPWithName() {
        String apName = Settings.System.getString(context.getContentResolver(), SAVED_AP_NAME_KEY);
        String apPass = Settings.System.getString(context.getContentResolver(), SAVED_AP_PASS_KEY);
        if (TextUtils.isEmpty(apName)) {
            apName = DEFAULT_AP_SSID+ getRandomString(4);
        }
        if (TextUtils.isEmpty(apPass)) {
            apPass = DEFAULT_AP_PASS;
        }
        int i = networkHelper.getWifiAp2AllowedKeyManagement();
        boolean bool = (Boolean) SPUtil.get(context.getApplicationContext(), "is_5G_hot_spot", true);
        configAp(apName, apPass, i, !bool);
        SystemPropertyUtil.setProperty(FIRST_INIT_HOT_SPOT_COMPLETE, "true");
    }

    public static final int WIFICIPHER_NOPASS = 0;
    public static final int WIFICIPHER_WPA = 4;
    public static final int WIFI_AP_2_4G_CHANNEL = 9;
    public static final int WIFI_AP_5G_CHANNEL = 149;
    public void configAp(String apName, String apPass, int type, boolean is2_4) {
        int c;
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("configAp: is24G = ");
        stringBuilder.append(is2_4);
        Log.d("NetworkUtil", stringBuilder.toString());
        if (TextUtils.isEmpty(apName))
            return;

        boolean bool = networkHelper.isWifi2Support();
        StringBuilder stringBuilder1 = new StringBuilder();
        stringBuilder1.append("configAp: isWifi2Enable = ");
        stringBuilder1.append(bool);
        Log.d("NetworkUtil", stringBuilder1.toString());
        if (networkHelper.getWifiAp2State())
            networkHelper.disableWifiAp2();
        if (type != WIFICIPHER_NOPASS) {
            type = WIFICIPHER_WPA;
        }
        if (is2_4) {
            c = WIFI_AP_2_4G_CHANNEL;
        } else {
            c = WIFI_AP_5G_CHANNEL;
        }
        Log.i(TAG, "apName-->"+apName+" apPass-->"+apPass+" type-->"+type+" cc-->"+c);
        networkHelper.openWifiAp2();
        boolean isSuccess = networkHelper.enableWifiAp2State(apName, apPass, type, c);
        Settings.System.putString(context.getContentResolver(), "wifi_ap_status", Boolean.toString(isSuccess));
    }

    public static String getRandomString(int length) {
        StringBuffer buffer = new StringBuffer("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        StringBuffer sb = new StringBuffer();
        Random r = new Random();
        int range = buffer.length();
        for (int i = 0; i < length; i ++) {
            sb.append(buffer.charAt(r.nextInt(range)));
        }
        return sb.toString();
    }

}
