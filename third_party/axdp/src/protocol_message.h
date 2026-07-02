#ifndef AXDP_PROTOCOL_MESSAGE_H_
#define AXDP_PROTOCOL_MESSAGE_H_

#include <mutex>
#include <map>
#include <string>
#include <memory>
#include "protocol_defines.h"
#include "axdp_defines.h"

namespace axdp {
    class ProtocolMessage {
        friend class Header;

        friend class Payload;

    public:
        class Header {
            enum Flag {
                Magic, Version, Dst, Src, Cmd, Len, Crc, MaxFlag
            };
        public:

            explicit Header(ProtocolMessage *msg) :
                    msg_(msg), data_((uint16_t *) msg->data_) {

            }

            ~Header() = default;

            inline void setMagic(uint16_t magic) { data_[Magic] = magic; }

            inline void setVersion(uint16_t v) { data_[Version] = v; }

            inline void setDst(uint16_t dst) { data_[Dst] = dst; }

            inline void setSrc(uint16_t s) { data_[Src] = s; }

            inline void setCmd(uint16_t c) { data_[Cmd] = c; }

            inline void setLen(uint16_t l) { data_[Len] = l; }

            inline void setCrc(uint16_t c) { data_[Crc] = c; }

            inline uint16_t magic() const { return data_[Magic]; }

            inline uint16_t version() const { return data_[Version]; }

            inline uint16_t dst() const { return data_[Dst]; }

            inline uint16_t src() const { return data_[Src]; }

            inline uint16_t cmd() const { return data_[Cmd]; }

            inline uint16_t len() const { return data_[Len]; }

            inline uint16_t crc() const { return data_[Crc]; }

            void formatPrint() const;

        private:
            ProtocolMessage *msg_;
            uint16_t *data_;
            uint16_t cache_[MaxFlag]{kMagicNumber, kProtocolVersion, 2, 1, 1, 0, 0};
        };

        class Payload {
        public:
            explicit Payload(ProtocolMessage *msg) : msg_(msg) {

            };

            ~Payload() = default;

            inline const uint8_t *data() const { return msg_->data_ + kPayloadOffset; }

            inline uint16_t size() const { return msg_->size_ - kHeaderSize; }

        private:
            ProtocolMessage *msg_;
        };

        explicit ProtocolMessage(uint16_t cmd, uint8_t *payload, uint16_t payload_len, uint16_t dst = 2,
                                 uint16_t src = 1);

        explicit ProtocolMessage(uint16_t cmd, uint8_t *payload, uint16_t payload_len, uint8_t *sub_idx,
                                 uint16_t idx_len, uint16_t dst = 2, uint16_t src = 1);

        explicit ProtocolMessage(uint16_t payload_len);

        ProtocolMessage(const ProtocolMessage &) = delete;

        ~ProtocolMessage();

        const Header &header() const { return *head_; }

        const Payload &payload() const { return *payload_; }

        inline void setMagic(uint16_t magic) { head_->setMagic(magic); }

        inline void setVersion(uint16_t v) { head_->setVersion(v); }

        inline void setDst(uint16_t dst) { head_->setDst(dst); }

        inline void setSrc(uint16_t s) { head_->setSrc(s); }

        inline void setCmd(uint16_t c) { head_->setCmd(c); }

        inline void setLen(uint16_t l) { head_->setLen(l); }

        inline void setCrc(uint16_t c) { head_->setCrc(c); }

        inline void copyPayloadData(const uint8_t *data, uint16_t len, uint16_t offset) {
            memcpy(data_ + kHeaderSize + offset, data, len);
        }

        inline const uint8_t *data() { return data_; }

        inline uint16_t length() const { return size_; }

        void fomatPrint() const;

    protected:
        Header *head_;
        Payload *payload_;

    private:
        uint8_t *data_;
        uint16_t size_;
    };


    class MessageParser {
    public:
        enum class ParseState : int16_t {
            UnknownError = -32768,
            MemError = -2,
            CrcError = -1,
            Done = 0,
            Payload = 1,
            Header = 2,
            MagicSecond = 3,
            MagicFirst = 4,
        };

        MessageParser() {
            resetState();
        };

        ~MessageParser() {
        }

        /*
        * @Brief
        *		Get message content functions after parse() when state_ is
        *		Done mostly.
        *		Certainly you can get content anytime you want.
        *
        */
        inline std::shared_ptr<ProtocolMessage> message() {
            return std::move(cache_msg_);
        }

        /*
        * @Brief
        *		Parse data into message content may be executed more
        *       than one time.
        * @Param - data
        *		An array or memory block will be parsed according to
        *		the protocol into the msg content
        * @Param - bytes
        *		The data's length.
        * @return
        *		ParseState - Done means parse successfully;
        *				   - *Error means data wrong;
        *				   - Payload~MagicFirst means parsing is ongoing,
        *					and we need to data to parse.
        */
        ParseState parse(const uint8_t *data, uint16_t bytes);

    protected:
        void resetState();

        ParseState feed(const uint8_t *data, uint16_t len, int *consumed);

    private:
        ParseState state_;
        uint16_t payload_pos_{};
        uint8_t cache_header_[kHeaderSize]{0};
        uint16_t compute_crc_{};
        uint16_t header_pos_{};
        std::shared_ptr<ProtocolMessage> cache_msg_;
    };

    class MessageBuilder {
    public:
        static ProtocolMessage *pack(UniCmd uni_cmd, uint16_t dst, uint8_t *payload = nullptr, uint16_t len = 0, uint8_t *sub_index = nullptr, uint16_t idx_len = 0);
    };

    uint16_t removeMask(UniCmd req_cmd);

    UniCmd addMask(uint16_t resp_cmd, ProtocolType pt);

    class ProtocolTaskStateManager {
    public:
        enum class TaskState {
            Async,
            SyncWaiting,
            SyncDone,
        };
        ProtocolTaskStateManager();
        virtual ~ProtocolTaskStateManager();
        bool isTaskReady(UniCmd cmd);
        void setTaskState(UniCmd cmd, TaskState is_ready);
        bool isTaskAsync(UniCmd cmd);
        
    private:
        void registerAllSynchronizableCmds();
        std::map<UniCmd, TaskState> task_state_;
        std::mutex mutex_;
    };
    using TaskState = ProtocolTaskStateManager::TaskState;
}

#endif // AXDP_PROTOCOL_MESSAGE_H_
