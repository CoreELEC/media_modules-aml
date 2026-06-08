/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */

#include "scatter_input.h"

#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/crc32.h>

#include "decoder_dma_alloc.h"
#include "vdec.h"

static __u32 scatter_input_loglevel = 0;
module_param(scatter_input_loglevel, int, 0644);

static __u32 need_sw_pad = 1;
module_param(need_sw_pad, int, 0644);

static __u32 scatter_input_padding_size = 4096;
module_param(scatter_input_padding_size, int, 0644);

#define SCATTER_INPUT_LOG_INFO (1<<0)
#define SCATTER_INPUT_LOG_DEBUG (1<<1)

#define SCATTER_INPUT_LOG_DUMP_MAP (1<<2)

#define SCATTER_INPUT_MSG(format, ARGS...) pr_info("[%s:%d] " format , __func__ , __LINE__, ##ARGS);

#define SCATTER_INPUT_LOG(LOG_FLAG, format, ARGS...) do {							\
	if (scatter_input_loglevel & LOG_FLAG)	\
		SCATTER_INPUT_MSG(format, ##ARGS);						\
} while (0)

#define SCATTER_INPUT_LOG_ERROR(format, ARGS...)  SCATTER_INPUT_MSG(format, ##ARGS)
#define SCATTER_MAP_IS_FULL(rp, wp, offset) (((wp + offset) % MAX_MAP_ITEMS) == rp)

#define SCATTER_INPUT_REVERSE_ENDIAN_64(val) ( \
	(((val) & 0xFF00000000000000) >> 56) | \
	(((val) & 0x00FF000000000000) >> 40) | \
	(((val) & 0x0000FF0000000000) >> 24) | \
	(((val) & 0x000000FF00000000) >> 8)  | \
	(((val) & 0x00000000FF000000) << 8)  | \
	(((val) & 0x0000000000FF0000) << 24) | \
	(((val) & 0x000000000000FF00) << 40) | \
	(((val) & 0x00000000000000FF) << 56))


static void dump_device_map(struct scatter_input_ctx_t* this_ctx, int count)
{
	int i = 0;
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DUMP_MAP, "begin to dump device map\n");
	for (i = 0; i < count; ++i) {
		pr_info(" i(%d): map_data:%llx ", i , *((__u64*)this_ctx->map_start_vrit_addr + i));
	}
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DUMP_MAP, "end to dump device map\n");
}

static void dump_host_map(struct scatter_input_ctx_t* this_ctx, int begin, int count)
{
	int i = 0;
	__u32 data_size = 0;
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DUMP_MAP, "begin to dump host map\n");
	for (i = 0; i < count; ++i) {
		pr_info(" i(%d): map_data:%llx ", i , this_ctx->map[begin + i].map_data);
		data_size += this_ctx->map[begin + i].map_data & 0x3fffffff;
	}
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DUMP_MAP, "end to dump host map, data_size:%u\n", data_size);
}

u32 scatter_input_crc32(void *sg_table_p, u32 data_size)
{
	struct scatterlist *sg = NULL;
	struct sg_table* sgt = sg_table_p;
	u32 i = 0;
	u32 crc = ~0U;
	u32 remain_data = data_size;
	size_t len = 0;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		void *vaddr = NULL;
		if (remain_data >= sg_dma_len(sg)) {
			len = sg_dma_len(sg);
		}else {
			len = remain_data;
		}
		remain_data -= len;

		vaddr = kmap_local_page(sg_page(sg));
		if (!vaddr)
			continue;

		crc = crc32(crc, vaddr + sg->offset, len);

		kunmap_local(vaddr);
		if (remain_data == 0) {
			break;
		}
	}
	crc ^= ~0U;
	return crc;
}
EXPORT_SYMBOL(scatter_input_crc32);

u32 scatter_input_crc32_sum(void *sg_table_p, u32 data_size)
{
	struct scatterlist *sg = NULL;
	struct sg_table* sgt = sg_table_p;
	u32 i = 0;
	u32 crc = 0;
	u32 remain_data = data_size;
	size_t len = 0;
	u32 j = 0;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		char *vaddr = NULL;
		if (remain_data >= sg_dma_len(sg)) {
			len = sg_dma_len(sg);
		} else {
			len = remain_data;
		}
		remain_data -= len;

		vaddr = kmap_local_page(sg_page(sg));
		if (!vaddr)
			continue;
		for (j = 0; j < len; ++j) {
			crc += *(vaddr + sg->offset +j);
		}
		kunmap_local(vaddr);
		if (remain_data == 0) {
			break;
		}
	}
	return crc;
}
EXPORT_SYMBOL(scatter_input_crc32_sum);

void scatter_input_frame_mode_dump_data(struct scatter_input_ctx_t* this_ctx, struct scatter_input_data_package_t* pack, __u32 play_cnt)
{
	u32 i = 0;
	struct scatterlist* sg = NULL;
	struct sg_table* sgt = pack->sgt;
	size_t remain_data_size = pack->data_size;
	u32 dump_len = 0;
	char *size_addr = NULL;

	if (this_ctx->last_dump_wp == pack->map_idx) {
		return;
	}

	size_addr = vzalloc(64);

	if (size_addr) {
		snprintf(size_addr, 64, "%zu\n", pack->data_size);
		vdec_dbg_virt_write(size_addr, strlen(size_addr), this_ctx->vdec->id,
			"%d_%d_%d.sz", this_ctx->vdec->id, this_ctx->vdec->format, play_cnt);
		vfree(size_addr);
		size_addr = NULL;
	}
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {

		dump_len = remain_data_size >= sg_dma_len(sg) ? sg_dma_len(sg) : remain_data_size;
		vdec_dbg_phys_write(sg_dma_address(sg), dump_len, this_ctx->vdec->id,"%d_%d_%d_dump.es",
			this_ctx->vdec->id, this_ctx->vdec->format, play_cnt);

		remain_data_size -= dump_len;
		if (remain_data_size <= 0) {
			break;
		}
	}
}

void scatter_input_stream_mode_dump_data(struct scatter_input_ctx_t* this_ctx, __u32 wp, __u32 play_cnt)
{
	__u32 to_dumped_map_count = 0;
	__u32 dump_start_wp = this_ctx->last_dump_wp;
	if (wp >= this_ctx->last_dump_wp) {
		to_dumped_map_count = wp - this_ctx->last_dump_wp;
	} else {
		to_dumped_map_count =  MAX_MAP_ITEMS - this_ctx->last_dump_wp + wp;
	}
	if (to_dumped_map_count == 0) {
		return;
	}

	for (; to_dumped_map_count > 0; to_dumped_map_count--)  {
		vdec_dbg_phys_write((this_ctx->map[dump_start_wp].map_data >> 30), this_ctx->map[dump_start_wp].map_data & 0x3fffffff,
							this_ctx->vdec->id,"%d_%d_%d_dump.es", this_ctx->vdec->id, this_ctx->vdec->format, play_cnt);
		dump_start_wp = (dump_start_wp + 1) % MAX_MAP_ITEMS;
	}
}

static int frame_mode_write_data_to_map(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params* param)
{
	u32 i = 0;
	size_t remain_data = param->data_size;
	struct scatterlist* sg = NULL;
	int cur_write_idx = atomic_read(&this_ctx->write_idx);
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "sg_table: nents=%u, orig_nents=%u\n", param->sgt->nents, param->sgt->orig_nents);
	param->pack->map_idx = cur_write_idx;
	param->pack->sgt = param->sgt;
	param->pack->data_size = param->data_size;

	for_each_sg(param->sgt->sgl, sg, param->sgt->nents, i) {
		if (SCATTER_MAP_IS_FULL(atomic_read(&this_ctx->read_idx), cur_write_idx, 1)) {
			SCATTER_INPUT_LOG_ERROR("map full, can't write data\n");
			return -1;
		}
		this_ctx->map[cur_write_idx].map_data = sg_dma_address(sg) << 30;

		if (remain_data >= sg_dma_len(sg)) {
			this_ctx->map[cur_write_idx].map_data |= sg_dma_len(sg) & 0x3fffffff;
			remain_data -= sg_dma_len(sg);
		} else {
			this_ctx->map[cur_write_idx].map_data |= remain_data & 0x3fffffff;
			remain_data = 0;
		}
		param->pack->map_item_count++;
		cur_write_idx = (cur_write_idx + 1) % MAX_MAP_ITEMS;
		if ( remain_data == 0) {
			break;
		}
	}

	if (need_sw_pad) {
		if (SCATTER_MAP_IS_FULL(atomic_read(&this_ctx->read_idx), cur_write_idx, 1)) {
			SCATTER_INPUT_LOG_ERROR("map full, can't write padding data\n");
			return -1;
		}

		param->pack->map_item_count++;
		this_ctx->map[cur_write_idx].map_data = (this_ctx->software_padding_data_paddr << 30) | scatter_input_padding_size;
		cur_write_idx = (cur_write_idx + 1) % MAX_MAP_ITEMS;
	}
	// all page and write ok, update write_idx
	atomic_set(&this_ctx->write_idx, cur_write_idx);
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,
		"write data to map success, size:%zu, padding_data_size:%zu, scatter_block_count:%u,"
		"mapped count:%u, current map write_idx:%d, read_idx:%d, padding_size:%zu\n",
		param->data_size, param->padding_size, param->sgt->nents, param->pack->map_item_count,
		atomic_read(&this_ctx->write_idx), atomic_read(&this_ctx->read_idx), param->padding_size);

	if (scatter_input_loglevel & SCATTER_INPUT_LOG_DUMP_MAP) {
		dump_host_map(this_ctx, param->pack->map_idx, param->pack->map_item_count);
	}
	return 0;
}

static int frame_mode_write_data_to_device(struct scatter_input_ctx_t* this_ctx,  struct scatter_map_update_params* param)
{
	int cur_host_map_idx = param->pack->map_idx;
	int device_map_idx = 0;
	__u64 offset = param->offset;
	__u32 has_written_count = 0; // skiped map item count
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "map read_idx:%d, pack map_idx:%d, map_count:%u, offset:%llu\n", atomic_read(&this_ctx->read_idx), param->pack->map_idx, param->pack->map_item_count, offset);
	if (offset > 0) {
		for (; has_written_count < param->pack->map_item_count; ++has_written_count) {
			if (offset < (this_ctx->map[cur_host_map_idx].map_data & 0x3fffffff)) {
				break;
			}
			offset -= this_ctx->map[cur_host_map_idx].map_data & 0x3fffffff;
			cur_host_map_idx = (cur_host_map_idx + 1) % MAX_MAP_ITEMS;
		}
	}

	if (offset > 0) {
		__u64 size = (this_ctx->map[cur_host_map_idx].map_data & 0x3fffffff) - offset;
		__u64 map_data = (this_ctx->map[cur_host_map_idx].map_data >> 30) + offset;
		map_data = (map_data << 30) | size;
		SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "ori_map:%llx, res_map:%llx, offset:%llu\n",
			this_ctx->map[cur_host_map_idx].map_data, map_data, offset);
		*((__u64*)this_ctx->map_start_vrit_addr + device_map_idx) =  SCATTER_INPUT_REVERSE_ENDIAN_64(map_data);
		device_map_idx++;
		cur_host_map_idx = (cur_host_map_idx + 1) % MAX_MAP_ITEMS;
		has_written_count++;
	}

	for (; has_written_count < param->pack->map_item_count; ++device_map_idx, ++has_written_count) {
		*((__u64*)this_ctx->map_start_vrit_addr + device_map_idx) = SCATTER_INPUT_REVERSE_ENDIAN_64(this_ctx->map[cur_host_map_idx].map_data);
		cur_host_map_idx = (cur_host_map_idx + 1) % MAX_MAP_ITEMS;
	}

	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,
		"scatter_input_write_data_to_device success, data_size:%zu, write map count:%d, offset:%llu\n",
		param->pack->data_size, device_map_idx, offset);
	if (scatter_input_loglevel & SCATTER_INPUT_LOG_DUMP_MAP) {
		dump_device_map(this_ctx, device_map_idx);
	}
	return device_map_idx;
}

static int frame_mode_free_data(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params* param)
{
	int new_read_idx = atomic_read(&this_ctx->read_idx);
	new_read_idx = (new_read_idx + param->pack->map_item_count ) % MAX_MAP_ITEMS;
	atomic_set(&this_ctx->read_idx, new_read_idx);

	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "free data: size:%zu ,new read_idx:%d\n", param->pack->data_size, new_read_idx);
	return 0;
}

struct scatter_input_device_map_ops frame_mode_map_ops = {
	.scatter_input_write_data_to_device = &frame_mode_write_data_to_device,
	.scatter_input_write_data_to_map = &frame_mode_write_data_to_map,
	.scatter_input_free_data = &frame_mode_free_data,
};

static int stream_mode_write_data_to_map(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params* param)
{
	u32 cur_wp = atomic_read(&this_ctx->write_idx);
	__u64 map_data = 0;
	if (SCATTER_MAP_IS_FULL(atomic_read(&this_ctx->read_idx), cur_wp, 2)) {
		SCATTER_INPUT_LOG_ERROR("map full, can't write data\n");
		return -1;
	}

	map_data = ((param->addr & 0x3ffffffff) << 30)  | param->data_size;

	this_ctx->map[cur_wp].map_data = map_data;
	this_ctx->data_level += param->data_size;
	this_ctx->map[cur_wp].data_level = this_ctx->data_level;

	*((__u64*)this_ctx->map_start_vrit_addr + cur_wp) =  SCATTER_INPUT_REVERSE_ENDIAN_64(map_data);
	cur_wp = (cur_wp + 1) % MAX_MAP_ITEMS;
	atomic_set(&this_ctx->write_idx, cur_wp);
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,
		"write data to map success, size:%zu, addr:%llx,map_data:0x%llx, current map write_idx:%d, read_idx:%d, data_level:%llx\n",
		param->data_size, (unsigned long long)param->addr, map_data, atomic_read(&this_ctx->write_idx),
		atomic_read(&this_ctx->read_idx), this_ctx->data_level);

	if (this_ctx->vdec->need_more_data & VDEC_NEED_MORE_DATA) {
		this_ctx->vdec->need_more_data &= ~VDEC_NEED_MORE_DATA;
		vdec_up(this_ctx->vdec);
	}

	return 1; // one item written;
}

static int stream_mode_write_data_to_device(struct scatter_input_ctx_t* this_ctx,  struct scatter_map_update_params* param)
{
	return -1; //do nothing
}

static int stream_mode_free_data(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params* param)
{
	__u32 new_rp = param->rp;
	__u32 to_be_freed_map_count = 0;
	if (new_rp == this_ctx->old_rp) {
		SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,
			"last run consume all data of map or don't consume data, error maybe occurred, rp:%u\n", new_rp);
		return -1;
	}

	// 0 1 2 3 4 5
	if (new_rp > this_ctx->old_rp) {
		to_be_freed_map_count = new_rp - this_ctx->old_rp - 1;
		if (this_ctx->old_rp != 0 || this_ctx->is_wrap) {
			to_be_freed_map_count += 1;
		}
	} else {
		this_ctx->is_wrap = true;
		to_be_freed_map_count =  MAX_MAP_ITEMS - this_ctx->old_rp + new_rp;
	}

	if (to_be_freed_map_count == 0) {
		SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "the data within map item do not cousmued completely, need not free, rp:%u\n", new_rp);
		return 0;
	}

	atomic_set(&this_ctx->read_idx, new_rp);

	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "%u items has been freed, old_rp:%u, new_rp:%u\n", to_be_freed_map_count, this_ctx->old_rp, new_rp);
	this_ctx->old_rp = new_rp;
	return to_be_freed_map_count;
}

struct scatter_input_device_map_ops stream_mode_map_ops = {
	.scatter_input_write_data_to_map = &stream_mode_write_data_to_map,
	.scatter_input_write_data_to_device = &stream_mode_write_data_to_device,
	.scatter_input_free_data = &stream_mode_free_data,
};

int scatter_input_init(struct scatter_input_ctx_t* this_ctx, struct vdec_s* vdec)
{
	if (this_ctx == NULL || vdec == NULL) {
		SCATTER_INPUT_LOG_ERROR("can't init, scatter_input_ctx or vdec is null\n");
		return -1;
	}
	if (vdec_frame_based(vdec)) {
		this_ctx->map_ops = &frame_mode_map_ops;
		this_ctx->need_hw_padding = !need_sw_pad;
		if (!need_sw_pad) {
			SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "init without sw padding\n");
			goto continue_init;
		}
		this_ctx->software_padding_data_vaddr =
			decoder_dma_alloc_coherent(&this_ctx->padding_mem_handle,
				scatter_input_padding_size, &this_ctx->software_padding_data_paddr, "scatter_input_padding");

			if (!this_ctx->software_padding_data_vaddr) {
			SCATTER_INPUT_LOG_ERROR("fail to alloc mem for software padding\n");
			return -1;
		}

		if (vdec->format == VFORMAT_AVS2 || vdec->format == VFORMAT_AVS3)
			memset(this_ctx->software_padding_data_vaddr, 0xff, scatter_input_padding_size);
		else
			memset(this_ctx->software_padding_data_vaddr, 0, scatter_input_padding_size);

		if (vdec->format == VFORMAT_MPEG12) {
			// TODO: write  data once
			this_ctx->software_padding_data_vaddr[0] = 0x0;
			this_ctx->software_padding_data_vaddr[1] = 0x0;
			this_ctx->software_padding_data_vaddr[2] = 0x1;
			this_ctx->software_padding_data_vaddr[3] = 0x0;

		} else if (vdec->format == VFORMAT_MPEG4) {
			this_ctx->software_padding_data_vaddr[0] = 0x0;
			this_ctx->software_padding_data_vaddr[1] = 0x0;
			this_ctx->software_padding_data_vaddr[2] = 0x1;
			this_ctx->software_padding_data_vaddr[3] = 0xb6;
		}
	} else if (vdec_stream_based(vdec)) {
		this_ctx->map_ops = &stream_mode_map_ops;
	} else {
		SCATTER_INPUT_LOG_ERROR("vdec mode unsupport\n");
		return -1;
	}
continue_init:
	this_ctx->vdec = vdec;
	vdec->scatter_input_ctx = this_ctx;
	atomic_set(&this_ctx->read_idx, 0);
	atomic_set(&this_ctx->write_idx, 0);
	this_ctx->map_start_vrit_addr =
		decoder_dma_alloc_coherent(&this_ctx->mem_handle, SCATTER_INPUT_MAP_SIZE, &this_ctx->map_start_phy_addr, "scatter_input");
	memset(this_ctx->map, 0, SCATTER_INPUT_HOST_MAP_SIZE);
	this_ctx->data_level = 0;
	this_ctx->shift_byte = 0;
	this_ctx->prev_used_data_size = 0;
	this_ctx->old_rp = 0;
	this_ctx->is_wrap = false;
	this_ctx->last_dump_wp = 0;
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "scatter input ctx inited.\n");
	return 0;
}
EXPORT_SYMBOL(scatter_input_init);

void scatter_input_destroy(struct scatter_input_ctx_t* this_ctx)
{
	if (this_ctx == NULL) {
		return;
	}
	decoder_dma_free_coherent(this_ctx->mem_handle, SCATTER_INPUT_MAP_SIZE, this_ctx->map_start_vrit_addr, this_ctx->map_start_phy_addr);
	if (need_sw_pad)
		decoder_dma_free_coherent(this_ctx->padding_mem_handle, scatter_input_padding_size,
			this_ctx->software_padding_data_vaddr,this_ctx->software_padding_data_paddr);
}

static void scatter_input_wait_finish_padding(void)
{
	SET_VREG_MASK(HEVC_SCATTER_MAP_FIFO_LEVEL, 1 << 17); //force scatter manual finish enable
	CLEAR_VREG_MASK(HEVC_SCATTER_MAP_FIFO_LEVEL, 1 << 17); //clear force scatter manual finish enable
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO,"done :%x \n", READ_VREG(HEVC_SCATTER_MAP_FIFO_LEVEL));
}

void scatter_input_add_hw_padding_if_need(struct scatter_input_ctx_t* this_ctx)
{
	u32 data32 = 0;
	u32 data1 = 0x0;
	u32 data2 = 0x0;

	if (!this_ctx->need_hw_padding)
		return;

	if (this_ctx->vdec->format == VFORMAT_AVS2 || this_ctx->vdec->format == VFORMAT_AVS3) {
		data1 = 0xffffffff;
		data2 = 0xffffffff;
	} else if (this_ctx->vdec->format == VFORMAT_MPEG12) {
		data1 = 0x00000100;
		data2 = 0x00000100;
	} else if (this_ctx->vdec->format == VFORMAT_MPEG4) {
		data1 = 0x000001b6;
		data2 = 0x000001b6;
	}

	WRITE_VREG(HEVC_SCATTER_MANUAL_DATA1, data1);
	WRITE_VREG(HEVC_SCATTER_MANUAL_DATA2, data2);

	SET_VREG_MASK(HEVC_SCATTER_MAP_FIFO_LEVEL, 5 << 14); //delay mode
	data32 = READ_VREG(HEVC_SCATTER_MANUAL_CONTROL);
	data32 = data32 & 0x80000000;
	data32 |= 0x1;
	data32 |= (scatter_input_padding_size / 8) << 1;
	WRITE_VREG(HEVC_SCATTER_MANUAL_CONTROL, data32);
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "data32:0x%x SCATTER_MAP_FIFO_LEVEL:0x%x done\n", data32, READ_VREG(HEVC_SCATTER_MAP_FIFO_LEVEL));
}
EXPORT_SYMBOL(scatter_input_add_hw_padding_if_need);

void scatter_input_wait_idle(void)
{
	ulong timeout = 0;
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO, "waiting scatter_input device.\n");

	scatter_input_wait_finish_padding();

	timeout = jiffies + msecs_to_jiffies(10);
	WRITE_VREG(HEVC_SHIFT_STATUS, 0);
	while (READ_VREG(HEVC_STREAM_CONTROL) & 0x2) {
		SET_VREG_MASK(HEVC_PARSER_INT_STATUS, 1 << 8); //disable map empty
		if (time_after(jiffies, timeout)) {
			SCATTER_INPUT_LOG_ERROR("timeout, stream ctrl 0x%x\n", READ_VREG(HEVC_STREAM_CONTROL));
			break;
		}
		msleep(1);
	};
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO, "waiting scatter_input device done.\n");
}
EXPORT_SYMBOL(scatter_input_wait_idle);

void print_data_frame_mode(struct scatter_input_ctx_t* this_ctx, struct scatter_input_data_package_t* pack)
{
	struct sg_table* sgt = pack->sgt;
	char* buf = vmalloc(pack->data_size);
	char* vaddr = NULL;
	if (sg_copy_to_buffer(sgt->sgl, sgt->nents, buf, pack->data_size) <= 0 ) {
		pr_err("%s, copy err\n", __func__);
		vfree(buf);
		return;
	}

	pr_info("%s size:%zx, head:%02x %02x %02x %02x %02x %02x %02x %02x "
		"tail:%02x %02x %02x %02x %02x %02x %02x %02x\n",
		__func__, pack->data_size,
		(u8)buf[0], (u8)buf[1], (u8)buf[2], (u8)buf[3],
		(u8)buf[4], (u8)buf[5], (u8)buf[6], (u8)buf[7],
		(u8)buf[pack->data_size-8], (u8)buf[pack->data_size-7],
		(u8)buf[pack->data_size-6], (u8)buf[pack->data_size-5],
		(u8)buf[pack->data_size-4], (u8)buf[pack->data_size-3],
		(u8)buf[pack->data_size-2], (u8)buf[pack->data_size-1]);
	if (need_sw_pad) {
		vaddr = this_ctx->software_padding_data_vaddr;
		pr_info("padding: %02x %02x %02x %02x %02x %02x %02x %02x \n",
			vaddr[0], vaddr[1], vaddr[2], vaddr[3],
			vaddr[4], vaddr[5], vaddr[6], vaddr[7]);
	}

	vfree(buf);
}
EXPORT_SYMBOL(print_data_frame_mode);



void scatter_input_set_enable(bool enable)
{
	u32 data32 = READ_VREG(HEVC_SCATTER_CONTROL);

	if (enable)
		data32 |= 1;
	else
		data32 &= ~1;

	WRITE_VREG(HEVC_SCATTER_CONTROL, data32);
	SET_VREG_MASK(HEVC_SCATTER_MANUAL_CONTROL, 1 << 31);
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO,"set scatter_input enable:%d.\n", enable);
}

int scatter_input_set_prefix(struct scatter_input_ctx_t* this_ctx)
{
	u32 data32 = READ_VREG(HEVC_ASSIST_AXIADDR_PREFIX);
	u32 start_prefix = PREFIX_ADDR(this_ctx->map_start_phy_addr);
	u32 end_prefix = PREFIX_ADDR((this_ctx->map_start_phy_addr + SCATTER_INPUT_MAP_SIZE));

	data32 |= start_prefix << 26;
	data32 |= end_prefix << 24;

	WRITE_VREG(HEVC_ASSIST_AXIADDR_PREFIX, data32);

	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO, "phy_addr:0x%llx, set prefix:0x%x\n", (unsigned long long)this_ctx->map_start_phy_addr, data32);
	return 0;
}

int scatter_input_get_prefix(struct scatter_input_ctx_t* this_ctx)
{
	u32 data32 = 0;
	u32 start_prefix = PREFIX_ADDR(this_ctx->map_start_phy_addr);
	u32 end_prefix = PREFIX_ADDR((this_ctx->map_start_phy_addr + SCATTER_INPUT_MAP_SIZE));

	data32 |= start_prefix << 2;
	data32 |= end_prefix;

	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_INFO, "get prefix:0x%x\n", data32);
	return data32;
}

__u64 scatter_input_get_shift_byte(struct scatter_input_ctx_t* this_ctx)
{
	return this_ctx ? this_ctx->shift_byte : 0;
}
EXPORT_SYMBOL(scatter_input_get_shift_byte);

void scatter_input_set_shift_byte(struct scatter_input_ctx_t* this_ctx, __u32 shift_byte)
{
	__u32 cur_used_data_size = shift_byte - this_ctx->prev_used_data_size;
	this_ctx->shift_byte += cur_used_data_size;
	this_ctx->prev_used_data_size = shift_byte;
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,"shift_byte:%u, total:0x%llx\n", shift_byte, this_ctx->shift_byte);
}
EXPORT_SYMBOL(scatter_input_set_shift_byte);

__u32 scatter_input_get_wp(struct scatter_input_ctx_t* this_ctx)
{
	return this_ctx ? atomic_read(&this_ctx->write_idx) : 0;
}
EXPORT_SYMBOL(scatter_input_get_wp);

void scatter_input_set_rp(struct scatter_input_ctx_t* this_ctx, __u32 new_rp)
{
	atomic_set(&this_ctx->read_idx, new_rp);
}
EXPORT_SYMBOL(scatter_input_set_rp);

__u64 scatter_input_get_data_level(struct scatter_input_ctx_t* this_ctx, __u32 wp, __u64 shift_byte)
{
	if (this_ctx == NULL) {
		SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG,"this_ctx, is null\n");
		return 0;
	}
	if  (wp == 0) {
		wp = MAX_MAP_ITEMS - 1;
	}else {
		wp = wp - 1;
	}
	SCATTER_INPUT_LOG(SCATTER_INPUT_LOG_DEBUG, "level:%llx, shift_byte: %llx\n", this_ctx->map[wp].data_level - shift_byte, shift_byte);
	return this_ctx->map[wp].data_level > shift_byte ? this_ctx->map[wp].data_level - shift_byte : 0;
}
EXPORT_SYMBOL(scatter_input_get_data_level);

void scatter_input_set_last_dump_wp(struct scatter_input_ctx_t* this_ctx, __u32 wp)
{
	this_ctx->last_dump_wp = wp;
}
