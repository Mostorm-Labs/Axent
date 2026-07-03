#ifndef AXDP_BIN_PARSER_H_
#define AXDP_BIN_PARSER_H_

#include "axdp_defines.h"

namespace axdp {
    class BinParser {
    public:
        BinParser();

        ~BinParser();

        int loadLocalFile(const char *file_name);

        int loadMemBuffer(const char *buffer, size_t size);//will copy

        int parseContent();

        const uint8_t *md5Buffer(int idx) const;

        const uint8_t *blockBuffer(int idx) const;

        size_t blockLength(int idx) const;

        const uint8_t *data() const { return content_; }

        size_t dataLength() const { return size_; }

    private:
        int recoverData();

        int checkAuditoryHead();

        uint8_t *content_;
        
        size_t size_;

        //auditory fingerprint
        int32_t header_offset_;

#define MAX_BLOCKS_NUM 8
        uint32_t block_md5_offset_[MAX_BLOCKS_NUM];
        uint32_t block_offset_[MAX_BLOCKS_NUM];
        uint32_t block_length_[MAX_BLOCKS_NUM];
    };
}


#endif // !AXDP_BIN_PARSER_H_



