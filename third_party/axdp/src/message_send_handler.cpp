#include "device_accessor_impl.h"
#include "message_sender.h"
#include "log_utils.h"
#include "axdp_utils.h"

namespace axdp {
    typedef std::shared_ptr<ProtocolMessage> MsgPtr;


    int DeviceAccessorImpl::sendUpgradeBlockInfo(UniCmd cmd) {
        dfu_helper_.resetBlockStat2();

#if 0
        //original
        const BlockInfo& block_info = dfu_helper_.currnetBlockInfo();
        MsgPtr msg(axdp::MessageBuilder::pack(
                cmd,
                dfu_helper_.dst(),
                (uint8_t *) &block_info,
                sizeof(BlockInfo)
        ));
#else
        //For amp10 compitability
        MsgPtr msg(axdp::MessageBuilder::pack(
            cmd,
            dfu_helper_.dst(),
            dfu_helper_.currentBlockInfoData(),
            dfu_helper_.currentBlockInfoLength()
        ));
#endif

        int ret = 0;// send(msg);
        if (sender_ != nullptr) {
            ret = sender_->send(msg->data(), msg->length());
        }
        //if (ret <= 0)
        //{
        //	LOGV("When Block 0x%x send, write err occured: %d",
        //		block_info.flash_block, ret);
        //}
        return ret;
    }

    int DeviceAccessorImpl::sendUpgradeSliceData(UniCmd cmd) {
        const SliceData &slice_data = dfu_helper_.currentSlice();

#if 0
        MsgPtr msg(axdp::MessageBuilder::pack(
                cmd,
                dfu_helper_.dst(),
                (uint8_t *) &slice_data,
                dfu_helper_.currentSliceLen()
        ));
#else
        MsgPtr msg(axdp::MessageBuilder::pack(
            cmd,
            dfu_helper_.dst(),
            dfu_helper_.currentSliceData(),
            dfu_helper_.currentSliceDataLength()
        ));
#endif
        int ret = 0;// send(msg);
        //std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
        if (sender_ != nullptr) {
            ret = sender_->send(msg->data(), msg->length());
        }
        //std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
        //auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
        //LOGD("SEND UPGRADE DATA EX DURATION = %ld， send slice len = %d", delta, ret);
        //msg.length = 253
        //slice len = 235
        //if (ret != 0)
        //{
        //	LOGV("When slice %u send, write err occured: %d",
        //		utils::ntohl(slice_data.slice_index), ret);
        //}
        //else
        //{
        //	LOGV("When slice %u send, PRINT THIS length %d",
        //		utils::ntohl(slice_data.slice_index), ret);
        //}
        return ret;
    }

    int AlphaDeviceAccessor::resetDevice() {
        return 0;
    }

    int AlphaDeviceAccessor::getDeviceInfo() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::AlphaDeviceInfo, 2));
        return send(msg);
    }

    int AlphaDeviceAccessor::getDeviceType() {
        return 0;
    }

    int AlphaDeviceAccessor::startUpgrade() {
        dfu_helper_.resetProgress();
        return sendUpgradeInfo();
    }

    int AlphaDeviceAccessor::startUpgradeEx() {
        return 0;
    }

    int AlphaDeviceAccessor::sendUpgradeInfo() {
        dfu_helper_.resetReadyFlags(dfu_helper_.dst());
        int ret = sendUpgradeBlockInfo(UniCmd::AlphaUpgradeInfo);
        return ret > 0 ? 0 : ret;
    }

    int AlphaDeviceAccessor::sendUpgradeInfoEx() {
        return 0;
    }

    int AlphaDeviceAccessor::sendUpgradeData() {
        return sendUpgradeSliceData(UniCmd::AlphaUpgradeData);
    }

    int AlphaDeviceAccessor::sendUpgradeDataEx() {
        return 0;
    }

    int AlphaDeviceAccessor::stopUpgrade() {
        return 0;
    }

    const DeviceInfoList &AlphaDeviceAccessor::syncGetDeviceInfo() {
        std::unique_lock<std::mutex> lock(mutex_);
        dev_info_list_.clear();
        task_state_mgr_.setTaskState(UniCmd::AlphaDeviceInfo, TaskState::SyncWaiting);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::AlphaDeviceInfo, kDefaultDst));
        int ret = send(msg);
        if (ret == Success) {
            auto cv_status = cond_var_.wait_for(lock,
                                                std::chrono::milliseconds(default_timeout_ms_),
                                                [&, this]()mutable noexcept -> bool {
                                                    return task_state_mgr_.isTaskReady(UniCmd::AlphaDeviceInfo);
                                                });
            if (cv_status) {

            }
            else {
                //record error;
            }
        } else {
            //record error;
        }
        task_state_mgr_.setTaskState(UniCmd::AlphaDeviceInfo, TaskState::Async);
        return dev_info_list_;
    }

    int BetaDeviceAccessor::resetDevice() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::BetaDeviceReset, kBroadcastDst));
        return send(msg);
    }

    int BetaDeviceAccessor::getDeviceInfo() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::BetaDeviceInfo, kBroadcastDst));
        return send(msg);
    }

    const DeviceInfoList& BetaDeviceAccessor::syncGetDeviceInfo() {
        //DeviceInfoList result;
        std::unique_lock<std::mutex> lock(mutex_);
        dev_info_list_.clear();
        task_state_mgr_.setTaskState(UniCmd::BetaDeviceInfo, TaskState::SyncWaiting);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::BetaDeviceInfo, kBroadcastDst));
        int ret = send(msg);
        if (ret == Success) {
            auto cv_status = cond_var_.wait_for(lock,
                std::chrono::milliseconds(default_timeout_ms_),
                [&, this]()mutable noexcept -> bool {
                    return task_state_mgr_.isTaskReady(UniCmd::BetaDeviceInfo);
                });
            if (cv_status) {

            }
            else {
                //record error;
            }
        } else {
            //record error;
        }
        task_state_mgr_.setTaskState(UniCmd::BetaDeviceInfo, TaskState::Async);
        return dev_info_list_;
    }

    int BetaDeviceAccessor::getDeviceType() {
        return 0;
    }

    int BetaDeviceAccessor::startUpgrade() {
        uint16_t dst = dfu_helper_.dst();
        dfu_helper_.resetProgress();
        dfu_helper_.resetReadyFlags(dst);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::BetaStartUpgrade, kBroadcastDst
        ));
        return send(msg);
    }

    int BetaDeviceAccessor::startUpgradeEx() {
        dfu_helper_.resetProgress();
        return sendUpgradeInfoEx() > 0 ? 0 : -1;
    }

    int BetaDeviceAccessor::sendUpgradeInfo() {
        dfu_helper_.resetReadyFlags(dfu_helper_.dst());
        return sendUpgradeBlockInfo(UniCmd::BetaUpgradeInfo);
    }

    int BetaDeviceAccessor::sendUpgradeInfoEx() {
        return sendUpgradeBlockInfo(UniCmd::BetaUpgradeInfoEx);
    }

    int BetaDeviceAccessor::sendUpgradeData() {
        dfu_helper_.resetReadyFlags(dfu_helper_.dst());
        return sendUpgradeSliceData(UniCmd::BetaUpgradeData);
    }

    int BetaDeviceAccessor::sendUpgradeDataEx() {
        return sendUpgradeSliceData(UniCmd::BetaUpgradeDataEx);
    }

    int BetaDeviceAccessor::stopUpgrade() {
        uint32_t success = utils::ntohl((uint32_t) 0);
        dfu_helper_.resetReadyFlags(dfu_helper_.dst());
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::BetaStopUpgrade, kBroadcastDst,
                (uint8_t *) &success, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setNoiseSuppressionLevel(DegreeLevel level) {
        uint8_t tmp = static_cast<uint8_t>(level);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetNoiseSuppressionLevel, kBroadcastDst,
                &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getNoiseSuppressionLevel() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetNoiseSuppressionLevel, kBroadcastDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setReverberationSuppressionLevel(DegreeLevel level) {
        uint8_t tmp = static_cast<uint8_t>(level);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetReverberationSuppressionLevel, kBroadcastDst,
                &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getReverberationSuppressionLevel() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetReverberationSuppressionLevel, kBroadcastDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setEchoCancellationLevel(DegreeLevel level) {
        uint8_t tmp = static_cast<uint8_t>(level);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetEchoCancellationLevel, kBroadcastDst,
                &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getEchoCancellationLevel() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetEchoCancellationLevel, kBroadcastDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::resetAudioAlgorithmParams() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonResetAudioAlgorithmParams, kBroadcastDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::startAudioRecord(uint16_t time_second) {
        uint16_t tmp = utils::htons(time_second);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonAudioRecordStart, kBroadcastDst,
                (uint8_t *) &tmp, sizeof(tmp)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::stopAudioRecord() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonAudioRecordStop, kBroadcastDst
        ));
        return send(msg);
    }


    int DeviceAccessorImpl::getHidCallState() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetHidCall, kBroadcastDst
        ));
        return send(msg);
    }


    int DeviceAccessorImpl::setHidCallState(EnableState state) {
        uint8_t tmp = (state == EnableState::Disabled) ? 0 : 1;
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetHidCall, kBroadcastDst,
                (uint8_t *) &tmp, sizeof(tmp)
        ));
        return send(msg);
    }


    int DeviceAccessorImpl::setVideoMode(VideoMode mode) {
        uint32_t tmp = utils::htonl(uint32_t(mode));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetVideoMode, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getVideoMode() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetVideoMode, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getUacState(){
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetUacState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setUacState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetUacState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setVideoTrackMode(VideoTrackMode mode) {
        uint32_t tmp = utils::htonl(uint32_t(mode));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetVideoTrackMode, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getVideoTrackMode() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetVideoTrackMode, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setMirrorState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetMirrorState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getMirrorState() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetMirrorState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setSpeakerTrackDelay(uint32_t second) {
        uint32_t tmp = utils::htonl(uint32_t(second));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetSpeakerTrackDelay, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getSpeakerTrackDelay() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetSpeakerTrackDelay, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setSplitScreenNumber(uint32_t number) {
        uint32_t tmp = utils::htonl(uint32_t(number));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetSplitScreenNumber, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getSplitScreenNumber() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetSplitScreenNumber, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setPowerLineFreq(PowerlineFreqType freq) {
        uint8_t tmp = uint8_t(freq);
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetPowerLineFreq, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getPowerLineFreq() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetPowerLineFreq, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setFlipState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetFlipState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getFlipState() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetFlipState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setWatermark(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetWatermark, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getWatermark() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetWatermark, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setPiPMode(EnableState mode) {
        uint32_t tmp = utils::htonl(uint32_t(mode));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetPiPMode, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getPiPMode() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetPiPMode, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setConfigJson(const char* cfg, size_t len, uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetConfigJson, dst,
            (uint8_t*)cfg, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getConfigJson(uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetConfigJson, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::syncGetTask(UniCmd task_cmd, uint16_t dst, int32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        int result = 0;
        task_state_mgr_.setTaskState(task_cmd, TaskState::SyncWaiting);
        MsgPtr msg(axdp::MessageBuilder::pack(task_cmd, dst));
        int ret = send(msg);
        if (ret == Success) {
            auto cv_status = cond_var_.wait_for(lock,
                std::chrono::milliseconds(timeout_ms),
                [&, this]()mutable noexcept -> bool {
                    return task_state_mgr_.isTaskReady(task_cmd);
                });
            if (cv_status) {
                //success get content
            }
            else {
                //record error;
                result = -1;
            }
        }
        else {
            //record error;
            result = -2;
        }
        task_state_mgr_.setTaskState(task_cmd, TaskState::Async);
        return result;
    }

    const std::string& DeviceAccessorImpl::syncGetConfigJson(uint16_t dst)
    {
        //DeviceInfoList result;
        UniCmd task_cmd = UniCmd::CommonGetConfigJson;
        config_json_.clear();
        int ret = syncGetTask(UniCmd::CommonGetConfigJson, dst, default_timeout_ms_);
        if (ret == 0)
        {

        }
        return config_json_;
    }

    int DeviceAccessorImpl::setDefaultConfigJson(uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDefaultConfigJson, dst
            ));
        return send(msg);
    }
    int DeviceAccessorImpl::getDefaultConfigJson(uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDefaultConfigJson, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setAudioMuteState(bool mute, uint16_t dst){
        char json[64];
        int len = sprintf(json, "{\"mute\":%d }", mute == false ? 0 : 1);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAudioMuteState, dst, (uint8_t*)json, len
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::getAudioMuteState(uint16_t dst) {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAudioMuteState, dst
        ));
        return send(msg);
    };

    int DeviceAccessorImpl::getDereverationAlgParam() {
        MsgPtr msg(axdp::MessageBuilder::pack(UniCmd::CommonGetDereverationAlgParam, 2));
        return send(msg);
    }

    int DeviceAccessorImpl::setDereverationAlgParam(int32_t val) {
        uint32_t tmp = utils::htonl(uint32_t(val));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDereverationAlgParam, 2,
                                              (uint8_t*)&tmp, sizeof(tmp)));
        return send(msg);
    }

    int DeviceAccessorImpl::reboot() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetReboot, kDefaultDst
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::getUsbSpeedMode() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetUsbSpeedMode, kDefaultDst));
        return send(msg);
    }
    int DeviceAccessorImpl::setUsbSpeedMode(int32_t val) {
        uint8_t tmp = val;
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetUsbSpeedMode, kDefaultDst,
                &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::getMuteLightEnhancement() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetMuteLightEnhancement, kBroadcastDst));
        return send(msg);
    }
    int DeviceAccessorImpl::setMuteLightEnhancement(int32_t val) {
        uint8_t tmp = val;
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetMuteLightEnhancement, kBroadcastDst,
                       &tmp, sizeof(uint8_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getOsdMirrorState() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetOsdMirrorState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setOsdMirrorState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetOsdMirrorState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getPrivacyEnable() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetPrivacyEnable, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setPrivacyEnable(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetPrivacyEnable, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::getWdrState() {
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetWdrState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setWdrState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetWdrState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::setRtspStreamUrl(const char* url, size_t len){
        if (url == NULL || len < 0)
        {
            return -1;
        }
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetRTSPStreamURL, kDefaultDst,
                (uint8_t *) &url, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getRtspStreamUrl(){
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetRTSPStreamURL, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getIPConfig(){
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonGetIPConfig, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setDHCPState(uint8_t idx, EnableState flag){
        uint8_t tmp[2] = { 0,0 };
        tmp[0] = idx;
        tmp[1] = (uint8_t)flag;
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDHCPState, kDefaultDst,
            (uint8_t *) &tmp, sizeof(tmp))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getDHCPState(uint8_t idx){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDHCPState, kDefaultDst,
            (uint8_t *) &idx, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::setIPAddress(uint8_t idx, uint32_t ip_addr){
        uint8_t tmp[5];
        tmp[0] = idx;
        memcpy(tmp + 1, (uint8_t*)&ip_addr, 4);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetIPAddress, kDefaultDst,
            (uint8_t *) &tmp, sizeof(tmp))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getIPAddress(uint8_t idx){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetIPAddress, kDefaultDst,
            (uint8_t *) &idx, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::setNetmask(uint8_t idx, uint32_t netmask){
        uint8_t tmp[5];
        tmp[0] = idx;
        memcpy(tmp + 1, (uint8_t*)&netmask, 4);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetNetMask, kDefaultDst,
            (uint8_t *) &tmp, sizeof(tmp))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getNetmask(uint8_t idx){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetNetMask, kDefaultDst,
            (uint8_t *) &idx, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::setGateway(uint8_t idx, uint32_t gateway){
        uint8_t tmp[5];
        tmp[0] = idx;
        memcpy(tmp + 1, (uint8_t*)&gateway, 4);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetGateway, kDefaultDst,
            (uint8_t *) &tmp, sizeof(tmp))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getGateway(uint8_t idx){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetGateway, kDefaultDst,
            (uint8_t *) &idx, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::setMacAddress(uint8_t idx, uint64_t mac_addr){
        uint8_t tmp[7];
        tmp[0] = idx;
        //*((uint64_t*)&tmp[1]) = mac_addr;
        tmp[1] = *((uint8_t*)&mac_addr + 5);
        tmp[2] = *((uint8_t*)&mac_addr + 4);
        tmp[3] = *((uint8_t*)&mac_addr + 3);
        tmp[4] = *((uint8_t*)&mac_addr + 2);
        tmp[5] = *((uint8_t*)&mac_addr + 1);
        tmp[6] = *((uint8_t*)&mac_addr + 0);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetMacAddress, kDefaultDst,
            (uint8_t *) &tmp, 7)
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getMacAddress(uint8_t idx){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetMacAddress, kDefaultDst,
            (uint8_t *) &idx, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::setViscaUdpPort(uint32_t port){
        uint32_t tmp = utils::htonl(uint32_t(port));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetViscaUdpPort, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getViscaUdpPort(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetViscaUdpPort, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setManualFocusPosition(uint32_t port){
        uint32_t tmp = utils::htonl(uint32_t(port));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetManualFocusPosition, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getManualFocusPosition(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetManualFocusPosition, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::audioInputDetect(EnableState state){
        uint8_t tmp = (state == EnableState::Disabled) ? 0 : 1;
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonAudioInputDetect, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint8_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::audioBeamReport(EnableState state){
        uint8_t tmp = (state == EnableState::Disabled) ? 0 : 1;
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonAudioBeamReport, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint8_t))
                   );
        return send(msg);
    }

    int DeviceAccessorImpl::setBTRestore(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetBlueToothRestore, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getBatteryCap(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetBatteryCap, kDefaultDst
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::setExternalSpeakerConfigJson(const char* cfg, size_t len, uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetExternalSpeakerConfigJson, dst,
            (uint8_t*)cfg, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getExternalSpeakerConfigJson(uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetExternalSpeakerConfigJson, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setImageStyle(uint32_t style)
    {
        uint32_t tmp = utils::htonl(uint32_t(style));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetImageStyle, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getImageStyle()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetImageStyle, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getConfigJsonSubIndex(const uint8_t* sub_idx, size_t idx_len)
    {
        subindex_header_s idx = { 0, utils::htons(0x10), utils::htons(32), 0 };
        memcpy(idx.sub_idx, sub_idx, idx_len > 32 ? 32 : idx_len);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetConfigJsonSubIndex, kSubIndexDst,
        (uint8_t*)&idx, sizeof(subindex_header_s)));
        return send(msg);
    }

    int DeviceAccessorImpl::setConfigJsonSubIndex(const char* cfg, size_t len, const uint8_t* sub_idx, size_t idx_len)
    {
        subindex_header_s idx = { 0, utils::htons(0x10), utils::htons(32), 0 };
        memcpy(idx.sub_idx, sub_idx, idx_len > 32 ? 32 : idx_len);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetConfigJsonSubIndex,
            kSubIndexDst, (uint8_t*)cfg, len, (uint8_t*)&idx, sizeof(subindex_header_s)));
        return send(msg);
    }

    int DeviceAccessorImpl::setTailWifiSSID(const char* ssid, int length) {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetTailWiFiSSID, kDefaultDst,
            (uint8_t*)ssid, length
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getTailWifiSSID() {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetTailWiFiSSID, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setFbfParam(const char* file, uint16_t dst) {
        char md5_hex[64] = {0};
        char info_buffer[128];
        int ret = fbf_helper_.loadLocalFile(file);
        if (ret) return ret;
        fbf_helper_.resetReadyFlag();
        fbf_helper_.setFbfDeviceDst(dst);
        fbf_helper_.getFileMd5(md5_hex);
        if (cb_) cb_->onSetFbfParamState(UpgradeState::DataReady,
                                         fbf_helper_.getFbfDeviceDst());
        int info_len = sprintf(info_buffer, "{\"md5\":\"%s\",\"size\":%lld}",
                               md5_hex, fbf_helper_.getFileSize());
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetFbfParamsStart,
            dst, (uint8_t*)info_buffer, info_len));
        return send(msg);
    }

    int DeviceAccessorImpl::getFbfParam(uint16_t dst) {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetFbfParams, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::sendFbfParamData(const uint8_t* buf, size_t len, uint16_t dst) {
        fbf_helper_.resetReadyFlag();
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetFbfParamsData,
            dst, (uint8_t*)buf, len));
        return send(msg);
    }

    int DeviceAccessorImpl::setFbfParamEnd(uint16_t dst) {
        fbf_helper_.resetReadyFlag();
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetFbfParamsStop, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getExternalDeviceInfo()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetExternalDeviceInfo, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getPositionNumberJson(uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetPositionNumberJson, dst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setPositionNumberJson(const char* cfg, size_t len, uint16_t dst)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetPositionNumberJson, dst,
            (uint8_t*)cfg, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setDanteDevicelic(const char *str, size_t len)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDanteDevicelic, kDefaultDst,
            (uint8_t*)str, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getDanteDevicelic()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDanteDevicelic, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setDanteManufacturer(const char *str, size_t len)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDanteManufacturer, kDefaultDst,
            (uint8_t*)str, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getDanteManufacturer()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDanteManufacturer, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setConfigJsonBySn(const char* cfg, size_t len)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetConfigJsonBySn, kDefaultDst,
            (uint8_t*)cfg, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getConfigJsonBySn(const char* cfg, size_t len)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetConfigJsonBySn, kDefaultDst,
            (uint8_t*)cfg, len
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getSightAngle(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetSightAngle, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setSightAngle(int32_t angle){
        uint32_t tmp = utils::htonl(uint32_t(angle));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetSightAngle, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
        );
        return send(msg);
    }

    int DeviceAccessorImpl::getAllChannelScanState(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAllChannelScanState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setAllChannelScanState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetAllChannelScanState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getVerticalScreenMode(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetVerticalScreenMode, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setVerticalScreenMode(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetVerticalScreenMode, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getAutoFocusState(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAutoFocusState, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setAutoFocusState(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetAutoFocusState, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getStartupPosition(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetStartupPosition, kDefaultDst
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setStartupPosition(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
                UniCmd::CommonSetStartupPosition, kDefaultDst,
                (uint8_t *) &tmp, sizeof(uint32_t)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getFrameRateSwitch(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetFrameRateSwitch, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setFrameRateSwitch(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetFrameRateSwitch, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t)
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::getComboKey(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetComboKey, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setComboKey(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetComboKey, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t)
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::getAutoShutDown(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAutoShutDown, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setAutoShutDown(int32_t state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAutoShutDown, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t)
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::getTipsStatus(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetTipsStatus, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setTipsStatus(EnableState state) {
        uint32_t tmp = utils::htonl(uint32_t(state));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetTipsStatus, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t)
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::getDefaultVolume(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDefaultVolume, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setDefaultVolume(int32_t angle){
        uint32_t tmp = utils::htonl(uint32_t(angle));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetDefaultVolume, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
                   );
        return send(msg);
    }

    int DeviceAccessorImpl::getAudioEqParam(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAudioEqParam, kDefaultDst
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::setAudioEqParam(uint32_t angle){
        uint32_t tmp = utils::htonl(uint32_t(angle));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAudioEqParam, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
                   );
        return send(msg);
    }

    int DeviceAccessorImpl::setAudioEqMode(int32_t angle){
        uint32_t tmp = utils::htonl(uint32_t(angle));
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAudioEqMode, kDefaultDst,
            (uint8_t *) &tmp, sizeof(uint32_t))
                   );
        return send(msg);
    }

    int DeviceAccessorImpl::getAudioEqMode(){
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAudioEqMode, kDefaultDst
            ));
        return send(msg);
    }

}
