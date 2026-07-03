package com.auditoryworks.axdp;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Created by cjb on 2022/11/21
 * Desc :
 */
public class MessageData implements Parcelable {
    int cmd;
    byte []data;

    public MessageData(int cmd, byte[]data){
        this.cmd = cmd;
        this.data = new byte[data.length];
        System.arraycopy(data, 0, this.data, 0, data.length);
    }

    protected MessageData(Parcel in) {
        cmd = in.readInt();
        in.readByteArray(data);
    }

    public static final Creator<MessageData> CREATOR = new Creator<MessageData>() {
        @Override
        public MessageData createFromParcel(Parcel in) {
            return new MessageData(in);
        }

        @Override
        public MessageData[] newArray(int size) {
            return new MessageData[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(cmd);
        dest.writeByteArray(data);
    }
}
