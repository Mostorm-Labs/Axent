package com.auditoryworks.axdp.beans;

import java.io.Serializable;
import java.util.List;

public  class dfuBean implements Serializable{
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


   public class Data implements Serializable{

      private List<DeviceUpdates> deviceUpdates;

      public void setDeviceUpdates(List<DeviceUpdates> deviceUpdates) {
         this.deviceUpdates = deviceUpdates;
      }

      public List<DeviceUpdates> getDeviceUpdates() {
         return deviceUpdates;
      }

   }

   public class DeviceUpdates implements Serializable {

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
