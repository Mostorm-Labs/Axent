#include <string>
#include <fstream>
#include "bin_parser.h"
#include "axdp_utils.h"

//对于字数据（16位）：
#define BigtoLittle16(A) (( ((uint16_t)(A) & 0xff00) >> 8) | \
(( (uint16_t)(A) & 0x00ff) << 8))

//对于双字数据（32位）：
#define BigtoLittle32(A) ((( (uint32_t)(A) & 0xff000000) >> 24) | \
(( (uint32_t)(A) & 0x00ff0000) >> 8) | \
(( (uint32_t)(A) & 0x0000ff00) << 8) | \
(( (uint32_t)(A) & 0x000000ff) << 24))

#define FLASH_BLOCK_TIPS_INFO    0xFB000001
#define FLASH_BLOCK_TIPS_DATA    0xFB000002
#define FLASH_BLOCK_FW_INFO        0xFB000003
#define FLASH_BLOCK_FW_DATA        0xFB000004

static void back() {
}

namespace axdp {
    BinParser::BinParser() : content_(nullptr), size_(0), header_offset_(0){
    }

    BinParser::~BinParser() {
        content_ ? delete[]content_ : back();
    }

    int BinParser::loadLocalFile(const char *file_name) {
        std::ifstream ifs(file_name, std::ios::binary | std::ios::in);
        if (!ifs.is_open()) {
            return -1;
        }

        // 获取filestr对应buffer对象的指针
        auto pbuf = ifs.rdbuf();

        // 调用buffer对象方法获取文件大小
        size_t offset_size = (size_t) pbuf->pubseekoff(0, std::ios::end, std::ios::in);
        pbuf->pubseekpos(0, std::ios::in);

        if (size_ < offset_size) {
            //重新分配空间
            if (content_ != nullptr) {
                delete[] content_;
            }
            content_ = new uint8_t[offset_size];
        }
        size_ = offset_size;

        if (!content_) {
            return -5;
        }

        // 获取文件内容
        pbuf->sgetn((char *) content_, size_);

        ifs.close();
        return size_;
    }

    int BinParser::loadMemBuffer(const char *buffer, size_t size) {
        if (buffer == NULL) {
            return -1;
        }

        if (size_ < size) {
            //重新分配空间
            if (content_ != nullptr) {
                delete[] content_;
            }
            content_ = new uint8_t[size];
        }
        size_ = size;

        if (!content_) {
            return -5;
        }

        // 获取BUFFER内容
        memcpy(content_, buffer, size_);

        return size_;
    }

    static void ReverseBits(uint8_t *data, size_t bytes) {
        if (data != nullptr) {
            for (size_t i = 0; i < bytes; ++i) {
                data[i] = ~data[i];
            }
        }
    }

    int BinParser::checkAuditoryHead() {
        if (!content_) {
            return -1;
        }
        for (size_t i = 0; i < size_; i++) {
            content_[i] = ~content_[i];
        }

        //A u r y
        //A u d i t o r y
        if (!(content_[0] == 'A' && content_[1] == 'u' && content_[6] == 'r' && content_[7] == 'y')) {
            recoverData();
            return -1;
        }

        int i = 0;
        int total = 0;
        uint8_t pos = 8;
        while (total < 8) {
            if (pos >= 30) {
                return -1;
            }
            int sub = content_[pos++];
            if (pos + sub >= 30) {
                return -1;
            }
            int low = (sub - 1) << 5;
            int high = (sub << 5) - 1;

            for (i = 0; i < sub; ++i) {
                int posvalue = content_[pos + i] & 0xff;
                //if(buffer[pos + i] < low || buffer[pos + i] > high) {
                if (posvalue < low || posvalue > high) {
                    return -2;
                }
            }
            pos += sub;
            total += sub;
        }
        return pos;
    }

    int BinParser::parseContent() {
        header_offset_ = checkAuditoryHead();
        if (header_offset_ <= 0) {
            return header_offset_;
        }
        /*
        * >Bxxx4s32sII4s32sII4s32sII
        * |	1 byte | 1 byte null | 1 byte null | 1 byte null |
        * | 4 bytes string | 32 bytes string | 4 bytes int | 4 bytes int |  >>>  44bytes
        * | 4 bytes string | 32 bytes string | 4 bytes int | 4 bytes int |
        * | 4 bytes string | 32 bytes string | 4 bytes int | 4 bytes int |
        */
        int blocks_num = (int) *(unsigned char *) &content_[header_offset_];
        int info_start_offset = header_offset_ + 4;
        int offset_interval = 44;
        int block_str_offset = 0;
        int block_str_length = 4;
        int md5_str_offset = 4;
        int md5_str_length = 32;
        int block_position_offset = 36;
        int block_position_length = 4;
        int block_size_offset = 40;
        int block_size_length = 4;
        for (int i = 0; i < blocks_num; i++) {
            int block_info_start_offset = info_start_offset + offset_interval * i;
            std::string block_str((const char *) &content_[block_info_start_offset + block_str_offset],
                                  block_str_length);
            block_md5_offset_[i] = block_info_start_offset + md5_str_offset;
            block_offset_[i] = BigtoLittle32(
                    *(uint32_t *) (&content_[block_info_start_offset + block_position_offset]));
            block_length_[i] = BigtoLittle32(*(uint32_t *) (&content_[block_info_start_offset + block_size_offset]));
#if 0
            std::string file_name = block_str + ".bin";
            std::ofstream pack_data_out(file_name, std::ios::out | std::ios::binary);
            pack_data_out.write((const char*)&content_[block_offset_[i]], block_length_[i]);
            pack_data_out.close();
#endif // 1
        }
        return 0;
    }

    const uint8_t *BinParser::md5Buffer(int idx) const {
        if (idx < 0 || idx >= MAX_BLOCKS_NUM) {
            return nullptr;
        } else {
            return &content_[block_md5_offset_[idx]];
        }
    }

    const uint8_t *BinParser::blockBuffer(int idx) const {
        if (idx < 0 || idx >= MAX_BLOCKS_NUM) {
            return nullptr;
        } else {
            return &content_[block_offset_[idx]];
        }
    }

    size_t BinParser::blockLength(int idx) const {
        if (idx < 0 || idx >= MAX_BLOCKS_NUM) {
            return size_t();
        } else {
            return block_length_[idx];
        }
    }

    int BinParser::recoverData() {
        if (content_ != nullptr) {
            for (size_t i = 0; i < size_; i++) {
                content_[i] = ~content_[i];
            }
        }
        return 0;
    }

    /*void BinParser::formatPrintHeader()
    {
        std::string md51((const char*)content_ + binInfoMD5Offset_, 32);
        std::string md52((const char*)content_ + tipsInfoMD5Offset_, 32);
        std::string md53((const char*)content_ + tipsDataMD5Offset_, 32);
        std::string md54((const char*)content_ + firmInfoMD5Offset_, 32);
        std::string md55((const char*)content_ + firmDataMD5Offset_, 32);
        awx_print("%s\n", md51.c_str());
        awx_print("%s\n", md52.c_str());
        awx_print("%s\n", md53.c_str());
        awx_print("%s\n", md54.c_str());
        awx_print("%s\n", md55.c_str());
    }*/
}