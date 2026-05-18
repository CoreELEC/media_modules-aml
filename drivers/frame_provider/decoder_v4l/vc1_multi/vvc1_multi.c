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
#define DEBUG
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0))
#include <linux/amlogic/meson_uvm_allocator.h>
#else
#include <linux/amlogic/media/meson_uvm_allocator.h>
#endif

#include "../../../stream_input/amports/streambuf_reg.h"
#include "../../../stream_input/amports/amports_priv.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../amvdec_ports/vdec_drv_base.h"
#include "../../decoder/utils/amvdec.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_feature.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../decoder/utils/aml_buf_helper.h"
#include "../../decoder/utils/decoder_dma_alloc.h"
#include "../../decoder/utils/vdec_profile.h"
#include "../../../common/media_utils/media_utils.h"

#include <uapi/linux/tee.h>
#include <linux/delay.h>

#define DRIVER_NAME "ammvdec_vc1_v4l"
#define MODULE_NAME "ammvdec_vc1_v4l"

#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.vc1"
#define VC1_MAX_SUPPORT_SIZE (1920*1088)

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2
#define BI_PICTURE  3

#define ORI_BUFFER_START_ADDR   0x01000000

#define INTERLACE_FLAG          0x80
#define BOTTOM_FIELD_FIRST_FLAG 0x40

/* protocol registers */
#define VC1_PIC_RATIO       AV_SCRATCH_0
#define VC1_FRAME_RATE      AV_SCRATCH_1
#define VC1_ERROR_COUNT    AV_SCRATCH_6
#define VC1_SOS_COUNT     AV_SCRATCH_7
#define VC1_BUFFERIN       AV_SCRATCH_8
#define VC1_BUFFEROUT      AV_SCRATCH_9
#define VC1_REPEAT_COUNT    AV_SCRATCH_A
#define VC1_TIME_STAMP      AV_SCRATCH_B
#define VC1_OFFSET_REG      AV_SCRATCH_C
#define MEM_OFFSET_REG      AV_SCRATCH_F

#define CANVAS_BUF_REG      AV_SCRATCH_D
#define ANC0_CANVAS_REG     AV_SCRATCH_E
#define ANC1_CANVAS_REG     AV_SCRATCH_5
#define DECODE_STATUS       AV_SCRATCH_H
#define VC1_PIC_INFO        AV_SCRATCH_J
#define DEBUG_REG1          AV_SCRATCH_M
#define DEBUG_REG2          AV_SCRATCH_N

#define VC1_LMEM_BUF_ADR   AV_SCRATCH_I

#define DECODE_STATUS_SEQ_HEADER_DONE 0x1
#define DECODE_STATUS_PIC_HEADER_DONE 0x2
#define DECODE_STATUS_PIC_SKIPPED     0x3
#define DECODE_STATUS_BUF_INVALID     0x4
#define DECODE_STATUS_PARAM_CHECK     0x5
#define DECODE_STATUS_PIC_DONE        0x6

#define DEC_RESULT_NONE     0
#define DEC_RESULT_DONE     1
#define DEC_RESULT_AGAIN    2
#define DEC_RESULT_ERROR    3
#define DEC_RESULT_FORCE_EXIT 4
#define DEC_RESULT_EOS 5

/*bit0:1 support vc1 new version, 0: old version
  bit1:1 support DECODE_STATUS_PARAM_CHECK*/
#define NEW_DRV_VER         3

#define VF_POOL_SIZE		16
#define DECODE_BUFFER_NUM_MAX	4
#define WORKSPACE_SIZE		(2 * SZ_1M)
#define MAX_BMMU_BUFFER_NUM	(DECODE_BUFFER_NUM_MAX + 1)
#define VF_BUFFER_IDX(n)	(1 + n)
#define DCAC_BUFF_START_ADDR	0x01f00000
#define RP_WORKAROUND_SIZE  SZ_4K

#define PUT_INTERVAL        (HZ/100)

#define MEM_FIFO_CNT_BIT        16
#define MEM_LEVEL_CNT_BIT       18

#define VC1_Numerator_Index_MAX   8
#define VC1_Denominator_Index_MAX 3

static struct vframe_s *vvc1_vf_peek(void *);
static struct vframe_s *vvc1_vf_get(void *);
static void vvc1_vf_put(struct vframe_s *, void *);
static int vvc1_vf_states(struct vframe_states *states, void *);
static int vvc1_event_cb(int type, void *data, void *private_data);

static const char vvc1_dec_id[] = "vvc1-dev";

static const struct vframe_operations_s vvc1_vf_provider = {
	.peek = vvc1_vf_peek,
	.get = vvc1_vf_get,
	.put = vvc1_vf_put,
	.event_cb = vvc1_event_cb,
	.vf_states = vvc1_vf_states,
};

//static u32 buf_offset;
static u32 unstable_pts_debug;

enum {
	RATE_MEASURE_START_PTS = 0,
	RATE_MEASURE_END_PTS,
	RATE_MEASURE_DONE
};
#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_24_FPS  3755	/* 23.97 */
#define RATE_30_FPS  3003	/* 29.97 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)

#define VC1_DEBUG_DETAIL		0x01
#define VC1_DEBUG_WORK_DETAIL		0x02
#define VC1_DEBUG_BUFMGR		0x04

#define INVALID_IDX -1  /* Invalid buffer index.*/
#define MAX_SIZE_2K (1920 * 1088)

static u32 udebug_flag;
static int debug;
unsigned int debug_mask = 0xff;
static u32 wait_time = 5;

struct pic_info_t {
	u32 buffer_info;
	u32 index;
	u32 offset;
	u32 width;
	u32 height;
	u32 pts;
	u64 pts64;
	bool pts_valid;
	ulong v4l_ref_buf_addr;
	ulong cma_alloc_addr;
	u32 hw_decode_time;
	u32 frame_size; // For frame base mode
	u64 timestamp;
	u32 picture_type;
	unsigned short decode_pic_count;
	u32 repeat_cnt;
};

struct vdec_vc1_hw_s {
	spinlock_t lock;
	struct platform_device *platform_dev;
	struct work_struct work;
	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
	unsigned char again_flag;
	//unsigned char recover_flag;
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_prog;
	u32 saved_resolution;
	u32 avi_flag;
	u32 vavs_ratio;
	u32 pic_type;

	u32 vf_buf_num_used;
	u32 next_pts;
	u64 next_pts_us64;
	unsigned char throw_pb_flag;

	/*debug*/
	u32 ucode_pause_pos;
	u32 decode_pic_count;
	u8 reset_decode_flag;
	u32 display_frame_count;
	u32 buf_status;
	u32 pre_parser_wr_ptr;
	u32 eos;
	s32 refs[2];
	atomic_t prepare_num;
	atomic_t put_num;
	atomic_t peek_num;
	atomic_t get_num;
	s32 ref_use[DECODE_BUFFER_NUM_MAX];
	//s32 buf_use[DECODE_BUFFER_NUM_MAX];
	//s32 vf_ref[DECODE_BUFFER_NUM_MAX];
	u32 decoding_index;
	struct pic_info_t pics[DECODE_BUFFER_NUM_MAX];
	u32 interlace_flag;
	u32 new_type;
	void *v4l2_ctx;
	struct aml_buf *aml_buf;
	u32 res_ch_flag;
	u32 last_width;
	u32 last_height;
	bool v4l_params_parsed;
	struct vframe_s vframe_dummy;
	u32 dynamic_buf_num_margin;
	u32 cur_duration;
	u32 canvas_mode;
	dos_addr_t last_wp;
	dos_addr_t last_rp;
	int dec_result;
	volatile bool reset_flag;
	volatile bool remove_flag;
	volatile bool reload_task_start;
	spinlock_t reset_lock;

//new
	//unsigned char m_ins_flag;
	void *mm_blk_handle;
	//spinlock_t lock;
	//struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	struct vframe_chunk_s *chunk;
	u32 stat;
	u8 init_flag;
	struct firmware_s *fw;
	int vdec_pg_enable_flag;

	ulong lmem_phy_handle;
	void *lmem_addr;
	dma_addr_t lmem_phy_addr;

	u32 reg_scratch_0;
	u32 reg_scratch_1;
	u32 reg_scratch_2;
	u32 reg_scratch_3;
	u32 reg_scratch_4;
	u32 reg_scratch_5;
	u32 reg_scratch_6;
	u32 reg_scratch_7;
	u32 reg_scratch_8;
	u32 reg_scratch_9;
	u32 reg_scratch_A;
	u32 reg_scratch_B;
	u32 reg_scratch_C;
	u32 reg_scratch_D;
	u32 reg_scratch_E;
	u32 reg_scratch_F;
	u32 reg_scratch_G;
	u32 reg_scratch_H;
	u32 reg_scratch_I;
	u32 reg_mb_width;
	u32 reg_viff_bit_cnt;
	u32 reg_canvas_addr;
	u32 reg_dbkr_canvas_addr;
	u32 reg_dbkw_canvas_addr;
	u32 reg_anc2_canvas_addr;
	u32 reg_anc0_canvas_addr;
	u32 reg_anc1_canvas_addr;
	u32 reg_anc3_canvas_addr;
	u32 reg_anc4_canvas_addr;
	u32 reg_anc5_canvas_addr;
	u32 slice_ver_pos_pic_type;
	u32 avs_co_mb_wr_addr;
	u32 slice_start_byte_01;
	u32 slice_start_byte_23;
	u32 vcop_ctrl_reg;
	u32 iqidct_control;
	u32 rv_ai_mb_count;
	u32 slice_qp;
	u32 dc_scaler;
	u32 avsp_iq_wq_param_01;
	u32 avsp_iq_wq_param_23;
	u32 avsp_iq_wq_param_45;
	u32 avs_co_mb_rd_addr;
	u32 dblk_mb_wid_height;
	u32 mc_pic_w_h;
	u32 avs_co_mb_rw_ctl;
	u32 vld_decode_control;

	u32 reg_mpeg1_2_reg;
	u32 reg_pic_head_info;
	u32 reg_f_code_reg;
	u32 reg_slice_ver_pos_pic_type;
	u32 reg_vcop_ctrl_reg;
	u32 reg_mb_info;

	u32 reg_iqidct_control;

	bool restore_reg_flag;
	u32 reg_scratch_J;
	u32 reg_last_mvx;
	u32 reg_vc1_control;
	u32 reg_power_ctl_vld;
	u32 reg_mc_ctrl1;
	u32 reg_mdec_pic_dc_ctrl;

	struct timer_list check_timer;
	u32 decode_timeout_count;
	unsigned long int start_process_time;
	u32 last_vld_level;
	u32 canvas_spec[DECODE_BUFFER_NUM_MAX];
	struct canvas_config_s vc1_canvas_config[DECODE_BUFFER_NUM_MAX][2];

	u32 timeout_processing;
	struct work_struct timeout_work;
	void (*vdec_cb)(struct vdec_s *, void *, int);
	void *vdec_cb_arg;

	u32 run_count;
	u32	not_run_ready;
	u32 buffer_not_ready;
	u32	input_empty;

	bool process_busy;
	bool timeout;
	u32 buf_recycle_status;
	struct vdec_info *gvs;
	struct dec_sysinfo vvc1_amstream_dec_info;
	bool is_reset;

	u32 consume_byte;
	u32 start_bit_cnt;

	u32 buf_offset;
	u32 vvc1_ratio;
	u32 unstable_pts;

	u32 report_width;
	u32 report_height;
	u32 last_dur;

	u32 metadata_config_flag;
};

static struct task_ops_s task_dec_ops;
static u32 run_ready_min_buf_num = 1;
static u32 default_vc1_margin = 2;

#define DECODE_ID(hw) (hw_to_vdec(hw)->id)
static unsigned int max_process_time[MAX_INSTANCE_MUN];
static unsigned int decode_timeout_val = 200;
static int start_decode_buf_level = 0x4000;
#define LMEM_BUF_SIZE (0x500 * 2)
#define CHECK_INTERVAL        (HZ/100)

#undef pr_info
#define pr_info pr_cont

static int prepare_display_buf(struct vdec_vc1_hw_s *hw, struct pic_info_t *pic);
static int find_free_buffer(struct vdec_vc1_hw_s *hw);
static void flush_output(struct vdec_vc1_hw_s * hw);
static int notify_v4l_eos(struct vdec_s *vdec);

//#define T6D_PRINT_CRC           1

int vc1_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((debug_flag == 0) ||
		((debug & debug_flag) &&
		((1 << index) & debug_mask))) {
		unsigned char *buf = kzalloc(512, GFP_ATOMIC);
		int len = 0;
		va_list args;

		if (!buf)
			return 0;

		va_start(args, fmt);
		len = sprintf(buf, "%d: ", index);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
		kfree(buf);
	}
	return 0;
}

static inline bool close_to(int a, int b, int m)
{
	return abs(a - b) < m;
}

static inline u32 index2canvas(u32 index)
{
	const u32 canvas_tab[DECODE_BUFFER_NUM_MAX] = {
#if 1	/* ALWAYS.MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	0x010100, 0x030302, 0x050504, 0x070706/*,
	0x090908, 0x0b0b0a, 0x0d0d0c, 0x0f0f0e*/
#else
		0x020100, 0x050403, 0x080706, 0x0b0a09
#endif
	};

	return canvas_tab[index];
}


static void vvc1_save_regs(struct vdec_vc1_hw_s *hw)
{

	hw->restore_reg_flag = 1;

	hw->reg_scratch_J = READ_VREG(AV_SCRATCH_J);
	hw->reg_vc1_control = READ_VREG(VC1_CONTROL_REG);
	hw->reg_last_mvx = READ_VREG(LAST_MVX);
	hw->reg_power_ctl_vld = READ_VREG(POWER_CTL_VLD);
	hw->reg_mc_ctrl1 = READ_VREG(MC_CTRL1);
	hw->reg_mdec_pic_dc_ctrl = READ_VREG(MDEC_PIC_DC_CTRL);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s AV_SCRATCH_J = 0x%x, MDEC_PIC_DC_CTRL = 0x%x, AV_SCRATCH_4 = 0x%x, POWER_CTL_VLD = 0x%x, MC_CTRL1 0x%x\n",
			__func__, READ_VREG(AV_SCRATCH_J), READ_VREG(MDEC_PIC_DC_CTRL), READ_VREG(AV_SCRATCH_4),
			READ_VREG(POWER_CTL_VLD), READ_VREG(MC_CTRL1));
#if 0
	hw->reg_scratch_0 = READ_VREG(AV_SCRATCH_0);
	hw->reg_scratch_1 = READ_VREG(AV_SCRATCH_1);
	hw->reg_scratch_2 = READ_VREG(AV_SCRATCH_2);
	hw->reg_scratch_3 = READ_VREG(AV_SCRATCH_3);
	hw->reg_scratch_5 = READ_VREG(AV_SCRATCH_5);
	hw->reg_scratch_6 = READ_VREG(AV_SCRATCH_6);
	hw->reg_scratch_7 = READ_VREG(AV_SCRATCH_7);
	hw->reg_scratch_8 = READ_VREG(AV_SCRATCH_8);
	hw->reg_scratch_9 = READ_VREG(AV_SCRATCH_9);
	hw->reg_scratch_A = READ_VREG(AV_SCRATCH_A);
	hw->reg_scratch_C = READ_VREG(AV_SCRATCH_C);
	hw->reg_scratch_D = READ_VREG(AV_SCRATCH_D);
	hw->reg_scratch_E = READ_VREG(AV_SCRATCH_E);
	hw->reg_scratch_F = READ_VREG(AV_SCRATCH_F);
	hw->reg_scratch_G = READ_VREG(AV_SCRATCH_G);
	hw->reg_scratch_H = READ_VREG(AV_SCRATCH_H);
	hw->reg_scratch_I = READ_VREG(AV_SCRATCH_I);

	hw->reg_mb_width = READ_VREG(MB_WIDTH);
	//hw->reg_viff_bit_cnt = READ_VREG(VIFF_BIT_CNT);

	hw->reg_canvas_addr = READ_VREG(REC_CANVAS_ADDR);
	hw->reg_dbkr_canvas_addr = READ_VREG(DBKR_CANVAS_ADDR);
	hw->reg_dbkw_canvas_addr = READ_VREG(DBKW_CANVAS_ADDR);
	hw->reg_anc2_canvas_addr = READ_VREG(ANC2_CANVAS_ADDR);
	hw->reg_anc0_canvas_addr = READ_VREG(ANC0_CANVAS_ADDR);
	hw->reg_anc1_canvas_addr = READ_VREG(ANC1_CANVAS_ADDR);
	hw->reg_anc3_canvas_addr = READ_VREG(ANC3_CANVAS_ADDR);
	hw->reg_anc4_canvas_addr = READ_VREG(ANC4_CANVAS_ADDR);
	hw->reg_anc5_canvas_addr = READ_VREG(ANC5_CANVAS_ADDR);


	hw->slice_ver_pos_pic_type = READ_VREG(SLICE_VER_POS_PIC_TYPE);

	//hw->vc1_control_reg = READ_VREG(VC1_CONTROL_REG);
	hw->avs_co_mb_wr_addr = READ_VREG(VLD_C38);
	hw->slice_start_byte_01 = READ_VREG(SLICE_START_BYTE_01);
	hw->slice_start_byte_23 = READ_VREG(SLICE_START_BYTE_23);
	hw->vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
	hw->iqidct_control = READ_VREG(IQIDCT_CONTROL);
	hw->rv_ai_mb_count = READ_VREG(RV_AI_MB_COUNT);
	hw->slice_qp = READ_VREG(SLICE_QP);

	hw->dc_scaler = READ_VREG(DC_SCALER);
	//hw->avsp_iq_wq_param_01 = READ_VREG(AVSP_IQ_WQ_PARAM_01);
	//hw->avsp_iq_wq_param_23 = READ_VREG(AVSP_IQ_WQ_PARAM_23);
	//hw->avsp_iq_wq_param_45 = READ_VREG(AVSP_IQ_WQ_PARAM_45);
	hw->avs_co_mb_rd_addr = READ_VREG(VLD_C39);
	hw->dblk_mb_wid_height = READ_VREG(DBLK_MB_WID_HEIGHT);
	hw->mc_pic_w_h = READ_VREG(MC_PIC_W_H);
	hw->avs_co_mb_rw_ctl = READ_VREG(VLD_C3D);

	hw->vld_decode_control = READ_VREG(VLD_DECODE_CONTROL);

	hw->reg_mpeg1_2_reg = READ_VREG(MPEG1_2_REG);
	hw->reg_pic_head_info = READ_VREG(PIC_HEAD_INFO);
	hw->reg_f_code_reg = READ_VREG(F_CODE_REG);
	hw->reg_slice_ver_pos_pic_type = READ_VREG(SLICE_VER_POS_PIC_TYPE);
	hw->reg_vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
	hw->reg_mb_info = READ_VREG(MB_INFO);

	hw->reg_iqidct_control = READ_VREG(IQIDCT_CONTROL);
#endif
}

static void vvc1_restore_regs(struct vdec_vc1_hw_s *hw)
{
	if (!hw->restore_reg_flag)
		return;

	WRITE_VREG(AV_SCRATCH_J, hw->reg_scratch_J);
	WRITE_VREG(VC1_CONTROL_REG, hw->reg_vc1_control);
	WRITE_VREG(LAST_MVX, hw->reg_last_mvx);
	WRITE_VREG(POWER_CTL_VLD, hw->reg_power_ctl_vld);
	WRITE_VREG(MC_CTRL1, hw->reg_mc_ctrl1);
	WRITE_VREG(MDEC_PIC_DC_CTRL, hw->reg_mdec_pic_dc_ctrl);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s AV_SCRATCH_J = 0x%x, MDEC_PIC_DC_CTRL = 0x%x, AV_SCRATCH_4 = 0x%x, POWER_CTL_VLD = 0x%x, MC_CTRL1 0x%x\n",
			__func__, READ_VREG(AV_SCRATCH_J), READ_VREG(MDEC_PIC_DC_CTRL), READ_VREG(AV_SCRATCH_4),
			READ_VREG(POWER_CTL_VLD), READ_VREG(MC_CTRL1));
#if 0
	WRITE_VREG(MPEG1_2_REG, hw->reg_mpeg1_2_reg);
	WRITE_VREG(PIC_HEAD_INFO, hw->reg_pic_head_info);
	WRITE_VREG(F_CODE_REG, hw->reg_f_code_reg);
	WRITE_VREG(SLICE_VER_POS_PIC_TYPE, hw->reg_slice_ver_pos_pic_type);
	WRITE_VREG(VCOP_CTRL_REG, hw->reg_vcop_ctrl_reg);
	WRITE_VREG(MB_INFO, hw->reg_mb_info);

	WRITE_VREG(IQIDCT_CONTROL, hw->reg_iqidct_control);
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s 2 LAST_MVX 0x%x, PIC_HEAD_INFO 0x%x, DCAC_DMA_CTRL 0x%x, DCAC_MB_COUNT 0x%x, VCOP_CTRL_REG 0x%x MB_INFO 0x%x, IQIDCT_CONTROL 0x%x\n",
			__func__, READ_VREG(LAST_MVX), READ_VREG(PIC_HEAD_INFO), READ_VREG(F_CODE_REG),
			READ_VREG(SLICE_VER_POS_PIC_TYPE), READ_VREG(VCOP_CTRL_REG), READ_VREG(MB_INFO), READ_VREG(IQIDCT_CONTROL));

	WRITE_VREG(AV_SCRATCH_0, hw->reg_scratch_0);
	WRITE_VREG(AV_SCRATCH_1, hw->reg_scratch_1);
	WRITE_VREG(AV_SCRATCH_2, hw->reg_scratch_2);
	WRITE_VREG(AV_SCRATCH_3, hw->reg_scratch_3);
	WRITE_VREG(AV_SCRATCH_5, hw->reg_scratch_5);
	WRITE_VREG(AV_SCRATCH_6, hw->reg_scratch_6);
	WRITE_VREG(AV_SCRATCH_7, hw->reg_scratch_7);
	WRITE_VREG(AV_SCRATCH_8, hw->reg_scratch_8);
	WRITE_VREG(AV_SCRATCH_9, hw->reg_scratch_9);
	WRITE_VREG(AV_SCRATCH_A, hw->reg_scratch_A);
	WRITE_VREG(AV_SCRATCH_C, hw->reg_scratch_C);
	//WRITE_VREG(AV_SCRATCH_D, hw->reg_scratch_D);
	WRITE_VREG(AV_SCRATCH_E, hw->reg_scratch_E);
	WRITE_VREG(AV_SCRATCH_F, hw->reg_scratch_F);
	WRITE_VREG(AV_SCRATCH_G, hw->reg_scratch_G);
	WRITE_VREG(AV_SCRATCH_H, hw->reg_scratch_H);
	WRITE_VREG(AV_SCRATCH_I, hw->reg_scratch_I);

	WRITE_VREG(MB_WIDTH, hw->reg_mb_width);
	//WRITE_VREG(VIFF_BIT_CNT, hw->reg_viff_bit_cnt);

	WRITE_VREG(REC_CANVAS_ADDR, hw->reg_canvas_addr);
	WRITE_VREG(DBKR_CANVAS_ADDR, hw->reg_dbkr_canvas_addr);
	WRITE_VREG(DBKW_CANVAS_ADDR, hw->reg_dbkw_canvas_addr);
	WRITE_VREG(ANC2_CANVAS_ADDR, hw->reg_anc2_canvas_addr);
	WRITE_VREG(ANC0_CANVAS_ADDR, hw->reg_anc0_canvas_addr);
	WRITE_VREG(ANC1_CANVAS_ADDR, hw->reg_anc1_canvas_addr);
	WRITE_VREG(ANC3_CANVAS_ADDR, hw->reg_anc3_canvas_addr);
	WRITE_VREG(ANC4_CANVAS_ADDR, hw->reg_anc4_canvas_addr);
	WRITE_VREG(ANC5_CANVAS_ADDR, hw->reg_anc5_canvas_addr);
	if (hw->refs[1] == -1)
		WRITE_VREG(ANC0_CANVAS_ADDR, 0xffffffff);
	else
		WRITE_VREG(ANC0_CANVAS_ADDR, hw->canvas_spec[hw->refs[1]]);

	if (hw->refs[0] == -1) {
		if (hw->refs[1] == -1)
			WRITE_VREG(ANC1_CANVAS_ADDR, 0xffffffff);
		else
			WRITE_VREG(ANC1_CANVAS_ADDR, hw->canvas_spec[hw->refs[1]]);
	}
	else
		WRITE_VREG(ANC1_CANVAS_ADDR, hw->canvas_spec[hw->refs[0]]);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
		"%s ANC0_CANVAS_ADDR = 0x%x, ANC1_CANVAS_ADDR = 0x%x, ANC2_CANVAS_ADDR = 0x%x\n",
		__func__, READ_VREG(ANC0_CANVAS_ADDR), READ_VREG(ANC1_CANVAS_ADDR), READ_VREG(ANC2_CANVAS_ADDR));

	WRITE_VREG(MCRCC_CTL1, 0xff1);

	WRITE_VREG(SLICE_VER_POS_PIC_TYPE, hw->slice_ver_pos_pic_type);
	//WRITE_VREG(VC1_CONTROL_REG, hw->reg_vc1_control);
	WRITE_VREG(VLD_C38, hw->avs_co_mb_wr_addr);
	WRITE_VREG(SLICE_START_BYTE_01, hw->slice_start_byte_01);
	WRITE_VREG(SLICE_START_BYTE_23, hw->slice_start_byte_23);
	WRITE_VREG(VCOP_CTRL_REG, hw->vcop_ctrl_reg);
	WRITE_VREG(IQIDCT_CONTROL, hw->iqidct_control);
	WRITE_VREG(RV_AI_MB_COUNT, hw->rv_ai_mb_count);
	WRITE_VREG(SLICE_QP, hw->slice_qp);

	WRITE_VREG(DC_SCALER, hw->dc_scaler);
	//WRITE_VREG(AVSP_IQ_WQ_PARAM_01, hw->avsp_iq_wq_param_01);
	//WRITE_VREG(AVSP_IQ_WQ_PARAM_23, hw->avsp_iq_wq_param_23);
	//WRITE_VREG(AVSP_IQ_WQ_PARAM_45, hw->avsp_iq_wq_param_45);
	WRITE_VREG(VLD_C39, hw->avs_co_mb_rd_addr);
	WRITE_VREG(DBLK_MB_WID_HEIGHT, hw->dblk_mb_wid_height);
	WRITE_VREG(MC_PIC_W_H, hw->mc_pic_w_h);
	WRITE_VREG(VLD_C3D, hw->avs_co_mb_rw_ctl);

	WRITE_VREG(VLD_DECODE_CONTROL, hw->vld_decode_control);
#endif
}

static void reset_process_time(struct vdec_vc1_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}

static void start_process_time_set(struct vdec_vc1_hw_s *hw)
{
	if ((hw->refs[1] != -1) && (hw->refs[0] == -1))
		hw->decode_timeout_count = 1;
	else
		hw->decode_timeout_count = 10;
	hw->start_process_time = jiffies;
}

void vc1_buf_ref_process_for_exception(struct vdec_vc1_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *aml_buf;
	s32 index = hw->decoding_index;

	if (index < 0) {
		vc1_print(DECODE_ID(hw), 0, "[ERR]cur_idx is invalid!\n");
		return;
	}

	aml_buf = (struct aml_buf *)hw->pics[index].v4l_ref_buf_addr;
	if (aml_buf == NULL) {
		vc1_print(DECODE_ID(hw), 0, "[ERR]fb is NULL!\n");
		return;
	}

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
		"process_for_exception: dma addr(0x%lx) buf_ref %d vfbuf_use %d ref_use %d\n",
		hw->pics[index].cma_alloc_addr,
		atomic_read(&aml_buf->entry.ref),
		hw->vfbuf_use[index],
		hw->ref_use[index]/*,
		hw->buf_use[index]*/);

	aml_buf_put_ref(&ctx->bm, aml_buf);
	aml_buf_put_ref(&ctx->bm, aml_buf);
	if ((ctx->vpp_is_need || ctx->enable_di_post) && hw->interlace_flag) {
		aml_buf_put_ref(&ctx->bm, aml_buf);
	}

	if (ctx->enable_di_post && hw->interlace_flag)
		aml_buf_put_free_dmabuf(&ctx->bm, hw->pics[index].cma_alloc_addr, 0, true);

	hw->vfbuf_use[index] = 0;
	hw->ref_use[index] = 0;
	hw->pics[index].v4l_ref_buf_addr = 0;
	hw->pics[index].cma_alloc_addr = 0;
	hw->decoding_index = INVALID_IDX;
}

static void timeout_process(struct vdec_vc1_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (hw->process_busy) {
		pr_info("%s, process_busy\n", __func__);
		return;
	}

	if (work_pending(&hw->work) ||
	    work_busy(&hw->work) ||
	    work_busy(&hw->timeout_work) ||
	    work_pending(&hw->timeout_work)) {
		pr_err("%s vc1[%d] timeout_process return before do anything.\n",__func__, vdec->id);
		return;
	}

	hw->timeout = true;
	reset_process_time(hw);

	vc1_print(DECODE_ID(hw), 0,
		"%s decoder timeout, pc=%d, status = %d,level=%d\n",
		__func__,
		READ_VREG(MPC_E),
		READ_VREG(VC1_BUFFEROUT),
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));

	amvdec_stop();

	hw->dec_result = DEC_RESULT_DONE;
	/*
	 * In this very timeout point,the vmpeg12_work arrives,
	 * let it to handle the scenario.
	 */
	if (work_pending(&hw->work)) {
		pr_err("%s vc1[%d] return before schedule.", __func__, vdec->id);
		return;
	}
	vdec_schedule_work(&hw->timeout_work);
}

static void check_timer_func(struct timer_list *timer)
{
	struct vdec_vc1_hw_s *hw = container_of(timer, struct vdec_vc1_hw_s, check_timer);
	unsigned int timeout_val = decode_timeout_val;

	if (/*((debug_enable & PRINT_FLAG_TIMEOUT_STATUS) == 0) &&*/
		(timeout_val > 0) &&
		(hw->start_process_time > 0) &&
		((1000 * (jiffies - hw->start_process_time) / HZ)
				> timeout_val)) {
		if (hw->last_vld_level == READ_VREG(VLD_MEM_VIFIFO_LEVEL)) {
			if (hw->decode_timeout_count > 0)
				hw->decode_timeout_count--;
			if (hw->decode_timeout_count == 0)
				timeout_process(hw);
		}
		hw->last_vld_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static void vvc1_work_implement(struct vdec_vc1_hw_s *hw, int from)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
		"%s: result %d, status %d\n", __func__,	hw->dec_result, vdec->next_status);

	if (hw->dec_result == DEC_RESULT_DONE) {

		if (hw->timeout) {
			vc1_buf_ref_process_for_exception(hw);
			if (vdec_frame_based(vdec))
				vdec_v4l_post_error_frame_event(ctx);
			hw->timeout = false;
		}
		vdec_vframe_dirty(vdec, hw->chunk);
		hw->chunk = NULL;
	} else if (hw->dec_result == DEC_RESULT_AGAIN &&
		(vdec->next_status != VDEC_STATUS_DISCONNECTED)) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}

		if (input_stream_based(vdec)) {
			vdec_set_input_underrun(vdec, true);
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s: set input underrun status to true\n", __func__);
		}

		//hw->dec_again_cnt++;
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		vc1_print(DECODE_ID(hw), 0, "%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		hw->stat |= STAT_EOS;
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		hw->eos = 1;
		vdec_vframe_dirty(vdec, hw->chunk);
		hw->chunk = NULL;
		vdec_clean_input(vdec);
		flush_output(hw);
		notify_v4l_eos(vdec);

		vc1_print(DECODE_ID(hw), 0, "%s: end of stream\n", __func__);
	}

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	/*disable mbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 0);
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	if (from == 1) {
		/*This is a timeout work*/
		if (work_pending(&hw->work)) {
			pr_err("timeout work return before finishing.");
			/*
			 * The vmpeg12_work arrives at the last second,
			 * give it a chance to handle the scenario.
			 */
			return;
		}
	}

	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (ctx->param_sets_from_ucode &&
		!hw->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	if (from == 1)
		hw->timeout_processing = 0;

	if (hw->vdec_cb)
		hw->vdec_cb(vdec, hw->vdec_cb_arg, CORE_MASK_VDEC_1);
}


static void vvc1_work(struct work_struct *work)
{
	struct vdec_vc1_hw_s *hw = container_of(work, struct vdec_vc1_hw_s, work);

	vvc1_work_implement(hw, 0);
}

static void vvc1_timeout_work(struct work_struct *work)
{
	struct vdec_vc1_hw_s *hw = container_of(work, struct vdec_vc1_hw_s, timeout_work);

	if (work_pending(&hw->work)) {
		pr_err("timeout work return before executing.");
		return;
	}

	hw->timeout_processing = 1;
	vvc1_work_implement(hw, 1);
}

static void set_aspect_ratio(struct vdec_vc1_hw_s *hw, struct vframe_s *vf, unsigned int pixel_ratio)
{
	int ar = 0;
	u32 vvc1_ratio = hw->vvc1_ratio;

	if (vvc1_ratio == 0) {
		/* always stretch to 16:9 */
		vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT);
	} else if (pixel_ratio > 0x0f) {
		ar = (hw->vvc1_amstream_dec_info.height * (pixel_ratio & 0xff) *
			  vvc1_ratio) / (hw->vvc1_amstream_dec_info.width *
							 (pixel_ratio >> 8));
	} else {
		switch (pixel_ratio) {
		case 0:
			ar = (hw->vvc1_amstream_dec_info.height * vvc1_ratio) /
				 hw->vvc1_amstream_dec_info.width;
			break;
		case 1:
			vf->sar_width = 1;
			vf->sar_height = 1;
			ar = (vf->height * vvc1_ratio) / vf->width;
			break;
		case 2:
			vf->sar_width = 12;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 12);
			break;
		case 3:
			vf->sar_width = 10;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 10);
			break;
		case 4:
			vf->sar_width = 16;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 16);
			break;
		case 5:
			vf->sar_width = 40;
			vf->sar_height = 33;
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 40);
			break;
		case 6:
			vf->sar_width = 24;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 24);
			break;
		case 7:
			vf->sar_width = 20;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 20);
			break;
		case 8:
			vf->sar_width = 32;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 32);
			break;
		case 9:
			vf->sar_width = 80;
			vf->sar_height = 33;
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 80);
			break;
		case 10:
			vf->sar_width = 18;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 18);
			break;
		case 11:
			vf->sar_width = 15;
			vf->sar_height = 11;
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 15);
			break;
		case 12:
			vf->sar_width = 64;
			vf->sar_height = 33;
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 64);
			break;
		case 13:
			vf->sar_width = 160;
			vf->sar_height = 99;
			ar = (vf->height * 99 * vvc1_ratio) /
				(vf->width * 160);
			break;
		default:
			vf->sar_width = 1;
			vf->sar_height = 1;
			ar = (vf->height * vvc1_ratio) / vf->width;
			break;
		}
	}

	ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
	/*vf->ratio_control |= DISP_RATIO_FORCECONFIG | DISP_RATIO_KEEPRATIO;*/
}

static int vc1_recycle_frame_buffer(struct vdec_vc1_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *aml_buf;
	ulong phy_addr;
	ulong flags;
	int i;

	for (i = 0; i < hw->vf_buf_num_used; ++i) {
		if ((hw->vfbuf_use[i]) &&
			!(hw->ref_use[i]) &&
			hw->pics[i].v4l_ref_buf_addr){
			aml_buf = (struct aml_buf *)hw->pics[i].v4l_ref_buf_addr;

			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s buf idx: %d dma addr: 0x%lx vfbuf_use %d ref_use %d\n",
				__func__, i, hw->pics[i].cma_alloc_addr,
				hw->vfbuf_use[i],
				hw->ref_use[i]);

			spin_lock_irqsave(&hw->lock, flags);
			/*
			 * There will no be multiple threads running in
			 * the same vdec_vc1_hw_s context.
			 */
			/* coverity[thread1_overwrites_value_in_field] */
			phy_addr = hw->pics[i].cma_alloc_addr;
			hw->pics[i].v4l_ref_buf_addr = 0;
			hw->pics[i].cma_alloc_addr = 0;
			while (hw->vfbuf_use[i]) {
				atomic_add(1, &hw->put_num);
				hw->vfbuf_use[i]--;
			}

			spin_unlock_irqrestore(&hw->lock, flags);

			if (ctx->enable_di_post  &&
				hw->interlace_flag) {
					aml_buf_put_free_dmabuf(&ctx->bm, phy_addr, 0, true);
					continue;
			}

			aml_buf_put_ref(&ctx->bm, aml_buf);

			break;
		}
	}

	return 0;
}

static bool is_available_buffer(struct vdec_vc1_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *sub_buf;
	int i, free_count = 0;
	int free_slot =0;

	/* Ignore the buffer available check until the head parse done. */
	if (!hw->v4l_params_parsed) {
		/*
		 * If a resolution change and eos are detected, decoding will
		 * wait until the first valid buffer queue in driver
		 * before scheduling continues.
		 */
		if (ctx->v4l_resolution_change) {
			if (hw->eos)
				return false;

			/* Wait for buffers ready. */
			if (!ctx->dst_queue_streaming)
				return false;
		} else {
			return true;
		}
	}

	if (aml_buf_dmabuf_slot_occupied(&ctx->bm))
		return false;

	vc1_recycle_frame_buffer(hw);

	/* Wait for the buffer number negotiation to complete. */
	for (i = 0; i < hw->vf_buf_num_used; ++i) {
		if ((hw->vfbuf_use[i] == 0) &&
			(hw->ref_use[i] == 0) &&
			!hw->pics[i].v4l_ref_buf_addr) {
			free_slot++;

			break;
		}
	}

	if (!free_slot) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_BUFMGR,
			"%s not enough free_slot %d vf_buf_num_used %d\n",
					__func__, free_slot, hw->vf_buf_num_used);
		for (i = 0; i < hw->vf_buf_num_used; ++i) {
			vc1_print(0, VC1_DEBUG_DETAIL,
				"%s idx %d ref_use %d vfbuf_use %d cma_alloc_addr = 0x%lx\n",
				__func__, i, hw->ref_use[i],
				hw->vfbuf_use[i],
				hw->pics[i].v4l_ref_buf_addr);
		}

		return false;
	}

	if (((hw->interlace_flag) &&
		atomic_read(&ctx->vpp_cache_num) > 1) ||
		atomic_read(&ctx->vpp_cache_num) >= MAX_VPP_BUFFER_CACHE_NUM ||
		atomic_read(&ctx->ge2d_cache_num) > 1) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_BUFMGR,
			"%s vpp or ge2d cache: %d/%d full!\n",
		__func__, atomic_read(&ctx->vpp_cache_num), atomic_read(&ctx->ge2d_cache_num));

		return false;
	}

	if (!hw->aml_buf && !aml_buf_empty(&ctx->bm)) {
		hw->aml_buf = aml_buf_get(&ctx->bm, BUF_USER_DEC, false);
		if (!hw->aml_buf) {
			return false;
		}
		hw->aml_buf->task->attach(hw->aml_buf->task, &task_dec_ops,  hw_to_vdec(hw));
		hw->aml_buf->state = FB_ST_DECODER;
		if (hw->aml_buf->sub_buf[0]) {
			sub_buf = (struct aml_buf *)hw->aml_buf->sub_buf[0];
			sub_buf->task->attach(sub_buf->task, &task_dec_ops,  hw_to_vdec(hw));
			sub_buf->state = FB_ST_DECODER;
		}
		if (hw->aml_buf->sub_buf[1]) {
			sub_buf = (struct aml_buf *)hw->aml_buf->sub_buf[1];
			sub_buf->task->attach(sub_buf->task, &task_dec_ops,  hw_to_vdec(hw));
			sub_buf->state = FB_ST_DECODER;
		}
	}

	if (hw->aml_buf) {
		free_count++;
		free_count += aml_buf_ready_num(&ctx->bm);
		vc1_print(DECODE_ID(hw), VC1_DEBUG_BUFMGR,
			"%s get fb: 0x%lx fb idx: %d\n",
			__func__, hw->aml_buf, hw->aml_buf->index);
	}

	return free_count >= run_ready_min_buf_num ? 1 : 0;
}

static void flush_output(struct vdec_vc1_hw_s * hw)
{
	struct pic_info_t *pic;

	if (hw->refs[1] >= 0 &&
		hw->refs[1] < DECODE_BUFFER_NUM_MAX &&
		hw->vfbuf_use[hw->refs[1]] > 0) {
		pic = &hw->pics[hw->refs[1]];
		prepare_display_buf(hw, pic);
	}
}

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct aml_buf *aml_buf = NULL;
	int index = INVALID_IDX;
	ulong expires;

	expires = jiffies + msecs_to_jiffies(2000);

	while (!is_available_buffer(hw)) {
		if (ctx->is_stream_off) {
			pr_info("The capture is streamoff, no longer need eos.\n");
			return 0;
		}
		if (time_after(jiffies, expires)) {
			pr_info("[%d] VC1 isn't enough capture buff for notify eos.\n", ctx->id);
			return -1;
		}
	}

	index = find_free_buffer(hw);
	if (INVALID_IDX == index) {
		aml_buf_put_ref(&ctx->bm, hw->aml_buf);
		hw->aml_buf = 0;
		pr_info("[%d] VC1 EOS get free solt buff fail.\n", ctx->id);
		return -1;
	}

	aml_buf = (struct aml_buf *)
		hw->pics[index].v4l_ref_buf_addr;

	vf->type		|= VIDTYPE_V4L_EOS;
	vf->timestamp		= ULLONG_MAX;
	vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
	vf->v4l_mem_handle	= (ulong)aml_buf;

	//vdec_vframe_ready(vdec, vf);
	kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

	aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);

	hw->eos = true;

	vc1_print(DECODE_ID(hw), 0, "[%d] VC1 EOS notify.\n", ctx->id);

	return 0;
}

static int vvc1_get_ps_info(struct vdec_vc1_hw_s *hw, struct aml_vdec_ps_infos *ps)
{
	ps->visible_width 	= hw->frame_width;
	ps->visible_height 	= hw->frame_height;
	ps->coded_width 	= vdec_width_align_force(hw->frame_width, hw->canvas_mode);
	ps->coded_height 	= ALIGN(hw->frame_height, 64);
	ps->dpb_size 		= hw->vf_buf_num_used;
	ps->dpb_margin		= hw->dynamic_buf_num_margin;
	ps->dpb_frames		= DECODE_BUFFER_NUM_MAX;
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"%s margin %d/%d, dpb_frames %d, dpb_size %d\n",
		__func__, hw->dynamic_buf_num_margin, ps->dpb_margin, ps->dpb_frames, ps->dpb_size);

	ps->field		= hw->interlace_flag ?
		V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

	return 0;
}

static int v4l_res_change(struct vdec_vc1_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int ret = 0;

	if (ctx->param_sets_from_ucode &&
		hw->res_ch_flag == 0) {
		struct aml_vdec_ps_infos ps;

		if ((hw->last_width != 0 &&
			hw->last_height != 0) &&
			(hw->frame_width != hw->last_width ||
			hw->frame_height != hw->last_height)) {

			vc1_print(DECODE_ID(hw), 0, "%s (%d,%d)=>(%d,%d)\r\n", __func__,
				hw->last_width, hw->last_height, hw->frame_width, hw->frame_height);

			vvc1_get_ps_info(hw, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			ctx->v4l_resolution_change = 1;
			vdec_v4l_res_ch_event(ctx);
			ctx->decoder_status_info.frame_height = ps.visible_height;
			ctx->decoder_status_info.frame_width = ps.visible_width;
			hw->report_width = hw->frame_width;
			hw->report_height = hw->frame_height;

			hw->v4l_params_parsed = false;
			hw->res_ch_flag = 1;
			ctx->v4l_reqbuff_flag = false;
			flush_output(hw);
			notify_v4l_eos(vdec);
			ctx->vdec_configure_update(ctx);
			ret = 1;
		}
	}

	return ret;
}

static int vvc1_config_ref_buf(struct vdec_vc1_hw_s *hw)
{
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"%s: new_type %d\n",
		__func__, hw->new_type);

	if ((hw->new_type == I_PICTURE) ||
		(hw->new_type == P_PICTURE)) {
		if (hw->refs[1] == -1) {
			WRITE_VREG(ANC0_CANVAS_REG, 0xffffffff);
		} else {
			WRITE_VREG(ANC0_CANVAS_REG, hw->canvas_spec[hw->refs[1]]);
		}

		if (hw->refs[0] == -1) {
			WRITE_VREG(ANC1_CANVAS_REG, 0xffffffff);
		} else {
			WRITE_VREG(ANC1_CANVAS_REG, hw->canvas_spec[hw->refs[0]]);
		}
	} else {
		if (hw->refs[0] == -1) {
			WRITE_VREG(ANC0_CANVAS_REG, 0xffffffff);
		} else {
			WRITE_VREG(ANC0_CANVAS_REG, hw->canvas_spec[hw->refs[0]]);
		}

		if (hw->refs[1] == -1) {
			WRITE_VREG(ANC1_CANVAS_REG, 0xffffffff);
		} else {
			WRITE_VREG(ANC1_CANVAS_REG, hw->canvas_spec[hw->refs[1]]);
		}
	}

	return 0;
}

static int update_reference(struct vdec_vc1_hw_s *hw,	int index)
{
	hw->ref_use[index]++;
	if (hw->refs[1] == -1) {
		hw->refs[1] = index;
		/*
		* first pic need output to show
		* usecnt do not decrease.
		*/
	} else if (hw->refs[0] == -1) {
		hw->refs[0] = hw->refs[1];
		hw->refs[1] = index;
		/* second pic do not output */
		index = hw->vf_buf_num_used;
	} else {
		hw->ref_use[hw->refs[0]]--; 	//old ref0 unused
		hw->refs[0] = hw->refs[1];
		hw->refs[1] = index;
		index = hw->refs[0];
	}
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s: hw->refs[0] = %d, hw->refs[1] = %d index %d\n",
			__func__, hw->refs[0], hw->refs[1], index);
	return index;
}

static void vc1_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vvc1_vf_put(vf, vdec_ctx);
}

static void vc1_get_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	memcpy(vf, vvc1_vf_get(vdec_ctx), sizeof(struct vframe_s));
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= vc1_get_video_frame,
	.put_vframe	= vc1_put_video_frame,
};

static void config_canvas_hevc(struct vdec_vc1_hw_s *hw)
{
	uint data32 = 0, endian = 0;

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2); // reset mcrcc

	// program canvas0
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0 << 1) | 0);
	data32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
	data32 = data32 & 0xffff;
	data32 = data32 | (data32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL2, data32);

	// program canvas1
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (16 << 8) | (1 << 1) | 0);
	data32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
	data32 = data32 & 0xffff;
	data32 = data32 | (data32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL3, data32);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0); // enable mcrcc progressive-mode

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x3f);
	// [5:4] -- address_format 00:linear 01:32x32 10:64x32
	data32 |= (hw->canvas_mode << 4);
	if (hw->canvas_mode == CANVAS_BLKMODE_LINEAR)
		endian = 7;
	data32 |= (1 << 3) | endian;
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);

	WRITE_VREG(HEVCD_IPP_DYN_CACHE, 0x2b); // enable new mcrcc
}

static int v4l_alloc_buff_config_canvas(struct vdec_vc1_hw_s *hw, int i)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	unsigned canvas;

	dos_addr_t decbuf_start = 0, decbuf_uv_start = 0;
	int decbuf_y_size = 0, decbuf_uv_size = 0;
	u32 canvas_width = 0, canvas_height = 0;
	struct aml_buf *aml_buf = hw->aml_buf;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int endian = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	struct aml_buf *sub0_buf, *sub1_buf;
	int j;

	if (!aml_buf) {
		vc1_print(DECODE_ID(hw), 0, "%s not get aml_buf \n", __func__);
		return -1;
	}

	if (!hw->frame_width || !hw->frame_height) {
		struct vdec_pic_info pic = { 0 };
		vdec_v4l_get_pic_info(ctx, &pic);
		hw->frame_width = pic.visible_width;
		hw->frame_height = pic.visible_height;
		vc1_print(DECODE_ID(hw), 0, "[%d] set %d x %d from IF layer\n", ctx->id,
			hw->frame_width, hw->frame_height);
		if (hw->frame_width == 0 || hw->frame_height == 0)
			return -1;
	}

	hw->pics[i].v4l_ref_buf_addr = (ulong)aml_buf;
	hw->pics[i].cma_alloc_addr = aml_buf->planes[0].addr;
	if (aml_buf->num_planes == 1) {
		decbuf_start	= aml_buf->planes[0].addr;
		decbuf_y_size	= aml_buf->planes[0].offset;
		decbuf_uv_start	= decbuf_start + decbuf_y_size;
		decbuf_uv_size	= decbuf_y_size / 2;
		canvas_width	= vdec_width_align_force(hw->frame_width, hw->canvas_mode);
		canvas_height	= ALIGN(hw->frame_height, 64);
		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
	} else if (aml_buf->num_planes == 2) {
		decbuf_start	= aml_buf->planes[0].addr;
		decbuf_y_size	= aml_buf->planes[0].length;
		decbuf_uv_start	= aml_buf->planes[1].addr;
		decbuf_uv_size	= aml_buf->planes[1].length;
		canvas_width	= vdec_width_align_force(hw->frame_width, hw->canvas_mode);
		canvas_height	= ALIGN(hw->frame_height, 64);
		aml_buf->planes[0].bytes_used = decbuf_y_size;
		aml_buf->planes[1].bytes_used = decbuf_uv_size;
	}

	for (j = 0; j < aml_buf->num_planes; j++) {
		if (aml_buf->sub_buf[0]) {
			sub0_buf = (struct aml_buf *)aml_buf->sub_buf[0];
			sub0_buf->planes[j].bytes_used = aml_buf->planes[j].bytes_used;
		}

		if (aml_buf->sub_buf[1]) {
			sub1_buf = (struct aml_buf *)aml_buf->sub_buf[1];
			sub1_buf->planes[j].bytes_used = aml_buf->planes[j].bytes_used;
		}
	}

	if (is_vdec_hevc_combine()) {
		// config fix stride
		WRITE_VREG(HEVCD_MCR_FIXSIZE_CFG, ((1 << 15) | canvas_width));
	}

	if (vdec->parallel_dec == 1) {
		unsigned tmp;
		if (canvas_y(hw->canvas_spec[i]) == 0xff) {
			tmp = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
			hw->canvas_spec[i] &= ~0xff;
			hw->canvas_spec[i] |= tmp;
		}
		if (canvas_u(hw->canvas_spec[i]) == 0xff) {
			tmp = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
			hw->canvas_spec[i] &= ~(0xffff << 8);
			hw->canvas_spec[i] |= tmp << 8;
			hw->canvas_spec[i] |= tmp << 16;
		}
		canvas = hw->canvas_spec[i];
	} else {
		canvas = vdec->get_canvas(i, 2);
		hw->canvas_spec[i] = canvas;
	}

	hw->vc1_canvas_config[i][0].width = canvas_width;
	hw->vc1_canvas_config[i][0].height = canvas_height;
	hw->vc1_canvas_config[i][0].phy_addr = decbuf_start;
	hw->vc1_canvas_config[i][0].block_mode = hw->canvas_mode;
	hw->vc1_canvas_config[i][0].endian = endian;

	config_cav_lut(canvas_y(canvas), &hw->vc1_canvas_config[i][0], VDEC_1);

	hw->vc1_canvas_config[i][1].width = canvas_width;
	hw->vc1_canvas_config[i][1].height = canvas_height >> 1;
	hw->vc1_canvas_config[i][1].phy_addr = decbuf_uv_start;
	hw->vc1_canvas_config[i][1].block_mode = hw->canvas_mode;
	hw->vc1_canvas_config[i][1].endian = endian;

	config_cav_lut(canvas_u(canvas), &hw->vc1_canvas_config[i][1], VDEC_1);

	if (is_vdec_hevc_combine()) {
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, ((2 * i) << 8) | (1 << 1));
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, decbuf_start >> 5);

		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, ((2 * i + 1) << 8) | (1 << 1));
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, decbuf_uv_start >> 5);

		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (i << 8) | 1);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, ((2 * i + 1) << 8) | (2 * i));

		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);
		config_canvas_hevc(hw);
	}

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
		"[%d] %s y: %x uv: %x w: %d h: %d canvas_mode 0x%x endian %d \n",
		ctx->id, __func__,
		decbuf_start, decbuf_uv_start,
		canvas_width, canvas_height,
		hw->vc1_canvas_config[i][0].block_mode,
		hw->vc1_canvas_config[i][0].endian);

	if (aml_buf_is_dynamic_mode_inited(&ctx->bm))
		aml_buf_get_dmabuf_ref(&ctx->bm, hw->pics[i].cma_alloc_addr, true);

	aml_buf_get_ref(&ctx->bm, aml_buf);
	if ((ctx->vpp_is_need || ctx->enable_di_post) &&
		hw->interlace_flag) {
		aml_buf_get_ref(&ctx->bm, aml_buf);
	}

	hw->aml_buf = NULL;

	return 0;
}
static inline ulong vc1_reset_lock(struct vdec_vc1_hw_s *hw)
{
	ulong flags;

	spin_lock_irqsave(&hw->reset_lock, flags);

	return flags;
}

static inline void vc1_reset_unlock(struct vdec_vc1_hw_s *hw, ulong flags)
{
	spin_unlock_irqrestore(&hw->reset_lock, flags);
}

static void reset(struct vdec_s *vdec)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	int i;

	vc1_print(DECODE_ID(hw), 0, "vc1_v4l: reset in\n");

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->timeout_work);
	reset_process_time(hw);

	for (i = 0; i < hw->vf_buf_num_used; i++) {
		hw->pics[i].v4l_ref_buf_addr = 0;
		hw->pics[i].cma_alloc_addr = 0;
		hw->vfbuf_use[i] = 0;
		hw->ref_use[i] = 0;
	}

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];

		memset((void *)vf, 0, sizeof(*vf));
		hw->vfpool[i].index = -1;
		kfifo_put(&hw->newframe_q, vf);
	}

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		vdec->free_canvas_ex(canvas_y(hw->canvas_spec[i]), vdec->id);
		vdec->free_canvas_ex(canvas_u(hw->canvas_spec[i]), vdec->id);
		hw->canvas_spec[i] = 0xffffff;
	}

	hw->refs[0] 	= -1;
	hw->refs[1] 	= -1;
	hw->throw_pb_flag	= 1;
	hw->frame_width 	= 0;
	hw->frame_height	= 0;

	hw->eos 		= 0;
	hw->aml_buf 	= NULL;
	hw->dec_result = DEC_RESULT_NONE;

	atomic_set(&hw->get_num, 0);
	atomic_set(&hw->put_num, 0);

	//WRITE_VREG(VC1_BUFFEROUT, NEW_DRV_VER); //reuse the register VC1_BUFFEROUT to support new ucode version

	pr_info("vc1_v4l: reset.\n");
}

static int find_free_buffer(struct vdec_vc1_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->vf_buf_num_used; i++) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_BUFMGR,
			"%s: i %d, vfbuf_use %d, ref_use %d, addr 0x%x\n",
			__func__, i, hw->vfbuf_use[i], hw->ref_use[i], hw->pics[i].v4l_ref_buf_addr);
		if ((hw->vfbuf_use[i] == 0) &&
			(hw->ref_use[i] == 0) &&
			!hw->pics[i].v4l_ref_buf_addr) {
			break;
		}
	}

	if ((i == hw->vf_buf_num_used) &&
		(hw->vf_buf_num_used != 0)) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: buf INVALID_IDX\n", __func__);
		return INVALID_IDX;
	}

	if (v4l_alloc_buff_config_canvas(hw, i))
		return INVALID_IDX;

	return i;
}

static void v4l_vc1_collect_stream_info(struct vdec_s *vdec,
	struct vdec_vc1_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;
	struct dec_stream_info_s *str_info = NULL;

	if (ctx == NULL) {
		pr_info("param invalid\n");
		return;
	}
	str_info = &ctx->dec_intf.dec_stream;

	snprintf(str_info->vdec_name, sizeof(str_info->vdec_name),
		"%s", DRIVER_NAME);

	str_info->vdec_type = input_frame_based(vdec);
	str_info->dual_core_flag = vdec_dual(vdec);
	str_info->is_secure = vdec_secure(vdec);
	str_info->profile_idc = 3;//Advanced profile
	str_info->level_idc = 3;//1920x1080
	str_info->filed_flag = hw->interlace_flag;
	str_info->frame_height = hw->frame_height;
	str_info->frame_width = hw->frame_width;
	str_info->crop_top = 0;
	str_info->crop_bottom = 0;
	str_info->crop_left= 0;
	str_info->crop_right = 0;
	str_info->double_write_mode = 0;
	str_info->error_handle_policy = 0;
	str_info->bit_depth = 8;
	str_info->dpb_num = DECODE_BUFFER_NUM_MAX;
	str_info->margin_num = hw->dynamic_buf_num_margin;

	str_info->ratio_size.dar_width = -1;
	str_info->ratio_size.dar_height = -1;
	str_info->ratio_size.sar_width = -1;
	str_info->ratio_size.sar_height = -1;
	str_info->trick_mode = 0;
	str_info->frame_dur = hw->last_dur;
	if (str_info->frame_dur != 0)
		str_info->frame_rate = ((96000 * 10 / str_info->frame_dur) % 10) < 5 ?
				96000 / str_info->frame_dur : (96000 / str_info->frame_dur +1);
	else
		str_info->frame_rate = -1;
	ctx->dec_intf.decinfo_event_report(ctx, AML_DECINFO_EVENT_STREAM, NULL);
}

static void set_frame_info(struct vdec_vc1_hw_s *hw, struct vframe_s *vf)
{
	u32 endian_tmp;
	u32 buffer_index = vf->index;
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int vf_dur = ctx->duration;

	vf->duration = vf_dur ? vf_dur : hw->frame_dur;

	if (hw->last_dur != hw->frame_dur) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"decoder duration change old: %d new: %d\n", hw->last_dur, hw->frame_dur);
		hw->last_dur = hw->frame_dur;
		v4l_vc1_collect_stream_info(hw_to_vdec(hw), hw);
	}

	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 2;

	vf->canvas0_config[0] = hw->vc1_canvas_config[buffer_index][0];
	vf->canvas0_config[1] = hw->vc1_canvas_config[buffer_index][1];
	vf->canvas1_config[0] = hw->vc1_canvas_config[buffer_index][0];
	vf->canvas1_config[1] = hw->vc1_canvas_config[buffer_index][1];

	if (is_cpu_t7()) {
		endian_tmp = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	} else {
		endian_tmp = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 0 : 7;
	}

	vf->canvas0_config[0].endian = endian_tmp;
	vf->canvas0_config[1].endian = endian_tmp;
	vf->canvas1_config[0].endian = endian_tmp;
	vf->canvas1_config[1].endian = endian_tmp;

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: canvas_mode %d, endian %d/%d\n",
	    __func__, hw->canvas_mode, hw->vc1_canvas_config[buffer_index][0].endian, endian_tmp);
}

static int prepare_display_buf(struct vdec_vc1_hw_s *hw, struct pic_info_t *pic)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct vframe_s *vf = NULL;
	u32 picture_type = pic->picture_type;
	u32 buffer_index = pic->index;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *aml_buf = NULL;
	struct aml_buf *sub0_buf = NULL;
	struct aml_buf *sub1_buf = NULL;
	ulong nv_order = VIDTYPE_VIU_NV21;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;

	if (!hw->pics[buffer_index].v4l_ref_buf_addr) {
		vc1_print(DECODE_ID(hw), 0, "%s do not get aml_buf! \n", __func__);
		return -1;
	}

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s: index %d, picture_type %d, interlace_flag %d, pts %d/%lld/%llu\n",
				__func__, buffer_index, picture_type, hw->interlace_flag,
				pic->pts, pic->pts64, pic->timestamp);

	aml_buf = (struct aml_buf *)pic->v4l_ref_buf_addr;
	sub0_buf = (struct aml_buf *)aml_buf->sub_buf[0];
	sub1_buf = (struct aml_buf *)aml_buf->sub_buf[1];

	if (ctx->enable_di_post && hw->interlace_flag) {
		if (!aml_buf_check_uvm_dma_recycled(&ctx->bm, pic->cma_alloc_addr, sub1_buf->entry.key))
			aml_buf_put_free_dmabuf(&ctx->bm, pic->cma_alloc_addr, sub1_buf->entry.key, false);
		aml_buf_set_unbind_dmabuf(&ctx->bm, sub1_buf);
	}

	if (hw->interlace_flag &&
		(ctx->vpp_is_need || ctx->enable_di_post)) { /* interlace */
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: interlace\n", __func__);
		hw->throw_pb_flag = 0;
		if (kfifo_get(&hw->newframe_q, &vf) == 0) {
			pr_info
			("fatal error, no available buffer slot.\n");
			return IRQ_HANDLED;
		}
		vf->signal_type = 0;
		vf->index = buffer_index;
		vf->width = hw->vvc1_amstream_dec_info.width;
		vf->height = hw->vvc1_amstream_dec_info.height;
		vf->bufWidth = 1920;
		vf->flag = 0;
#if 0
		if (pts_valid) {
			vf->pts = pts;
			vf->pts_us64 = pts_us64;
			if ((repeat_count > 1) && avi_flag) {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate *
					(repeat_count >> 1);
				hw->next_pts = pts +
					(hw->vvc1_amstream_dec_info.rate *
					 repeat_count >> 1) * 15 / 16;
				hw->next_pts_us64 = pts_us64 +
					((hw->vvc1_amstream_dec_info.rate *
					repeat_count >> 1) * 15 / 16) *
					100 / 9;
			} else {
				vf->duration =
				hw->vvc1_amstream_dec_info.rate >> 1;
				hw->next_pts = 0;
				hw->next_pts_us64 = 0;
				if (picture_type != I_PICTURE &&
					unstable_pts) {
					vf->pts = 0;
					vf->pts_us64 = 0;
				}
			}
		} else {
			vf->pts = hw->next_pts;
			vf->pts_us64 = hw->next_pts_us64;
			if ((repeat_count > 1) && avi_flag) {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate *
					(repeat_count >> 1);
				if (hw->next_pts != 0) {
					hw->next_pts += ((vf->duration) -
					((vf->duration) >> 4));
				}
				if (hw->next_pts_us64 != 0) {
					hw->next_pts_us64 +=
					div_u64((u64)((vf->duration) -
					((vf->duration) >> 4)) *
					100, 9);
				}
			} else {
				vf->duration =
				hw->vvc1_amstream_dec_info.rate >> 1;
				hw->next_pts = 0;
				hw->next_pts_us64 = 0;
				if (picture_type != I_PICTURE &&
					unstable_pts) {
					vf->pts = 0;
					vf->pts_us64 = 0;
				}
			}
		}
#endif
		vf->duration_pulldown = 0;
		vf->type = (pic->buffer_info & BOTTOM_FIELD_FIRST_FLAG) ?
					VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;

		vf->type |= nv_order;//VIDTYPE_VIU_NV21;
		//vf->canvas0Addr = vf->canvas1Addr = hw->canvas_spec[buffer_index];
		vf->orientation = 0;
		vf->type_original = vf->type;
		set_aspect_ratio(hw, vf, READ_VREG(VC1_PIC_RATIO));

		hw->vfbuf_use[buffer_index]++;

		vf->v4l_mem_handle = hw->pics[buffer_index].v4l_ref_buf_addr;

		if (hw->chunk) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
			vf->timestamp = pic->timestamp;
		}

		set_frame_info(hw, vf);

#if 0
		vf->canvas0Addr = vf->canvas1Addr = -1;
		vf->canvas0_config[0] = hw->vc1_canvas_config[buffer_index][0];
		vf->canvas0_config[1] = hw->vc1_canvas_config[buffer_index][1];
		vf->plane_num = 2;

#endif
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D && vdec->use_vfm_path &&
			vdec_stream_based(vdec)) {
			vf->type |= VIDTYPE_FORCE_SIGN_IP_JOINT;
		}
		decoder_do_frame_check(vdec, vf);
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s [%d]: display_q index %d, pts %d/%lld/%llu, type 0x%x, w %d, h %d\n",
			__func__, __LINE__, vf->index, vf->pts, vf->pts_us64, vf->timestamp,
			vf->type, vf->width, vf->height);

		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(MODULE_NAME, vf->pts);

		if (ctx->is_stream_off && !(ctx->enable_di_post  &&
				hw->interlace_flag)) {
			vvc1_vf_put(vvc1_vf_get(vdec), vdec);
		} else {
			if (ctx->enable_di_post)
				ctx->fbc_transcode_and_set_vf(ctx, aml_buf, vf);
			aml_buf_set_vframe(aml_buf, vf);
			aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);
		}

		if (kfifo_get(&hw->newframe_q, &vf) == 0) {
			pr_info
			("fatal error, no available buffer slot.\n");
			return IRQ_HANDLED;
		}
		vf->signal_type = 0;
		vf->index = buffer_index;
		vf->width = hw->vvc1_amstream_dec_info.width;
		vf->height = hw->vvc1_amstream_dec_info.height;
		vf->bufWidth = 1920;
		vf->flag = 0;
#if 0
		vf->pts = hw->next_pts;
		vf->pts_us64 = hw->next_pts_us64;
		if ((repeat_count > 1) && avi_flag) {
			vf->duration =
				hw->vvc1_amstream_dec_info.rate *
				(repeat_count >> 1);
			if (hw->next_pts != 0) {
				hw->next_pts +=
					((vf->duration) -
					 ((vf->duration) >> 4));
			}
			if (hw->next_pts_us64 != 0) {
				hw->next_pts_us64 += div_u64((u64)((vf->duration) -
				((vf->duration) >> 4)) * 100, 9);
			}
		} else {
			vf->duration =
				hw->vvc1_amstream_dec_info.rate >> 1;
			hw->next_pts = 0;
			hw->next_pts_us64 = 0;
			if (picture_type != I_PICTURE &&
				unstable_pts) {
				vf->pts = 0;
				vf->pts_us64 = 0;
			}
		}
#endif
		vf->duration_pulldown = 0;
		vf->type = (pic->buffer_info & BOTTOM_FIELD_FIRST_FLAG) ?
				VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;

		vf->type |= nv_order;//VIDTYPE_VIU_NV21;
		//vf->canvas0Addr = vf->canvas1Addr =	hw->canvas_spec[buffer_index];
		vf->orientation = 0;
		vf->type_original = vf->type;
		set_aspect_ratio(hw, vf, READ_VREG(VC1_PIC_RATIO));

		hw->vfbuf_use[buffer_index]++;
		vc1_print(DECODE_ID(hw), VC1_DEBUG_BUFMGR, "%s: index %d vfbuf_use %d, second_field_pts_mode %d\n",
					__func__, buffer_index, hw->vfbuf_use[buffer_index], v4l2_ctx->second_field_pts_mode);
		vf->v4l_mem_handle = hw->pics[buffer_index].v4l_ref_buf_addr;

		if (hw->chunk) {
			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->timestamp = pic->timestamp;
			if (v4l2_ctx->second_field_pts_mode) {
				vf->timestamp = 0;
			}
		}

		set_frame_info(hw, vf);
#if 0
		vf->canvas0Addr = vf->canvas1Addr = -1;
		vf->canvas0_config[0] = hw->vc1_canvas_config[buffer_index][0];
		vf->canvas0_config[1] = hw->vc1_canvas_config[buffer_index][1];
		vf->plane_num = 2;
#endif

		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D && vdec->use_vfm_path &&
			vdec_stream_based(vdec)) {
			vf->type |= VIDTYPE_FORCE_SIGN_IP_JOINT;
		}
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s [%d]: display_q index %d, pts %d/%lld/%llu, type 0x%x, w %d, h %d\n",
			__func__, __LINE__, vf->index, vf->pts, vf->pts_us64, vf->timestamp,
			vf->type, vf->width, vf->height);

		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(MODULE_NAME, vf->pts);
		if (ctx->is_stream_off && !(ctx->enable_di_post  &&
				hw->interlace_flag)) {
			vvc1_vf_put(vvc1_vf_get(vdec), vdec);
		} else {
			if (sub0_buf)
				aml_buf = sub0_buf;
			if (ctx->enable_di_post)
				ctx->fbc_transcode_and_set_vf(ctx, aml_buf, vf);
			aml_buf_set_vframe(aml_buf, vf);
			aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);
		}
	} else {	/* progressive */
		hw->throw_pb_flag = 0;
		if (kfifo_get(&hw->newframe_q, &vf) == 0) {
			pr_info
			("fatal error, no available buffer slot.\n");
			return IRQ_HANDLED;
		}
		vf->signal_type = 0;
		vf->index = buffer_index;
		vf->width = hw->vvc1_amstream_dec_info.width;
		vf->height = hw->vvc1_amstream_dec_info.height;
		vf->bufWidth = 1920;
		vf->flag = 0;
#if 0
		if (pts_valid) {
			vf->pts = pts;
			vf->pts_us64 = pts_us64;
			if ((repeat_count > 1) && avi_flag) {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate *
					repeat_count;
				hw->next_pts =
					pts +
					(hw->vvc1_amstream_dec_info.rate *
					 repeat_count) * 15 / 16;
				hw->next_pts_us64 = pts_us64 +
					((hw->vvc1_amstream_dec_info.rate *
					repeat_count) * 15 / 16) *
					100 / 9;
			} else {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate;
				hw->next_pts = 0;
				hw->next_pts_us64 = 0;
				if (picture_type != I_PICTURE &&
					unstable_pts) {
					vf->pts = 0;
					vf->pts_us64 = 0;
				}
			}
		} else {
			vf->pts = hw->next_pts;
			vf->pts_us64 = hw->next_pts_us64;
			if ((repeat_count > 1) && avi_flag) {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate *
					repeat_count;
				if (hw->next_pts != 0) {
					hw->next_pts += ((vf->duration) -
						((vf->duration) >> 4));
				}
				if (hw->next_pts_us64 != 0) {
					hw->next_pts_us64 +=
					div_u64((u64)((vf->duration) -
					((vf->duration) >> 4)) *
					100, 9);
				}
			} else {
				vf->duration =
					hw->vvc1_amstream_dec_info.rate;
				hw->next_pts = 0;
				hw->next_pts_us64 = 0;
				if (picture_type != I_PICTURE &&
					unstable_pts) {
					vf->pts = 0;
					vf->pts_us64 = 0;
				}
			}
		}
#endif
		vf->duration_pulldown = 0;
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
			nv_order;//VIDTYPE_VIU_NV21;

		//vf->canvas0Addr = vf->canvas1Addr =	hw->canvas_spec[buffer_index];
		vf->orientation = 0;
		vf->type_original = vf->type;
		set_aspect_ratio(hw, vf, READ_VREG(VC1_PIC_RATIO));

		hw->vfbuf_use[buffer_index]++;
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s:  progressive vfbuf_use[%d] %d\n",
			__func__, buffer_index, hw->vfbuf_use[buffer_index]);

		vf->v4l_mem_handle = hw->pics[buffer_index].v4l_ref_buf_addr;
		aml_buf = (struct aml_buf *)vf->v4l_mem_handle;

		if (hw->chunk) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
			vf->timestamp = pic->timestamp;
		}

		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"[%d] %s(), v4l mem handle: 0x%lx\n",
			((struct aml_vcodec_ctx *)(hw->v4l2_ctx))->id,
			__func__, vf->v4l_mem_handle);

		set_frame_info(hw, vf);
#if 0
		vf->canvas0Addr = vf->canvas1Addr = -1;
		vf->canvas0_config[0] = hw->vc1_canvas_config[buffer_index][0];
		vf->canvas0_config[1] = hw->vc1_canvas_config[buffer_index][1];
		vf->plane_num = 2;
#endif

		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D && vdec->use_vfm_path &&
			vdec_stream_based(vdec)) {
			vf->type |= VIDTYPE_FORCE_SIGN_IP_JOINT;
		}
		decoder_do_frame_check(vdec, vf);
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s [%d]: display_q index %d, pts %d/%lld/%llu, type 0x%x, w %d, h %d\n",
			__func__, __LINE__, vf->index, vf->pts, vf->pts_us64, vf->timestamp,
			vf->type, vf->width, vf->height);

		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(MODULE_NAME, vf->pts);

		if (ctx->is_stream_off) {
			vvc1_vf_put(vvc1_vf_get(vdec), vdec);
		} else {
			if (ctx->enable_di_post)
				ctx->fbc_transcode_and_set_vf(ctx, aml_buf, vf);
			aml_buf_set_vframe(aml_buf, vf);
			aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);
		}
	}

	return 0;
}

static int is_oversize(int w, int h)
{
	if (w < 64 || h < 64)
		return true;

	if (format_resolution_fatal_error(VFORMAT_VC1, w, h))
		return true;

	return false;
}
#ifdef T6D_PRINT_CRC
void t6d_crc_print(struct vdec_vc1_hw_s *hw)
{
	int i;

	pr_info("---------------->>Dump frame %d CRC:\n", hw->decode_pic_count);

	for (i = 0; i < 4; i++) {
		WRITE_VREG_BITS(VDEC_VLD_CRC_CTL, i, 0, 2);
		pr_info("VDEC_VLD_CRC[%d] %x\n", i, READ_VREG(VDEC_VLD_CRC));

		WRITE_VREG_BITS(VDEC_VLD_CRC_CTL, i, 2, 2);
		WRITE_VREG(VDEC_VLD_CRC_SEED, 0);
	}
	//-----------------
	for (i = 0; i < 12; i++) {
		WRITE_VREG_BITS(VDEC_IQIDCT_CRC_CTL, i, 1, 3);
		pr_info("VDEC_IQIDCT_CRC[%d] %x\n", i, READ_VREG(VDEC_IQIDCT_CRC));
	}
	for (i = 0; i < 4; i++) {
		WRITE_VREG_BITS(VDEC_IQIDCT_CRC_CTL, i, 4, 2);
		WRITE_VREG(VDEC_IQIDCT_CRC_SEED, 0);
	}
	//-----------------------

	SET_VREG_MASK(MC_OTHER_GCLK_CTRL, 1<<1);
	for (i = 0; i < 4; i++) {
		WRITE_VREG_BITS(VDEC_MC_CRC_CTL, i, 0, 2);
		pr_info("VDEC_MC_CRC[%d] %x\n", i, READ_VREG(VDEC_MC_CRC));

		WRITE_VREG_BITS(VDEC_MC_CRC_CTL, i, 2, 2);
		WRITE_VREG(VDEC_MC_CRC_SEED, 0);
	}
	CLEAR_VREG_MASK(MC_OTHER_GCLK_CTRL, 1<<1);

	for (i = 0; i < 4; i++) {
		WRITE_VREG_BITS(VDEC_DBLK_CRC_CTL, i, 0, 2);
		pr_info("VDEC_DBLK_CRC[%d] %x\n", i, READ_VREG(VDEC_DBLK_CRC));
		WRITE_VREG_BITS(VDEC_DBLK_CRC_CTL, i, 2, 2);
		WRITE_VREG(VDEC_DBLK_CRC_SEED, 0);
	}

	for (i = 0; i < 2; i++) {
		WRITE_VREG_BITS(VDEC_PICDC_EXTIF_CRC_CTL, i, 0, 1);
		pr_info("VDEC_PICDC_EXTIF_CRC[%d] %x\n", i, READ_VREG(VDEC_PICDC_EXTIF_CRC));
		WRITE_VREG_BITS(VDEC_PICDC_EXTIF_CRC_CTL, i, 1, 1);
		WRITE_VREG(VDEC_PICDC_EXTIF_CRC_SEED, 0);
	}

	for (i = 0; i < 4; i++) {
		WRITE_VREG_BITS(VDEC_PSC_IQITCAV_CRC_CTL, i, 0, 2);
		pr_info("VDEC_PSC_IQITCAV_CRC[%d] %x\n", i, READ_VREG(VDEC_PSC_IQITCAV_CRC));
		WRITE_VREG_BITS(VDEC_PSC_IQITCAV_CRC_CTL, i, 2, 2);
		WRITE_VREG(VDEC_PSC_IQITCAV_CRC_SEED, 0);
	}
}
#endif

static int cal_frame_dur(void)
{
	u32 frame_dur;
	u32 nr_index = READ_VREG(VC1_FRAME_RATE) & 0xff;
	u32 dr_index = (READ_VREG(VC1_FRAME_RATE) >> 8) & 0xf;
	u32 frame_rate_exp = (READ_VREG(VC1_FRAME_RATE) >> 16) & 0xffff;
	u32 NumeratorValue[VC1_Numerator_Index_MAX] = {
		0, 24000, 25000, 30000,
		50000, 60000, 48000, 72000
	};
	u32 DenominatorValue[VC1_Denominator_Index_MAX] = { 0, 1000, 1001 };

	vc1_print(0, VC1_DEBUG_DETAIL, "%s: nr_index %d, dr_index %d, frame_rate_exp 0x%x\n",
				__func__, nr_index, dr_index, frame_rate_exp);

	if (frame_rate_exp != 0) {
		frame_dur = 96000 * 32 / (frame_rate_exp + 1);
	} else {
		if ((nr_index == 0) || (nr_index >= VC1_Numerator_Index_MAX)
			|| (dr_index == 0) || (dr_index >= VC1_Denominator_Index_MAX))
			return 0;

		frame_dur = 96000 * DenominatorValue[dr_index] / NumeratorValue[nr_index];
	}

	return frame_dur;
}

static bool v4l_resolution_double_check(struct vdec_vc1_hw_s *hw)
{
	bool ret = false;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int temp_last_width = hw->last_width;
	int temp_last_height = hw->last_height;

	if (!ctx->v4l_reqbuff_flag) {
		hw->res_ch_flag = 0;
	} else if (ctx->v4l_reqbuff_flag &&
		hw->report_height != 0 &&
		hw->report_width != 0 &&
		((hw->report_height != hw->frame_height) ||
		(hw->report_width != hw->frame_width))) {
		hw->res_ch_flag = 0;
		hw->last_width = hw->report_width;
		hw->last_height = hw->report_height;
	} else {
		ret = true;
	}

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
		"%s %s! reqbuff:%d flag:%d report:(w:%d, h:%d), frame:(w:%d, h:%d) last:(w:%d, h:%d)\n",
		__func__, ret ? "pass" : "fail", ctx->v4l_reqbuff_flag, hw->res_ch_flag,
		hw->report_width, hw->report_height,
		hw->frame_width, hw->frame_height,
		temp_last_width, temp_last_height);

	if (ret == false) {
		struct aml_vdec_ps_infos ps;

		hw->frame_width = hw->last_width;
		hw->frame_height = hw->last_height;

		vvc1_get_ps_info(hw, &ps);
		/*notice the v4l2 codec.*/
		vdec_v4l_set_ps_infos(ctx, &ps);
		vdec_v4l_res_ch_event(ctx);
		aml_buf_update_planes(&ctx->bm);
		//if (!ctx->v4l_reqbuff_flag && ctx->resolution_event_done)
		//	vdec_v4l_post_event(ctx, V4L2_EVENT_RES_CHANGE_CLEAR);
	}
	ctx->resolution_event_done = false;

	return ret;
}

static irqreturn_t vmvc1_isr_thread_handler(struct vdec_s *vdec, int irq)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	u32 reg;
	u32 picture_type;
	unsigned int offset = 0;
	u32 debug_tag;
	u32 status_reg;

	if (hw->eos) {
		WRITE_VREG(DECODE_STATUS, 0);
		return IRQ_HANDLED;
	}

	debug_tag = READ_VREG(DEBUG_REG1);
	if (debug_tag != 0) {
		vc1_print(DECODE_ID(hw), 0, "%s: dbg%x: %x, wp 0x%x, rp 0x%x, bitcnt %d, stream 0x%x\n", __func__,
			debug_tag, READ_VREG(DEBUG_REG2),
			READ_VREG(VLD_MEM_VIFIFO_WP) /*| stream_prefix_get()*/,
			READ_VREG(VLD_MEM_VIFIFO_RP) /*| stream_prefix_get()*/,
			READ_VREG(VIFF_BIT_CNT),
			stream_prefix_get());
		WRITE_VREG(DEBUG_REG1, 0);
		return IRQ_HANDLED;
	}

	status_reg = READ_VREG(DECODE_STATUS);

	if (status_reg == DECODE_STATUS_SEQ_HEADER_DONE) {
		reg = READ_VREG(VC1_PIC_INFO);
		hw->frame_width = READ_VREG(VC1_PIC_INFO) & 0x3fff;
		hw->frame_height = (READ_VREG(VC1_PIC_INFO) >> 14) & 0x3fff;
		hw->interlace_flag = (READ_VREG(VC1_PIC_INFO) >> 28) & 0x1;
		hw->frame_dur = cal_frame_dur();
		if (hw->frame_dur == 0)
			hw->frame_dur = hw->vvc1_amstream_dec_info.rate;
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s: SEQ_HEADER_DONE frame_width %d/%d, interlace_flag %d, frame_dur %d\n", __func__,
			hw->frame_width, hw->frame_height, hw->interlace_flag, hw->frame_dur);

		if (is_oversize(hw->frame_width, hw->frame_height)) {
			vdec_v4l_post_error_frame_event(ctx);
			vc1_buf_ref_process_for_exception(hw);
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: SEQ_HEADER_DONE oversize timestamp %lld\n",
				__func__, ctx->current_timestamp);
			WRITE_VREG(DECODE_STATUS, 0);
			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		if (!v4l_res_change(hw)) {
			if (ctx->param_sets_from_ucode && !hw->v4l_params_parsed) {
				struct aml_vdec_ps_infos ps;

				if (v4l_resolution_double_check(hw)) {
					vc1_print(0, VC1_DEBUG_DETAIL, "set ucode parse\n");
					vvc1_get_ps_info(hw, &ps);
					vdec_v4l_set_ps_infos(ctx, &ps);
					hw->last_width = hw->frame_width;
					hw->last_height = hw->frame_height;
					hw->v4l_params_parsed = true;
					ctx->decoder_status_info.frame_height = ps.visible_height;
					ctx->decoder_status_info.frame_width = ps.visible_width;
					v4l_vc1_collect_stream_info(vdec, hw);

					if (hw->frame_width && hw->frame_width <= 4096
						&& (hw->frame_width != hw->vvc1_amstream_dec_info.width)) {
						vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "frame width changed %d to %d\n",
							   hw->vvc1_amstream_dec_info.width, hw->frame_width);
						hw->vvc1_amstream_dec_info.width = hw->frame_width;
					}
					if (hw->frame_height && hw->frame_height <= 4096
						&& (hw->frame_height != hw->vvc1_amstream_dec_info.height)) {
						vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "frame height changed %d to %d\n",
							   hw->vvc1_amstream_dec_info.height, hw->frame_height);
						hw->vvc1_amstream_dec_info.height = hw->frame_height;
					}
				}

				hw->dec_result = DEC_RESULT_AGAIN;
				vdec_schedule_work(&hw->work);
			}else {
				struct vdec_pic_info pic = { 0 };

				vdec_v4l_get_pic_info(ctx, &pic);
				hw->vf_buf_num_used = pic.dpb_frames + pic.dpb_margin;
				if (hw->vf_buf_num_used > DECODE_BUFFER_NUM_MAX)
					hw->vf_buf_num_used = DECODE_BUFFER_NUM_MAX;

				WRITE_VREG(DECODE_STATUS, 0);

				hw->res_ch_flag = 0;
				vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s buf_num: %d dpb_frames: %d dpb_margin: %d\n",
						__func__, hw->vf_buf_num_used, pic.dpb_frames, pic.dpb_margin);
				vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_DECODER_START, CORE_MASK_VDEC_1);
			}
		}else  {
			//reset_process_time(hw);
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
		}

		return IRQ_HANDLED;
	}  else if (status_reg == DECODE_STATUS_BUF_INVALID) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: BUF_INVALID \n", __func__);
		WRITE_VREG(DECODE_STATUS, 0);
		return IRQ_HANDLED;
	} else if (status_reg == DECODE_STATUS_PIC_HEADER_DONE) {
		hw->new_type = READ_VREG(AV_SCRATCH_K);
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: PIC_HEADER_DONE picture_type %d(%s)\n",
			__func__, hw->new_type,
			(hw->new_type == I_PICTURE) ? "I" :
				((hw->new_type == P_PICTURE) ? "P" :
					((hw->new_type == B_PICTURE) ? "B" : "BI")));
		vvc1_config_ref_buf(hw);
		vvc1_save_regs(hw);
		vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_DECODER_START, CORE_MASK_VDEC_1);

		if (hw->metadata_config_flag & THUMBNAIL_MODE_FLAG) {
			int dec_res_available = -1;
			dec_res_available = check_dec_res_available(vdec, hw->chunk->size, hw->frame_width, hw->frame_height);
			if (dec_res_available != true) {
				vc1_print(DECODE_ID(hw), 0, "i_oversize, post unsupport\n");
				vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_UNSUPPORT);
				amvdec_stop();
				hw->dec_result = DEC_RESULT_DONE;
				vdec_schedule_work(&hw->work);
				return IRQ_HANDLED;
			}
		}
	}

	status_reg = READ_VREG(DECODE_STATUS);

	if (status_reg == DECODE_STATUS_PARAM_CHECK) {
		u32 v_width, v_height;
		v_width = READ_VREG(VC1_PIC_INFO) & 0x3fff;
		v_height = (READ_VREG(VC1_PIC_INFO) >> 14) & 0x3fff;

		if (is_oversize(v_width, v_height)) {
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s: oversize v_width %d, v_height %d timestamp %lld\n",
						__func__, v_width, v_height, ctx->current_timestamp);
			vdec_v4l_post_error_frame_event(ctx);
			vc1_buf_ref_process_for_exception(hw);

			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		WRITE_VREG(DECODE_STATUS, 0);
		return IRQ_HANDLED;
	} else if (status_reg == DECODE_STATUS_PIC_SKIPPED) {//PIC Skipped
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"%s: DECODE_STATUS_PIC_SKIPPED timestamp %llu\n",
				__func__, ctx->current_timestamp);
		vdec_v4l_post_error_frame_event(ctx);
		vc1_buf_ref_process_for_exception(hw);

		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
		return IRQ_HANDLED;
	} else if (status_reg == DECODE_STATUS_PIC_DONE) {
		struct pic_info_t *new_pic = &(hw->pics[hw->decoding_index]);
		picture_type = hw->new_type;

		vdec_profile(vdec, VDEC_PROFILE_DECODED_FRAME, CORE_MASK_VDEC_1);

		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s DECODE_STATUS_PIC_DONE, picture_type %d, decoding_index %d, bit_cnt=0x%x/0x%x\n",
			__func__, picture_type, hw->decoding_index, READ_VREG(VIFF_BIT_CNT), (READ_VREG(VIFF_BIT_CNT) >> 3));

#ifdef T6D_PRINT_CRC
		t6d_crc_print(hw);
#endif
		hw->decode_pic_count++;

		if (input_frame_based(vdec)) {
			u8 *data = NULL;
			u8 *data1 = NULL;
			u32 i;
			u32 size = hw->start_bit_cnt >> 3;
			u32 res_bytes = READ_VREG(VIFF_BIT_CNT) >> 3;
			u32 counts = (res_bytes > 16) ? 13 : (res_bytes < 4 ? 0 : (res_bytes - 3));

			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s: size 0x%x, res_bytes 0x%x counts 0x%x, bit_cnt 0x%x/0x%x\n",
					__func__, size, res_bytes, counts, hw->start_bit_cnt, READ_VREG(VIFF_BIT_CNT));

			if (hw->start_bit_cnt < READ_VREG(VIFF_BIT_CNT)) {
				counts = 0;
				vc1_print(DECODE_ID(hw), 0, "%s: bitcnt error, bit_cnt 0x%x/0x%x \n",
						__func__, hw->start_bit_cnt, READ_VREG(VIFF_BIT_CNT));
			}

			if (counts != 0) {
				hw->consume_byte = (hw->start_bit_cnt - READ_VREG(VIFF_BIT_CNT)) >> 3;
				hw->consume_byte -= 2;

				vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: consume_byte 0x%x is_mapped %d \n",
						__func__, hw->consume_byte, hw->chunk->block->is_mapped);

				if (!hw->chunk->block->is_mapped)
					data = codec_mm_vmap(hw->chunk->block->start +
						hw->chunk->offset, size);
				else
					data = ((u8 *)hw->chunk->block->start_virt) +
						hw->chunk->offset;

				data1 = (u8 *)data + hw->consume_byte;
#if 0
				vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s size 0x%x data 0x%x data1 0x%x, consume_byte 0x%x, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x .. %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					__func__, size, data, data1, hw->consume_byte,
					data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7], data[8], data[9],
					data[10], data[11], data[12], data[13],
					data[14], data[15], data[16], data[17], data[18], data[19],
					data[size - 4], data[size - 3], data[size - 2], data[size - 1],
					data1[0], data1[1], data1[2], data1[3],
					data1[4], data1[5], data1[6], data1[7], data1[8], data1[9]);
#endif
				for (i = 0; i < counts; i++) {
					if ((data1[i] == 0x00) && (data1[i+1] == 0x00) && (data1[i+2] == 0x01) && (data1[i+3] == 0x0c)) {
						vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"%s: find BDU type field header i %d data 0x%x\n",
							__func__, i, data1[i+3]);

						if (!hw->chunk->block->is_mapped)
							codec_mm_unmap_phyaddr(data);

						WRITE_VREG(DECODE_STATUS, 0);
						return IRQ_HANDLED;
					}
				}

				if (!hw->chunk->block->is_mapped)
					codec_mm_unmap_phyaddr(data);
			}
		}

		reset_process_time(hw);
		hw->dec_result = DEC_RESULT_DONE;

		//hw->interlace_flag = (reg & INTERLACE_FLAG) ? 1 : 0;
		new_pic->offset = READ_VREG(VC1_OFFSET_REG);
		new_pic->repeat_cnt = READ_VREG(VC1_REPEAT_COUNT);
		//new_pic->buffer_info = reg;
		new_pic->index = hw->decoding_index;
		//new_pic->decode_pic_count = decode_pic_count;
		new_pic->picture_type = picture_type;


		if (hw->chunk) {
			new_pic->pts_valid = hw->chunk->pts_valid;
			new_pic->pts = hw->chunk->pts;
			new_pic->pts64 = hw->chunk->pts64;
			new_pic->timestamp = hw->chunk->timestamp;
		} else {
			if (vdec_stream_based(vdec)) {
				struct checkoutptsoffset pts_st = { 0 };
				u64 dur_offset = hw->frame_dur;
				offset = READ_VREG(VC1_OFFSET_REG);

				dur_offset = ((dur_offset << 32) & 0xffffffff00000000) | offset;
				if (!ctx->pts_serves_ops->cal_offset(ctx->ptsserver_id, dur_offset, &pts_st)) {
					new_pic->pts_valid = true;
					new_pic->pts = pts_st.pts;
					new_pic->pts64 = pts_st.pts_64;
					new_pic->timestamp = pts_st.pts_64;
				} else {
					new_pic->pts_valid = false;
					new_pic->pts = 0;
					new_pic->pts64 = 0;
					new_pic->timestamp = 0;
				}
			}
		}

		if (hw->throw_pb_flag && picture_type != I_PICTURE) {
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s: decoding_index %d, throwing picture with type of %d, timestamp %llu\n",
				__func__, hw->decoding_index, picture_type, ctx->current_timestamp);

			WRITE_VREG(VC1_BUFFERIN, ~(1 << hw->decoding_index));

			vdec_v4l_post_error_frame_event(ctx);
			vc1_buf_ref_process_for_exception(hw);
		} else {
			u32 out_buf_index;

			if ((picture_type == I_PICTURE) ||
				(picture_type == P_PICTURE)) {
				out_buf_index = update_reference(hw, hw->decoding_index);
			} else {
				out_buf_index = hw->decoding_index;

				/* drop b frame before reference pic ready */
				if (hw->refs[0] == -1) {
					out_buf_index = hw->vf_buf_num_used;
					WRITE_VREG(VC1_BUFFERIN, ~(1 << out_buf_index));

					vdec_v4l_post_error_frame_event(ctx);
					vc1_buf_ref_process_for_exception(hw);
					vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: drop b frame before 2 ref,timestamp %llu\n",
						__func__, out_buf_index, ctx->current_timestamp);
				}
			}

			if (out_buf_index < hw->vf_buf_num_used) {
				vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s: show buffer_index %d\n", __func__, out_buf_index);
				prepare_display_buf(hw, &hw->pics[out_buf_index]);
			} else {
				vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s: drop buffer_index %d\n", __func__, out_buf_index);
			}
		}

		/*count info*/
		hw->gvs->frame_dur = hw->frame_dur;
		vdec_count_info(hw->gvs, 0, offset);

		amvdec_stop();

		vc1_recycle_frame_buffer(hw);
		vvc1_save_regs(hw);

		WRITE_VREG(DECODE_STATUS, 0);

		vdec_schedule_work(&hw->work);
		return IRQ_HANDLED;
	}

	WRITE_VREG(DECODE_STATUS, 0);
	return IRQ_HANDLED;

}

static irqreturn_t vmvc1_isr_thread_fn(struct vdec_s *vdec, int irq)
{
	irqreturn_t ret;
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;

	ret = vmvc1_isr_thread_handler(vdec, irq);

	hw->process_busy = false;

	return ret;
}

static irqreturn_t vmvc1_isr(struct vdec_s *vdec, int irq)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	u32 status_reg;

	if (hw->eos)
		return IRQ_HANDLED;

	if (hw->process_busy) {
		pr_info("%s process_busy\n", __func__);
		return IRQ_HANDLED;
	}

	status_reg = READ_VREG(DECODE_STATUS);

	if (status_reg == DECODE_STATUS_PIC_DONE) {
		vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_DECODER_PIC_END, CORE_MASK_VDEC_1);

	} else if (status_reg == DECODE_STATUS_PIC_HEADER_DONE) {
		vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_DECODER_HEADER_END, CORE_MASK_VDEC_1);
	}

	hw->process_busy = true;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
}

static struct vframe_s *vvc1_vf_peek(void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct vframe_s *vf;

	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vvc1_vf_get(void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct vframe_s *vf;

	if (kfifo_get(&hw->display_q, &vf)) {
		vf->index_disp = atomic_read(&hw->get_num);
		atomic_add(1, &hw->get_num);
		//ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));

		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s, index = %d, w %d h %d, type 0x%x \n",
				__func__,
				vf->index,
				vf->width,
				vf->height,
				vf->type);

		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
		//ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));

		return vf;
	}

	return NULL;
}

static void vvc1_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *aml_buf = (struct aml_buf *)vf->v4l_mem_handle;

	if (!aml_buf) {
		vc1_print(DECODE_ID(hw), 0,	"invalid vf: %lx\n", (ulong)vf);
		return;
	}
	ctx->current_timestamp = vf->timestamp;
	vdec_v4l_post_error_frame_event(ctx);

	aml_buf_put_ref(&ctx->bm, aml_buf);

	vdec_up(vdec);
}

static int vvc1_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)op_arg;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	//states->buf_recycle_num = kfifo_len(&hw->recycle_q);

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}

static int vvc1_event_cb(int type, void *data, void *private_data)
{
	struct vdec_s *vdec = private_data;

	if (type & VFRAME_EVENT_RECEIVER_RESET) {
#if 0
		unsigned long flags;

		amvdec_stop();

		spin_lock_irqsave(&lock, flags);
		vvc1_local_init(true);
		vvc1_prot_init();
		spin_unlock_irqrestore(&lock, flags);

		amvdec_start();
#endif
	}

	if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE && vdec)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}
	return 0;
}

int vvc1_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	int vf_dur = vdec_get_vf_dur();

	if (!(hw->stat & STAT_VDEC_RUN))
		return -1;

	vstatus->frame_width = hw->vvc1_amstream_dec_info.width;
	vstatus->frame_height = hw->vvc1_amstream_dec_info.height;
	vf_dur = vf_dur ? vf_dur : hw->frame_dur;
	if (vf_dur != 0)
		vstatus->frame_rate = 96000 / vf_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_C);
	vstatus->status = hw->stat;
	vstatus->bit_rate = hw->gvs->bit_rate;
	vstatus->frame_dur = vf_dur;
	vstatus->frame_data = hw->gvs->frame_data;
	vstatus->total_data = hw->gvs->total_data;
	vstatus->frame_count = hw->gvs->frame_count;
	vstatus->error_frame_count = hw->gvs->error_frame_count;
	vstatus->drop_frame_count = hw->gvs->drop_frame_count;
	vstatus->total_data = hw->gvs->total_data;
	vstatus->samp_cnt = hw->gvs->samp_cnt;
	vstatus->offset = hw->gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);

	return 0;
}
#if 0
int vvc1_set_isreset(struct vdec_s *vdec, int isreset)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;

	vc1_print(DECODE_ID(hw), 0,"%s: isreset 0x%x\n", __func__, isreset);

	hw->is_reset = isreset;
	return 0;
}
#endif
static int vvc1_vdec_info_init(struct vdec_vc1_hw_s *hw)
{

	hw->gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == hw->gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

/****************************************/
static int vvc1_workspace_init(struct vdec_vc1_hw_s *hw)
{
	int ret;
	u32 alloc_size;
	dos_addr_t buf_start;

	/* workspace mem */
	alloc_size = WORKSPACE_SIZE;
	if (is_need_fix_streambuf_rp())
		alloc_size += RP_WORKAROUND_SIZE;

	ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, 0,
			alloc_size, DRIVER_NAME, &buf_start);
	if (ret < 0)
		return ret;

	/* calculate workspace offset */
	hw->buf_offset = buf_start - DCAC_BUFF_START_ADDR;
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s: alloc_size 0x%x, buf_offset 0x%x\n",
				__func__, alloc_size, hw->buf_offset);

	vdec_prefix_config(PREFIX_ADDR(buf_start));

	return 0;
}

static int vvc1_prot_init(struct vdec_vc1_hw_s *hw)
{

	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 8, MEM_LEVEL_CNT_BIT, 6);

	/* index v << 16 | u << 8 | y */
#if 1//def NV21
	WRITE_VREG(AV_SCRATCH_0, 0x010100);
	WRITE_VREG(AV_SCRATCH_1, 0x030302);
	WRITE_VREG(AV_SCRATCH_2, 0x050504);
	WRITE_VREG(AV_SCRATCH_3, 0x070706);
/*	WRITE_VREG(AV_SCRATCH_G, 0x090908);
	WRITE_VREG(AV_SCRATCH_H, 0x0b0b0a);
	WRITE_VREG(AV_SCRATCH_I, 0x0d0d0c);
	WRITE_VREG(AV_SCRATCH_J, 0x0f0f0e);*/
#else
	WRITE_VREG(AV_SCRATCH_0, 0x020100);
	WRITE_VREG(AV_SCRATCH_1, 0x050403);
	WRITE_VREG(AV_SCRATCH_2, 0x080706);
	WRITE_VREG(AV_SCRATCH_3, 0x0b0a09);
	WRITE_VREG(AV_SCRATCH_G, 0x090908);
	WRITE_VREG(AV_SCRATCH_H, 0x0b0b0a);
	WRITE_VREG(AV_SCRATCH_I, 0x0d0d0c);
	WRITE_VREG(AV_SCRATCH_J, 0x0f0f0e);
#endif

	/* notify ucode the buffer offset */
	WRITE_VREG(AV_SCRATCH_F, hw->buf_offset);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	//WRITE_VREG(VC1_BUFFEROUT, NEW_DRV_VER); //reuse the register VC1_BUFFEROUT to support new ucode version
	/*vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s VC1_BUFFEROUT %d, buf_offset 0x%x/0x%x\n",
			__func__, NEW_DRV_VER, hw->buf_offset, READ_VREG(AV_SCRATCH_F));*/

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

	//SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
	//CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);

	WRITE_VREG(DEBUG_REG1, 0);
	WRITE_VREG(DEBUG_REG2, 0);

	WRITE_VREG(AV_SCRATCH_L, udebug_flag);

	if (hw->vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WMV3) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "WMV3 dec format\n");
		WRITE_VREG(AV_SCRATCH_4, 0);
	} else if (hw->vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WVC1) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "WVC1 dec format\n");
		WRITE_VREG(AV_SCRATCH_4, 1);
	} else
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "not supported VC1 format\n");

	return 0;
}

static void vvc1_local_init(struct vdec_vc1_hw_s *hw, bool is_reset)
{
	int i;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	/* vvc1_ratio = 0x100; */
	hw->vvc1_ratio = hw->vvc1_amstream_dec_info.ratio;

	hw->avi_flag = (unsigned long) hw->vvc1_amstream_dec_info.param & 0x01;

	hw->unstable_pts = (((unsigned long) hw->vvc1_amstream_dec_info.param & 0x40) >> 6);
#if 0
	if (unstable_pts_debug == 1) {
		hw->unstable_pts = 1;
		pr_info("vc1 init , unstable_pts_debug = %u\n",unstable_pts_debug);
	}
#endif
	hw->next_pts = 0;

	hw->next_pts_us64 = 0;
	hw->frame_width = hw->frame_height = hw->frame_dur = 0;
	hw->process_busy = false;
	hw->decoding_index = INVALID_IDX;

	if (!is_reset) {
		hw->refs[0] = -1;
		hw->refs[1] = -1;
		hw->throw_pb_flag = 1;
		hw->vf_buf_num_used = DECODE_BUFFER_NUM_MAX;
		if (hw->vf_buf_num_used > DECODE_BUFFER_NUM_MAX)
			hw->vf_buf_num_used = DECODE_BUFFER_NUM_MAX;

		for (i = 0; i < hw->vf_buf_num_used; i++) {
			hw->vfbuf_use[i] = 0;
			hw->ref_use[i] = 0;
		}

		INIT_KFIFO(hw->display_q);
		INIT_KFIFO(hw->newframe_q);
		for (i = 0; i < VF_POOL_SIZE; i++) {
			const struct vframe_s *vf;
			vf = &hw->vfpool[i];
			hw->vfpool[i].index = DECODE_BUFFER_NUM_MAX;
			kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
		}
	}

	if (hw->mm_blk_handle) {
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}

	hw->mm_blk_handle = decoder_bmmu_box_alloc_box(
		DRIVER_NAME,
		ctx->id,
		MAX_BMMU_BUFFER_NUM,
		4 + PAGE_SHIFT,
		CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_FOR_VDECODER,
		BMMU_ALLOC_FLAGS_WAITCLEAR);
}

static s32 vvc1_init(struct vdec_vc1_hw_s *hw)
{
	struct firmware_s *fw;
	u32 fw_size = 0x1000 * 16;
	int fw_type = VIDEO_DEC_VC1_MULTI;
	int size = -1;

	pr_info("vvc1_init, format %d\n", hw->vvc1_amstream_dec_info.format);

	if (is_vdec_hevc_combine())
		WRITE_VREG(HEVC_CORE_ENABLE, 0);

	vvc1_local_init(hw, false);

	if (hw->vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WMV3) {
		pr_info("WMV3 dec format\n");
		//vvc1_format = VIDEO_DEC_FORMAT_WMV3;
		WRITE_VREG(AV_SCRATCH_4, 0);
	} else if (hw->vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WVC1) {
		pr_info("WVC1 dec format\n");
		//vvc1_format = VIDEO_DEC_FORMAT_WVC1;
		WRITE_VREG(AV_SCRATCH_4, 1);
	} else
		pr_info("not supported VC1 format\n");

	fw = fw_firmware_s_creat(fw_size);
	if (!fw)
		return -ENOMEM;

	size = get_firmware_data(fw_type, fw->data);

	if (size < 0) {
		amvdec_disable();
		pr_err("get firmware fail.");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	/*if (hw->m_ins_flag) */{
		timer_setup(&hw->check_timer, check_timer_func, 0);
		hw->check_timer.expires = jiffies + CHECK_INTERVAL;

		hw->stat |= STAT_TIMER_ARM;

		INIT_WORK(&hw->work, vvc1_work);
		INIT_WORK(&hw->timeout_work, vvc1_timeout_work);
		return 0;
	}

	return 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	int ret = 0;

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: in\n", __func__);

	if (hw->eos)
		return 0;

	if (hw->timeout_processing &&
	    (work_pending(&hw->work) || work_busy(&hw->work) ||
	    work_pending(&hw->timeout_work) || work_busy(&hw->timeout_work))) {
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"vc1 work pending,not ready for run.\n");
		return 0;
	}
	hw->timeout_processing = 0;

#if 0
	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
		&& pre_decode_buf_level != 0) {
		u32 rp, wp, level;

		rp = STBUF_READ(&vdec->vbuf, get_rp);
		wp = STBUF_READ(&vdec->vbuf, get_wp);
		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;

		if (level < pre_decode_buf_level) {
			hw->not_run_ready++;
			return PRE_LEVEL_NOT_ENOUGH;
		}
	}

	if (again_threshold > 0 &&
		hw->pre_parser_wr_ptr != 0 &&
		hw->again_flag && (!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr = STBUF_READ(&vdec->vbuf, get_rp);
		if (parser_wr_ptr >= hw->pre_parser_wr_ptr &&
			(parser_wr_ptr - hw->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
					"%s buf level:%x\n",  __func__, r);
			ret = 0;
		}
	}
#endif

	ret = is_available_buffer(hw) ? CORE_MASK_VDEC_1 : 0;
	if (ret) {
		hw->not_run_ready = 0;
		hw->buffer_not_ready = 0;
	} else {
		hw->not_run_ready++;
		hw->buffer_not_ready = 1;
	}
	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL, "%s: ret %d\n", __func__, ret);

	return ret;

}

static int vc1_hw_ctx_restore(struct vdec_vc1_hw_s *hw)
{
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	u32 index = -1;
	u32 canvas1_info = 0;

	if (is_vdec_hevc_combine()) {
		WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) | (1 << 0));
		WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) | (0 << 0));

		WRITE_VREG(DBLK_MB_WID_HEIGHT, ((hw->frame_width << 16) | hw->frame_height) >> 4);
		WRITE_VREG(HEVCD_MPP_VDEC_MCR_CTL, (1 << 4) | 1);
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 1 << 31);

		SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 18);
	}

	if (!hw->init_flag) {
		vvc1_workspace_init(hw);
		WRITE_VREG(VC1_SOS_COUNT, 0);
		WRITE_VREG(VC1_BUFFERIN, 0);
		WRITE_VREG(CANVAS_BUF_REG, 0);
		WRITE_VREG(ANC0_CANVAS_REG, 0);
		WRITE_VREG(ANC1_CANVAS_REG, 0);
		WRITE_VREG(DECODE_STATUS, 0);
		WRITE_VREG(VC1_PIC_INFO, 0);
	}

	if (hw->v4l_params_parsed) {
		struct vdec_pic_info pic = { 0 };
		int i;

		if (!hw->vf_buf_num_used) {
			vdec_v4l_get_pic_info(v4l2_ctx, &pic);
			hw->vf_buf_num_used = pic.dpb_frames + pic.dpb_margin;
			if (hw->vf_buf_num_used > DECODE_BUFFER_NUM_MAX)
				hw->vf_buf_num_used = DECODE_BUFFER_NUM_MAX;
		}

		index = find_free_buffer(hw);
		if ((index < 0) || (index >= hw->vf_buf_num_used))
			return -1;

		hw->decoding_index = index;
		canvas1_info = (hw->canvas_spec[index] << 8) | index;
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s: index %d, canvas1_info 0x%x canvas_spec 0x%x\n",
			__func__, index, canvas1_info, hw->canvas_spec[index]);
		WRITE_VREG(CANVAS_BUF_REG, canvas1_info);

		for (i = 0; i < hw->vf_buf_num_used; i++) {
			if (hw->pics[i].v4l_ref_buf_addr) {
				config_cav_lut(canvas_y(hw->canvas_spec[i]),
					&hw->vc1_canvas_config[i][0], VDEC_1);
				config_cav_lut(canvas_u(hw->canvas_spec[i]),
					&hw->vc1_canvas_config[i][1], VDEC_1);

				if (is_vdec_hevc_combine()) {
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
						(canvas_y(hw->canvas_spec[i]) << 8) | (1 << 1));
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
						hw->vc1_canvas_config[i][0].phy_addr >> 5);

					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
						(canvas_u(hw->canvas_spec[i]) << 8) | (1 << 1));
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
						hw->vc1_canvas_config[i][1].phy_addr >> 5);

					WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
						(canvas_y(hw->canvas_spec[i]) << 7) | 1);
					WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
						(canvas_u(hw->canvas_spec[i]) << 8) | canvas_y(hw->canvas_spec[i]));
				}
			}
		}
	}

	if (hw->init_flag)
		vvc1_restore_regs(hw);

	if (is_vdec_hevc_combine()) {
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);
		config_canvas_hevc(hw);
	}

	vvc1_prot_init(hw);

	WRITE_VREG(VC1_LMEM_BUF_ADR, (u32)hw->lmem_phy_addr);
	vdec_prefix_config(PREFIX_ADDR(hw->lmem_phy_addr));

//#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1<<17);
//#endif
	/* cbcr_merge_swap_en */
	if (is_cpu_t7()) {
		if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
			(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
			CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
		else
			SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
	} else {
		if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
			(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M)) {
			SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
			if (is_vdec_hevc_combine())
				SET_VREG_MASK(HEVCD_IPP_AXIIF_CONFIG, 1 << 12);
		} else {
			CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
			if (is_vdec_hevc_combine())
				CLEAR_VREG_MASK(HEVCD_IPP_AXIIF_CONFIG, 1 << 12);
		}
	}

	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 3);

	return 0;
}

static unsigned char get_data_check_sum
	(struct vdec_vc1_hw_s *hw, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!hw->chunk->block->is_mapped)
		data = codec_mm_vmap(hw->chunk->block->start +
			hw->chunk->offset, size);
	else
		data = ((u8 *)hw->chunk->block->start_virt) +
			hw->chunk->offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	if (!hw->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}
static void run(struct vdec_s *vdec, unsigned long mask,
		void (*callback)(struct vdec_s *, void *, int), void *arg)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int save_reg;
	int size, ret;

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"run in\n");

	//hw->run_flag = 1;
	if (!hw->vdec_pg_enable_flag) {
		hw->vdec_pg_enable_flag = 1;
		amvdec_enable();
	}
	save_reg = READ_VREG(POWER_CTL_VLD);
	/* reset everything except DOS_TOP[1] and APB_CBUS[0]*/
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);
	hw->run_count++;
	vdec_reset_core(vdec);
	if (is_vdec_hevc_combine()) {
		hevc_reset_core(vdec);
		WRITE_VREG(HEVC_CORE_ENABLE, 0);
	}

	if (vdec->mc_loaded) {
		/* firmware have load before,
		 * and not changes to another.
		 * ignore reload.
		 */
	} else {
		ret = amvdec_vdec_loadmc_buf_ex(VFORMAT_VC1, "vc1_multi", vdec,
			hw->fw->data, hw->fw->len);
		if (ret < 0) {
			pr_err("[%d] %s: the %s fw loading failed, err: %x\n", vdec->id,
				hw->fw->name, fw_tee_enabled() ? "TEE" : "local", ret);

			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
			vdec_schedule_work(&hw->work);
			return;
		}
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_VC1;
	}

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

#if 0
	if (vdec_stream_based(vdec)) {
		hw->pre_parser_wr_ptr =	STBUF_READ(&vdec->vbuf, get_wp);
	}
#endif

	size = vdec_prepare_input(vdec, &hw->chunk);
	if ((size < 0) ||
		(input_frame_based(vdec) && (hw->chunk == NULL))) {
		hw->input_empty++;
		hw->dec_result = DEC_RESULT_AGAIN;
		vdec_schedule_work(&hw->work);
		//hw->run_flag = 0;
		return;
	}

	hw->input_empty = 0;
	if (vdec_frame_based(vdec)) {
		size = hw->chunk->size + (hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		WRITE_VREG(VIFF_BIT_CNT, size * 8);
		hw->start_bit_cnt = size * 8;
		ctx->current_timestamp = hw->chunk->timestamp;
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s: VIFF_BIT_CNT %d, start_bit_cnt 0x%x, timestamp %llu\n", __func__,
			READ_VREG(VIFF_BIT_CNT), hw->start_bit_cnt, ctx->current_timestamp);
	}

	if (input_frame_based(vdec) && !(vdec_secure(vdec))) {
		u8 *data = NULL;

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, size);
		else
			data = ((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;

		if (debug & VC1_DEBUG_DETAIL)
			vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
				"%s size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. "
				"%02x %02x %02x %02x\n",
				__func__, size, get_data_check_sum(hw, size),
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[size - 4],
				data[size - 3], data[size - 2],
				data[size - 1]);

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else
		vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,
			"%s  %x %x %x %x %x size 0x%x, bitcnt %d\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			STBUF_READ(&vdec->vbuf, get_rp),
			STBUF_READ(&vdec->vbuf, get_wp),
			size, READ_VREG(VIFF_BIT_CNT));

	hw->dec_result = DEC_RESULT_NONE;
	if (vc1_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		vdec_schedule_work(&hw->work);
		//hw->run_flag = 0;
		return;
	}

	/*
		This configuration of VC1_CONTROL_REG will
		pop bits (even no data in the stream buffer) if input is enabled,
		so it can only be configured before vdec_enable_input() is called.
		So move this code from ucode to here
	*/
#define DISABLE_DBLK_HCMD   0
#define DISABLE_MC_HCMD 0
	WRITE_VREG(VC1_CONTROL_REG, (DISABLE_DBLK_HCMD<<6) | (DISABLE_MC_HCMD<<5) | 1);

	vdec_enable_input(vdec);
	hw->dec_result = DEC_RESULT_NONE;
	hw->stat |= STAT_MC_LOAD;
	hw->last_vld_level = 0;

	start_process_time_set(hw);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"amvdec_start \n");
	vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_DECODER_START, CORE_MASK_VDEC_1);

	amvdec_start();
	hw->stat |= STAT_VDEC_RUN;
	hw->stat |= STAT_TIMER_ARM;
	hw->init_flag = 1;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
	//hw->run_flag = 0;
}


static int ammvdec_vc1_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_vc1_hw_s *hw = NULL;
	struct aml_vcodec_ctx *ctx = NULL;
	int config_val = 0;

	pr_info("%s start WORKSPACE_SIZE 0x%x\n", __func__, WORKSPACE_SIZE);

	if (pdata == NULL) {
		pr_info("ammvdec_vc1 memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_vc1_hw_s *)vzalloc(sizeof(struct vdec_vc1_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_vc1 decoder driver alloc failed\n");
		return -ENOMEM;
	}

	//hw->m_ins_flag = 1;
	//hw->run_flag = 0;

	if (pdata->sys_info) {
		hw->vvc1_amstream_dec_info = *pdata->sys_info;

		if ((hw->vvc1_amstream_dec_info.height != 0) &&
			(hw->vvc1_amstream_dec_info.width >
			(VC1_MAX_SUPPORT_SIZE/hw->vvc1_amstream_dec_info.height))) {
			pr_info("ammvdec_vc1_v4l: over size, unsupport: %d * %d\n",
				hw->vvc1_amstream_dec_info.width,
				hw->vvc1_amstream_dec_info.height);
			return -EFAULT;
		}
	}
	pdata->dec_status = vvc1_dec_status;
	//pdata->set_isreset = vvc1_set_isreset;
	pdata->reset = reset;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->irq_handler = vmvc1_isr;
	pdata->threaded_irq_handler = vmvc1_isr_thread_fn;
	hw->is_reset = 0;

	hw->canvas_mode = pdata->canvas_mode;

	/* the ctx from v4l2 driver. */
	hw->v4l2_ctx = pdata->private;
	ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	ctx->v4l_reqbuff_flag = true;

	pdata->private = hw;
	if (pdata->config_len) {
		if (get_config_int(pdata->config, "parm_v4l_buffer_margin",
			&config_val) == 0)
			hw->dynamic_buf_num_margin = config_val;
		else
			hw->dynamic_buf_num_margin = default_vc1_margin;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hw->canvas_mode = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag", &config_val) == 0)
			hw->metadata_config_flag = config_val;

		if (get_config_int(pdata->config, "parm_v4l_duration",
			&config_val) == 0)
			hw->cur_duration = config_val;
	} else
		hw->dynamic_buf_num_margin = default_vc1_margin;

	pr_info("canvas_mode %d\n", hw->canvas_mode);

	hw->decode_pic_count = 0;

	vvc1_vdec_info_init(hw);

	hw->lmem_addr = decoder_dma_alloc_coherent(&hw->lmem_phy_handle,
						LMEM_BUF_SIZE, &hw->lmem_phy_addr, "VC1_LMEM_BUF");
	if (hw->lmem_addr == 0) {
		pr_info("%s: failed to alloc lmem buffer\n", __func__);

		if (hw->gvs) {
			kfree(hw->gvs);
			hw->gvs = NULL;
		}
		vfree(hw);

		pdata->dec_status = NULL;
		return -ENODEV;
	}

	if (vvc1_init(hw) < 0) {
		pr_info("ammvdec_vc1_v4l init failed.\n");

		if (hw->lmem_addr) {
			decoder_dma_free_coherent(hw->lmem_phy_handle,
					LMEM_BUF_SIZE, (void *)hw->lmem_addr, hw->lmem_phy_addr);
			hw->lmem_addr = 0;
		}

		if (hw->gvs) {
			kfree(hw->gvs);
			hw->gvs = NULL;
		}
		vfree(hw);
		pdata->dec_status = NULL;
		return -ENODEV;
	}

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			    VFM_DEC_PROVIDER_NAME);
		//hw->frameinfo_enable = 1;
	}
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vvc1_vf_provider, pdata);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	vdec_set_vframe_comm(pdata, DRIVER_NAME);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);
	}

	return 0;
}

static KV_INT_TO_VOID ammvdec_vc1_remove(struct platform_device *pdev)
{
	struct vdec_vc1_hw_s *hw = (struct vdec_vc1_hw_s *)
			(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec = hw_to_vdec(hw);

	vc1_print(DECODE_ID(hw), VC1_DEBUG_DETAIL,"%s \n", __func__);

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)hw);
		hw->stat &= ~STAT_ISR_REG;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->timeout_work);

#if 0
	amvdec_disable();
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TM2)
		vdec_reset_core(vdec);
#endif
	if (hw->mm_blk_handle) {
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}

	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);

	if (vdec->parallel_dec == 1) {
		u32 i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
			vdec->free_canvas_ex(canvas_y(hw->canvas_spec[i]), vdec->id);
			vdec->free_canvas_ex(canvas_u(hw->canvas_spec[i]), vdec->id);
		}
	}

	if (hw->lmem_addr) {
		decoder_dma_free_coherent(hw->lmem_phy_handle,
					LMEM_BUF_SIZE, (void *)hw->lmem_addr, hw->lmem_phy_addr);
		hw->lmem_addr = 0;
	}

	if (hw->gvs) {
		kfree(hw->gvs);
		hw->gvs = NULL;
	}

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}

	vfree(hw);

	return KV_RET_x_TO_VOID(0);
}

/****************************************/

static struct platform_driver ammvdec_vc1_driver = {
	.probe = ammvdec_vc1_probe,
	.remove = ammvdec_vc1_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static int __init ammvdec_vc1_driver_init_module(void)
{
	vc1_print(0, 0, "ammvdec_vc1_v4l module init\n");

	if (platform_driver_register(&ammvdec_vc1_driver)) {
		pr_err("failed to register ammvdec_vc1 driver\n");
		return -ENODEV;
	}

	vcodec_profile_register_v2("VC1-V4L", VFORMAT_VC1, 1);
	vcodec_feature_register(VFORMAT_VC1, 1);

	return 0;
}

static void __exit ammvdec_vc1_driver_remove_module(void)
{
	pr_debug("ammvdec_vc1_v4l module remove.\n");

	platform_driver_unregister(&ammvdec_vc1_driver);
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static struct param_entry ammvdec_vc1_v4l_params[] = {
	PARAM_UINT(unstable_pts_debug),
	PARAM_UINT(udebug_flag),
	PARAM_UINT(debug),
	PARAM_UINT(debug_mask),
	PARAM_UINT(wait_time),
	{ /* sentinel */ }
};
module_param_cb(params, &key_value_param_ops, &ammvdec_vc1_v4l_params, 0644);
#endif

MEDIA_PARAM(unstable_pts_debug, uint, 0664);
MODULE_PARM_DESC(unstable_pts_debug, "\n ammvdec_vc1_v4l unstable_pts\n");

MEDIA_PARAM(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n ammvdec_vc1_v4l udebug_flag\n");

MEDIA_PARAM(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n ammvdec_vc1_v4l debug\n");

MEDIA_PARAM(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n ammvdec_vc1_v4l debug_mask\n");

MEDIA_PARAM(wait_time, uint, 0664);
MODULE_PARM_DESC(wait_time, "\n ammvdec_vc1_v4l wait_time\n");

MEDIA_PARAM(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n ammvdec_vc1 decode_timeout_val\n");
/****************************************/
module_init(ammvdec_vc1_driver_init_module);
module_exit(ammvdec_vc1_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC VC1 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_AUTHOR("Qi Wang <qi.wang@amlogic.com>");
