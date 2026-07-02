#ifndef DFU_HELPER_H_
#define DFU_HELPER_H_

#include "axdp_defines.h"
#include <map>
#include <vector>

#define MAX_CASCADE_DEVS                64
#define MAX_FLASH_BLOCKS                4

#define PROTOCOL_BIN_UPGRADE_SUCCESS    0xFFFFFFFF
#define PROTOCOL_BIN_UPGRADE_ING        0xFFFFFFEE
#define PROTOCOL_BIN_UPGRADE_FAIL        0xFFFFEEEE

#define DEFAULT_HEAD_LENGTH                14
#define RESERVE_BYTES                    8
#define MAX_BIN_STEP                    4096
#define MIN_BIN_STEP                    (257 - DEFAULT_HEAD_LENGTH - RESERVE_BYTES)
#define MD5_SIZE                        16

#define RESULT_SUCCESS                    0
#define RESULT_FAIL                        1
#define UPGRADE_PROCEDURE_TYPE_NUM        9

#define MAX_BUFFER_SIZE 4096

#define FLASH_BLOCK_TIPS_INFO    0xFB000001
#define FLASH_BLOCK_TIPS_DATA    0xFB000002
#define FLASH_BLOCK_FW_INFO        0xFB000003
#define FLASH_BLOCK_FW_DATA        0xFB000004

#define FLASH_BLOCK_UBOOT        0XFB000001
#define FLASH_BLOCK_KERNEL        0XFB000002
#define FLASH_BLOCK_ROOTS        0XFB000003
#define FLASH_BLOCK_ALGO        0XFB000004

#define FLASH_BLOCK_CAM_FW            0xFBCA0001
#define FLASH_BLOCK_CAM_UBOOT        0xFBCA0002
#define FLASH_BLOCK_CAM_SN            0xFBCA0003
#define FLASH_BLOCK_CAM_HWID        0xFBCA0004
#define FLASH_BLOCK_CAM_MAC_ADDR    0xFBCA0005

namespace axdp {
#pragma pack(1)

    /*
    * can't be modified these structs
    */
    typedef struct block_info_s {
        SubindexHeader sub_hdr;
        uint32_t flash_block;
        uint32_t slice_size;
        uint32_t slices_total;
        uint32_t block_size;
        uint8_t block_md5[MD5_SIZE];
    } BlockInfo;

    typedef struct slice_data_s {
        SubindexHeader sub_hdr;
        uint32_t slice_index;
        uint8_t data[MAX_BIN_STEP];
    } SliceData;

    typedef struct dev_info_s {
        uint8_t product_name[32];
        uint8_t software_ver[32];
        uint8_t serial_number[32];
        uint8_t dev_id;
    } DevInfoSt;

    typedef struct devs_type_s {
        uint16_t type;
        uint16_t count;
    } DevTypeSt;
    /*
    * can't be modified these structs
    */
#pragma pack()

    typedef struct flash_block_info_t {
        uint8_t *data;//0XFB000001
        uint32_t size;
    } FlashBlock;

    enum UpgradeStratagyOffset {
        SplitBlocks = 0,
        FlashBlock1,
        FlashBlock2,
        FlashBlock3,
        FlashBlock4,
        SliceSizeFlag
    };

    /*
    * |     byte 0	    |	  byte 1      |   bytes 2	    |	 bytes 3	  | ... |
    * |  SplitBlocks    | FlashBlock1     | FlashBlock2     | FlashBlock3     | ... |
    * |					|slice1|slice2|...|slice1|slice2|...|slice1|slice2|...|||||||
    */
    static uint32_t UpgradeStrategyData[UPGRADE_PROCEDURE_TYPE_NUM][MAX_FLASH_BLOCKS + 2] = {
            {1,
                    FLASH_BLOCK_CAM_FW,
                    0,
                    0,
                    0,
                    MIN_BIN_STEP},
            {4,     FLASH_BLOCK_FW_DATA,
                    FLASH_BLOCK_FW_INFO,
                    FLASH_BLOCK_TIPS_DATA,
                    FLASH_BLOCK_TIPS_INFO,
                    MAX_BIN_STEP},
            {3,
                    FLASH_BLOCK_KERNEL,
                    FLASH_BLOCK_ROOTS,
                    FLASH_BLOCK_ALGO,
                    0,
                    MAX_BIN_STEP},
            {1,
                    FLASH_BLOCK_CAM_FW,
                    0,
                    0,
                    0,
                    MIN_BIN_STEP},
            {1,
                    FLASH_BLOCK_CAM_UBOOT,
                    0,
                    0,
                    0,
                    MIN_BIN_STEP
            },
            {3,//FOR A50 MIC BAN
                    FLASH_BLOCK_FW_INFO,
                    FLASH_BLOCK_TIPS_DATA,
                    FLASH_BLOCK_TIPS_INFO,
                    0,
                    MAX_BIN_STEP
            },
            {2, //FOR A50 DSP
                    FLASH_BLOCK_TIPS_DATA,
                    FLASH_BLOCK_TIPS_INFO,
                    0,
                    0,
                    MAX_BIN_STEP
            },
            {1, //FOR A50 Controller
                    FLASH_BLOCK_FW_INFO,
                    0,
                    0,
                    0,
                    MAX_BIN_STEP
            },
            {3,//FOR A50 MIC BAN
                FLASH_BLOCK_FW_INFO,
                FLASH_BLOCK_TIPS_DATA,
                FLASH_BLOCK_TIPS_INFO,
                0,
                MAX_BIN_STEP
            }
    };

    /**
    * 假设有一个 *.bin 文件*
    * 根据上面的策略类型，我们将之划分为几个blocks，记录每个block中的信息*
    **/
    struct BlockData {
        uint8_t *data;
        uint32_t size;
    };

    //BlockStatics
    struct BlockStat {
        uint32_t cur_slice_index;
        uint32_t slice_request;
        uint32_t cur_slice_len;
        uint32_t cur_buf_offset;
    };

    class BinParser;

    class DfuHelper {
    public:
        DfuHelper();

        ~DfuHelper();

        void setDevCount(uint16_t count) { dev_count_ = count; }

        uint16_t dst() { return dst_; }

        void resetProgress() { progress_ = 0; };

        uint16_t progress() { return upgradeProgressPercentage(); }

        void resetReadyFlags(uint16_t dst);

        void setDowning(bool d) { downing_ = d; }

        uint16_t upgradeProgressPercentage();

        void setUpgradeData(const uint8_t *data, uint32_t len,
                            uint32_t block_number,
                            uint16_t slice_size,
                            uint16_t dst);

        void setBlockData(int index, uint8_t *data, uint32_t len, uint32_t flash_block);

        void setBlockMD5(int index, uint8_t *md5, uint32_t len);

        void increaseBlockIndex();

        void resetBlockStat();

        void resetBlockStat2();

        uint16_t currentBlockIndex() { return block_index_; }

        const BlockInfo &currnetBlockInfo();

        const SliceData &currentSlice();

        //added for dst == 14
        uint8_t* currentBlockInfoData();

        uint16_t currentBlockInfoLength();

        uint8_t* currentSliceData();

        uint16_t currentSliceDataLength();

        const SubindexHeader& currentSubindexHdr();//unused

        void increaseSliceIndex();

        int requestedSliceIndex();

        uint32_t curSliceIndex();

        uint16_t currentSliceLen();//unused

        bool isBlockEmpty();

        bool isSliceEmpty();

        bool isSliceIndexDivisible(int divisor);

        bool isAllDevicesReady(uint16_t src);

        UpdateStrategy updateStrategy() const { return strategy_; }

        void setUpdateStrategy(UpdateStrategy us) { strategy_ = us; }

        uint16_t upgradeDeviceDst() const { return dst_; }

        void setUpdateDeviceDst(uint16_t dst);

        uint16_t updatePacketInterval() const { return update_packet_interval_; }

        void setUpdatePacketInterval(uint16_t interval){update_packet_interval_ = interval; }

        void setUpdateUltraDstEnabled(EnableState st);

        void setUpdateDeviceUltraDst(const uint8_t* dst, int32_t size);

        int config(BinParser &bin_parser);

    private:
        UpdateStrategy strategy_;
        EnableState ultra_dst_enabled_{EnableState::Disabled};
        bool downing_{ false };
        bool ready[MAX_CASCADE_DEVS];
        uint16_t dst_{ 0 };
        uint16_t dev_count_{ 0 };
        uint16_t progress_{ 0 };

        const uint8_t *bin_buf_{ 0 };         //读取整个文件后的数据buffer指针，指向一块malloc/new内存
        uint32_t total_bin_size_{ 0 };       //整个bin文件大小
        uint32_t total_slice_number_{ 0 };   //整个bin文件可以被切分的切片数
        uint32_t total_slice_count_{ 0 };    // 全局slice计数器
        uint32_t block_index_{ 0 };          //block info 的索引
        uint32_t block_number_{ 0 };         //被区分的block 数量
        uint16_t slice_size_{ 0 };
        uint16_t update_packet_interval_{ 0 }; //更新时数据包发送间隔

        BlockInfo block_info_[MAX_FLASH_BLOCKS]; //每个分块中的数据分布，可以根据此算出当前的bininfo
        BlockData block_data_[MAX_FLASH_BLOCKS]; //每个分块数据具体的指向
        BlockStat block_stat_[MAX_FLASH_BLOCKS]; //每个分块的数据记录

        BlockInfo cur_block_{ 0 };        //临时存储网络字节序的当前blockinfo的数据
        SliceData slice_data_{ 0 };    //临时存储slicedata信息，网络字节序

        SubindexHeader subidx_hdr_;
    };
}

#endif // !DFU_HELPER_H_
