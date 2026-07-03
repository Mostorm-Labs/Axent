package com.auditoryworks.axdp.beans;

import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.RequiresApi;

public class WifiConfig implements Parcelable {
    public static final Creator<WifiConfig> CREATOR = new Creator<WifiConfig>() {
        @RequiresApi(api = Build.VERSION_CODES.Q)
        public WifiConfig createFromParcel(Parcel in) {
            return new WifiConfig(in);
        }

        public WifiConfig[] newArray(int size) {
            return new WifiConfig[size];
        }
    };

    private String caCert;

    private String clientCert;

    private String eapAnonymous;

    private String eapIdentity;

    private int eapMethod;

    private String exclusionList;

    private String gateway;

    private boolean hiddeSsid;

    private String host;

    private String ipAddr;

    private boolean isUpdate;

    private String mDns1;

    private String mDns2;

    private int mIpAssignment;

    private int networkId;

    private int networkPrefixLength;

    private String password;

    private int phase2Method;

    private String portStr;

    private int proxySelectedPosition;

    private int security;

    private String ssid;

    private String uriSequence;

    public WifiConfig() {
        this.hiddeSsid = false;
        this.networkId = -1;
        this.isUpdate = false;
    }

    @RequiresApi(api = Build.VERSION_CODES.Q)
    public WifiConfig(Parcel parcel) {
        this.hiddeSsid = false;
        this.networkId = -1;
        this.isUpdate = false;
        this.password = parcel.readString();
        this.ssid = parcel.readString();
        this.hiddeSsid = parcel.readBoolean();
        this.security = parcel.readInt();
        this.ipAddr = parcel.readString();
        this.gateway = parcel.readString();
        this.networkPrefixLength = parcel.readInt();
        this.mDns1 = parcel.readString();
        this.mDns2 = parcel.readString();
        this.host = parcel.readString();
        this.portStr = parcel.readString();
        this.exclusionList = parcel.readString();
        this.uriSequence = parcel.readString();
        this.proxySelectedPosition = parcel.readInt();
        this.mIpAssignment = parcel.readInt();
        this.eapMethod = parcel.readInt();
        this.phase2Method = parcel.readInt();
        this.caCert = parcel.readString();
        this.clientCert = parcel.readString();
        this.eapIdentity = parcel.readString();
        this.eapAnonymous = parcel.readString();
        this.networkId = parcel.readInt();
        this.isUpdate = parcel.readBoolean();
    }

    public int describeContents() {
        return 0;
    }

    public String getCaCert() {
        return this.caCert;
    }

    public String getClientCert() {
        return this.clientCert;
    }

    public String getEapAnonymous() {
        return this.eapAnonymous;
    }

    public String getEapIdentity() {
        return this.eapIdentity;
    }

    public int getEapMethod() {
        return this.eapMethod;
    }

    public String getExclusionList() {
        return this.exclusionList;
    }

    public String getGateway() {
        return this.gateway;
    }

    public String getHost() {
        return this.host;
    }

    public String getIpAddr() {
        return this.ipAddr;
    }

    public int getNetworkId() {
        return this.networkId;
    }

    public int getNetworkPrefixLength() {
        return this.networkPrefixLength;
    }

    public String getPassword() {
        return this.password;
    }

    public int getPhase2Method() {
        return this.phase2Method;
    }

    public String getPortStr() {
        return this.portStr;
    }

    public int getProxySelectedPosition() {
        return this.proxySelectedPosition;
    }

    public int getSecurity() {
        return this.security;
    }

    public String getSsid() {
        return this.ssid;
    }

    public String getUriSequence() {
        return this.uriSequence;
    }

    public String getmDns1() {
        return this.mDns1;
    }

    public String getmDns2() {
        return this.mDns2;
    }

    public int getmIpAssignment() {
        return this.mIpAssignment;
    }

    public boolean isHiddeSsid() {
        return this.hiddeSsid;
    }

    public boolean isUpdate() {
        return this.isUpdate;
    }

    public void setCaCert(String caCert) {
        this.caCert = caCert;
    }

    public void setClientCert(String clientCert) {
        this.clientCert = clientCert;
    }

    public void setEapAnonymous(String eapAnonymous) {
        this.eapAnonymous = eapAnonymous;
    }

    public void setEapIdentity(String eapIdentity) {
        this.eapIdentity = eapIdentity;
    }

    public void setEapMethod(int eapMethod) {
        this.eapMethod = eapMethod;
    }

    public void setExclusionList(String exclusionList) {
        this.exclusionList = exclusionList;
    }

    public void setGateway(String gateway) {
        this.gateway = gateway;
    }

    public void setHiddeSsid(boolean hiddeSsid) {
        this.hiddeSsid = hiddeSsid;
    }

    public void setHost(String host) {
        this.host = host;
    }

    public void setIpAddr(String ipAddr) {
        this.ipAddr = ipAddr;
    }

    public void setNetworkId(int networkId) {
        this.networkId = networkId;
    }

    public void setNetworkPrefixLength(int networkPrefixLength) {
        this.networkPrefixLength = networkPrefixLength;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public void setPhase2Method(int phase2Method) {
        this.phase2Method = phase2Method;
    }

    public void setPortStr(String portStr) {
        this.portStr = portStr;
    }

    public void setProxySelectedPosition(int proxySelectedPosition) {
        this.proxySelectedPosition = proxySelectedPosition;
    }

    public void setSecurity(int security) {
        this.security = security;
    }

    public void setSsid(String ssid) {
        this.ssid = ssid;
    }

    public void setUpdate(boolean isUpdate) {
        this.isUpdate = isUpdate;
    }

    public void setUriSequence(String uriSequence) {
        this.uriSequence = uriSequence;
    }

    public void setmDns1(String mDns1) {
        this.mDns1 = mDns1;
    }

    public void setmDns2(String mDns2) {
        this.mDns2 = mDns2;
    }

    public void setmIpAssignment(int mIpAssignment) {
        this.mIpAssignment = mIpAssignment;
    }

    public String toString() {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("WifiConfig{password='");
        stringBuilder.append(this.password);
        stringBuilder.append('\'');
        stringBuilder.append(", ssid='");
        stringBuilder.append(this.ssid);
        stringBuilder.append('\'');
        stringBuilder.append(", hiddeSsid=");
        stringBuilder.append(this.hiddeSsid);
        stringBuilder.append(", security=");
        stringBuilder.append(this.security);
        stringBuilder.append(", ipAddr='");
        stringBuilder.append(this.ipAddr);
        stringBuilder.append('\'');
        stringBuilder.append(", gateway='");
        stringBuilder.append(this.gateway);
        stringBuilder.append('\'');
        stringBuilder.append(", networkPrefixLength=");
        stringBuilder.append(this.networkPrefixLength);
        stringBuilder.append(", mDns1='");
        stringBuilder.append(this.mDns1);
        stringBuilder.append('\'');
        stringBuilder.append(", mDns2='");
        stringBuilder.append(this.mDns2);
        stringBuilder.append('\'');
        stringBuilder.append(", host='");
        stringBuilder.append(this.host);
        stringBuilder.append('\'');
        stringBuilder.append(", portStr='");
        stringBuilder.append(this.portStr);
        stringBuilder.append('\'');
        stringBuilder.append(", exclusionList='");
        stringBuilder.append(this.exclusionList);
        stringBuilder.append('\'');
        stringBuilder.append(", uriSequence='");
        stringBuilder.append(this.uriSequence);
        stringBuilder.append('\'');
        stringBuilder.append(", proxySelectedPosition=");
        stringBuilder.append(this.proxySelectedPosition);
        stringBuilder.append(", mIpAssignment=");
        stringBuilder.append(this.mIpAssignment);
        stringBuilder.append(", eapMethod=");
        stringBuilder.append(this.eapMethod);
        stringBuilder.append(", phase2Method=");
        stringBuilder.append(this.phase2Method);
        stringBuilder.append(", caCert='");
        stringBuilder.append(this.caCert);
        stringBuilder.append('\'');
        stringBuilder.append(", clientCert='");
        stringBuilder.append(this.clientCert);
        stringBuilder.append('\'');
        stringBuilder.append(", eapIdentity='");
        stringBuilder.append(this.eapIdentity);
        stringBuilder.append('\'');
        stringBuilder.append(", eapAnonymous='");
        stringBuilder.append(this.eapAnonymous);
        stringBuilder.append('\'');
        stringBuilder.append(", networkId=");
        stringBuilder.append(this.networkId);
        stringBuilder.append(", isEdit=");
        stringBuilder.append(this.isUpdate);
        stringBuilder.append('}');
        return stringBuilder.toString();
    }

    @RequiresApi(api = Build.VERSION_CODES.Q)
    public void writeToParcel(Parcel parcel, int i) {
        parcel.writeString(this.password);
        parcel.writeString(this.ssid);
        parcel.writeBoolean(this.hiddeSsid);
        parcel.writeInt(this.security);
        parcel.writeString(this.ipAddr);
        parcel.writeString(this.gateway);
        parcel.writeInt(this.networkPrefixLength);
        parcel.writeString(this.mDns1);
        parcel.writeString(this.mDns2);
        parcel.writeString(this.host);
        parcel.writeString(this.portStr);
        parcel.writeString(this.exclusionList);
        parcel.writeString(this.uriSequence);
        parcel.writeInt(this.proxySelectedPosition);
        parcel.writeInt(this.mIpAssignment);
        parcel.writeInt(this.eapMethod);
        parcel.writeInt(this.phase2Method);
        parcel.writeString(this.caCert);
        parcel.writeString(this.clientCert);
        parcel.writeString(this.eapIdentity);
        parcel.writeString(this.eapAnonymous);
        parcel.writeInt(this.networkId);
        parcel.writeBoolean(this.isUpdate);
    }
}
