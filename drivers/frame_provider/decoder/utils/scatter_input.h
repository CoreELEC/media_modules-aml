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
#ifndef _SCATTER_INPUT_H_
#define _SCATTER_INPUT_H_

#include <linux/types.h>
#include <linux/atomic.h>


#define MAX_MAP_ITEMS 8091
#define SCATTER_INPUT_MAP_SIZE (MAX_MAP_ITEMS * 8)
#define SCATTER_INPUT_HOST_MAP_SIZE (MAX_MAP_ITEMS * sizeof(struct map_item))


#ifdef T6X_12_BIT_MAP

#define SCATTER_GET_RP(reg) ((READ_VREG(reg) & 0x03ffe000) >> 13)
#define SCATTER_SET_WP(reg, wp) WRITE_VREG(reg, (READ_VREG(reg) & 0xffffe001) | (wp << 1))
#define SCATTER_GET_WP(reg)	((READ_VREG(reg) & 0x00001ffe) >> 1)

#else
#define SCATTER_GET_RP(reg) ((READ_VREG(reg) & 0x07ffc000) >> 14)
#define SCATTER_SET_WP(reg, wp) WRITE_VREG(reg, (READ_VREG(reg) & 0xffffc001) | (wp << 1))
#define SCATTER_GET_WP(reg)	((READ_VREG(reg) & 0x00003ffe) >> 1)
#endif

struct vdec_s;
struct sg_table;
struct scatter_input_data_package_t{
	char*  user_addr;
	size_t data_size;
	__u32 map_idx; // position of this pack first map_item in host map
	__u32 map_item_count;
	struct sg_table* sgt;
};
struct map_item {
	__u64 map_data;
	__u64 data_level;
};

struct scatter_map_update_params {
	size_t data_size;
	dma_addr_t addr;
	__u32 rp;
	// the follow filed used in frame mode;
	struct scatter_input_data_package_t* pack;
	struct sg_table* sgt;
	size_t padding_size;
	__u32 offset; //how many data bytes shoudle be skiped in this data_pack
};

struct scatter_input_ctx_t{
	struct scatter_input_device_map_ops* map_ops;
	unsigned long mem_handle;
	struct vdec_s* vdec;
	struct map_item map[MAX_MAP_ITEMS];
	dma_addr_t map_start_phy_addr;
	void* map_start_vrit_addr;
	atomic_t read_idx;
	atomic_t write_idx;
	bool is_wrap;
	u8* software_padding_data_vaddr;
	dma_addr_t software_padding_data_paddr;
	unsigned long padding_mem_handle;
	bool need_hw_padding;
	__u32 last_dump_wp; // used for data dump
	// the follow filed used in stream mode
	__u64 shift_byte;
	__u64 data_level;
	__u32 prev_used_data_size;
	__u32 old_rp; // rp before decode
};
struct scatter_input_device_map_ops {
	// @return  the count of map items that has been written to hw
	int (*scatter_input_write_data_to_device)(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params*);
	/// write data to host map that software use
	int (*scatter_input_write_data_to_map)(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params*);
	int (*scatter_input_free_data)(struct scatter_input_ctx_t* this_ctx, struct scatter_map_update_params*);
};
int scatter_input_init(struct scatter_input_ctx_t* this_ctx, struct vdec_s* vdec);

void scatter_input_destroy(struct scatter_input_ctx_t* this_ctx);

u32 scatter_input_crc32(void *sg_table_p, u32 data_size);
u32 scatter_input_crc32_sum(void *sg_table_p, u32 data_size);
/// @brief dump data_pack with scatter addr, this func use  "dump_cb" to dump sg_list in sg_table one by one
/// @param this_ctx
/// @param pack data_pack to be dumped
/// @param dump_cb same as dbg_data_wr defined in common.h
void scatter_input_frame_mode_dump_data(struct scatter_input_ctx_t* this_ctx, struct scatter_input_data_package_t* pack, __u32 play_cnt);

void scatter_input_stream_mode_dump_data(struct scatter_input_ctx_t* this_ctx, __u32 wp, __u32 play_cnt);

void scatter_input_wait_idle(void);

void scatter_input_set_enable(bool enable);

int scatter_input_set_prefix(struct scatter_input_ctx_t* this_ctx);
int scatter_input_get_prefix(struct scatter_input_ctx_t* this_ctx);

__u64 scatter_input_get_shift_byte(struct scatter_input_ctx_t* this_ctx);
void scatter_input_set_shift_byte(struct scatter_input_ctx_t* this_ctx, __u32 shift_byte);

__u32 scatter_input_get_wp(struct scatter_input_ctx_t* this_ctx);

void scatter_input_set_last_dump_wp(struct scatter_input_ctx_t* this_ctx, __u32 wp);

void scatter_input_set_rp(struct scatter_input_ctx_t* this_ctx,  __u32 new_rp);
void print_data_frame_mode(struct scatter_input_ctx_t* this_ctx, struct scatter_input_data_package_t* pack);
__u64 scatter_input_get_data_level(struct scatter_input_ctx_t* this_ctx, __u32 wp, __u64 shift_byte);

void scatter_input_add_hw_padding_if_need(struct scatter_input_ctx_t* this_ctx);
#endif