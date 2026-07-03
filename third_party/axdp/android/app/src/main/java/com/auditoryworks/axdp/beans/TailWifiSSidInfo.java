package com.auditoryworks.axdp.beans;

public class TailWifiSSidInfo {
    private String b;
    private int c;
    private String k;
    private String s;
    private int t;

    public TailWifiSSidInfo() {
    }

    public TailWifiSSidInfo(String b, String p, String s) {
        this.b = b;
        this.k = p;
        this.s = s;
        this.c = 0;
        this.t = 0;
    }

    public String getB() {
        return b;
    }

    public void setB(String b) {
        this.b = b;
    }

    public String getK() {
        return k;
    }

    public void setK(String k) {
        this.k = k;
    }

    public String getS() {
        return s;
    }

    public void setS(String s) {
        this.s = s;
    }

    public int getC() {
        return c;
    }

    public void setC(int c) {
        this.c = c;
    }

    public int getT() {
        return t;
    }

    public void setT(int t) {
        this.t = t;
    }
}
