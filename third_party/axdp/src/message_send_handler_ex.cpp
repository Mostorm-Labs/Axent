#include "device_accessor_impl.h"
#include "message_sender.h"
#include "log_utils.h"
#include "axdp_utils.h"

namespace axdp {
    typedef std::shared_ptr<ProtocolMessage> MsgPtr;


    int DeviceAccessorImpl::setMicUsed(uint32_t mic_index)
    {
        uint32_t tmp = utils::htonl(mic_index);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetMicUsed, 2, (uint8_t*)&tmp, sizeof(tmp)
            ));
        return send(msg);
    }

    int DeviceAccessorImpl::startAudioRecord(uint32_t mic_mask)
    {
        uint32_t tmp = utils::htonl(mic_mask);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonAudioRecord, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        if (mic_mask != 0)
        {
            int mic_number = 0;
            for (size_t i = 0; i < 32; i++)
            {
                int tmp = (1 << i);
                int a = (mic_mask & tmp);
                if (a > 0)
                {
                    mic_number++;
                }
            }
        }
        return send(msg);
    }

    int DeviceAccessorImpl::setTestResult(uint32_t test_index, uint32_t test_result)
    {
        uint32_t tmp[2] = { 0 };
        tmp[0] = utils::htonl(test_index);
        tmp[1] = utils::htonl(test_result);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetTestResult, 2, (uint8_t*)&tmp[0], 8
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getTestResult(uint32_t test_index)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetTestResult, 2
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setEncryptedInfoHardwareId(const char* hwid, uint32_t bytes)
    {
        uint8_t tmp[MAX_HARDWARE_ID_LENGTH + 4] = { 0 };
        uint32_t flash_block = utils::htonl(FLASH_BLOCK_CAM_HWID);
        memcpy(tmp, &flash_block, 4);
        memcpy(tmp + 4, hwid, bytes);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetEncryptedInfo, 2, tmp, bytes + 4
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::setEncryptedInfoSerialNumber(const char* sn, uint32_t bytes)
    {
        uint8_t tmp[MAX_SERIAL_NUMBER_LENGTH+4] = { 0 };
        uint32_t flash_block = utils::htonl(FLASH_BLOCK_CAM_SN);
        memcpy(tmp, &flash_block, 4);
        memcpy(tmp + 4, sn, bytes);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetEncryptedInfo, 2, tmp, bytes + 4
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getEncryptedInfo(uint32_t state)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetEncryptedInfo, 2
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::testAudioConsistency(TestTaskCommand cmd)
    {
        uint32_t tmp = utils::htonl((int32_t)cmd);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestAudioConsistency, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::testNetworkPort(TestTaskCommand cmd)
    {
        uint32_t tmp = utils::htonl((int32_t)cmd);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestNetworkPort, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getDeviceUniqueId(const char* lic_content, uint32_t bytes)
    {
        //ProtocolMessage msg((uint16_t)CommonProtocol::GetDeviceUniqueId, 2);
        //msg.setPayload((uint8_t*)lic_content, bytes);
        //return pImpl()->sendMessage(msg);
        return 0;
        //todo : implement here
    }

    int DeviceAccessorImpl::setDeviceUniqueId()
    {
        return 0;
    }

    int DeviceAccessorImpl::getAlgAuthContent()
    {
        return 0;
    }

    int DeviceAccessorImpl::setAlgAuthContent(const char* auth_content, uint32_t bytes)
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAlgAuthContent, 2, (uint8_t*)&auth_content, bytes
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::setAFCalibration()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetAFCalibration, 2
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getAFCalibrationResult()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetAFCalibration, 2
        ));
        return send(msg);
    }

    int DeviceAccessorImpl::getDDRCapacity()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetDDRCapacity, 2
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::setLensCenter(uint32_t value)
    {
        uint32_t tmp = utils::htonl(value);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonSetLensCenter, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::getRS232TestResult()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonGetRS232TestResult, 2
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testKey(TestTaskCommand cmd)
    {
        int32_t flag = 0;
        if (cmd == TestTaskCommand::CommandStart)
        {
            flag = 0;
        }
        if (cmd == TestTaskCommand::CommandResult)
        {
            flag = 1;
        }
        uint32_t tmp = utils::htonl(flag);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestKey, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testLed(TestTaskCommand cmd)
    {
        int32_t flag = 0;
        if (cmd == TestTaskCommand::CommandStart)
        {
            flag = 0;
        }
        if (cmd == TestTaskCommand::CommandResult)
        {
            flag = 1;
        }
        uint32_t tmp = utils::htonl(flag);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestLed, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testSDCardState()
    {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestSDCardState, 2
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testWIFIState(TestTaskCommand cmd) {
        uint32_t tmp = utils::htonl((int32_t)cmd);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestWIFIState, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testBluetoothState(TestTaskCommand cmd) {
        uint32_t tmp = utils::htonl((int32_t)cmd);
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestBluetoothState, 2, (uint8_t*)&tmp, sizeof(tmp)
        ));
        return send(msg);
    }
    int DeviceAccessorImpl::testBadFlashBlock() {
        MsgPtr msg(axdp::MessageBuilder::pack(
            UniCmd::CommonTestBadFlashBlock, 2
        ));
        return send(msg);
    }
}
