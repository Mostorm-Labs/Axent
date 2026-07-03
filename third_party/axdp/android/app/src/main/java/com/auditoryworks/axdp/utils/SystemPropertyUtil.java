package com.auditoryworks.axdp.utils;

import android.text.TextUtils;
import android.util.Log;

import java.lang.reflect.Method;

public class SystemPropertyUtil {
    private static final String TAG = "SystemPropertyUtil";

    public static String getString(String property, String defaultValue) {
        try {
            Class clazz = Class.forName("android.os.SystemProperties");
            Method getter = clazz.getDeclaredMethod("get", String.class);
            String value = (String) getter.invoke(null, property);
            if (!TextUtils.isEmpty(value)) {
                return value;
            }
        } catch (Exception e) {
            Log.d(TAG, "Unable to read system properties");
        }
        return defaultValue;
    }

    public static void setProperty(String key, String value) {
        try {
            Class<?> c = Class.forName("android.os.SystemProperties");
            Method set = c.getMethod("set", String.class, String.class);
            set.invoke(c, key, value);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static int getInt(String property, int defaultValue) {
        try {
            Class clazz = Class.forName("android.os.SystemProperties");
            Method getter = clazz.getDeclaredMethod("get", String.class);
            String value = (String)getter.invoke(null, property);
            if (!TextUtils.isEmpty(value)) {
                return Integer.parseInt(value);
            } else
                return 0;
        } catch (Exception e) {
            Log.d(TAG, "Unable to read system properties");
        }
        return defaultValue;
    }

    public static void setInt(String key, int value) {
        try {
            Class<?> c = Class.forName("android.os.SystemProperties");
            Method set = c.getMethod("set", String.class, String.class);
            set.invoke(c, key, String.valueOf(value));
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
