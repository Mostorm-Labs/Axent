#ifndef AXDP_DEFINES_H_
#define AXDP_DEFINES_H_

#include <cstdint>
#include <string>
#include <vector>

#define AXDP_BUILD_SHARED
#ifndef AXDP_API
#  ifdef _WIN32
#     if defined(AXDP_BUILD_SHARED) /* build dll */
#         define AXDP_API __declspec(dllexport)
#     elif !defined(AXDP_BUILD_STATIC) /* use dll */
#         define AXDP_API __declspec(dllimport)
#     else /* static library */
#         define AXDP_API
#     endif
#  else
#     if __GNUC__ >= 4
#         define AXDP_API __attribute__((visibility("default")))
#     else
#         define AXDP_API
#     endif
#  endif
#endif

#define AXDP_VERSION_MAJOR 1
#define AXDP_VERSION_MINOR 0
#define AXDP_VERSION ((AXDP_API_VERSION_MAJOR << 16) | AXDP_API_VERSION_MINOR)

namespace axdp {

    enum class UpdateStrategy : uint32_t {
        Falcon = 0x00100000,    //v20 like
        Dolphin = 0x00200000,   //a20 like
        Gopher = 0x00300000,    //c20 like
        Viper = 0x00400000,        //c30rk like
        Cobra = 0x00500000,     //for prd test
        Gecko = 0x00600000,     //Ceilingmic like A50
        Discus = 0x00700000,    //A50 DSP
        Camel = 0x00800000,        //A50 ctrl
        Jagger = 0x00900000,        //Gecko with endpoint change commond
        Unknown = 0xFFFFFFFF,      //Error strategy;
    };

    enum ErrorCode : int32_t {
        /**
        * 成功，无异常
        */
        Success = 0,
        /**
        * 升级文件异常
        */
        FileAbnormal,
        /**
        * 升级文件校验失败
        */
        FileVerifyFailed,
        /**
        * 通信异常
        */
        TransportFailed,
        /**
        * 设备内部异常
        */
        InsideAbnormal,
    };

    enum class ResultState : int32_t {
        Failed = -1,
        Success = 0
    };

    enum class VideoMode : uint32_t {
        PanoramicView = 0,
        SmartTracking = 1 //auto framing
    };

    enum class VideoTrackMode : uint32_t {
        NoTrack = 0,
        SingleSpeaker = 1,
        SplitScreens = 2,
        ZoneFollowing = 3,
        Manual = 4,
        PanoramaShot = 5,
        AutoFraming = 6
    };

    enum class SpeakerTrackDelay : uint32_t {
        MinDelaySecond = 1,
        MaxDelaySecond = 5
    };

    enum class EnableState : uint32_t {
        Disabled = 0,
        Enabled = 1
    };

    enum class PowerlineFreqType : uint32_t {
        Freq50Hz = 1,
        Freq60Hz = 2
    };

    enum class UpgradeState {
        DataReady,      //数据准备完成，向设备请求升级
        Transferring,   //数据传输中，此时开始progress变化
        Verifying,      //传输完成，等待数据确认
        Success,        //设备升级成功，等待重启
        Failed
    };

    typedef struct DeviceInformationSt {
        uint16_t dev_type;
        char product_name[64];
        char phy_version[64];
        char soft_version[64];
        char serial_number[64];
        char unique_id[64];
        int32_t index_id;
    } DeviceInfo;

    typedef struct RoiRectSt {
        int left;
        int right;
        int top;
        int bottom;
    } RoiRect;

    enum class HIDEvent {
        Mute,
        Unmute,
        Answer,
        HangUp,
        VolDown,
        VolUp
    };

    enum class DegreeLevel : int8_t {
        UltraLow = 0,
        Low = 1,
        Medium = 2,
        High = 3,
        UltraHigh = 4
    };


    /*
    音频类型
        0x01 PCM, 0x02 OggOpus
    采样率
        16000，32000， 48000
    声道数
        1 单声道， 2 双声道，
    采样位宽
        以字节算
    */
    typedef struct AudioFormat {
        uint32_t            audio_type;
        uint32_t            sample_rate;
        uint32_t            channel_num;
        uint32_t            bit_width;
    }AudioFormat;

    typedef struct subindex_header_s {
        uint16_t magic;//0
        uint16_t type;//0x10
        uint16_t len;//32
        uint8_t  sub_idx[32];
    }SubindexHeader;

    typedef std::vector<DeviceInfo> DeviceInfoList;
}


#endif // !AXDP_DEFINES_H_
