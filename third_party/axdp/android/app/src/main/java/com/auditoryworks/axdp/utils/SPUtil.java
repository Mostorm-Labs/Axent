package com.auditoryworks.axdp.utils;

import android.content.Context;
import android.content.SharedPreferences;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;

import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class SPUtil {
    public static final String FILE_NAME = "SystemSetting";

    public static void clear(Context context) {
        SharedPreferences.Editor editor = context.getSharedPreferences(FILE_NAME, 0).edit();
        editor.clear();
        SharedPreferencesCompat.apply(editor);
    }

    public static boolean contains(Context context, String key) {
        return context.getSharedPreferences(FILE_NAME, 0).contains(key);
    }

    public static boolean deleteSP(Context context) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("/data/data/");
        stringBuilder.append(context.getPackageName());
        stringBuilder.append("/shared_prefs");
        File file = new File(stringBuilder.toString(), "SystemSetting.xml");
        return file.exists() ? file.delete() : false;
    }

    public static Object get(Context context, String key, Object value) {
        SharedPreferences sharedPreferences = context.getSharedPreferences(FILE_NAME, 0);
        return (value instanceof String) ? sharedPreferences.getString(key, (String) value) : ((value instanceof Integer) ? Integer.valueOf(sharedPreferences.getInt(key, ((Integer) value).intValue())) : ((value instanceof Boolean) ? Boolean.valueOf(sharedPreferences.getBoolean(key, ((Boolean) value).booleanValue())) : ((value instanceof Float) ? Float.valueOf(sharedPreferences.getFloat(key, ((Float) value).floatValue())) : ((value instanceof Long) ? Long.valueOf(sharedPreferences.getLong(key, ((Long) value).longValue())) : value))));
    }

    public static Map<String, ?> getAll(Context context) {
        return context.getSharedPreferences(FILE_NAME, 0).getAll();
    }

    public static <T> List<T> getDataList(Context context, String key) {
        SharedPreferences sharedPreferences = context.getSharedPreferences(FILE_NAME, 0);
        ArrayList<T> arrayList = new ArrayList();
        String value = sharedPreferences.getString(key, null);
        return (value == null) ? arrayList : (List<T>) (new Gson()).fromJson(value, (new TypeToken<List<T>>() {

        }).getType());
    }

    public static boolean isSPExist(Context context) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("/data/data/");
        stringBuilder.append(context.getPackageName());
        stringBuilder.append("/shared_prefs");
        return (new File(stringBuilder.toString(), "SystemSetting.xml")).exists();
    }

    public static void put(Context context, String key, Object value) {
        SharedPreferences.Editor editor = context.getSharedPreferences(FILE_NAME, 0).edit();
        if (value instanceof String) {
            editor.putString(key, (String) value);
        } else if (value instanceof Integer) {
            editor.putInt(key, ((Integer) value).intValue());
        } else if (value instanceof Boolean) {
            editor.putBoolean(key, ((Boolean) value).booleanValue());
        } else if (value instanceof Float) {
            editor.putFloat(key, ((Float) value).floatValue());
        } else if (value instanceof Long) {
            editor.putLong(key, ((Long) value).longValue());
        } else {
            editor.putString(key, value.toString());
        }
        SharedPreferencesCompat.apply(editor);
    }

    public static void putMap(Context context, Map<String, Object> map) {
        SharedPreferences.Editor editor = context.getSharedPreferences(FILE_NAME, 0).edit();
        for (Map.Entry<String, Object> entry : map.entrySet()) {
            String str = entry.getKey();
            Object object = entry.getValue();
//      entry = (Map.Entry<String, Object>)entry.getValue();
            if (object instanceof String) {
                editor.putString(str, (String) object);
                continue;
            }
            if (object instanceof Integer) {
                editor.putInt(str, (Integer) object);
                continue;
            }
            if (object instanceof Boolean) {
                editor.putBoolean(str, (Boolean) object);
                continue;
            }
            if (object instanceof Float) {
                editor.putFloat(str, (Float) object);
                continue;
            }
            if (object instanceof Long) {
                editor.putLong(str, (Long) object);
                continue;
            }
            editor.putString(str, entry.toString());
        }
        editor.apply();
    }

    public static void remove(Context context, String key) {
        SharedPreferences.Editor editor = context.getSharedPreferences(FILE_NAME, 0).edit();
        editor.remove(key);
        SharedPreferencesCompat.apply(editor);
    }

    public static <T> void setDataList(Context context, String key, List<T> list) {
        SharedPreferences.Editor editor = context.getSharedPreferences(FILE_NAME, 0).edit();
        if (list != null) {
            if (list.size() <= 0)
                return;
            String str = (new Gson()).toJson(list);
            editor.clear();
            editor.putString(key, str);
            SharedPreferencesCompat.apply(editor);
            return;
        }
    }

    private static class SharedPreferencesCompat {
        private static final Method sApplyMethod = findApplyMethod();

        public static void apply(SharedPreferences.Editor editor) {
            try {
                if (sApplyMethod != null) {
                    sApplyMethod.invoke(editor);
                    return;
                }
            } catch (IllegalArgumentException | IllegalAccessException | java.lang.reflect.InvocationTargetException e) {
                e.printStackTrace();
            }
            editor.commit();
        }

        private static Method findApplyMethod() {
            try {
                return SharedPreferences.Editor.class.getMethod("apply");
            } catch (NoSuchMethodException e) {
                e.printStackTrace();
                return null;
            }
        }
    }
}
