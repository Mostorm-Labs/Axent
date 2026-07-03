#include "protocol_message.h"

#include <memory>
#include "axdp_utils.h"
#include "third_party/crc/include/checksum.h"
#include "third_party/md5/include/md5.h"
#include "string_buffer.h"
#include "log_utils.h"

namespace axdp {
    static void pack_proc(uint8_t *msg_buf, uint16_t msg_size,
                          uint16_t cmd, uint8_t *payload, uint16_t payload_len,
                          uint8_t *sub_idx, uint16_t idx_len,
                          uint16_t dst, uint16_t src) {
        uint8_t *buffer = msg_buf;
        if (buffer == nullptr) return;
        //trans head_ to net endian
        *(uint16_t *) (buffer + kMagicNumberOffset) = utils::htons(kMagicNumber);
        *(uint16_t *) (buffer + kVersionOffset) = utils::htons(kProtocolVersion);
        *(uint16_t *) (buffer + kDestinationOffset) = utils::htons(dst);
        *(uint16_t *) (buffer + kSourceOffset) = utils::htons(src);
        *(uint16_t *) (buffer + kCommandOffset) = utils::htons(cmd);
        *(uint16_t *) (buffer + kPayloadLenOffset) = utils::htons(payload_len + idx_len);
        *(uint16_t *) (buffer + kCrcOffset) = utils::htons(0);//for crc calculate
        if (idx_len > 0 && sub_idx != nullptr)
            memcpy(buffer + kPayloadOffset, sub_idx, idx_len);
        if (payload_len > 0 && payload != nullptr)
            memcpy(buffer + kPayloadOffset + idx_len, payload, payload_len);
        uint16_t msg_crc = crc_xmodem((const unsigned char *) buffer, msg_size);
        *(uint16_t *) (buffer + kCrcOffset) = utils::htons(msg_crc);
    }

    ProtocolMessage::ProtocolMessage(uint16_t cmd, uint8_t *payload, uint16_t bytes, uint16_t dst, uint16_t src) {
        size_ = kHeaderSize + bytes;
        data_ = new uint8_t[size_];
        head_ = new Header(this);
        payload_ = new Payload(this);
        pack_proc(data_, size_, cmd, payload, bytes, nullptr, 0, dst, src);
    }

    ProtocolMessage::ProtocolMessage(uint16_t cmd, uint8_t *payload, uint16_t bytes,
                                     uint8_t *sub_idx, uint16_t idx_len, uint16_t dst, uint16_t src) {
        size_ = kHeaderSize + bytes + idx_len;
        data_ = new uint8_t[size_];
        head_ = new Header(this);
        payload_ = new Payload(this);
        pack_proc(data_, size_, cmd, payload, bytes, sub_idx, idx_len, dst, src);
    }


    ProtocolMessage::ProtocolMessage(uint16_t bytes) {
        size_ = kHeaderSize + bytes;
        data_ = new uint8_t[size_];
        head_ = new Header(this);
        payload_ = new Payload(this);
    }

    ProtocolMessage::~ProtocolMessage() {
        if (payload_) delete payload_;
        if (head_) delete head_;
        if (data_) delete[] data_;
    }

    void ProtocolMessage::fomatPrint() const {
        StringBuffer buf;
        buf.format("magic:0x%X version:0x%X dst:0x%X src:0x%X cmd:0x%X len:0x%X crc:0x%X",
                   *(uint16_t *) (data_ + kMagicNumberOffset),
                   *(uint16_t *) (data_ + kVersionOffset),
                   *(uint16_t *) (data_ + kDestinationOffset),
                   *(uint16_t *) (data_ + kSourceOffset),
                   *(uint16_t *) (data_ + kCommandOffset),
                   *(uint16_t *) (data_ + kPayloadLenOffset),
                   *(uint16_t *) (data_ + kCrcOffset));//for crc calculate
        LOGV("%s\n", buf.c_str());
    }

    MessageParser::ParseState MessageParser::parse(const uint8_t *data, uint16_t bytes) {
        MessageParser::ParseState state = MessageParser::ParseState::UnknownError;
        int pos = 0;
        while (pos < bytes) {
            int consumed = 0;
            state = feed(data + pos, bytes - pos, &consumed);
            pos += consumed;
            if (state > MessageParser::ParseState::Done) {
                continue;
            }
            if (state <= MessageParser::ParseState::Done) {
                resetState();
                break;
            }
        }
        return state;
    }

    MessageParser::ParseState MessageParser::feed(const uint8_t *data, uint16_t len, int *consumed) {
        int pos = 0;
        while (pos < len) {
            switch (state_) {
                case MessageParser::ParseState::MagicFirst:
                    if (kMagicNumberFirstByte == data[pos]) {
                        state_ = MessageParser::ParseState::MagicSecond;
                    }
                    ++pos;
                    break;
                case MessageParser::ParseState::MagicSecond:
                    if (kMagicNumberSecondByte == data[pos]) {
                        state_ = MessageParser::ParseState::Header;
                        ++pos;
                        header_pos_ = 2;
                    } else {
                        state_ = MessageParser::ParseState::MagicFirst;
                    }
                    break;
                case MessageParser::ParseState::Header:
                    cache_header_[header_pos_] = data[pos];
                    ++pos;
                    ++header_pos_;
                    if (header_pos_ == kHeaderSize) {
                        uint16_t payload_len = utils::ntohs(*(uint16_t *) (cache_header_ + kPayloadLenOffset));
                        payload_len = aux_align((payload_len + 1), 16);
                        cache_msg_ = std::make_shared<ProtocolMessage>(payload_len);
                        cache_msg_->setVersion(utils::ntohs(*(uint16_t *) (cache_header_ + kVersionOffset)));
                        cache_msg_->setDst(utils::ntohs(*(uint16_t *) (cache_header_ + kDestinationOffset)));
                        cache_msg_->setSrc(utils::ntohs(*(uint16_t *) (cache_header_ + kSourceOffset)));
                        cache_msg_->setCmd(utils::ntohs(*(uint16_t *) (cache_header_ + kCommandOffset)));
                        cache_msg_->setLen(utils::ntohs(*(uint16_t *) (cache_header_ + kPayloadLenOffset)));
                        cache_msg_->setCrc(utils::ntohs(*(uint16_t *) (cache_header_ + kCrcOffset)));
                        *(uint16_t *) (cache_header_ + kCrcOffset) = 0;
                        compute_crc_ = crc_xmodem((const unsigned char *) cache_header_, kHeaderSize);
                        /*if (msg_.head_.len() > msg_.payload_.size()) {
                            msg_.payload_.realloc(aux_align((msg_.head_.len() + 1), 16));
                            if (nullptr == msg_.payload_.data()) {
                                msg_.payload_.setData(nullptr, 0);
                                state_ = MessageParser::ParseState::MemError;
                            }
                        }*/
                        if (cache_msg_->header().len() > 0) {
                            payload_pos_ = 0;
                            state_ = MessageParser::ParseState::Payload;
                        } else {
                            if (compute_crc_ != cache_msg_->header().crc()) {
                                state_ = MessageParser::ParseState::CrcError;
                            } else {
                                state_ = MessageParser::ParseState::Done;
                            }
                        }
                    }
                    break;
                case MessageParser::ParseState::Payload:
                    if (cache_msg_->payload().size() > 0) {
                        //msg_.payload_.data()[payload_pos_] = data[pos];
                        cache_msg_->copyPayloadData(data + pos, 1, payload_pos_);
                        ++pos;
                        ++payload_pos_;
                        if (payload_pos_ == cache_msg_->header().len()) {
                            compute_crc_ = crc_xmodem_update(
                                    (const unsigned char *) cache_msg_->payload().data(),
                                    payload_pos_,
                                    compute_crc_
                            );
                            if (compute_crc_ != cache_msg_->header().crc()) {
                                state_ = MessageParser::ParseState::CrcError;
                            } else {
                                state_ = MessageParser::ParseState::Done;
                            }
                        }
                    } else {
                        state_ = MessageParser::ParseState::MemError;
                    }
                    break;
                case MessageParser::ParseState::Done:
                    //goto end;
                    break;
                case MessageParser::ParseState::CrcError:
                    break;
                default:
                    break;
            }
            if (state_ <= MessageParser::ParseState::Done) {
                //resetState();
                break;
            }
        }
        *consumed = pos;
        return state_;
    }

    void MessageParser::resetState() {
        state_ = MessageParser::ParseState::MagicFirst;
        cache_header_[0] = kMagicNumberFirstByte;
        cache_header_[1] = kMagicNumberSecondByte;
    }


    ProtocolMessage *MessageBuilder::pack(UniCmd uni_cmd, uint16_t dst, uint8_t *payload, uint16_t len,
                                          uint8_t *sub_index, uint16_t idx_len) {
        uint16_t cmd = removeMask(uni_cmd);
        auto *msg = new ProtocolMessage(cmd, payload, len, sub_index, idx_len, dst, 1);
        return msg;
    }

    uint16_t removeMask(UniCmd req_cmd) {
        uint16_t cmd = (uint32_t(req_cmd) & 0xffff);
        return cmd;
    }

    UniCmd addMask(uint16_t resp_cmd, ProtocolType pt) {
        uint32_t ucmd = resp_cmd - kMessageResponseInterval + uint32_t(pt);
        return static_cast<UniCmd>(ucmd);
    }

    /*
    * ProtocolTaskStateManager implementions
    */
    ProtocolTaskStateManager::ProtocolTaskStateManager()
    {
        registerAllSynchronizableCmds();
    }
    ProtocolTaskStateManager::~ProtocolTaskStateManager()
    {
    }
   
    bool ProtocolTaskStateManager::isTaskReady(UniCmd cmd)
    {
        std::lock_guard <std::mutex> lock(mutex_);
        auto it = task_state_.find(cmd);
        if (it != task_state_.end())
        {
            return it->second == TaskState::SyncDone;
        }
        return false;
    }
    void ProtocolTaskStateManager::setTaskState(UniCmd cmd, TaskState state)
    {
        std::lock_guard <std::mutex> lock(mutex_);
        auto it = task_state_.find(cmd);
        if (it != task_state_.end())
        {
            it->second = state;
        }
    }
    bool ProtocolTaskStateManager::isTaskAsync(UniCmd cmd)
    {
        std::lock_guard <std::mutex> lock(mutex_);
        auto it = task_state_.find(cmd);
        if (it != task_state_.end())
        {
            return it->second == TaskState::Async;
        }
        return false;
    }
    
    void ProtocolTaskStateManager::registerAllSynchronizableCmds()
    {
        task_state_.emplace(UniCmd::AlphaDeviceInfo, TaskState::Async);
        task_state_.emplace(UniCmd::BetaDeviceInfo, TaskState::Async);
        task_state_.emplace(UniCmd::CommonGetConfigJson, TaskState::Async);
    }
}
