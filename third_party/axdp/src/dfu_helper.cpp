#include "dfu_helper.h"
#include "axdp_utils.h"
#include "md5.h"
#include "bin_parser.h"
#include "log_utils.h"
namespace axdp {
	DfuHelper::DfuHelper():dst_(0xffff)
	{
	}
	DfuHelper::~DfuHelper()
	{
	}

	void DfuHelper::increaseBlockIndex() {
		block_index_++;
		//resetBlockStat();
	}

	//这个函数很重要
	void DfuHelper::resetBlockStat() {
		uint16_t index = block_index_;
		block_stat_[index].cur_buf_offset = 0;
		block_stat_[index].cur_slice_index = 0;
		block_stat_[index].cur_slice_len = 0;
		block_stat_[index].slice_request = 0;
	}

	//it's old version
	void DfuHelper::resetBlockStat2() {
		uint16_t index = block_index_;
		block_stat_[index].cur_buf_offset = 0;
		block_stat_[index].cur_slice_index = 0;
		block_stat_[index].cur_slice_len =
			awx_min(
				block_data_[index].size - block_stat_[index].cur_buf_offset,
				block_info_[index].slice_size
			);
		block_stat_[index].slice_request = 0;
	}

	const BlockInfo& DfuHelper::currnetBlockInfo() {
		uint16_t index = block_index_;
		BlockInfo& info = block_info_[index];

		cur_block_.flash_block = utils::htonl(info.flash_block);
		cur_block_.slice_size = utils::htonl(info.slice_size);
		cur_block_.slices_total = utils::htonl(info.slices_total);
		cur_block_.block_size = utils::htonl(info.block_size);
		memcpy(cur_block_.block_md5, info.block_md5, MD5_SIZE);

		return cur_block_;
	}

	void DfuHelper::resetReadyFlags(uint16_t dst) {
		for (size_t i = 0; i < MAX_CASCADE_DEVS; i++) {
			ready[i] = true;
		}
		for (int i = 1; i <= dev_count_; ++i) {
			if (dst & (1 << i)) {
				ready[i - 1] = false;
			}
		}
	}

	uint16_t DfuHelper::upgradeProgressPercentage() {
		total_slice_count_ = 0;
		for (uint32_t i = 0; i < block_index_; i++)
		{
			total_slice_count_ += block_info_[i].slices_total;
		}
		total_slice_count_ += block_stat_[block_index_].cur_slice_index;
		int a = (total_slice_count_ * 100 / total_slice_number_);
		return a;
	}

	void DfuHelper::setUpgradeData(const uint8_t* data, uint32_t len,
		uint32_t block_number, uint16_t slice_size, uint16_t dst) {
		this->dst_ = dst;
		this->bin_buf_ = data;
		this->total_bin_size_ = len;
		this->block_number_ = block_number;
		this->slice_size_ = slice_size;
		this->block_index_ = 0;
	}

	void DfuHelper::setBlockData(int index, uint8_t* data, uint32_t len, uint32_t flash_block) {
		block_data_[index].data = data;
		block_data_[index].size = len;
		block_info_[index].flash_block = flash_block;
		block_info_[index].block_size = len;
		block_info_[index].slice_size = slice_size_;
		block_info_[index].slices_total = (len + slice_size_ - 1) / slice_size_;
		md5_calc(block_info_[index].block_md5, data, len);
		total_slice_number_ += block_info_[index].slices_total;
		LOGV("bin index = %d, slice total: %d", index, block_info_[index].slices_total);
	}

	void DfuHelper::setBlockMD5(int index, uint8_t* md5, uint32_t len) {
		if (md5 == nullptr)
		{
			return;
		}
		//memcpy(block_info[index].block_md5, md5, len);
		str2md5((const char*)md5, len, (char*)block_info_[index].block_md5);
	}

	void DfuHelper::increaseSliceIndex()
	{
		uint16_t  i = block_index_;
		block_stat_[i].cur_buf_offset += block_stat_[i].cur_slice_len;
		block_stat_[i].cur_slice_len = awx_min(block_data_[i].size - block_stat_[i].cur_buf_offset, block_info_[i].slice_size);

		//if ((int)block_stat[i].cur_slice_len < 0)
		//{
		//	block_stat[i].cur_slice_len = 0;
		//}
		block_stat_[i].slice_request++;
		block_stat_[i].cur_slice_index++;
		//resetReadyFlags(dst_);
		progress_ = upgradeProgressPercentage();
	}

	const SliceData& DfuHelper::currentSlice() {
		uint8_t* p = nullptr;
		uint16_t i = block_index_;

		//BlockInfo& info = block_info[i];
		BlockStat& stat = block_stat_[i];
		BlockData& block = block_data_[i];

		//stat.cur_buf_offset = stat.cur_slice_index * info.slice_size;
		p = block.data + stat.cur_buf_offset;
		//stat.cur_slice_len = aux_min(block.size - stat.cur_buf_offset, info.slice_size);

		slice_data_.slice_index = utils::htonl(stat.cur_slice_index);
		memcpy(slice_data_.data, p, stat.cur_slice_len);
		return slice_data_;
	}

	uint8_t* DfuHelper::currentBlockInfoData()
	{
		currnetBlockInfo();
		if (ultra_dst_enabled_ == EnableState::Enabled)
		{
			memcpy(&cur_block_.sub_hdr, &subidx_hdr_, sizeof(SubindexHeader));
			return (uint8_t*)&(cur_block_.sub_hdr);
		}
		else
		{
			return (uint8_t*)&(cur_block_.flash_block);
		}
		return nullptr;
	}

	uint16_t DfuHelper::currentBlockInfoLength()
	{
		if (ultra_dst_enabled_ == EnableState::Enabled)
		{
			return sizeof(BlockInfo);
		}
		else
		{
			return sizeof(BlockInfo) - sizeof(SubindexHeader);
		}
		return 0;
	}

	uint8_t* DfuHelper::currentSliceData()
	{
		currentSlice();
		if (ultra_dst_enabled_ == EnableState::Enabled)//for am10p
		{
			memcpy(&slice_data_.sub_hdr, &subidx_hdr_, sizeof(SubindexHeader));
			return (uint8_t*)&(slice_data_.sub_hdr);
		}
		else
		{
			return (uint8_t*)&(slice_data_.slice_index);
		}
		return nullptr;
	}

	uint16_t DfuHelper::currentSliceDataLength()
	{
		if (ultra_dst_enabled_ == EnableState::Enabled)
		{
			return currentSliceLen() + sizeof(SubindexHeader);
		}
		else
		{
			return currentSliceLen();
		}
		return 0;
	}

	const SubindexHeader& DfuHelper::currentSubindexHdr()
	{
		return subidx_hdr_;
	}

	int DfuHelper::requestedSliceIndex()
	{
		uint16_t index = block_index_;
		return  block_stat_[index].cur_slice_index + 1;
	}

	uint32_t DfuHelper::curSliceIndex()
	{
		uint16_t index = block_index_;
		return  block_stat_[index].cur_slice_index;
	}

	uint16_t DfuHelper::currentSliceLen()
	{
		uint16_t i = block_index_;
		BlockStat& stat = block_stat_[i];
#define SLICE_INDEX_SIZE 4
		return stat.cur_slice_len + SLICE_INDEX_SIZE;
	}

	bool DfuHelper::isBlockEmpty()
	{
		return block_index_ == block_number_;
	}

	bool DfuHelper::isSliceEmpty()
	{
		uint16_t i = block_index_;
		BlockStat& stat = block_stat_[i];
		BlockInfo& info = block_info_[i];
		return stat.cur_slice_index == info.slices_total;
	}

	bool DfuHelper::isSliceIndexDivisible(int divisor)
	{
		return (block_stat_[block_index_].cur_slice_index % divisor == 0);
	}

	bool DfuHelper::isAllDevicesReady(uint16_t src)
	{
		bool pass = true;
		for (int i = 1; i <= dev_count_; ++i) {
			if ((src & (1 << i))) {
				ready[i - 1] = true;
			}
			else if (ready[i - 1] == false) {
				pass = false;
			}
			//LOG(TRACE) << i << " : ready = " << dfu_proc->ready[i - 1];
		}
		return pass;
	}

	void DfuHelper::setUpdateDeviceDst(uint16_t dst)
	{
		dst_ = dst;
	}

	void DfuHelper::setUpdateUltraDstEnabled(EnableState st)
	{
		ultra_dst_enabled_ = st;
	}

	void DfuHelper::setUpdateDeviceUltraDst(const uint8_t* dst, int32_t size)
	{
		memset(&subidx_hdr_.sub_idx[0], 0, 32);
		if (size <= 32)
		{
			subidx_hdr_.magic = utils::htons(0);
			subidx_hdr_.type = utils::htons(0x10);
			subidx_hdr_.len = utils::htons(32);
			memcpy(&subidx_hdr_.sub_idx[0], dst, size);
		}
	}

	int DfuHelper::config(BinParser& bin_parser)
	{
		int strg_idx = ((int)strategy_ >> 20) - 1;
		int block_number = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::SplitBlocks];
		uint16_t slice_size = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::SliceSizeFlag];
		slice_size_ = slice_size;
		uint32_t flash_block = 0;
		setUpgradeData(
			bin_parser.data(),
			bin_parser.dataLength(),
			block_number, slice_size,
			dst_);
		switch (strategy_)
		{
		case UpdateStrategy::Falcon:
		case UpdateStrategy::Viper:
		case UpdateStrategy::Cobra: {
			for (int i = 0; i < block_number; i++)
			{
				flash_block = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::FlashBlock1 + i];
				setBlockData(i,
					const_cast<uint8_t*>(bin_parser.data()),
					bin_parser.dataLength(),
					flash_block);
			}
			break;
		}
		case UpdateStrategy::Dolphin:
		{
			int ret = bin_parser.parseContent();
			if (ret != 0)
			{
				return ErrorCode::FileAbnormal;
			}
			for (int i = 0; i < block_number; i++)
			{
				flash_block = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::FlashBlock1 + i];
				setBlockData(i,
					const_cast<uint8_t*>(bin_parser.blockBuffer(4 - i)),
					bin_parser.blockLength(4 - i),
					flash_block);
				setBlockMD5(i,
					const_cast<uint8_t*>(bin_parser.md5Buffer(4 - i)),
					MD5_SIZE);
			}
			break;
		}
		case UpdateStrategy::Gopher: {
			int ret = bin_parser.parseContent();
			if (ret != 0)
			{
				return ErrorCode::FileAbnormal;
			}
			for (int i = 0; i < block_number; i++)
			{
				flash_block = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::FlashBlock1 + i];
				setBlockData(i,
					const_cast<uint8_t*>(bin_parser.blockBuffer(i + 2)),
					bin_parser.blockLength(i + 2),
					flash_block);
				setBlockMD5(i, 
					const_cast<uint8_t*>(bin_parser.md5Buffer(i + 2)),
					MD5_SIZE);
			}
			break;
		}
		case UpdateStrategy::Gecko://3 blocks , start from fb000003
		case UpdateStrategy::Discus://2 blocks , start from fb000002
		case UpdateStrategy::Camel://1 blocks , start from fb000003
        case UpdateStrategy::Jagger:
		{
			int ret = bin_parser.parseContent();
			if (ret != 0)
			{
				return ErrorCode::FileAbnormal;
			}
			for (int i = 0; i < block_number; i++)
			{
				flash_block = UpgradeStrategyData[strg_idx][UpgradeStratagyOffset::FlashBlock1 + i];
				setBlockData(i,
					const_cast<uint8_t*>(bin_parser.blockBuffer(block_number - i - 1)),
					bin_parser.blockLength(block_number - i - 1),
					flash_block);
				setBlockMD5(i, 
					const_cast<uint8_t*>(bin_parser.md5Buffer(block_number - i - 1)),
					MD5_SIZE);
			}
			break;
		}
		default:
			break;
		}
		return 0;
	}

}
