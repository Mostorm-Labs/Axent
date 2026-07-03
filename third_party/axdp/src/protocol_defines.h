#ifndef AXDP_PROTOCOL_DEFINES_H_
#define AXDP_PROTOCOL_DEFINES_H_

#include <stdint.h>
#include "axdp_defines.h"

#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_) || defined(WIN64) || defined(_WIN64) || defined(_WIN64_)
    #define PLATFORM_WINDOWS 1 //Windows
#elif defined(ANDROID) || defined(_ANDROID_)
    #define PLATFORM_ANDROID 1 //Android
#elif defined(__linux__)
    #define PLATFORM_LINUX	 1 //Linux
#elif defined(__APPLE__)
    #if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
        #define PLATFORM_IOS	 1 //iOS\Mac
    #elif defined(TARGET_OS_MAC)
        #define PLATFORM_DARWIN
    #endif
#else
    #define PLATFORM_UNKNOWN 1
#endif

#define MAX_KEY_NUMBER 8
#define MAX_MIC_NUMBER 96
#define GUID_STR_LENGTH 36
#define MAX_SERIAL_NUMBER_LENGTH 64
#define MAX_HARDWARE_ID_LENGTH 64
#define MAX_AUTH_DEVICE_UID_LENGTH 4096
#define MAX_AUDIO_RECORD_DATA (16000*10*2)//3min audio length

namespace axdp {
    constexpr int32_t  kDefaultUsagePage = 129;
    constexpr int32_t  kDefaultUsage = 130;
    constexpr uint16_t kMaxPayloadSize = 4096;
    constexpr uint16_t kMagicNumber = 0xFFA5;
    constexpr uint16_t kBroadcastDst = 0xFFFF;
    constexpr uint16_t kDefaultDst = 0x02;
    constexpr uint16_t kSubIndexDst = 0x4000;
    constexpr uint16_t kMessageResponseInterval = 0x0080;
    constexpr uint16_t kMinimumCommonCmd = 0x0021;
    constexpr uint32_t kDefaultReadTimeWaitMS = 3000;
    constexpr int32_t kBlockingReadTimeWaitMs = -1;
    constexpr int32_t kNonBlockingReadTimeWaitMs = 0;
    constexpr uint16_t kProtocolVersion = 1;
    constexpr uint16_t kDefaultDestination = 2;
    constexpr uint16_t kDefaultSource = 1;
    constexpr uint16_t kDefaultCommand = 1;
    constexpr uint8_t kMagicNumberFirstByte = (kMagicNumber & 0xFF00) >> 8;
    constexpr uint8_t kMagicNumberSecondByte = kMagicNumber & 0x00FF;
    constexpr uint16_t kMagicNumberOffset = 0;
    constexpr uint16_t kMagicNumberSize = 2;
    constexpr uint16_t kVersionOffset = kMagicNumberOffset + kMagicNumberSize;
    constexpr uint16_t kVersionSize = 2;
    constexpr uint16_t kDestinationOffset = kVersionOffset + kVersionSize;
    constexpr uint16_t kDestinationSize = 2;
    constexpr uint16_t kSourceOffset = kDestinationOffset + kDestinationSize;
    constexpr uint16_t kSourceSize = 2;
    constexpr uint16_t kCommandOffset = kSourceOffset + kSourceSize;
    constexpr uint16_t kCommandSize = 2;
    constexpr uint16_t kPayloadLenOffset = kCommandOffset + kCommandSize;
    constexpr uint16_t kPayloadLenSize = 2;
    constexpr uint16_t kCrcOffset = kPayloadLenOffset + kPayloadLenSize;
    constexpr uint16_t kCrcSize = 2;
    constexpr uint16_t kHeaderSize = kCrcOffset + kCrcSize;
    constexpr uint16_t kPayloadOffset = kHeaderSize;

    /*
    * @Brief
    *		Protocol Type Mask.
    *		Actually there only alpha and beta type device,
    *		Common is not a type, but it also has a mask.
    */
    enum class ProtocolType : uint32_t {
        Alpha = 0xA0000,
        Beta = 0xB0000,
        Common = 0xC0000,
    };

    enum class UniCmd : int {
        AlphaUpgradeInfo = 0xA0001,
        AlphaUpgradeData = 0xA0002,
        AlphaDeviceInfo = 0xA0003,
        AlphaDeviceType = 0xA0004,
        BetaDeviceReset = 0xB0001,
        BetaDeviceInfo = 0xB0002,
        BetaStartUpgrade = 0xB0003,
        BetaStopUpgrade = 0xB0004,
        BetaUpgradeInfo = 0xB0005,
        BetaUpgradeData = 0xB0006,
        BetaUpgradeInfoEx = 0xB0011,
        BetaUpgradeDataEx = 0xB0012,
        CommonSetVideoMode = 0xC0021,
        CommonGetVideoMode = 0xC0022,
        CommonGetPeopleNumber = 0xC0023,
        CommonSetMicUsed = 0xC0024,
        CommonAudioRecord = 0xC0025,
        CommonSetTestResult = 0xC0026,
        CommonGetTestResult = 0xC0027,
        CommonSetEncryptedInfo = 0xC0028,
        CommonGetEncryptedInfo = 0xC0029,
        CommonGetGyroSlopeAngle = 0xC002A,
        CommonSetUacState = 0xC002B,
        CommonGetUacState = 0xC002C,
        CommonTestAudioConsistency = 0xC002D,
        CommonTestNetworkPort = 0xC002E,
        CommonSetVideoTrackMode = 0xC0031,
        CommonGetVideoTrackMode = 0xC0032,
        CommonSetMirrorState = 0xC0033,
        CommonGetMirrorState = 0xC0034,
        CommonSetSpeakerTrackDelay = 0xC0035,
        CommonGetSpeakerTrackDelay = 0xC0036,
        CommonSetSplitScreenNumber = 0xC0037,
        CommonGetSplitScreenNumber = 0xC0038,
        CommonSetUsbName = 0xC0039,
        CommonGetUsbName = 0xC003A,
        CommonSetUsbPid = 0xC003B,
        CommonGetUsbPid = 0xC003C,
        CommonSetUsbVid = 0xC003D,
        CommonGetUsbVid = 0xC003E,
        CommonSetReboot = 0xC003F,
        CommonSetPowerLineFreq = 0xC0040,
        CommonGetPowerLineFreq = 0xC0041,
        CommonGetDeviceUniqueId = 0xC0042,
        CommonSetDeviceUniqueId = 0xC0043,
        CommonGetAlgAuthContent = 0xC0044,
        CommonSetAlgAuthContent = 0xC0045,
        CommonSetWdrState = 0xC0046,
        CommonGetWdrState = 0xC0047,
        CommonSetOsdMirrorState = 0xC0048,
        CommonGetOsdMirrorState = 0xC0049,
        CommonSetAFCalibration = 0xC004A,
        CommonStartAudioTest = 0xC004B,
        CommonSetFlipState = 0xC004C,
        CommonGetFlipState = 0xC004D,
        CommonGetNoiseSuppressionLevel = 0xC004E,
        CommonSetNoiseSuppressionLevel = 0xC004F,
        CommonGetBootDetect = 0xC0050,
        CommonGetReverberationSuppressionLevel = 0xC0051,
        CommonSetReverberationSuppressionLevel = 0xC0052,
        CommonGetEchoCancellationLevel = 0xC0053,
        CommonSetEchoCancellationLevel = 0xC0054,
        CommonGetRecordEqParams = 0xC0055,
        CommonSetRecordEqParams = 0xC0056,
        CommonGetDefaultNoiseSuppressionLevel = 0xC0057,
        CommonSetDefaultNoiseSuppressionLevel = 0xC0058,
        CommonGetDefaultReverberationSuppressionLevel = 0xC0059,
        CommonSetDefaultReverberationSuppressionLevel = 0xC005A,
        CommonGetDefaultEchoCancellationLevel = 0xC005B,
        CommonSetDefaultEchoCancellationLevel = 0xC005C,
        CommonGetDefaultRecordEqParams = 0xC005D,
        CommonSetDefaultRecordEqParams = 0xC005F,
        CommonResetAudioAlgorithmParams = 0xC0060,
        CommonGetMuteLightEnhancement = 0xC0061,
        CommonSetMuteLightEnhancement = 0xC0062,
        CommonAudioRecordStart = 0xC0063,
        CommonAudioRecordStop = 0xC0064,
        CommonAudioRecordData = 0xBFFE5,//0xC0065,下位机错误
        CommonGetAFCalibration = 0xC0066,
        CommonGetDDRCapacity = 0xC0067,
        CommonSetPanTiltZoom = 0xC0068,
        CommonGetPanTiltZoom = 0xC0069,
        CommonGetDumpInfo = 0xC006A,
        CommonSetAlgoEnable = 0xC006B,
        CommonGetAlgoEnable = 0xC006C,
        CommonSetHidCall = 0xC006D,
        CommonGetHidCall = 0xC006E,
        CommonSetUsbSpeedMode = 0xC006F,
        CommonGetUsbSpeedMode = 0xC0070,
        CommonGetBootInfo = 0xC0071,
        CommonSetBootErase = 0xC0072,
        CommonSetPrivacyEnable = 0xC0073,
        CommonGetPrivacyEnable = 0xC0074,
        CommonSetNDIState = 0xC0101,
        CommonGetNDIState = 0xC0102,
        CommonGetIPConfig = 0xC0103,
        CommonSetDHCPState = 0xC0104,
        CommonGetDHCPState = 0xC0105,
        CommonSetIPAddress = 0xC0106,
        CommonGetIPAddress = 0xC0107,
        CommonSetNetMask = 0xC0108,
        CommonGetNetMask = 0xC0109,
        CommonSetGateway = 0xC010A,
        CommonGetGateway = 0xC010B,
        CommonSetMacAddress = 0xC010C,
        CommonGetMacAddress = 0xC010D,
        CommonSetLensCenter = 0xC010E,
        CommonSetConfigJson = 0xC010F,
        CommonGetConfigJson = 0xC0110,
        CommonSetFbfParamsStart = 0xC0111,
        CommonSetFbfParamsData = 0xC0112,
        CommonSetFbfParamsStop = 0xC0113,
        CommonGetFbfParams = 0xC0114,
        CommonGetRegionTracking = 0xC0115,
        CommonSetRegionTracking = 0xC0116,
        CommonPauseAiAlgThrd = 0xC0117,
        CommonContinueAiAlgThrd = 0xC0118,
        CommonSetNoTargetStrategyState = 0xC0119,
        CommonGetNoTargetStrategyState = 0xC011A,
        CommonSetStartupPosition = 0xC011B,
        CommonGetStartupPosition = 0xC011C,
        CommonSetMenuBarLanguage = 0xC011D,
        CommonGetMenuBarLanguage = 0xC011E,
        CommonGetRS232TestResult = 0xC011F,
        CommonTestKey = 0xC0120,
        CommonTestLed = 0xC0121,
        CommonSetDebugJson = 0xC0122,
        CommonGetDebugJson = 0xC0123,
        CommonSetViscaUdpPort = 0xC0124,
        CommonGetViscaUdpPort = 0xC0125,
        CommonSetHorTrackingStrategy = 0xC0126,
        CommonGetHorTrackingStrategy = 0xC0127,
        CommonSetVerTrackingStrategy = 0xC0128,
        CommonGetVerTrackingStrategy = 0xC0129,
        CommonSetImageStyle = 0xC0132,
        CommonGetImageStyle = 0xC0133,
        CommonSetPiPMode = 0xC0134,
        CommonGetPiPMode = 0xC0135,
        CommonSetAudioMuteState = 0xC0138,
        CommonGetAudioMuteState = 0xC0139,
        CommonSetDefaultConfigJson = 0xC013A,
        CommonGetDefaultConfigJson = 0xC013B,
        CommonSetRTSPStreamURL = 0xC013C,
        CommonGetRTSPStreamURL = 0xC013D,
        CommonGetLogData =    0xC013E,
        CommonGetStreamMediaStatus = 0xC013F,
        CommonTestSDCardState = 0xC0140,
        CommonTestWIFIState = 0xC0141,
        CommonTestBluetoothState = 0xC0142,
        CommonTestBadFlashBlock = 0xC0143,
        CommonSetVerticalScreenMode = 0xC0144,
        CommonGetVerticalScreenMode = 0xC0145,
        CommonSetAutoFocusState = 0XC0146,
        CommonGetAutoFocusState = 0XC0147,
        CommonGetBluetoothMacAddr = 0xC0148,
        CommonGetDereverationAlgParam = 0xC0149,
        CommonSetDereverationAlgParam = 0xC014A,
        CommonSetBlueToothBQBMode = 0xC0154,
        CommonSetBlueToothRestore = 0xC0155,
        CommonSetBlueToothName = 0xC0156,
        CommonGetBlueToothName = 0xC0157,
        CommonGetBatteryCap = 0xC0158,
        CommonAudioInputDetect = 0xC0159,
        CommonSetManualFocusPosition = 0xC015F,
        CommonGetManualFocusPosition = 0xC0160,
        CommonGetExternalSpeakerConfigJson = 0xC0162,
        CommonSetExternalSpeakerConfigJson = 0xC0163,
        CommonGetConfigJsonSubIndex = 0xC0164,
        CommonSetConfigJsonSubIndex = 0xC0165,
        CommonGetExternalDeviceInfo = 0xC0166,
        // tail
        CommonSetTailWiFiSSID = 0xCFF36,
        CommonGetTailWiFiSSID = 0xC0167,
        CommonGetTipsStatus = 0xC0179,
        CommonSetTipsStatus = 0xC017A,
        CommonGetPositionNumberJson = 0xC017C,
        CommonSetPositionNumberJson = 0xC017D,
        CommonGetDanteDevicelic	= 0xC0181,
        CommonGetDanteManufacturer = 0xC0182,
        CommonSetDanteDevicelic = 0xC0183,
        CommonSetDanteManufacturer = 0xC0184,
        CommonSetConfigJsonBySn = 0xC0186,
        CommonGetConfigJsonBySn = 0xC0187,
        CommonGetSightAngle = 0xC0190,
        CommonSetSightAngle = 0xC0191,
        CommonGetAllChannelScanState = 0xC0192,
        CommonSetAllChannelScanState = 0xC0193,
        CommonAudioBeamReport = 0xC0201,
        CommonSetFrameRateSwitch = 0xC0202,
        CommonGetFrameRateSwitch = 0xC0203,
        CommonSetAutoShutDown = 0xC0205,
        CommonGetAutoShutDown = 0xC0206,
        CommonSetDefaultVolume = 0xC0207,
        CommonGetDefaultVolume = 0xC0208,
        CommonGetAudioEqParam = 0xC0209,
        CommonSetAudioEqParam = 0xC020A,
        CommonSetAudioEqMode = 0xC020B,
        CommonGetAudioEqMode = 0xC020C,
        CommonSetWatermark = 0xC020D,
        CommonGetWatermark = 0xC020E,
        CommonSetComboKey = 0xC020F,
        CommonGetComboKey = 0xC0210,
    };
}

#endif // !AXDP_PROTOCOL_DEFINES_H_
