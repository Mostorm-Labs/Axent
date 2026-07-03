package com.auditoryworks.axdp.utils;

import android.content.Context;
import android.os.Environment;
import android.util.Log;
import androidx.annotation.NonNull;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.RandomAccessFile;
import java.io.Serializable;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.Locale;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.ResponseBody;

public class DownloadUtil {
    public static final int STATE_SUCCESS = 0;
    public static final int STATE_DOWNLOADING = 1;
    public static final int STATE_FAILED = 2;

    private static DownloadUtil downloadUtil;
    private final OkHttpClient okHttpClient;
    private Context context;
    private String TAG = "upgrader-DownloadUtil";
    long downloadLength = 0;   //记录已经下载的文件长度
    public static final int neterror = 0x01;
    public static final int fileerror = 0x02;
    public static final int retrytime = 13;
    private int curretnTime = 0;
    private long filesize = 7112565;
    private int last_progress = 0;
    private RandomAccessFile savedFile = null;
    private final String DFU_SAVE_URL = "/cache/dfu";
    private final String ANDROIDSAVERUL = "/cache/android";
    private final String DFU_JSON = "/cache/dfu/dfu.bean";
    private final String ANDROIDJSON = "/cache/android/android.bean";

    public static DownloadUtil get() {
        if (downloadUtil == null) {
            downloadUtil = new DownloadUtil();
        }
        return downloadUtil;
    }

    public DownloadUtil() {

        okHttpClient = new OkHttpClient();
    }

    /**
     * @param url      下载连接
     * @param saveDir  储存下载文件的SDCard目录
     * @param listener 下载监听
     */
    public void download(Context context, final String url, final String saveDir, final String fileName, final OnDownloadListener listener) {
        this.context = context;
        // 需要token的时候可以这样做
        // SharedPreferences sp=MyApp.getAppContext().getSharedPreferences("loginInfo", MODE_PRIVATE);
        // Request request = new Request.Builder().header("token",sp.getString("token" , "")).url(url).build();

        //    Request request = new Request.Builder().url(url).build();


        File file = new File(saveDir, fileName);


        if (file.exists()) {
            //如果文件存在的话，得到文件的大小
            downloadLength = file.length();
            Log.w(TAG, "file.exists()" + downloadLength);
        }
        Request request = new Request.Builder()
                //确定下载的范围,添加此头,则服务器就可以跳过已经下载好的部分
                .addHeader("Range", "bytes=" + downloadLength + "-")
                .url(url)
                .build();

        try {
            savedFile = new RandomAccessFile(file, "rwd");
            savedFile.seek(downloadLength);//跳过已经下载的字节
        } catch (Exception e) {
            e.printStackTrace();
        }

        Call call = okHttpClient.newCall(request);
        // okHttpClient.newCall(request).enqueue(new Callback() {
        call.enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                // 下载失败
                e.printStackTrace();
                listener.onDownloadFailed(neterror, retrytime);
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                InputStream is = null;

                byte[] buf = new byte[1024];
                int len = 0;
                //    FileOutputStream fos = null;

                try {
                    is = response.body().byteStream();
                    long total = response.body().contentLength();

                    //得到下载内容的大小
                    long contentLength = getContentLength(url);
                    Log.w(TAG, "contentLength=" + contentLength + "   total=" + total + "   downloadLength=" + downloadLength);
                    if (contentLength == 0) {
                        listener.onDownloadFailed(neterror, 1);
                        return;
                    } else if (contentLength == downloadLength) {
                        //已下载字节和文件总字节相等，说明已经下载完成了
                        listener.onDownloadSuccess();
                        return;
                    } else if (downloadLength > contentLength) {
                        file.delete();
                        call.cancel();
                        call = null;
                        Log.w(TAG, "文件过大，重新下载" + curretnTime);
                        curretnTime++;
                        listener.onDownloadFailed(fileerror, 1);
                        return;
                    }

                    String range = String.format(Locale.CHINESE, "bytes=%d-", file.length());
                    Log.w(TAG, "最终路径：" + file);
                    //    fos = new FileOutputStream(file);
                    long sum = 0;
                    sum += downloadLength;
                    long beforeTime = System.currentTimeMillis(); // 前一秒
                    long secondCount = 0; // 每秒的下载量
                    while ((len = is.read(buf)) != -1) {
                        synchronized (savedFile) {
                            savedFile.write(buf, 0, len);
                        }
                        sum += len;
                        long currentTime = System.currentTimeMillis();
                        if (currentTime - beforeTime > 1000) {
                            if (secondCount > 0) {
                                listener.onDownloadSpeed(secondCount, sum, total);

                            }
                            beforeTime = currentTime;
                            secondCount = 0;
                        } else {
                            secondCount += len;
                        }

                        int progress = (int) (sum * 1.0f / contentLength * 100);
                        if (last_progress != progress){
                            Log.w(TAG, file.getName() + "下载进度：" + progress);
                            last_progress = progress;
                        }

                        if (progress > 100) {
                            file.delete();
                            listener.onDownloadFailed(0x03, 3);
                        }
                        // 下载中
                        listener.onDownloading(progress, sum, total);
                    }
                    //     fos.flush();
                    savedFile.close();
                    // 下载完成
                    listener.onDownloadSuccess();
                    return;
                } catch (Exception e) {
                    e.printStackTrace();
                    listener.onDownloadFailed(0x03, 3);
                } finally {
                    try {
                        if (is != null)
                            is.close();
                    } catch (IOException e) {
                    }

                }
            }
        });
    }


    /**
     * 得到下载内容的完整大小
     *
     * @param downloadUrl
     * @return
     */
    private long getContentLength(String downloadUrl) {
        OkHttpClient client = new OkHttpClient();
        long contentLength = 0;
        Request request = new Request.Builder().url(downloadUrl).build();
        try {
            Response response = client.newCall(request).execute();
            if (response != null && response.isSuccessful()) {
                contentLength = response.body().contentLength();
                response.body().close();
                return contentLength;
            }
/*
            Headers headers = response.headers();
            boolean isAcceptRanges = false;
            for (int i = 0; i < headers.size(); i++){
                String headerName = headers.name(i);
                if (headerName.equalsIgnoreCase("Content-Range")) {
                    isAcceptRanges = true;
                    String headerValue = "" + headers.get(headerName); //值示例：bytes 0-1/6764378
                    String strNum = headerValue.indexOf("/") > 0 ? headerValue.substring(headerValue.indexOf("/")+1) : "0";
                     contentLength = Long.parseLong(strNum);
                    break;
                }
            }

            response.close();
*/

        } catch (IOException e) {
            e.printStackTrace();
        }
        return 0;
    }

    /**
     * SD卡文件MD5文件校验
     *
     * @param file
     * @return
     */
    public static String file2MD5(File file) {

        try {
            byte[] hash;
            byte[] buffer = new byte[8192];
            MessageDigest md = MessageDigest.getInstance("MD5");
            FileInputStream fis = new FileInputStream(file);
            int len;
            while ((len = fis.read(buffer)) != -1) {
                md.update(buffer, 0, len);
            }
            hash = md.digest();

            //对生成的16字节数组进行补零操作
            StringBuilder hex = new StringBuilder(hash.length * 2);
            for (byte b : hash) {
                if ((b & 0xFF) < 0x10) {
                    hex.append("0");
                }
                hex.append(Integer.toHexString(b & 0xFF));
            }
            return hex.toString().toUpperCase();
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException("NoSuchAlgorithmException", e);
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException("UnsupportedEncodingException", e);
        } catch (IOException e) {
            e.printStackTrace();
        }

        return "";

    }


    /**
     * 使用OkHttp下载文件，支持断点续传
     */
    public void downloadByOkHttp() throws IOException {
        // 文件下载地址
        String url = "https://alissl.ucdl.pp.uc.cn/fs08/2019/07/05/1/110_17e4089aa3a4b819b08069681a9de74b.apk";
        // 创建下载文件对象
        File directory = Environment.getExternalStorageDirectory();
        File file = new File(directory, "20190715.apk");
        RandomAccessFile accessFile = new RandomAccessFile(file, "rw");
        // 断点续传：重新开始下载的位置：file.length()
        String range = String.format(Locale.CHINESE, "bytes=%d-", file.length());
        OkHttpClient client = new OkHttpClient();
        Request request = new Request.Builder()
                .url(url)
                .header("range", range)
                .build();
        // 使用OkHttp请求服务器
        Call call = client.newCall(request);
        Response response = call.execute();
        // 连接服务器成功
        ResponseBody body = response.body();
        System.out.println("文件大小：" + body.contentLength());
        // 移动文件指针到断点续传的位置
        accessFile.seek(file.length());
        // 开始断点续传
        InputStream inputStream = body.byteStream();
        byte[] bytes = new byte[1024];
        int len = inputStream.read(bytes);
        while (len != -1) {
            accessFile.write(bytes, 0, len);
            System.out.println("已下载字节：" + file.length());
            len = inputStream.read(bytes);
        }
        System.out.println("文件下载完毕：" + accessFile.getFilePointer());
    }

    private Object readFile2Obj(String filePath) {
        FileInputStream fis = null;
        Object obj = null;
        File f  = new File(filePath);
        if(!f.exists()) {
            return null;
        }
        try {
            fis = new FileInputStream(filePath);
            ObjectInputStream ois = new ObjectInputStream(fis);
            obj =  ois.readObject();
            fis.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return  obj;
    }

    public File getDFUFile() {
        File dfuDir = new File(DFU_SAVE_URL);
        File[] files = dfuDir.listFiles();
        if (files != null && files.length > 0) {
            dfuBean.DeviceUpdates deviceUpdates = (dfuBean.DeviceUpdates) readFile2Obj(DFU_JSON);
            if (deviceUpdates == null) {
                return null;
            }
            for (File f : files) {
                if ((f.getName().equals(deviceUpdates.getPackgeName())) && (f.length() == deviceUpdates.getPackgeSize())) {
                    return f;
                }
            }
        }
        return null;
    }

    public dfuBean.DeviceUpdates getDFUBean() {
        File dfudir = new File(DFU_SAVE_URL);
        if (dfudir.listFiles().length == 0) {
            return null;
        } else {

            dfuBean.DeviceUpdates deviceUpdates = (dfuBean.DeviceUpdates) readFile2Obj(DFU_JSON);
            if (deviceUpdates != null) {
                return deviceUpdates;
            }

        }
        return null;
    }


    /**
     * @param saveDir
     * @return
     * @throws IOException 判断下载目录是否存在
     */
    private String isExistDir(String saveDir) throws IOException {
        // 下载位置
        File downloadFile = new File(Environment.getExternalStorageDirectory().getPath() + "/download/", saveDir);
        if (!downloadFile.mkdirs()) {
            downloadFile.createNewFile();
        }
        String savePath = downloadFile.getAbsolutePath();
        Log.w(TAG, "下载目录：" + savePath);
        return savePath;
    }

    /**
     * @param url
     * @return 传入文件名
     */
    @NonNull
    public String getNameFromUrl(String url) {
        return url;
    }

    public interface OnDownloadListener {
        /**
         * 下载成功
         */
        void onDownloadSuccess();

        /**
         * @param progress 下载进度
         */
        void onDownloading(int progress, float download, float total);

        /**
         * 下载失败
         */
        void onDownloadFailed(int status, int retrytime);

        /**
         * 下载失败
         */
        void onDownloadSpeed(long speedcount, long download, long total);

    }

    public  class dfuBean {
        private static final long serialVersionUID = -1L;
        private String code;
        private String msg;
        private Data data;

        public void setCode(String code) {
            this.code = code;
        }

        public String getCode() {
            return code;
        }

        public void setMsg(String msg) {
            this.msg = msg;
        }

        public String getMsg() {
            return msg;
        }

        public void setData(Data data) {
            this.data = data;
        }

        public Data getData() {
            return data;
        }


        public class Data {

            private List<DeviceUpdates> deviceUpdates;

            public void setDeviceUpdates(List<DeviceUpdates> deviceUpdates) {
                this.deviceUpdates = deviceUpdates;
            }

            public List<DeviceUpdates> getDeviceUpdates() {
                return deviceUpdates;
            }

        }

        public class DeviceUpdates  {

            private int id;
            private String packageId;
            private String packgeVersion;
            private String packgeName;
            private long packgeSize;
            private String packageUrl;
            private List<packageFeature> packageFeature;
            private String packageRely;
            private String productModel;
            private String customer;
            private String md5;
            private String ossBucketName;
            private String releaseType;
            private String releaseTime;
            private String firmwareVersion;
            private String hardwareVersion;
            private String softwareVersion;
            private int tipsVersion;
            private String tipsName;
            private String productName;

            public void setId(int id) {
                this.id = id;
            }

            public int getId() {
                return id;
            }

            public void setPackageId(String packageId) {
                this.packageId = packageId;
            }

            public String getPackageId() {
                return packageId;
            }

            public void setPackgeVersion(String packgeVersion) {
                this.packgeVersion = packgeVersion;
            }

            public String getPackgeVersion() {
                return packgeVersion;
            }

            public void setPackgeName(String packgeName) {
                this.packgeName = packgeName;
            }

            public String getPackgeName() {
                return packgeName;
            }

            public void setPackgeSize(long packgeSize) {
                this.packgeSize = packgeSize;
            }

            public long getPackgeSize() {
                return packgeSize;
            }

            public void setPackageUrl(String packageUrl) {
                this.packageUrl = packageUrl;
            }

            public String getPackageUrl() {
                return packageUrl;
            }

            public void setPckageFeature(List<packageFeature> packageFeature) {
                this.packageFeature = packageFeature;
            }

            public List<packageFeature> getPckageFeature() {
                return packageFeature;
            }

            public void setPackageRely(String packageRely) {
                this.packageRely = packageRely;
            }

            public String getPackageRely() {
                return packageRely;
            }

            public void setProductModel(String productModel) {
                this.productModel = productModel;
            }

            public String getProductModel() {
                return productModel;
            }

            public void setCustomer(String customer) {
                this.customer = customer;
            }

            public String getCustomer() {
                return customer;
            }

            public void setMd5(String md5) {
                this.md5 = md5;
            }

            public String getMd5() {
                return md5;
            }

            public void setOssBucketName(String ossBucketName) {
                this.ossBucketName = ossBucketName;
            }

            public String getOssBucketName() {
                return ossBucketName;
            }

            public void setReleaseType(String releaseType) {
                this.releaseType = releaseType;
            }

            public String getReleaseType() {
                return releaseType;
            }

            public void setReleaseTime(String releaseTime) {
                this.releaseTime = releaseTime;
            }

            public String getReleaseTime() {
                return releaseTime;
            }

            public void setFirmwareVersion(String firmwareVersion) {
                this.firmwareVersion = firmwareVersion;
            }

            public String getFirmwareVersion() {
                return firmwareVersion;
            }

            public void setHardwareVersion(String hardwareVersion) {
                this.hardwareVersion = hardwareVersion;
            }

            public String getHardwareVersion() {
                return hardwareVersion;
            }

            public void setSoftwareVersion(String softwareVersion) {
                this.softwareVersion = softwareVersion;
            }

            public String getSoftwareVersion() {
                return softwareVersion;
            }

            public void setTipsVersion(int tipsVersion) {
                this.tipsVersion = tipsVersion;
            }

            public int getTipsVersion() {
                return tipsVersion;
            }

            public void setTipsName(String tipsName) {
                this.tipsName = tipsName;
            }

            public String getTipsName() {
                return tipsName;
            }

            public void setProductName(String productName) {
                this.productName = productName;
            }

            public String getProductName() {
                return productName;
            }


        }

        public class packageFeature implements Serializable {

            private String region;
            private String msg;

            public void setRegion(String region) {
                this.region = region;
            }

            public String getRegion() {
                return region;
            }

            public void setMsg(String msg) {
                this.msg = msg;
            }

            public String getMsg() {
                return msg;
            }

        }
    }
}