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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/sched/clock.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/dma-map-ops.h>
#else
#include <linux/dma-contiguous.h>
#endif
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <uapi/linux/tee.h>
#include <media/v4l2-mem2mem.h>

#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../stream_input/amports/amports_priv.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../decoder/utils/decoder_dma_alloc.h"
#include "../../decoder/utils/aml_buf_helper.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../../decoder/utils/vdec_feature.h"
#include "../../../media_sync/pts_server/pts_server_core.h"
#include "../../decoder/utils/vdec_profile.h"

//#define USE_OLD_CHIP
#define SEND_LMEM_WITH_RPM
#define SUPPORT_10BIT
#define USE_NV21_EXTRA_BUF
#define VVC_10B_MMU
#define VVC_10B_MMU_DW
#ifndef USE_OLD_CHIP
#define DYN_CACHE
#endif
//#define HEVC_PIC_STRUCT_SUPPORT
#define MULTI_INSTANCE_SUPPORT
#define LOSLESS_COMPRESS_MODE

static u32 debug_mask = 0xffffffff;
static u32 debug;

struct hevc_state_s;
struct PIC_s;
static int get_dbg_flag(struct hevc_state_s * hevc);
static int hevc_print(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static int hevc_print_cont(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static void put_mv_buf(struct PIC_s *pic);

#define H266_DEBUG_BUFMGR                   0x01
#define H266_DEBUG_BUFMGR_MORE              0x02
#define H266_DEBUG_DETAIL                   0x04
#define H266_DEBUG_OUT_PTS                  0x08
#define H266_DEBUG_REG_CFG                  0x10
#define H266_DEBUG_REF_LIST                 0x20
#define H266_DEBUG_DUMP_REF_LIST_BUF        0x40
#define H266_DEBUG_REG                      0x80
#define H266_DEBUG_SEND_PARAM_WITH_REG      0x100
#define H266_DEBUG_NO_DISPLAY               0x200
#define H266_DEBUG_DISPLAY_CUR_FRAME        0x400
#define H266_DEBUG_DUMP_PIC_LIST            0x1000
#define H266_DEBUG_PRINT_SEI		        0x2000
#define H266_DEBUG_HAS_AUX_IN_SLICE			0x8000
#define H266_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define H266_DEBUG_DIS_SYS_ERROR_PROC       0x20000
#define H266_NO_CHANG_DEBUG_FLAG_IN_CODE    0x40000
#define H266_DEBUG_TRIG_SLICE_SEGMENT_PROC  0x80000
#define H266_DEBUG_HW_RESET                 0x100000
#define H266_CFG_CANVAS_IN_DECODE           0x200000
#define H266_DEBUG_DV                       0x400000
#define H266_DEBUG_NO_EOS_SEARCH_DONE       0x800000
#define H266_DEBUG_NOT_USE_LAST_DISPBUF     0x1000000
#define H266_DEBUG_IGNORE_CONFORMANCE_WINDOW	0x2000000
#define H266_DEBUG_WAIT_DECODE_DONE_WHEN_STOP   0x4000000
#ifdef MULTI_INSTANCE_SUPPORT
#define PRINT_FLAG_ERROR		0x0
#define IGNORE_PARAM_FROM_CONFIG	0x08000000
#define PRINT_FRAMEBASE_DATA		0x10000000
#define PRINT_FLAG_VDEC_STATUS		0x20000000
#define PRINT_FLAG_VDEC_DETAIL		0x40000000
#define PRINT_FLAG_V4L_DETAIL		0x80000000
#endif

#define DEF_PRINT_PARAM
#include "h266_global.h"
#include "vvc_global.h"
#include "h266_bufmgr.c"
#define P010_DW_ENABLE
//#define OW_TRIPLE_WRITE
#define DOS_PROJECT

/*
to enable DV of frame mode
#define DOLBY_META_SUPPORT in ucode
*/
#define HEVC_8K_LFTOFFSET_FIX
//#define SUPPORT_LONG_TERM_RPS

#define CONSTRAIN_MAX_BUF_NUM

#define SWAP_HEVC_UCODE

#define AGAIN_HAS_THRESHOLD


			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
#define MPRED_8K_MV_BUF_SIZE		(0x120000*4)
#define MPRED_4K_MV_BUF_SIZE		(0x120000)
#define MPRED_MV_BUF_SIZE		(0x40000)

#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K  0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K  0x120000
#define DB_NUM 20

#define MAX_FRAME_4K_NUM 0x1200
#define MAX_FRAME_8K_NUM ((MAX_FRAME_4K_NUM) * 4)

#define H266_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7

#define HEVC_ASSIST_MMU_MAP_ADDR                   0x3009

#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_CM_HEADER_START_ADDR2                 0x364a
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a
#define HEVC_SAO_MMU_VH0_ADDR2                     0x364d
#define HEVC_SAO_MMU_VH1_ADDR2                     0x364e

#define HEVC_SAO_MMU_DMA_CTRL2                     0x364c
#define HEVC_SAO_MMU_STATUS2                       0x3650
#define HEVC_DW_VH0_ADDDR                          0x365e
#define HEVC_DW_VH1_ADDDR                          0x365f

#define HEVC_DBLK_CFGB                             0x350b
//#define HEVCD_MPP_DECOMP_AXIURG_CTL                0x34c7

#define SWAP_HEVC_OFFSET (3 * 0x1000)

#define MEM_NAME "codec_266"

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     9
#define MULTI_DRIVER_NAME "ammvdec_h266_v4l"
#endif
#define DRIVER_NAME "amvdec_h266_v4l"
#define DRIVER_HEADER_NAME "amvdec_h266_header"

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THRESHOLD 3

#define DUR2PTS(x) ((x)*90/96)

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)

#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

#define SEI_UserDataITU_T_T35	4
#define INVALID_IDX -1  /* Invalid buffer index.*/

static int vh266_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vh266_vf_peek(void *);
static struct vframe_s *vh266_vf_get(void *);
static void vh266_vf_put(struct vframe_s *, void *);
static int vh266_event_cb(int type, void *data, void *private_data);

static int vh266_stop(struct hevc_state_s *hevc);
#ifdef MULTI_INSTANCE_SUPPORT
static int vmh266_stop(struct hevc_state_s *hevc);
static s32 vh266_init(struct vdec_s *vdec);
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask);
static void reset_process_time(struct hevc_state_s *hevc);
static void start_process_time(struct hevc_state_s *hevc);
static void restart_process_time(struct hevc_state_s *hevc);
static void timeout_process(struct hevc_state_s *hevc);
#else
static s32 vh266_init(struct hevc_state_s *hevc);
#endif
static void vh266_prot_init(struct hevc_state_s *hevc);
static int vh266_local_init(struct hevc_state_s *hevc);
static void vh266_check_timer_func(struct timer_list *timer);
static void config_decode_mode(struct hevc_state_s *hevc);
static int check_data_size(struct vdec_s *vdec);

static const char vh266_dec_id[] = "vh266-dev";

#define PROVIDER_NAME   "decoder.h266"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.h266"

static const struct vframe_operations_s vh266_vf_provider = {
	.peek = vh266_vf_peek,
	.get = vh266_vf_get,
	.put = vh266_vf_put,
	.event_cb = vh266_event_cb,
	.vf_states = vh266_vf_states,
};

static struct vframe_provider_s vh266_vf_prov;

//0.3.42-g8104942
#define UCODE_SWAP_VERSION 3
#define UCODE_SWAP_SUBMIT_COUNT 42


static u32 enable_swap;
static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 video_signal_type;
static int start_decode_buf_level = 0x8000;
static unsigned int decode_timeout_val = 200;

static u32 run_ready_min_buf_num = 1;
static u32 disable_ip_mode;
static u32 print_lcu_error = 1;
/*data_resend_policy:
	bit 0, stream base resend data when decoding buf empty
*/
static u32 data_resend_policy = 1;
static int poc_num_margin = 1000;
static int poc_error_limit = 30;

static u32 dirty_again_threshold = 100;
static u32 dirty_buffersize_threshold = 0x800000;

#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000

#ifdef SUPPORT_10BIT
#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b
#define HEVC_SAO_CTRL9                             0x362d

#define HEVC_CM_BODY_LENGTH2                       0x3663
#define HEVC_CM_HEADER_OFFSET2                     0x3664
#define HEVC_CM_HEADER_LENGTH2                     0x3665

/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* double_write_mode:
 *	0, no double write;
 *	1, 1:1 ratio;
 *	2, (1/4):(1/4) ratio;
 *	3, (1/4):(1/4) ratio, with both compressed frame included
 *	4, (1/2):(1/2) ratio;
 *	5, (1/2):(1/2) ratio, with both compressed frame included
 *	8, (1/8):(1/8) ratio,  from t7
 *	0x10, double write only
 *	0x100, if > 1080p,use mode 4,else use mode 1;
 *	0x200, if > 1080p,use mode 2,else use mode 1;
 *	0x300, if > 720p, use mode 4, else use mode 1;
 *	0x1000,if > 1080p,use mode 3, else if > 960*540, use mode 4, else use mode 1;
 */
static u32 double_write_mode;

static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
#ifndef MULTI_INSTANCE_SUPPORT
static u32 workaround_enable;
#endif
static u32 force_w_h;
#endif
static u32 force_fps;
static u32 pts_unstable;

#define BUF_POOL_SIZE	32
#define MAX_REF_PIC_NUM PIC_POOL_SIZE
#define MAX_BUF_NUM PIC_POOL_SIZE

#ifdef MV_USE_FIXED_BUF
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#else
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1 + MAX_REF_PIC_NUM)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#define MV_BUFFER_IDX(n) (BUF_POOL_SIZE + 1 + n)
#endif

#define HEVC_ERROR_FRAME_DISPLAY 0
#define HEVC_ERROR_FRAME_DROP 2

const u32 h266_version = 20231205;
static u32 log_mask;
static u32 radr;
static u32 rval;
static u32 dbg_cmd;
static u32 dump_nal;
static u32 dbg_skip_decode_index;
/*
 * bit 0~3, for HEVCD_IPP_AXIIF_CONFIG endian config
 * bit 8~23, for HEVC_SAO_CTRL1 endian config
 */
static u32 endian;
#define HEVC_CONFIG_BIG_ENDIAN     ((0x880 << 8) | 0x8)
#define HEVC_CONFIG_LITTLE_ENDIAN  ((0xff0 << 8) | 0xf)

#ifdef ERROR_HANDLE_DEBUG
static u32 dbg_nal_skip_flag;
		/* bit[0], skip vps; bit[1], skip sps; bit[2], skip pps */
static u32 dbg_nal_skip_count;
#endif
/*for debug*/
static u32 force_bufspec;

/*
	udebug_flag:
	bit 0, enable ucode print
	bit 1, enable ucode detail print
	bit [31:16] not 0, pos to dump lmem
		bit 2, pop bits to lmem
		bit [11:8], pre-pop bits for alignment (when bit 2 is 1)
*/
static u32 udebug_flag;
/*
	when udebug_flag[1:0] is not 0
	udebug_pause_pos not 0,
		pause position
*/
static u32 udebug_pause_pos;
/*
	when udebug_flag[1:0] is not 0
	and udebug_pause_pos is not 0,
		pause only when DEBUG_REG2 is equal to this val
*/
static u32 udebug_pause_val;

static u32 udebug_pause_decode_idx;

static u32 decode_pic_begin;
static uint slice_parse_begin;
static u32 step;
static bool is_reset;

#ifdef CONSTRAIN_MAX_BUF_NUM
static u32 run_ready_max_vf_only_num;
static u32 run_ready_display_q_num;
	/*0: not check
	  0xff: work_pic_num
	  */
static u32 run_ready_max_buf_num = 0xff;
#endif

static u32 dynamic_buf_num_margin = 4;
static u32 buf_alloc_width;
static u32 buf_alloc_height;

static u32 max_buf_num = 16;
static u32 buf_alloc_size;
/*static u32 re_config_pic_flag;*/
/*
 *bit[0]: 0,
 *bit[1]: 0, always release cma buffer when stop
 *bit[1]: 1, never release cma buffer when stop
 *bit[0]: 1, when stop, release cma buffer if blackout is 1;
 *do not release cma buffer is blackout is not 1
 *
 *bit[2]: 0, when start decoding, check current displayed buffer
 *	 (only for buffer decoded by h266) if blackout is 0
 *	 1, do not check current displayed buffer
 *
 *bit[3]: 1, if blackout is not 1, do not release current
 *			displayed cma buffer always.
 */
/* set to 1 for fast play;
 *	set to 8 for other case of "keep last frame"
 */
static u32 buffer_mode = 1;

/* buffer_mode_dbg: debug only*/
static u32 buffer_mode_dbg = 0xffff0000;
/**/
/*
 *bit[1:0]PB_skip_mode: 0, start decoding at begin;
 *1, start decoding after first I;
 *2, only decode and display none error picture;
 *3, start decoding and display after IDR,etc
 *bit[31:16] PB_skip_count_after_decoding (decoding but not display),
 *only for mode 0 and 1.
 */
static u32 nal_skip_policy = 2;

/*
 *bit 0, 1: only display I picture;
 *bit 1, 1: only decode I picture;
 */
static u32 i_only_flag;
static u32 skip_nal_count = 500;
/*
bit 0, fast output first I picture
*/
static u32 fast_output_enable = 1;

static u32 frmbase_cont_bitlevel = 0;//0x60;
#ifdef USE_OLD_CHIP
static u32 frmbase_muti_slice = 0;
#else
static u32 frmbase_muti_slice = 0;
#endif

/*
use_cma: 1, use both reserver memory and cma for buffers
2, only use cma for buffers
*/
static u32 use_cma = 2;

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
/*
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;
*/
static u32 prefix_aux_buf_size = (12 * 1024);
static u32 suffix_aux_buf_size = (12 * 1024);

static u32 max_decoding_time;
/*
 *error handling
 */
/*error_handle_policy:
 *bit 0: 0, auto skip error_skip_nal_count nals before error recovery;
 *1, skip error_skip_nal_count nals before error recovery;
 *bit 1 (valid only when bit0 == 1):
 *1, wait vps/sps/pps after error recovery;
 *bit 2 (valid only when bit0 == 0):
 *0, auto search after error recovery (hevc_recover() called);
 *1, manual search after error recovery
 *(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))
 *
 *bit 4: 0, set error_mark after reset/recover
 *	1, do not set error_mark after reset/recover
 *
 *bit 5: 0, check total lcu for every picture
 *	1, do not check total lcu
 *
 *bit 6: 0, do not check head error
 *	1, check head error
 *
 *bit 7: 0, allow to print over decode
 *       1, NOT allow to print over decode
 *
 *bit 8: 0, use interlace policy
 *       1, NOT use interlace policy
 *bit 9: 0, discard dirty data on playback start
 *       1, do not discard dirty data on playback start
 *bit 10:0, when ucode always returns again, it supports discarding data
 *		 1, When ucode always returns again, it does not support discarding data
 */

static u32 error_handle_policy;
static u32 error_skip_nal_count = 6;
static u32 error_handle_threshold = 30;
static u32 error_handle_nal_skip_threshold = 10;
static u32 error_handle_system_threshold = 30;
static u32 interlace_enable = 1;
static u32 fr_hint_status;

/*
 *parser_sei_enable:
 *  bit 0, sei;
 *  bit 1, sei_suffix (fill aux buf)
 *  bit 2, fill sei to aux buf (when bit 0 is 1)
 *  bit 8, debug flag
 */
static u32 parser_sei_enable;
static u32 parser_dolby_vision_enable = 1;
/* this is only for h266 mmu enable */

static u32 mmu_enable = 1;
static u32 mmu_enable_force;
static u32 work_buf_size;
static unsigned int force_disp_pic_index;
static unsigned int disp_vframe_valve_level;
static int pre_decode_buf_level = 0x1000;
static unsigned int pic_list_debug;
#ifdef HEVC_8K_LFTOFFSET_FIX
/* performance_profile: bit 0, multi slice in ucode
*/
static unsigned int performance_profile = 1;
#endif
#ifdef MULTI_INSTANCE_SUPPORT
static unsigned int max_decode_instance_num = MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int display_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_get_frame_interval[MAX_DECODE_INSTANCE_NUM];
static unsigned int run_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int input_empty[MAX_DECODE_INSTANCE_NUM];
static unsigned int not_run_ready[MAX_DECODE_INSTANCE_NUM];
static unsigned int ref_frame_mark_flag[MAX_DECODE_INSTANCE_NUM] =
	{1, 1, 1, 1, 1, 1, 1, 1, 1};

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc);
#endif

#endif

/*
 *[3:0] 0: default use config from omx.
 *      1: force enable fence.
 *      2: disable fence.
 *[7:4] 0: fence use for driver.
 *      1: fence fd use for app.
 */
static u32 force_config_fence;

/*
 *The parameter sps_max_dec_pic_buffering_minus1_0+1
 *in SPS is the minimum DPB size required for stream
 *(note: this parameter does not include the frame
 *currently being decoded) +1 (decoding the current
 *frame) +1 (decoding the current frame will only
 *update reference frame information, such as reference
 *relation, when the next frame is decoded)
 */
static u32 detect_stuck_buffer_margin = 3;


#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
//#define get_dbg_flag(hevc) ((debug_mask & (1 << hevc->index)) ? debug : 0)
#define get_dbg_flag2(hevc) ((debug_mask & (1 << get_idx(hevc))) ? debug : 0)
#define is_log_enable(hevc) ((log_mask & (1 << hevc->index)) ? 1 : 0)
#else
#define get_dbg_flag(hevc) debug
#define get_dbg_flag2(hevc) debug
#define is_log_enable(hevc) (log_mask ? 1 : 0)
#define get_valid_double_write_mode(hevc) double_write_mode
#define get_buf_alloc_width(hevc) buf_alloc_width
#define get_buf_alloc_height(hevc) buf_alloc_height
#define get_dynamic_buf_num_margin(hevc) dynamic_buf_num_margin
#endif
#define get_buffer_mode(hevc) buffer_mode


static DEFINE_SPINLOCK(h266_lock);
struct task_struct *h266_task = NULL;
#define DEBUG_REG
#ifdef DEBUG_REG
void WRITE_VREG_DBG(unsigned adr, unsigned val)
{
	if (debug & H266_DEBUG_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
extern u32 trickmode_i;
#else
u32 trickmode_i;
#endif

static DEFINE_MUTEX(vh266_mutex);

static DEFINE_MUTEX(vh266_log_mutex);

static u32 without_display_mode;

static u32 mv_buf_dynamic_alloc;

/* --------------------------------------------------- */
/* Amrisc Software Interrupt */
/* --------------------------------------------------- */
#define AMRISC_STREAM_EMPTY_REQ 0x01
#define AMRISC_PARSER_REQ       0x02
#define AMRISC_MAIN_REQ         0x04

/* --------------------------------------------------- */
/* HEVC_DEC_STATUS define */
/* --------------------------------------------------- */
/*VVC NEW*/
#define VVC_DEC_IDLE                           0
#define VVC_SEQUENCE                           1
#define VVC_PICTURE                            2
#define VVC_PREFIX_APS                         3
#define VVC_DISCARD_NAL                        4
#define VVC_SEQUENCE_END                       5
#define VVC_SLICE_DECODING                     6

#define HEVC_SEI_DAT                         0xc
#define HEVC_SEI_DAT_DONE                    0xd
#define HEVC_OVER_DECODE                    0xf

#define SWAP_IN_CMD                          0x10
#define SWAP_OUT_CMD                         0x11
#define SWAP_OUTIN_CMD                       0x12
#define SWAP_DONE                            0x13
#define SWAP_POST_INIT                       0x14

/*head*/
#define VVC_HEAD_SEQ_READY                  0x21
#define VVC_HEAD_PIC_READY                  0x22
#define VVC_HEAD_SLICE_INFO_READY           0x23
#define VVC_HEAD_SEQ_END_READY              0x24
#define VVC_STARTCODE_SEARCH_DONE           0x25
/*pic done*/
#define HEVC_DECPIC_DATA_DONE       0x30
#define HEVC_DECPIC_DATA_ERROR      0x31
#define HEVC_NAL_DECODE_DONE        0x32
#define VVC_DECODE_BUFEMPTY        0x33
#define VVC_DECODE_TIMEOUT         0x34
#define VVC_DECODE_OVER_SIZE       0x35

#define HEVC_SEARCH_BUFEMPTY        0x38
#define HEVC_DECODE_OVER_SIZE       0x39
#define HEVC_DECODE_PARAMS_ERR      0x3a

/*cmd*/
#define VVC_DECODE_SLICE                0xf0
#define VVC_SEND_DUMP_INFO                  0xf1
#define VVC_SEND_DUMP_INFO_DONE             0xf2
#define VVC_SKIP_DECODING          0xf3
#define HEVC_ACTION_DEC_CONT           0xfd
#define VVC_ACTION_ERROR                    0xfe
#define HEVC_ACTION_ERROR                    0xfe
#define VVC_ACTION_DONE                     0xff
#define HEVC_ACTION_DONE                     0xff
/*VVC_DEC_STATUS end*/
/**/


/* --------------------------------------------------- */
/* Include "parser_cmd.h" */
/* --------------------------------------------------- */
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

#define   MCRCC_ENABLE
#define INVALID_POC 0x80000000
#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define VVC_ALF_SWAP_BUFFER       HEVC_ASSIST_SCRATCH_2
#define HEVC_RCS_BUFFER           HEVC_ASSIST_SCRATCH_3
#define HEVC_SPS_BUFFER           HEVC_ASSIST_SCRATCH_4
#define HEVC_PPS_BUFFER           HEVC_ASSIST_SCRATCH_5
#define HEVC_SAO_UP               HEVC_ASSIST_SCRATCH_6
#define H266_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7
#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#define HEVC_STREAM_SWAP_BUFFER2  HEVC_ASSIST_SCRATCH_8
#define HEVC_REF_LIST_BUFFER	HEVC_ASSIST_SCRATCH_9
//#define HEVC_sao_mem_unit         P_HEVC_ASSIST_SCRATCH_9
//#define HEVC_SAO_ABV              P_HEVC_ASSIST_SCRATCH_A
//#define HEVC_sao_vb_size          P_HEVC_ASSIST_SCRATCH_B
//#define HEVC_SAO_VB               P_HEVC_ASSIST_SCRATCH_C
#define VVC_CONTEXT_BUFF          HEVC_ASSIST_SCRATCH_A
#define HEVC_DECODE_SIZE          HEVC_ASSIST_SCRATCH_B
#define VVC_SBAC_TOP_BUFFER       HEVC_ASSIST_SCRATCH_C
#define HEVC_SCALELUT             HEVC_ASSIST_SCRATCH_D
#define HEVC_WAIT_FLAG            HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L
#define HEVC_DECODE_PIC_BEGIN_REG HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_PIC_NUM_REG   HEVC_ASSIST_SCRATCH_N

#define DEBUG_REG1                HEVC_ASSIST_SCRATCH_G
#define DEBUG_REG2                HEVC_ASSIST_SCRATCH_H

/*
ucode parser/search control
bit 0:  0, header auto parse; 1, header manual parse
bit 1:  0, auto skip for noneseamless stream; 1, no skip
bit [3:2]: valid when bit1 == 0;
0, auto skip nal before first rcs/sps/pps/idr;
1, auto skip nal before first rcs/sps/pps
2, auto skip nal before fist  rcs/sps/pps, and not decode until the first I slice (with slice address of 0)
*/
#define NAL_SEARCH_CTL        HEVC_ASSIST_SCRATCH_I
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS           HEVC_ASSIST_SCRATCH_K

#define DECODE_MODE              HEVC_ASSIST_SCRATCH_J

    /*do not define ENABLE_SWAP_TEST*/
#define HEVC_AUX_ADR            HEVC_ASSIST_SCRATCH_L
#define HEVC_AUX_DATA_SIZE      HEVC_ASSIST_SCRATCH_M

#define HEVC_SHORT_TERM_RPS       HEVC_ASSIST_SCRATCH_2
#define LMEM_DUMP_ADR                 HEVC_ASSIST_SCRATCH_F



/*
 *ucode parser/search control
 *bit 0:  0, header auto parse; 1, header manual parse
 *bit 1:  0, auto skip for noneseamless stream; 1, no skip
 *bit [3:2]: valid when bit1 == 0;
 *0, auto skip nal before first vps/sps/pps/idr;
 *1, auto skip nal before first vps/sps/pps
 *2, auto skip nal before first  vps/sps/pps,
 *	and not decode until the first I slice (with slice address of 0)
 *
 *3, auto skip before first I slice (nal_type >=16 && nal_type <= 21)
 *bit [15:4] nal skip count (valid when bit0 == 1 (manual mode) )
 *bit [16]: for NAL_UNIT_EOS when bit0 is 0:
 *	0, send SEARCH_DONE to arm ;  1, do not send SEARCH_DONE to arm
 *bit [17]: for NAL_SEI when bit0 is 0:
 *	0, do not parse/fetch SEI in ucode;
 *	1, parse/fetch SEI in ucode
 *bit [18]: for NAL_SEI_SUFFIX when bit0 is 0:
 *	0, do not fetch NAL_SEI_SUFFIX to aux buf;
 *	1, fetch NAL_SEL_SUFFIX data to aux buf
 *bit [19]:
 *	0, parse NAL_SEI in ucode
 *	1, fetch NAL_SEI to aux buf
 *bit [20]: for DOLBY_VISION_META
 *	0, do not fetch DOLBY_VISION_META to aux buf
 *	1, fetch DOLBY_VISION_META to aux buf
 */
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
	/*read only*/
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
	/*
	[15 : 8] rps_set_id
	[7 : 0] start_decoding_flag
	*/
#define HEVC_DECODE_INFO       HEVC_ASSIST_SCRATCH_1
	/*set before start decoder*/
#define HEVC_DECODE_MODE		HEVC_ASSIST_SCRATCH_J
#define HEVC_DECODE_MODE2		HEVC_ASSIST_SCRATCH_H
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4

//#define MAX_INT 0x7FFFFFFF

#define RPM_BUF_SIZE ((RPM_END - RPM_BEGIN)*2)
/* non mmu mode lmem size : 0x400, mmu mode : 0x500*/
#define LMEM_BUF_SIZE (0x500 * 2)

typedef struct buff_s {
	u32 buf_start;
	u32 buf_size;
	u32 buf_end;
} buff_t;

typedef struct BuffInfo_s {
    uint32_t max_width;
    uint32_t max_height;
    uint32_t start_adr;
    uint32_t end_adr;
    buff_t ipp;
    buff_t sao_abv;
    buff_t sao_vb;
    buff_t short_term_rps;
    buff_t rcs;
    buff_t ref_list;
    buff_t sps;
    buff_t subpics_info;
    buff_t pps;
    buff_t apsalf;
    buff_t apslmcs;
    buff_t slice_info;
    buff_t coeff_hold;
    buff_t entrop_context;
    buff_t sbac_top;
    buff_t sao_up;
    buff_t swap_buf;
    buff_t swap_buf2;
    buff_t scalelut;
    buff_t dblk_para;
    buff_t dblk_data;
    buff_t dblk_data2;
#ifdef VVC_10B_MMU
    buff_t mmu_vbh;
    buff_t cm_header;
#endif
#ifdef VVC_10B_MMU_DW
    buff_t mmu_vbh_dw;
    buff_t cm_header_dw;
#endif
    buff_t mpred_above;
    buff_t mpred_mv;
    buff_t rpm;
    buff_t lmem;
} BuffInfo_t;

/*mmu_vbh buf is used by HEVC_SAO_MMU_VH0_ADDR, HEVC_SAO_MMU_VH1_ADDR*/
#define VBH_BUF_SIZE_1080P 0x3000
#define VBH_BUF_SIZE_4K 0x5000
#define VBH_BUF_SIZE_8K 0xa000
#define VBH_BUF_SIZE(bufspec) (bufspec->mmu_vbh.buf_size / 2)
	/*mmu_vbh_dw buf is used by HEVC_SAO_MMU_VH0_ADDR2,HEVC_SAO_MMU_VH1_ADDR2,
		HEVC_DW_VH0_ADDDR, HEVC_DW_VH1_ADDDR*/
#define DW_VBH_BUF_SIZE_1080P (VBH_BUF_SIZE_1080P * 2)
#define DW_VBH_BUF_SIZE_4K (VBH_BUF_SIZE_4K * 2)
#define DW_VBH_BUF_SIZE_8K (VBH_BUF_SIZE_8K * 2)
#define DW_VBH_BUF_SIZE(bufspec) (bufspec->mmu_vbh_dw.buf_size / 4)

/* necessary 4K page size align for t7/t3 decoder and after */
#define WORKBUF_ALIGN(addr) (ALIGN(addr, PAGE_SIZE))

#define WORK_BUF_SPEC_NUM 3
static struct BuffInfo_s amvh266_workbuff_spec[WORK_BUF_SPEC_NUM] = {
    { //8M bytes
        .max_width = 1920,
        .max_height = 1088,
        .ipp = {
            // IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
            .buf_size = 0x4000,
        },
        .sao_abv = {
            .buf_size = 0x30000,
        },
        .sao_vb = {
            .buf_size = 0x30000,
        },
        .short_term_rps = {
            // SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
            .buf_size = 0x800,
        },
        .rcs = {
            // RLP STORE AREA - Max 64 L0 + 64 L1 RLP, each has 32 bytes, total 0x1000 bytes
            .buf_size = 0x1000,
        },
#if 0
        .ref_list = {
            // ref_list STOREAREA
	    // (seq_parameter_set_id * 64 +1)(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x40100(16 x 16kBytes+256) -- use 0x40800
	    // 32 ref_entries map :
	    //   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	    //   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
            .buf_size = REF_LIST_BUF_SIZE,
        },
#endif
        .sps = {
            // SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
            .buf_size = 0x800,
        },
        .subpics_info = {
	    // must after sps sps_addr+0x800 for this addr
            // Eech sps has 1024*4 Bytes data, total 16 sps, need 64k Bytes ( 0x10000)
            .buf_size = 0x10000,
        },
        .pps = {
            // PPS STORE AREA - Max 64 PPS, each has 0x100 bytes (0x40(PPS)+0x40(TILE_COL)+0x40(TILE_ROW)+0x40(Reserved)), total 0x4000 bytes
            .buf_size = 0x4000,
        },
        .apsalf = {
	    // must after pps, microcode will use pps_addr+0x2000 for this addr
            // APS ALF STORE AREA - Max 8 sets ALF Filter, each has 576 bytes, use 1024 (0x400) Bytes per Filter, Total 8k Bytes (0x2000)
            .buf_size = 0x2000,
        },
        .apslmcs = {
	    // must after pps and apsalf, microcode will use pps_addr+0x2000+0x2000 for this addr
            // APS LMCS STORE AREA - Max 8 sets LMCS pamater, each has 36 bytes, use 64 (0x40) Bytes per set, Total 512 Bytes (0x200), use 0x800 to make sure no buff across 4k
            .buf_size = 0x800,
        },
        .slice_info = {
	    // must after pps and apsalf and apslmcs, microcode will use pps_addr+0x2000+0x2000+0x800 for this addr
            // Eech pps has 1024*4 Bytes data, total 64 pps, need 256k Bytes ( 0x40000)
            .buf_size = 0x40000,
        },
        .coeff_hold = {
	    // share with AV1_GMC_PARAM_BUFF_ADDR
            // Hold extra coeff hold before sent to iqit -- Need 16k
            .buf_size = 0x4000,
        },
        .entrop_context = {
            // Entrop context STORE AREA - used when sps_entropy_coding_sync_enabled_flag active  -- Size 384x31 bits ( Use 512x32 bits, 2k Bytes)
            .buf_size = 0x0800,
        },
        .sbac_top = {
            // DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
            .buf_size = 0x2000,
        },
        .sao_up = {
            // SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
            .buf_size = 0x2800,
        },
        .swap_buf = {
            // 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
            .buf_size = 0x800,
        },
        .swap_buf2 = {
            .buf_size = 0x800,
        },
        .scalelut = {
	    // VVC need 8 * 1400 = 11200 (0x2BC0). use 0x4000
            .buf_size = 0x4000,
        },
        .dblk_para  = { .buf_size = 0x40000, },
        .dblk_data  = { .buf_size = 0x80000, },
        .dblk_data2 = { .buf_size = 0x80000, },
#ifdef VVC_10B_MMU
        .mmu_vbh = {
          //.buf_size = 0x5000, //2*16*2304/4, 4K
          .buf_size = 0xa000, //2*16*2304/4*2, 8K
        },
        //.cm_header = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
#ifdef VVC_10B_MMU_DW
        .mmu_vbh_dw = {
          //.buf_size = 0x5000, //2*16*2304/4, 4K
          .buf_size = 0xa000, //2*16*2304/4*2, 8K
        },
        //.cm_header_dw = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE_DW*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
        .mpred_above = {
            .buf_size = 0x8000,
        },
#if 0
        .mpred_mv = {
           .buf_size = 0x40000*VVC_BUFFER_NUM, //1080p, 0x40000 per buffer
        },
#endif
        .rpm = {
           .buf_size = 0x600*2,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}

    },
    {
        .max_width = 4096*2,
        .max_height = 2304*2,
        .ipp = {
            // IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
            .buf_size = 0x4000*2,
        },
        .sao_abv = {
            .buf_size = 0x30000*2,
        },
        .sao_vb = {
            .buf_size = 0x30000*2,
        },
        .short_term_rps = {
            // SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
            .buf_size = 0x800,
        },
        .rcs = {
            // RLP STORE AREA - Max 64 L0 + 64 L1 RLP, each has 32 bytes, total 0x1000 bytes
            .buf_size = 0x1000,
        },
#if 0
        .ref_list = {
            // ref_list STOREAREA
	    // (seq_parameter_set_id * 64 +1)(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x40100(16 x 16kBytes+256) -- use 0x40800
	    // 32 ref_entries map :
	    //   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	    //   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
            .buf_size = REF_LIST_BUF_SIZE,
        },
#endif
        .sps = {
            // SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
            .buf_size = 0x800,
        },
        .subpics_info = {
	    // must after sps sps_addr+0x800 for this addr
            // Eech sps has 1024*4 Bytes data, total 16 sps, need 64k Bytes ( 0x10000)
            .buf_size = 0x10000,
        },
        .pps = {
            // PPS STORE AREA - Max 64 PPS, each has 0x100 bytes (0x40(PPS)+0x40(TILE_COL)+0x40(TILE_ROW)+0x40(Reserved)), total 0x4000 bytes
            .buf_size = 0x4000,
        },
        .apsalf = {
	    // must after pps, microcode will use pps_addr+0x2000 for this addr
            // APS ALF STORE AREA - Max 8 sets iALF Filter, each has 576 bytes, use 1024 (0x400) Bytes per Filter, Total 8k Bytes (0x2000)
            .buf_size = 0x2000,
        },
        .apslmcs = {
	    // must after pps and apsalf, microcode will use pps_addr+0x2000+0x2000 for this addr
            // APS LMCS STORE AREA - Max 8 sets LMCS pamater, each has 36 bytes, use 64 (0x40) Bytes per set, Total 512 Bytes (0x200), use 0x800 to make sure no buff across 4k
            .buf_size = 0x800,
        },
        .slice_info = {
	    // must after pps and apsalf and apslmcs, microcode will use pps_addr+0x2000+0x2000+0x800 for this addr
            // Eech pps has 1024*4 Bytes data, total 64 pps, need 256k Bytes ( 0x40000)
            .buf_size = 0x40000,
        },
        .coeff_hold = {
	    // share with AV1_GMC_PARAM_BUFF_ADDR
            // Hold extra coeff hold before sent to iqit -- Need 16k
            .buf_size = 0x4000,
        },
        .entrop_context = {
            // Entrop context STORE AREA - used when sps_entropy_coding_sync_enabled_flag active  -- Size 384x31 bits ( Use 512x32 bits, 2k Bytes)
            .buf_size = 0x0800,
        },
        .sbac_top = {
            // DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
            .buf_size = 0x2000*2,
        },
        .sao_up = {
            // SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
            .buf_size = 0x2800*2,
        },
        .swap_buf = {
            // 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
            .buf_size = 0x800,
        },
        .swap_buf2 = {
            .buf_size = 0x800,
        },
        .scalelut = {
	    // VVC need 8 * 1400 = 11200 (0x2BC0). use 0x4000
            .buf_size = 0x4000*2,
        },
        .dblk_para  = { .buf_size = 0x40000*2, },
        .dblk_data  = { .buf_size = 0x80000*2, },
        .dblk_data2 = { .buf_size = 0x80000*2, },
#ifdef VVC_10B_MMU
        .mmu_vbh = {
          //.buf_size = 0x5000*2, //2*16*2304/4, 4K
          .buf_size = 0xa000*2, //2*16*2304/4*2, 8K
        },
        //.cm_header = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
#ifdef VVC_10B_MMU_DW
        .mmu_vbh_dw = {
          //.buf_size = 0x5000*2, //2*16*2304/4, 4K
          .buf_size = 0xa000*2, //2*16*2304/4*2, 8K
        },
        //.cm_header_dw = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE_DW*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
        .mpred_above = {
            .buf_size = 0x8000*2,
        },
#if 0
        .mpred_mv = {
           .buf_size = 0x100000*VVC_BUFFER_NUM*4, //4k2k , 0x100000 per buffer
        },
#endif
        .rpm = {
           .buf_size = 0x600*2,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
    },
    {
        .max_width = 4096,
        .max_height = 2304,
        .ipp = {
            // IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
            .buf_size = 0x4000,
        },
        .sao_abv = {
            .buf_size = 0x30000,
        },
        .sao_vb = {
            .buf_size = 0x30000,
        },
        .short_term_rps = {
            // SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
            .buf_size = 0x800,
        },
        .rcs = {
            // RLP STORE AREA - Max 64 L0 + 64 L1 RLP, each has 32 bytes, total 0x1000 bytes
            .buf_size = 0x1000,
        },
#if 0
        .ref_list = {
            // ref_list STOREAREA
	    // (seq_parameter_set_id * 64 +1)(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x40100(16 x 16kBytes+256) -- use 0x40800
	    // 32 ref_entries map :
	    //   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	    //   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
            .buf_size = REF_LIST_BUF_SIZE,
        },
#endif
        .sps = {
            // SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
            .buf_size = 0x800,
        },
        .subpics_info = {
	    // must after sps sps_addr+0x800 for this addr
            // Eech sps has 1024*4 Bytes data, total 16 sps, need 64k Bytes ( 0x10000)
            .buf_size = 0x10000,
        },
        .pps = {
            // PPS STORE AREA - Max 64 PPS, each has 0x100 bytes (0x40(PPS)+0x40(TILE_COL)+0x40(TILE_ROW)+0x40(Reserved)), total 0x4000 bytes
            .buf_size = 0x4000,
        },
        .apsalf = {
	    // must after pps, microcode will use pps_addr+0x2000 for this addr
            // APS ALF STORE AREA - Max 8 sets iALF Filter, each has 576 bytes, use 1024 (0x400) Bytes per Filter, Total 8k Bytes (0x2000)
            .buf_size = 0x2000,
        },
        .apslmcs = {
	    // must after pps and apsalf, microcode will use pps_addr+0x2000+0x2000 for this addr
            // APS LMCS STORE AREA - Max 8 sets LMCS pamater, each has 36 bytes, use 64 (0x40) Bytes per set, Total 512 Bytes (0x200), use 0x800 to make sure no buff across 4k
            .buf_size = 0x800,
        },
        .slice_info = {
	    // must after pps and apsalf and apslmcs, microcode will use pps_addr+0x2000+0x2000+0x800 for this addr
            // Eech pps has 1024*4 Bytes data, total 64 pps, need 256k Bytes ( 0x40000)
            .buf_size = 0x40000,
        },
        .coeff_hold = {
	    // share with AV1_GMC_PARAM_BUFF_ADDR
            // Hold extra coeff hold before sent to iqit -- Need 16k
            .buf_size = 0x4000,
        },
        .entrop_context = {
            // Entrop context STORE AREA - used when sps_entropy_coding_sync_enabled_flag active  -- Size 384x31 bits ( Use 512x32 bits, 2k Bytes)
            .buf_size = 0x0800,
        },
        .sbac_top = {
            // DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
            .buf_size = 0x2000,
        },
        .sao_up = {
            // SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
            .buf_size = 0x2800,
        },
        .swap_buf = {
            // 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
            .buf_size = 0x800,
        },
        .swap_buf2 = {
            .buf_size = 0x800,
        },
        .scalelut = {
	    // VVC need 8 * 1400 = 11200 (0x2BC0). use 0x4000
            .buf_size = 0x4000,
        },
        .dblk_para  = { .buf_size = 0x40000, },
        .dblk_data  = { .buf_size = 0x80000, },
        .dblk_data2 = { .buf_size = 0x80000, },
#ifdef VVC_10B_MMU
        .mmu_vbh = {
          //.buf_size = 0x5000, //2*16*2304/4, 4K
          .buf_size = 0xa000, //2*16*2304/4*2, 8K
        },
        //.cm_header = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
#ifdef VVC_10B_MMU_DW
        .mmu_vbh_dw = {
          //.buf_size = 0x5000, //2*16*(more than 2304)/4, 4K
          .buf_size = 0xa000, //2*16*2304/4*2, 8K
        },
        //.cm_header_dw = {
        //  .buf_size = MMU_COMPRESS_HEADER_SIZE_DW*VVC_BUFFER_NUM, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
        //},
#endif
        .mpred_above = {
            .buf_size = 0x8000,
        },
#if 0
        .mpred_mv = {
           .buf_size = 0x100000*VVC_BUFFER_NUM, //4k2k , 0x100000 per buffer
        },
#endif
        .rpm = {
           .buf_size = 0x600*2,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
    }
};

static int init_buff_spec(struct hevc_state_s *hevc,
	struct BuffInfo_s *buf_spec)
{
	buf_spec->ipp.buf_start =
		WORKBUF_ALIGN(buf_spec->start_adr);
	buf_spec->sao_abv.buf_start =
		WORKBUF_ALIGN(buf_spec->ipp.buf_start + buf_spec->ipp.buf_size);
	buf_spec->sao_vb.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_abv.buf_start + buf_spec->sao_abv.buf_size);
	buf_spec->short_term_rps.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_vb.buf_start + buf_spec->sao_vb.buf_size);
	buf_spec->rcs.buf_start =
		WORKBUF_ALIGN(buf_spec->short_term_rps.buf_start + buf_spec->short_term_rps.buf_size);
	buf_spec->ref_list.buf_start =
		WORKBUF_ALIGN(buf_spec->rcs.buf_start + buf_spec->rcs.buf_size);
	buf_spec->sps.buf_start =
		WORKBUF_ALIGN(buf_spec->ref_list.buf_start + buf_spec->ref_list.buf_size);
	buf_spec->subpics_info.buf_start =
		WORKBUF_ALIGN(buf_spec->sps.buf_start + buf_spec->sps.buf_size);
	buf_spec->pps.buf_start =
		WORKBUF_ALIGN(buf_spec->subpics_info.buf_start + buf_spec->subpics_info.buf_size);
	buf_spec->apsalf.buf_start =
		WORKBUF_ALIGN(buf_spec->pps.buf_start + buf_spec->pps.buf_size);
	buf_spec->apslmcs.buf_start =
		WORKBUF_ALIGN(buf_spec->apsalf.buf_start + buf_spec->apsalf.buf_size);
	buf_spec->slice_info.buf_start =
		WORKBUF_ALIGN(buf_spec->apslmcs.buf_start + buf_spec->apslmcs.buf_size);
	buf_spec->coeff_hold.buf_start =
		WORKBUF_ALIGN(buf_spec->slice_info.buf_start + buf_spec->slice_info.buf_size);
	buf_spec->entrop_context.buf_start =
		WORKBUF_ALIGN(buf_spec->coeff_hold.buf_start + buf_spec->coeff_hold.buf_size);
	buf_spec->sbac_top.buf_start =
		WORKBUF_ALIGN(buf_spec->entrop_context.buf_start + buf_spec->entrop_context.buf_size);
	buf_spec->sao_up.buf_start =
		WORKBUF_ALIGN(buf_spec->sbac_top.buf_start + buf_spec->sbac_top.buf_size);
	buf_spec->swap_buf.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_up.buf_start + buf_spec->sao_up.buf_size);
	buf_spec->swap_buf2.buf_start =
		WORKBUF_ALIGN(buf_spec->swap_buf.buf_start + buf_spec->swap_buf.buf_size);
	buf_spec->scalelut.buf_start =
		WORKBUF_ALIGN(buf_spec->swap_buf2.buf_start + buf_spec->swap_buf2.buf_size);
	buf_spec->dblk_para.buf_start =
		WORKBUF_ALIGN(buf_spec->scalelut.buf_start + buf_spec->scalelut.buf_size);
	buf_spec->dblk_data.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_para.buf_start + buf_spec->dblk_para.buf_size);
	buf_spec->dblk_data2.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size);
	buf_spec->mmu_vbh.buf_start  =
		WORKBUF_ALIGN(buf_spec->dblk_data2.buf_start + buf_spec->dblk_data2.buf_size);
#ifdef VVC_10B_MMU_DW
	buf_spec->mmu_vbh_dw.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size);
	//buf_spec->cm_header_dw.buf_start =
	//	WORKBUF_ALIGN(buf_spec->mmu_vbh_dw.buf_start + buf_spec->mmu_vbh_dw.buf_size);
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh_dw.buf_start + buf_spec->mmu_vbh_dw.buf_size);
#else
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size);
#endif
#ifdef MV_USE_FIXED_BUF
	buf_spec->mpred_mv.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_above.buf_start + buf_spec->mpred_above.buf_size);
	buf_spec->rpm.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_mv.buf_start + buf_spec->mpred_mv.buf_size);
#else
	buf_spec->rpm.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_above.buf_start + buf_spec->mpred_above.buf_size);
#endif
	buf_spec->lmem.buf_start =
		WORKBUF_ALIGN(buf_spec->rpm.buf_start + buf_spec->rpm.buf_size);
	buf_spec->end_adr =
		WORKBUF_ALIGN(buf_spec->lmem.buf_start + buf_spec->lmem.buf_size);

	if (hevc && get_dbg_flag2(hevc)) {
		hevc_print(hevc, 0,
			"%s workspace (%x %x) size = %x\n", __func__,
			buf_spec->start_adr, buf_spec->end_adr,
			buf_spec->end_adr - buf_spec->start_adr);

		hevc_print(hevc, 0,
			"ipp.buf_start             :%x\n",
			buf_spec->ipp.buf_start);
		hevc_print(hevc, 0,
			"sao_abv.buf_start          :%x\n",
			buf_spec->sao_abv.buf_start);
		hevc_print(hevc, 0,
			"sao_vb.buf_start          :%x\n",
			buf_spec->sao_vb.buf_start);
		hevc_print(hevc, 0,
			"short_term_rps.buf_start  :%x\n",
			buf_spec->short_term_rps.buf_start);
		hevc_print(hevc, 0,
			"rcs.buf_start  :%x\n",
			buf_spec->rcs.buf_start);
		hevc_print(hevc, 0,
			"ref_list.buf_start  :%x\n",
			buf_spec->ref_list.buf_start);
		hevc_print(hevc, 0,
			"sps.buf_start             :%x\n",
			buf_spec->sps.buf_start);
		hevc_print(hevc, 0,
			"subpics_info.buf_start             :%x\n",
			buf_spec->subpics_info.buf_start);
		hevc_print(hevc, 0,
			"pps.buf_start             :%x\n",
			buf_spec->pps.buf_start);
		hevc_print(hevc, 0,
			"apsalf.buf_start             :%x\n",
			buf_spec->apsalf.buf_start);
		hevc_print(hevc, 0,
			"apslmcs.buf_start             :%x\n",
			buf_spec->apslmcs.buf_start);
		hevc_print(hevc, 0,
			"slice_info.buf_start             :%x\n",
			buf_spec->slice_info.buf_start);
		hevc_print(hevc, 0,
			"coeff_hold.buf_start             :%x\n",
			buf_spec->coeff_hold.buf_start);
		hevc_print(hevc, 0,
			"entrop_context.buf_start             :%x\n",
			buf_spec->entrop_context.buf_start);
		hevc_print(hevc, 0,
			"sbac_top.buf_start             :%x\n",
			buf_spec->sbac_top.buf_start);
		hevc_print(hevc, 0,
			"sao_up.buf_start          :%x\n",
			buf_spec->sao_up.buf_start);
		hevc_print(hevc, 0,
			"swap_buf.buf_start        :%x\n",
			buf_spec->swap_buf.buf_start);
		hevc_print(hevc, 0,
			"swap_buf2.buf_start       :%x\n",
			buf_spec->swap_buf2.buf_start);
		hevc_print(hevc, 0,
			"scalelut.buf_start        :%x\n",
			buf_spec->scalelut.buf_start);
		hevc_print(hevc, 0,
			"dblk_para.buf_start       :%x\n",
			buf_spec->dblk_para.buf_start);
		hevc_print(hevc, 0,
			"dblk_data.buf_start       :%x\n",
			buf_spec->dblk_data.buf_start);
		hevc_print(hevc, 0,
			"dblk_data2.buf_start       :%x\n",
			buf_spec->dblk_data2.buf_start);
		hevc_print(hevc, 0,
			"mmu_vbh.buf_start       :%x\n",
			buf_spec->mmu_vbh.buf_start);
#ifdef VVC_10B_MMU_DW
		hevc_print(hevc, 0,
			"mmu_vbh_dw.buf_start       :%x\n",
			buf_spec->mmu_vbh_dw.buf_start);
#endif
		hevc_print(hevc, 0,
			"mpred_above.buf_start     :%x\n",
			buf_spec->mpred_above.buf_start);
#ifdef MV_USE_FIXED_BUF
		hevc_print(hevc, 0,
			"mpred_mv.buf_start        :%x\n",
			  buf_spec->mpred_mv.buf_start);
#endif
		if ((get_dbg_flag2(hevc) & H266_DEBUG_SEND_PARAM_WITH_REG) == 0) {
			hevc_print(hevc, 0, "rpm.buf_start             :%x\n", buf_spec->rpm.buf_start);
		}
		hevc_print(hevc, 0,
			"lmem.buf_start        :%x\n",
			  buf_spec->lmem.buf_start);
	}
	if (hevc && (work_buf_size > 0) && (buf_spec->end_adr - buf_spec->start_adr > work_buf_size)) {
		hevc_print(hevc, 0, "Error, buf_spec workspace size 0x%x is larger than allocated size 0x%x\n",
			buf_spec->end_adr - buf_spec->start_adr, work_buf_size);
		return -1;
	}
	return 0;
}

/*USE_BUF_BLOCK*/
struct BUF_s {
	ulong	start_adr;
	u32	size;
	u32	luma_size;
	ulong	header_addr;
	u32 	header_size;
	int	used_flag;
	ulong	v4l_ref_buf_addr;
	ulong	chroma_addr;
	u32	chroma_size;
} /*BUF_t */;

#define SEI_MASTER_DISPLAY_COLOR_MASK 0x00000001
#define SEI_CONTENT_LIGHT_LEVEL_MASK  0x00000002
#define SEI_HDR10PLUS_MASK			  0x00000004
#define SEI_HDR_CUVA_MASK	      0x00000008
#define SEI_HDR_FMM_MASK	      0x00000010

#define VF_POOL_SIZE        32

#ifdef MULTI_INSTANCE_SUPPORT
#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_CONFIG_PARAM     3
#define DEC_RESULT_ERROR            4
#define DEC_INIT_PICLIST			5
#define DEC_UNINIT_PICLIST			6
#define DEC_RESULT_GET_DATA         7
#define DEC_RESULT_GET_DATA_RETRY   8
#define DEC_RESULT_EOS              9
#define DEC_RESULT_FORCE_EXIT       10
#define DEC_RESULT_FREE_CANVAS      11
#define DEC_RESULT_ERROR_DATA      	12
#define DEC_RESULT_UNFINISH	        14

static void vh266_work(struct work_struct *work);
static void vh266_timeout_work(struct work_struct *work);
static void vh266_notify_work(struct work_struct *work);

#endif

struct debug_log_s {
	struct list_head list;
	uint8_t data; /*will alloc more size*/
};

#define H266_USERDATA_ENABLE

#ifdef H266_USERDATA_ENABLE

static u32 itu_t_t35_enable = 1;

struct h266_userdata_record_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};
/*
struct h266_ud_record_wait_node_t {
	struct list_head list;
	struct mh264_userdata_record_t ud_record;
};*/
#define USERDATA_FIFO_NUM    256
#define MAX_FREE_USERDATA_NODES		5

struct h266_userdata_info_t {
	struct h266_userdata_record_t records[USERDATA_FIFO_NUM];
	u8 *data_buf;
	u8 *data_buf_end;
	u32 buf_len;
	u32 read_index;
	u32 write_index;
	u32 last_wp;
};
#endif

struct mh266_fence_vf_t {
	u32 used_size;
	struct vframe_s *fence_vf[VF_POOL_SIZE];
};

struct afbc_buf {
	ulong fb;
	int   used;
};

struct hevc_state_s {
	vvc_decoder_t g_vvc_dec;
	vvc_decoder_t *vvc_dec;

#ifdef MULTI_INSTANCE_SUPPORT
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *, int);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	u32 timeout_processing;
	struct work_struct work;
	struct work_struct timeout_work;
	struct work_struct notify_work;
	struct work_struct set_clk_work;
	/* timeout handle */
	unsigned long int start_process_time;
	unsigned int last_lcu_idx;
	unsigned int decode_timeout_count;
	unsigned int timeout_num;
	unsigned char rps_set_id;
	unsigned char eos;
	int pic_decoded_lcu_idx;
	u8 over_decode;
	u8 empty_flag;
#endif
	struct vframe_s vframe_dummy;
	char *provider_name;
	int index;
	struct device *cma_dev;
	unsigned char m_ins_flag;
	unsigned char dolby_enhance_flag;
	unsigned long buf_start;
	u32 buf_size;
	u32 mv_buf_size;
	u32 curr_pic_offset;
	struct BuffInfo_s work_space_buf_store;
	struct BuffInfo_s *work_space_buf;

	u8 aux_data_dirty;
	u32 prefix_aux_size;
	u32 suffix_aux_size;
	void *aux_addr;
	void *rpm_addr;
	void *lmem_addr;
    void *ref_list_buffer_addr;
	dma_addr_t aux_phy_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;
    dma_addr_t ref_list_buffer_phy_addr;

	unsigned int use_cma_flag;

	unsigned short *rpm_ptr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;
	int debug_ptr_size;
	int pic_w;
	int pic_h;
	int lcu_x_num;
	int lcu_y_num;
	int lcu_total;
	int lcu_size;
	int lcu_size_log2;
	int lcu_x_num_pre;
	int lcu_y_num_pre;
	int first_pic_after_recover;
	u32 sar_width;
	u32 sar_height;

	int num_tile_col;
	int num_tile_row;
	int tile_enabled;
	int tile_x;
	int tile_y;
	int tile_y_x;
	int tile_start_lcu_x;
	int tile_start_lcu_y;
	int tile_width_lcu;
	int tile_height_lcu;

	int slice_type;
	unsigned int slice_addr;
	unsigned int slice_segment_addr;

	unsigned char interlace_flag;
	unsigned char curr_pic_struct;
	unsigned char frame_field_info_present_flag;

	unsigned short sps_num_reorder_pics_0;
	unsigned short misc_flag0;
	int m_temporalId;
	int m_nalUnitType;
	int TMVPFlag;
	int isNextSliceSegment;
	int LDCFlag;
	int m_pocRandomAccess;
	int plevel;
	int MaxNumMergeCand;

	int new_pic;
	int new_tile;
	//int curr_POC;
	int iPrevPOC;
	int iPrevTid0POC;
	int list_no;
	int RefNum_L0;
	int RefNum_L1;
	int ColFromL0Flag;
	int LongTerm_Curr;
	int LongTerm_Col;
	int Col_POC;
	int LongTerm_Ref;
	//struct PIC_s *cur_pic;
	struct PIC_s *col_pic;
	int skip_flag;
	int decode_idx;
	int slice_idx;
	unsigned char have_vps;
	unsigned char have_sps;
	unsigned char have_pps;
	unsigned char have_valid_start_slice;
	unsigned char wait_buf;
	unsigned char error_flag;
	unsigned int error_skip_nal_count;
	long used_4k_num;

	unsigned char
	ignore_bufmgr_error;	/* bit 0, for decoding;
			bit 1, for displaying
			bit 1 must be set if bit 0 is 1*/
	int PB_skip_mode;
	int PB_skip_count_after_decoding;
#ifdef SUPPORT_10BIT
	int mem_saving_mode;
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	int pts_mode;
	int last_lookup_pts;
	int last_pts;
	u64 last_lookup_pts_us64;
	u64 last_pts_us64;
	u32 shift_byte_count_lo;
	u32 shift_byte_count_hi;
	int pts_mode_switching_count;
	int pts_mode_recovery_count;

	int pic_num;

	struct timer_list timer;
	struct BUF_s m_BUF[BUF_POOL_SIZE];
	struct BUF_s m_mv_BUF[MAX_REF_PIC_NUM];
	//struct PIC_s *m_PIC[MAX_REF_PIC_NUM];

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];

	u32 stat;
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_ar;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
	u32 video_signal_type;
	u32 video_signal_type_debug;
	u32 saved_resolution;
	bool get_frame_dur;
	u32 error_skip_nal_wt_cnt;

#ifdef DEBUG_PTS
	unsigned long pts_missed;
	unsigned long pts_hit;
#endif
	struct dec_sysinfo vh266_amstream_dec_info;
	unsigned char init_flag;
	unsigned char first_sc_checked;
	unsigned char uninit_list_done;
	u32 start_decoding_time;

	int show_frame_num;
	int fatal_error;

	u32 sei_hdr10_flag;
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
	unsigned int mmu_mc_buf_start;
	unsigned int mmu_mc_buf_end;
	unsigned int mmu_mc_start_4k_adr;
	void *mmu_box;
	void *bmmu_box;
	int mmu_enable;
#ifdef VVC_10B_MMU_DW
	void *frame_dw_mmu_map_addr;
	dma_addr_t frame_dw_mmu_map_phy_addr;
	void *mmu_box_dw;
	int dw_mmu_enable;
#endif

	unsigned int dec_status;

	/* data for SEI_MASTER_DISPLAY_COLOR */
	unsigned int primaries[3][2];
	unsigned int white_point[2];
	unsigned int luminance[2];
	/* data for SEI_CONTENT_LIGHT_LEVEL */
	unsigned int content_light_level[2];

	struct PIC_s *pre_top_pic;
	struct PIC_s *pre_bot_pic;

#ifdef MULTI_INSTANCE_SUPPORT
	int double_write_mode;
	int dynamic_buf_num_margin;
	int start_action;
	int save_buffer_mode;
#endif
	u32 i_only;
	struct list_head log_list;
	u32 ucode_pause_pos;
	u32 start_shift_bytes;

	u32 vf_pre_count;
	atomic_t vf_get_count;
	atomic_t vf_put_count;
#ifdef SWAP_HEVC_UCODE
	dma_addr_t mc_dma_handle;
	void *mc_cpu_addr;
	int swap_size;
	ulong swap_addr;
#endif
	u8 head_error_flag;
	int valve_count;
	struct firmware_s *fw;
	int max_pic_w;
	int max_pic_h;
#ifdef AGAIN_HAS_THRESHOLD
	u8 next_again_flag;
	u32 pre_parser_wr_ptr;
#endif
	u32 ratio_control;
	u32 first_pic_flag;
	u32 decode_size;
	struct mutex chunks_mutex;
	int need_cache_size;
	u64 sc_start_time;
	u32 skip_nal_count;
	bool is_swap;
	bool is_4k;

	int frameinfo_enable;
	struct vframe_qos_s vframe_qos;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	u32 mem_map_mode;
	u32 performance_profile;
	struct vdec_info *gvs;
	bool ip_mode;
	u32 kpi_first_i_coming;
	u32 kpi_first_i_decoded;
	int sidebind_type;
	int sidebind_channel_id;
	u32 pre_parser_video_rp;
	u32 pre_parser_video_wp;
	bool dv_duallayer;
	u32 poc_error_count;
	u32 timeout_flag;
	ulong timeout;
	bool discard_dv_data;
	bool enable_fence;
	int fence_usage;
	int buffer_wrap[MAX_REF_PIC_NUM];
	int low_latency_flag;
	u32 metadata_config_flag;
	int last_width;
	int last_height;
	int used_buf_num;
	u32 dirty_shift_flag;
	u32 endian;
	ulong fb_token;
	int dec_again_cnt;
	struct mh266_fence_vf_t fence_vf_s;
	struct mutex fence_mutex;
	dma_addr_t rdma_phy_adr;
	unsigned *rdma_adr;
	struct trace_decoder_name trace;
	int nal_skip_policy;
	bool high_bandwidth_flag;
	bool resolution_change;

	char *hdr10p_data_buf[BUF_POOL_SIZE];
	int crop_w;
	int crop_h;
	bool enable_ucode_swap;
	int slice_count;
	ulong aux_mem_handle;
	ulong rpm_mem_handle;
	ulong lmem_phy_handle;
    ulong ref_list_buffer_phy_handle;
	ulong frame_mmu_map_handle;
	ulong frame_dw_mmu_map_handle;
	ulong mc_cpu_handle;
	ulong det_buf_handle;
	ulong rdma_mem_handle;
#ifdef H266_USERDATA_ENABLE
	/*user data*/
	struct mutex userdata_mutex;
	struct h266_userdata_info_t userdata_info;
	struct h266_userdata_record_t ud_record;
	int wait_for_udr_send;

	/* buffer for storing one itu35 recored */
	void *sei_itu_data_buf;
	u32 sei_itu_data_len;
	/* recycle buffer for user data storing all itu35 records */
	void *sei_user_data_buffer;
	u32 sei_user_data_wp;
	struct work_struct user_data_ready_work;
#endif
	u32 data_size;
	u32 data_offset;
	u32 data_invalid;
	u32 consume_byte;
	u32 muti_frame_flag;
	char *aux_data_buf[BUF_POOL_SIZE];
#define PROCESS_STATE_INIT 0
#define PROCESS_STATE_DECODING 1
#define PROCESS_STATE_HEAD_AGAIN 2
#define PROCESS_STATE_DECODE_AGAIN 3
	u8 process_state;
	u32 dv_profile;
	s32 cur_idx;
	struct aml_buf *aml_buf;
	struct afbc_buf afbc_buf_table[BUF_FBC_NUM_MAX];
} /*hevc_stru_t */;

static int get_dbg_flag(struct hevc_state_s * hevc)
{
	return ((debug_mask & (1 << hevc->index)) ? debug : 0);
}

#ifdef AGAIN_HAS_THRESHOLD
static u32 again_threshold;
#endif
#ifdef SEND_LMEM_WITH_RPM
#define get_lmem_params(hevc, ladr) \
	hevc->lmem_ptr[ladr - (ladr & 0x3) + 3 - (ladr & 0x3)]

#define FRAME_MMU_MAP_ALIGNMENT_BITS 6
#define FRAME_MMU_MAP_ALIGNMENT_SIZE (1 << FRAME_MMU_MAP_ALIGNMENT_BITS)
static int get_frame_mmu_map_size(void)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TXHD2))
		return (MAX_FRAME_8K_NUM * 4) + FRAME_MMU_MAP_ALIGNMENT_SIZE;

	return (MAX_FRAME_4K_NUM * 4) + FRAME_MMU_MAP_ALIGNMENT_SIZE;
}

static int is_oversize(int w, int h)
{
	int max = MAX_SIZE_8K;

	if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SM1) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5M))
		max = MAX_SIZE_4K;
	else if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TXHD2))
		max = MAX_SIZE_2K;

	if (w <= 0 || h <= 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

void check_head_error(struct hevc_state_s *hevc)
{
#define pcm_enabled_flag                                  0x040
#define pcm_sample_bit_depth_luma                         0x041
#define pcm_sample_bit_depth_chroma                       0x042
	hevc->head_error_flag = 0;
	if ((error_handle_policy & 0x40) == 0)
		return;
	if (get_lmem_params(hevc, pcm_enabled_flag)) {
		uint16_t pcm_depth_luma = get_lmem_params(
			hevc, pcm_sample_bit_depth_luma);
		uint16_t pcm_sample_chroma = get_lmem_params(
			hevc, pcm_sample_bit_depth_chroma);
		if (pcm_depth_luma >
			hevc->bit_depth_luma ||
			pcm_sample_chroma >
			hevc->bit_depth_chroma) {
			hevc_print(hevc, 0,
				"error, pcm bit depth %d, %d is greater than normal bit depth %d, %d\n",
				pcm_depth_luma,
				pcm_sample_chroma,
				hevc->bit_depth_luma,
				hevc->bit_depth_chroma);
			hevc->head_error_flag = 1;
		}
	}
}
#endif

#ifdef SUPPORT_10BIT
/* Losless compression body buffer size 4K per 64x32 (jt) */
static int compute_losless_comp_body_size(struct hevc_state_s *hevc,
	int width, int height, int mem_saving_mode)
{
	int width_x64;
	int     height_x32;
	int     bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	height_x32 = height + 31;
	height_x32 >>= 5;
	if (mem_saving_mode == 1 && hevc->mmu_enable)
		bsize = 3200 * width_x64 * height_x32;
	else if (mem_saving_mode == 1)
		bsize = 3072 * width_x64 * height_x32;
	else
		bsize = 4096 * width_x64 * height_x32;

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt) */
static int compute_losless_comp_header_size(int width, int height)
{
	int     width_x128;
	int     height_x64;
	int     hsize;

	width_x128 = width + 127;
	width_x128 >>= 7;

	height_x64 = height + 63;
	height_x64 >>= 6;

	hsize = 32 * width_x128 * height_x64;

	return  hsize;
}
#endif

static int add_log(struct hevc_state_s *hevc,
	const char *fmt, ...)
{
#define HEVC_LOG_BUF		196
	struct debug_log_s *log_item;
	unsigned char buf[HEVC_LOG_BUF];
	int len = 0;
	va_list args;
	mutex_lock(&vh266_log_mutex);
	va_start(args, fmt);
	len = sprintf(buf, "<%ld>   <%05d> ", jiffies, hevc->decode_idx);
	len += vsnprintf(buf + len, HEVC_LOG_BUF - len, fmt, args);
	va_end(args);
	log_item = kmalloc(sizeof(struct debug_log_s) + len, GFP_KERNEL);
	if (log_item) {
		INIT_LIST_HEAD(&log_item->list);
		strcpy(&log_item->data, buf);
		list_add_tail(&log_item->list, &hevc->log_list);
	}
	mutex_unlock(&vh266_log_mutex);
	return 0;
}

static void dump_log(struct hevc_state_s *hevc)
{
	int i = 0;
	struct debug_log_s *log_item, *tmp;
	mutex_lock(&vh266_log_mutex);
	list_for_each_entry_safe(log_item, tmp, &hevc->log_list, list) {
		hevc_print(hevc, 0,
			"[LOG%04d]%s\n",
			i++,
			&log_item->data);
		list_del(&log_item->list);
		kfree(log_item);
	}
	mutex_unlock(&vh266_log_mutex);
}

static unsigned char is_skip_decoding(struct hevc_state_s *hevc,
		struct PIC_s *pic)
{
	if (pic->error_mark
		&& ((hevc->ignore_bufmgr_error & 0x1) == 0))
		return 1;
	return 0;
}

static int get_pic_poc(struct hevc_state_s *hevc,
		unsigned int idx)
{
	if (idx != 0xff
		&& idx < MAX_REF_PIC_NUM)
		return hevc->vvc_dec->pic_pool[idx].poc;
	return INVALID_POC;
}

static int get_double_write_mode(struct hevc_state_s *hevc)
{
	unsigned int out;
	u32 dw = 0x1; /*1:1*/

	vdec_v4l_get_dw_mode(hevc->v4l2_ctx, &out);
	dw = out;

	return (dw & 0Xffff);
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static int get_valid_double_write_mode(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((double_write_mode & 0x80000000) == 0)) ?
		get_double_write_mode(hevc) :
		(double_write_mode & 0x7fffffff);
}

int get_dynamic_buf_num_margin(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((dynamic_buf_num_margin & 0x80000000) == 0)) ?
		hevc->dynamic_buf_num_margin :
		(dynamic_buf_num_margin & 0x7fffffff);
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc)
{
	return hevc->index;
}
#endif

#undef pr_info
#define pr_info pr_cont
static int hevc_print(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		512
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
#endif
		va_list args;

		va_start(args, fmt);
		if (hevc)
			len = sprintf(buf, "[%d]", hevc->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		if (flag == 0)
			pr_debug("%s", buf);
		else
			pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	}
#endif
	return 0;
}

static int hevc_print_cont(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
#endif
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	}
#endif
	return 0;
}

#if 0
static void update_vf_memhandle(struct hevc_state_s *hevc,
	struct vframe_s *vf, struct PIC_s *pic);
#endif

static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic);

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic);

#ifdef MULTI_INSTANCE_SUPPORT
static void set_decode_again_state(struct hevc_state_s *hevc)
{
	if (hevc->process_state == PROCESS_STATE_DECODING ||
        hevc->process_state == PROCESS_STATE_DECODE_AGAIN)
        hevc->process_state = PROCESS_STATE_DECODE_AGAIN;
    else
        hevc->process_state = PROCESS_STATE_HEAD_AGAIN;
}
#endif

static void hevc_init_stru(struct hevc_state_s *hevc,
		struct BuffInfo_s *buf_spec_i)
{
	INIT_LIST_HEAD(&hevc->log_list);
	hevc->work_space_buf = buf_spec_i;
	hevc->prefix_aux_size = 0;
	hevc->suffix_aux_size = 0;
	hevc->aux_addr = NULL;
	hevc->rpm_addr = NULL;
	hevc->lmem_addr = NULL;
    hevc->ref_list_buffer_addr = NULL;

	//hevc->curr_POC = INVALID_POC;

	hevc->use_cma_flag = 0;
	hevc->decode_idx = 0;
	hevc->slice_idx = 0;
	hevc->new_pic = 0;
	hevc->new_tile = 0;
	hevc->iPrevPOC = 0;
	hevc->list_no = 0;

	hevc->m_pocRandomAccess = MAX_INT;
	hevc->tile_enabled = 0;
	hevc->tile_x = 0;
	hevc->tile_y = 0;
	hevc->iPrevTid0POC = 0;
	hevc->slice_addr = 0;
	hevc->slice_segment_addr = 0;
	hevc->skip_flag = 0;
	hevc->misc_flag0 = 0;

	//hevc->vvc_dec->cur_pic = NULL;
	hevc->col_pic = NULL;
	hevc->wait_buf = 0;
	hevc->error_flag = 0;
	hevc->head_error_flag = 0;
	hevc->error_skip_nal_count = 0;
	hevc->have_vps = 0;
	hevc->have_sps = 0;
	hevc->have_pps = 0;
	hevc->have_valid_start_slice = 0;

	hevc->pts_mode = PTS_NORMAL;
	hevc->last_pts = 0;
	hevc->last_lookup_pts = 0;
	hevc->last_pts_us64 = 0;
	hevc->last_lookup_pts_us64 = 0;
	hevc->pts_mode_switching_count = 0;
	hevc->pts_mode_recovery_count = 0;

	hevc->PB_skip_mode = hevc->nal_skip_policy & 0x3;
	hevc->PB_skip_count_after_decoding = (hevc->nal_skip_policy >> 16) & 0xffff;
	if (hevc->PB_skip_mode == 0)
		hevc->ignore_bufmgr_error = 0x1;
	else
		hevc->ignore_bufmgr_error = 0x0;

	hevc->pic_num = 0;
	hevc->lcu_x_num_pre = 0;
	hevc->lcu_y_num_pre = 0;
	hevc->first_pic_after_recover = 0;

	hevc->pre_top_pic = NULL;
	hevc->pre_bot_pic = NULL;

	hevc->sei_hdr10_flag = 0;
	hevc->valve_count = 0;
	hevc->first_pic_flag = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	//hevc->decoded_poc = INVALID_POC;
	hevc->start_process_time = 0;
	hevc->last_lcu_idx = 0;
	hevc->decode_timeout_count = 0;
	hevc->timeout_num = 0;
	hevc->eos = 0;
	hevc->pic_decoded_lcu_idx = -1;
	hevc->over_decode = 0;
	hevc->used_4k_num = -1;
	hevc->rps_set_id = 0;
#endif
	hevc->vvc_dec = &hevc->g_vvc_dec;
}

//static int post_picture_early(struct vdec_s *vdec, int index);
int H266_alloc_mmu(struct hevc_state_s *hevc,
			struct PIC_s *new_pic,	unsigned short bit_depth,
			unsigned int *mmu_index_adr);
#ifdef VVC_10B_MMU_DW
int H266_alloc_mmu_dw(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr);
#endif

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;

	for (i = 0; i < 128; i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32 & 0xffff;
		WRITE_VREG(RPM_CMD_REG, 0);
	}
}

static vvc_frame_t * pic_buf_cfg_alloc(struct hevc_state_s *hevc)
{
	vvc_frame_t * pic = NULL;
	int i;
	for (i = 0; i < PIC_POOL_SIZE; i++) {
		if (hevc->vvc_dec->pic_pool[i].used == 0) {
			break;
		}
	}
	if (i < PIC_POOL_SIZE) {
		//vvc_frame_t pic_cfg;
		pic = &hevc->vvc_dec->pic_pool[i];
		pic->used = 1;
#ifdef AML
		pic->vf_ref = 0;
		pic->backend_ref = 0;
#endif

	}
	if (pic)
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE, "%s: pic index %d\n", __func__, pic->index);
	else
		hevc_print(hevc, 0, "%s: ret NULL\n", __func__);
	return pic;
	//return com_picbuf_alloc(pa->width, pa->height, pa->pad_l, pa->pad_c, ret);
}

static void pic_buf_cfg_free(vvc_frame_t *pic)
{
	pic->used = 0;
	put_mv_buf(pic);
	hevc_print(pic->hevc, H266_DEBUG_BUFMGR_MORE, "%s: pic index %d\n", __func__, pic->index);
}

static void init_pic_buf_cfg_list(struct hevc_state_s *hevc)
{
	int i;
	vvc_frame_t * pic;
	for (i = 0; i < PIC_POOL_SIZE; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		memset(pic, 0, sizeof (vvc_frame_t));
		pic->used = 0;
		pic->index = i;
	}
}

#if 0
static struct PIC_s *get_pic_by_POC(struct hevc_state_s *hevc, int poc)
{
	int i;
	struct PIC_s *pic;
	struct PIC_s *ret_pic = NULL;
	if (poc == INVALID_POC)
		return NULL;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic == NULL || pic->index == -1 ||
			pic->BUF_index == -1)
			continue;
		if (pic->poc == poc) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}
	return ret_pic;
}
#endif

static vvc_frame_t *get_ref_pic_by_POC(struct vvc_decoder *hw, int poc)
{
	DecLib *p_declib = &hw->m_decApp.m_cDecLib;
	Picture *pcPic;
	vvc_frame_t *pic = NULL;
	int ii;
	for (ii = 0; ii < PIC_LIST_SIZE; ii++)
	{
		pcPic = p_declib->m_cListPic.pic[ii];
		if ((pcPic == NULL) || (pcPic->poc == poc))
			break;
	}
	if (pcPic) {
		pic = pcPic->buf_cfg;
		//pic->longTerm = pcPic->longTerm;
	}
	return pic;
}

static void uninit_mmu_buffers(struct hevc_state_s *hevc)
{
	if (hevc->mmu_box) {
		decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
	}
#ifdef VVC_10B_MMU_DW
	if (hevc->mmu_box_dw) {
		decoder_mmu_box_free(hevc->mmu_box_dw);
		hevc->mmu_box_dw = NULL;
	}
#endif
	if (hevc->bmmu_box) {
		/* release workspace */
		decoder_bmmu_box_free_idx(hevc->bmmu_box,
			BMMU_WORKSPACE_ID);
		decoder_bmmu_box_free(hevc->bmmu_box);
		hevc->bmmu_box = NULL;
	}
}

/* return in MB */
static int hevc_max_mmu_buf_size(int max_w, int max_h)
{
	int buf_size = 64;

	if ((max_w * max_h) > 0 &&
		(max_w * max_h) <= 1920*1088) {
		buf_size = 24;
	}
	return buf_size;
}

static int init_mmu_buffers(struct hevc_state_s *hevc, int bmmu_flag)
{
	int tvp_flag = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = hevc_max_mmu_buf_size(hevc->max_pic_w,
			hevc->max_pic_h);

	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0, "%s max_w %d max_h %d\n",
			__func__, hevc->max_pic_w, hevc->max_pic_h);
	}

	hevc->need_cache_size = buf_size * SZ_1M;
	hevc->sc_start_time = get_jiffies_64();

	if (!bmmu_flag)
		return 0;

	hevc->bmmu_box = decoder_bmmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			BMMU_MAX_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag,
			BMMU_ALLOC_FLAGS_WAITCLEAR);

	if (!hevc->bmmu_box) {
		if (hevc->mmu_box)
			decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
		pr_err("h265 alloc mmu box failed!!\n");
		return -1;
	}

	return 0;
}

struct buf_stru_s
{
	int lcu_total;
	int mc_buffer_size_h;
	int mc_buffer_size_u_v_h;
};

#ifndef MV_USE_FIXED_BUF
static void dealloc_mv_bufs(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_mv_BUF[i].start_adr) {
			if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
				"dealloc mv buf(%d) adr 0x%p size 0x%x used_flag %d\n",
				i, hevc->m_mv_BUF[i].start_adr,
				hevc->m_mv_BUF[i].size,
				hevc->m_mv_BUF[i].used_flag);
			decoder_bmmu_box_free_idx(
				hevc->bmmu_box,
				MV_BUFFER_IDX(i));
			hevc->m_mv_BUF[i].start_adr = 0;
			hevc->m_mv_BUF[i].size = 0;
			hevc->m_mv_BUF[i].used_flag = 0;
		}
	}
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		hevc->vvc_dec->pic_pool[i].mv_buf_index = -1;
	}
}

static int alloc_mv_buf(struct hevc_state_s *hevc, int i)
{
	int ret = 0;
	/*get_cma_alloc_ref();*/ /*DEBUG_TMP*/
	if (decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box,
		MV_BUFFER_IDX(i),
		hevc->mv_buf_size,
		DRIVER_NAME,
		&hevc->m_mv_BUF[i].start_adr) < 0) {
		hevc->m_mv_BUF[i].start_adr = 0;
		ret = -1;
	} else {
		hevc->m_mv_BUF[i].size = hevc->mv_buf_size;
		hevc->m_mv_BUF[i].used_flag = 0;
		ret = 0;
		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"MV Buffer %d: start_adr %p size %x\n",
				i,
				(void *)hevc->m_mv_BUF[i].start_adr,
				hevc->m_mv_BUF[i].size);
		}
		if (!vdec_secure(hw_to_vdec(hevc)) && (hevc->m_mv_BUF[i].start_adr)) {
			void *mem_start_virt;
			mem_start_virt = codec_mm_phys_to_virt(hevc->m_mv_BUF[i].start_adr);
			if (mem_start_virt) {
					memset(mem_start_virt, 0, hevc->m_mv_BUF[i].size);
					codec_mm_dma_flush(mem_start_virt,
							hevc->m_mv_BUF[i].size, DMA_TO_DEVICE);
			} else {
					mem_start_virt = codec_mm_vmap(hevc->m_mv_BUF[i].start_adr,
							hevc->m_mv_BUF[i].size);
					if (mem_start_virt) {
							memset(mem_start_virt, 0, hevc->m_mv_BUF[i].size);
							codec_mm_dma_flush(mem_start_virt,
									hevc->m_mv_BUF[i].size,
									DMA_TO_DEVICE);
							codec_mm_unmap_phyaddr(mem_start_virt);
					} else {
							/*not virt for tvp playing,
							may need clear on ucode.*/
							pr_err("ref %s	mem_start_virt failed\n", __func__);
					}
			}
		}
	}
	return ret;
}
#endif

int get_mv_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
#ifdef MV_USE_FIXED_BUF
	if (pic && pic->index >= 0) {
		int mv_size;
		if (IS_8K_SIZE(pic->width, pic->height))
			mv_size = MPRED_8K_MV_BUF_SIZE;
		else if (IS_4K_SIZE(pic->width, pic->height))
			mv_size = MPRED_4K_MV_BUF_SIZE; /*0x120000*/
		else
			mv_size = MPRED_MV_BUF_SIZE;

		pic->mpred_mv_wr_start_addr =
			hevc->work_space_buf->mpred_mv.buf_start
			+ (pic->index * mv_size);
		pic->mv_size = mv_size;
	}
	return 0;
#else
	int i;
	int ret = -1;
	int new_size;
	if (mv_buf_dynamic_alloc) {
		int MV_MEM_UNIT =
			hevc->lcu_size_log2 == 6 ? 0x200 : hevc->lcu_size_log2 ==
			5 ? 0x80 : 0x20;
		int extended_pic_width = (pic->width + hevc->lcu_size -1)
				& (~(hevc->lcu_size - 1));
		int extended_pic_height = (pic->height + hevc->lcu_size -1)
				& (~(hevc->lcu_size - 1));
		int lcu_x_num = extended_pic_width / hevc->lcu_size;
		int lcu_y_num = extended_pic_height / hevc->lcu_size;
		new_size =  lcu_x_num * lcu_y_num * MV_MEM_UNIT;
		hevc->mv_buf_size = (new_size + 0xffff) & (~0xffff);
	} else {
		if (IS_8K_SIZE(pic->width, pic->height))
			new_size = MPRED_8K_MV_BUF_SIZE;
		else if (IS_4K_SIZE(pic->width, pic->height))
			new_size = MPRED_4K_MV_BUF_SIZE; /*0x120000*/
		else
			new_size = MPRED_MV_BUF_SIZE;

		if (new_size != hevc->mv_buf_size) {
			dealloc_mv_bufs(hevc);
			hevc->mv_buf_size = new_size;
		}
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			if (hevc->m_mv_BUF[i].start_adr &&
				hevc->m_mv_BUF[i].used_flag == 0) {
				hevc->m_mv_BUF[i].used_flag = 1;
				ret = i;
				break;
			}
		}
	}
	if (ret < 0) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			if (hevc->m_mv_BUF[i].start_adr == 0) {
				if (alloc_mv_buf(hevc, i) >= 0) {
					hevc->m_mv_BUF[i].used_flag = 1;
					ret = i;
				}
				break;
			}
		}
	}

	if (ret >= 0) {
		pic->mv_buf_index = ret;
		pic->mv_size = hevc->m_mv_BUF[ret].size;
		pic->mpred_mv_wr_start_addr =
			(hevc->m_mv_BUF[ret].start_adr + 0xffff) & (~0xffff);
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
			"%s => %d (0x%x) size 0x%x\n",
			__func__, ret,
			pic->mpred_mv_wr_start_addr,
			pic->mv_size);

	} else {
		hevc_print(hevc, 0,
			"%s: Error, mv buf is not enough\n", __func__);
	}
	return ret;

#endif
}

static void put_mv_buf(struct PIC_s *pic)
{
	struct hevc_state_s *hevc = pic->hevc;
#ifndef MV_USE_FIXED_BUF
	int i = pic->mv_buf_index;
	if (i < 0 || i >= MAX_REF_PIC_NUM) {
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
			"%s: index %d beyond range\n", __func__, i);
		return;
	}
	if (mv_buf_dynamic_alloc) {
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
			"%s(%d)\n", __func__, i);

		decoder_bmmu_box_free_idx(
			hevc->bmmu_box,
			MV_BUFFER_IDX(i));
		hevc->m_mv_BUF[i].start_adr = 0;
		hevc->m_mv_BUF[i].size = 0;
		hevc->m_mv_BUF[i].used_flag = 0;
		pic->mv_buf_index = -1;
		return;
	}

	hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
		"%s(%d): used_flag(%d)\n",
		__func__, i,
		hevc->m_mv_BUF[i].used_flag);

	if (hevc->m_mv_BUF[i].start_adr &&
		hevc->m_mv_BUF[i].used_flag)
		hevc->m_mv_BUF[i].used_flag = 0;
	pic->mv_buf_index = -1;
#endif
}

static int hevc_get_header_size(int w, int h)
{
	w = ALIGN(w, 64);
	h = ALIGN(h, 64);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
			(IS_8K_SIZE(w, h)))
		return MMU_COMPRESS_HEADER_SIZE_8K;
	else if (IS_4K_SIZE(w, h))
		return MMU_COMPRESS_HEADER_SIZE_4K;
	else
		return MMU_COMPRESS_HEADER_SIZE_1080P;
}

static struct aml_buf *index_to_afbc_aml_buf(struct hevc_state_s *hevc, int index)
{
	int i;
	for (i = 0; i < BUF_FBC_NUM_MAX; i++) {
		if (hevc->m_BUF[index].v4l_ref_buf_addr
			== hevc->afbc_buf_table[i].fb) {
			hevc_print(hevc, H266_DEBUG_BUFMGR,
				"cur fb idx mmu %d\n",
				i);
			break;
		}
	}

	if (i >= BUF_FBC_NUM_MAX) {
		hevc_print(hevc, 0, "[ERR]%s afbc_index: %d i: %d fb: %lx\n",
		__func__, index, i, hevc->m_BUF[index].v4l_ref_buf_addr);

		return NULL;
	}

	return (struct aml_buf *)hevc->afbc_buf_table[i].fb;
}

#if 0
static int cal_current_buf_size(struct hevc_state_s *hevc,
	struct buf_stru_s *buf_stru)
{
	int buf_size;
	int pic_width = hevc->pic_w;
	int pic_height = hevc->pic_h;
	int lcu_size = hevc->lcu_size;
	int pic_width_lcu = (pic_width % lcu_size) ? pic_width / lcu_size +
				 1 : pic_width / lcu_size;
	int pic_height_lcu = (pic_height % lcu_size) ? pic_height / lcu_size +
				 1 : pic_height / lcu_size;
	/*SUPPORT_10BIT*/
	int losless_comp_header_size = compute_losless_comp_header_size
		(pic_width, pic_height);
		/*always alloc buf for 10bit*/
	int losless_comp_body_size = compute_losless_comp_body_size
		(hevc, pic_width, pic_height, 0);
	int mc_buffer_size = losless_comp_header_size
		+ losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff) >> 16;
	int mc_buffer_size_u_v_h = 0;

	int dw_mode = get_double_write_mode(hevc);

	if (hevc->mmu_enable)
		buf_size = hevc_get_header_size(hevc->pic_w, hevc->pic_h);
	else
		buf_size = 0;
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable) {
		buf_size = ((buf_size + 0xffff) >> 16) << 16;
		buf_size <<= 1;
	}
#endif
	if (dw_mode && ((dw_mode & 0x20) == 0)) {
		int pic_width_dw = pic_width /
			get_double_write_ratio(dw_mode);
		int pic_height_dw = pic_height /
			get_double_write_ratio(dw_mode);

		int pic_width_lcu_dw = (pic_width_dw % lcu_size) ?
			pic_width_dw / lcu_size + 1 :
			pic_width_dw / lcu_size;
		int pic_height_lcu_dw = (pic_height_dw % lcu_size) ?
			pic_height_dw / lcu_size + 1 :
			pic_height_dw / lcu_size;
		int lcu_total_dw = pic_width_lcu_dw * pic_height_lcu_dw;

		int mc_buffer_size_u_v = lcu_total_dw * lcu_size * lcu_size >> 1;
		mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		buf_size += ((mc_buffer_size_u_v_h << 16) * 3);
	}

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
		buf_size += (mc_buffer_size_u_v_h << 15) * 3;	//s1a ext buf
	} else {
		if ((!hevc->mmu_enable) &&
			((dw_mode & 0x10) == 0)) {
			/* use compress mode without mmu,
			need buf for compress decoding*/
			buf_size += (mc_buffer_size_h << 16);
		}
	}

	/*in case start adr is not 64k alignment*/
	if (buf_size > 0)
		buf_size += 0x10000;

	if (buf_stru) {
		buf_stru->lcu_total = pic_width_lcu * pic_height_lcu;
		buf_stru->mc_buffer_size_h = mc_buffer_size_h;
		buf_stru->mc_buffer_size_u_v_h = mc_buffer_size_u_v_h;
	}

	hevc_print(hevc, PRINT_FLAG_V4L_DETAIL,"pic width: %d, pic height: %d, headr: %d, body: %d, size h: %d, size uvh: %d, buf size: %x\n",
		pic_width, pic_height, losless_comp_header_size,
		losless_comp_body_size, mc_buffer_size_h,
		mc_buffer_size_u_v_h, buf_size);

	return buf_size;
}
#endif

#if 0
static int alloc_buf(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;
	int buf_size = cal_current_buf_size(hevc, NULL);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (hevc->fatal_error & DECODER_FATAL_ERROR_NO_MEM)
		return ret;

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr == 0)
			break;
	}
	if (i < BUF_POOL_SIZE) {
		if (buf_size > 0) {
			ret = decoder_bmmu_box_alloc_buf_phy
				(hevc->bmmu_box,
				VF_BUFFER_IDX(i), buf_size,
				DRIVER_NAME,
				&hevc->m_BUF[i].start_adr);
			if (ret < 0) {
				hevc->m_BUF[i].start_adr = 0;
				if (i <= 8) {
					hevc->fatal_error |= DECODER_FATAL_ERROR_NO_MEM;
					hevc_print(hevc, PRINT_FLAG_ERROR,
						"%s[%d], size: %d, no mem fatal err\n",
						__func__, i, buf_size);
				}
			}

			if (ret >= 0) {
				if (hevc->enable_fence) {
					vdec_fence_buffer_count_increase((ulong)vdec->sync);
					INIT_LIST_HEAD(&vdec->sync->release_callback[VF_BUFFER_IDX(i)].node);
					decoder_bmmu_box_add_callback_func(hevc->bmmu_box, VF_BUFFER_IDX(i), (void *)&vdec->sync->release_callback[VF_BUFFER_IDX(i)]);
				}
				hevc->m_BUF[i].size = buf_size;
				hevc->m_BUF[i].used_flag = 0;
				ret = 0;
				if (vdec->vdata == NULL ||
					atomic_read(&vdec->vdata->use_flag) == 0) {
					vdec->vdata = vdec_data_get();
				}

				if (vdec->vdata != NULL) {
					int index = 0;
					struct vdec_data_buf_s data_buf;
					data_buf.alloc_policy = ALLOC_AUX_BUF;
					data_buf.aux_buf_size = AUX_DATA_SIZE1;

					data_buf.alloc_policy |= ALLOC_HDR10P_BUF;
					data_buf.hdr10p_buf_size = HDR10P_BUF_SIZE;

					index = vdec_data_get_index((ulong)vdec->vdata, &data_buf);
					if (index >= 0) {
						hevc->aux_data_buf[i] = vdec->vdata->data[index].aux_data_buf;
						hevc->hdr10p_data_buf[i] = vdec->vdata->data[index].hdr10p_data_buf;
						vdec_data_buffer_count_increase((ulong)vdec->vdata, index, i);
						INIT_LIST_HEAD(&vdec->vdata->release_callback[i].node);
						decoder_bmmu_box_add_callback_func(hevc->bmmu_box, VF_BUFFER_IDX(i), (void *)&vdec->vdata->release_callback[i]);
					} else {
						hevc_print(hevc, 0, "vdec data is full\n");
					}
				}
				if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
						"Buffer %d: start_adr %p size %x\n",
						i,
						(void *)hevc->m_BUF[i].start_adr,
						hevc->m_BUF[i].size);
				}
				/*flush the buffer make sure no cache dirty*/
				if (!vdec_secure(hw_to_vdec(hevc)) && (hevc->m_BUF[i].start_adr)) {
					void *mem_start_virt;
					mem_start_virt =
					codec_mm_phys_to_virt(hevc->m_BUF[i].start_adr);
					if (mem_start_virt) {
						memset(mem_start_virt, 0, hevc->m_BUF[i].size);
						codec_mm_dma_flush(mem_start_virt,
						hevc->m_BUF[i].size, DMA_TO_DEVICE);
					} else {
						codec_mm_memset(hevc->m_BUF[i].start_adr,
							0, hevc->m_BUF[i].size);
					}
				}
			}
		} else
			ret = 0;
	}

	if (ret >= 0) {
		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"alloc buf(%d) for %d/%d size 0x%x) => %p\n",
				i, hevc->pic_w, hevc->pic_h,
				buf_size,
				hevc->m_BUF[i].start_adr);
		}
	} else {
		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"alloc buf(%d) for %d/%d size 0x%x) => Fail!!!\n",
				i, hevc->pic_w, hevc->pic_h,
				buf_size);
		}
	}
	return ret;
}
#endif

#if 0
static void set_buf_unused(struct hevc_state_s *hevc, int i)
{
	if (i >= 0 && i < BUF_POOL_SIZE)
		hevc->m_BUF[i].used_flag = 0;
}
#endif
void dealloc_unused_buf(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr &&
			hevc->m_BUF[i].used_flag == 0) {
			if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
					"dealloc buf(%d) adr 0x%p size 0x%x\n",
					i, hevc->m_BUF[i].start_adr,
					hevc->m_BUF[i].size);
			}
			hevc->m_BUF[i].start_adr = 0;
			hevc->m_BUF[i].header_addr = 0;
			hevc->m_BUF[i].size = 0;
		}
	}
}

#if 0
void dealloc_pic_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	int i = pic->BUF_index;
	pic->BUF_index = -1;
	if (i >= 0 &&
		i < BUF_POOL_SIZE &&
		hevc->m_BUF[i].start_adr) {
		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"dealloc buf(%d) adr 0x%p size 0x%x\n",
				i, hevc->m_BUF[i].start_adr,
				hevc->m_BUF[i].size);
		}
		decoder_bmmu_box_free_idx(
			hevc->bmmu_box,
			VF_BUFFER_IDX(i));
		hevc->m_BUF[i].used_flag = 0;
		hevc->m_BUF[i].start_adr = 0;
		hevc->m_BUF[i].header_addr = 0;
		hevc->m_BUF[i].size = 0;
	}
}
#endif
static int get_work_pic_num(struct hevc_state_s *hevc)
{
	int used_buf_num = 0;
#if 0
	used_buf_num = hevc->param.p.sps_max_dec_pic_buffering_minus1_0 + 1;
	/*
	1. decoding the current frame
	2. decoding the current frame will only update reference frame information,
	   such as reference relation, when the next frame is decoded.
	*/

	used_buf_num += 1;
	if (!save_buffer)
		used_buf_num += 1;

	if (hevc->save_buffer_mode)
		hevc_print(hevc, 0,
			"save buf _mode : dynamic_buf_num_margin %d ----> %d \n",
			dynamic_buf_num_margin,  hevc->dynamic_buf_num_margin);

	used_buf_num += get_dynamic_buf_num_margin(hevc);

	if (used_buf_num > MAX_BUF_NUM)
		used_buf_num = MAX_BUF_NUM;
#else
	used_buf_num = PIC_POOL_SIZE;
#endif
	return used_buf_num;
}
#if 0
static int get_alloc_pic_count(struct hevc_state_s *hevc)
{
	int alloc_pic_count = 0;
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic && pic->index >= 0)
			alloc_pic_count++;
	}
	return alloc_pic_count;
}
#endif
#if 0
static int config_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int ret = -1;
	int i;
	unsigned int y_adr = 0;
	struct buf_stru_s buf_stru;
	int buf_size = cal_current_buf_size(hevc, &buf_stru);
	int dw_mode = get_double_write_mode(hevc);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr != 0 &&
			hevc->m_BUF[i].used_flag == 0 &&
			buf_size <= hevc->m_BUF[i].size) {
			hevc->m_BUF[i].used_flag = 1;
			break;
		}
	}

	if (i >= BUF_POOL_SIZE)
		return -1;

	if (vdec->vdata != NULL) {
		pic->hdr10p_data_buf = hevc->hdr10p_data_buf[i];
		pic->aux_data_buf = hevc->aux_data_buf[i];
	}

	if (hevc->mmu_enable) {
		pic->header_adr = hevc->m_BUF[i].start_adr;
		y_adr = hevc->m_BUF[i].start_adr +
			hevc_get_header_size(hevc->pic_w, hevc->pic_h);
	} else
		y_adr = hevc->m_BUF[i].start_adr;

	y_adr = ((y_adr + 0xffff) >> 16) << 16; /*64k alignment*/

#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable) {
#ifdef USE_FIXED_MMU_DW_HEADER
		pic->header_dw_adr = hevc->work_space_buf->cm_header_dw.buf_start +
			(i * hevc_get_header_size(hevc->pic_w, hevc->pic_h));
#else
		pic->header_dw_adr = y_adr;
		y_adr = pic->header_dw_adr +
			hevc_get_header_size(hevc->pic_w, hevc->pic_h);
#endif
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"MMU header_dw_adr %d: %x\n", pic->header_dw_adr);
	}
#endif

	pic->poc = INVALID_POC;
	/*ensure get_pic_by_POC()
	not get the buffer not decoded*/
	pic->BUF_index = i;
	if (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S1A) {
		if ((!hevc->mmu_enable) && ((dw_mode & 0x10) == 0)) {
			pic->mc_y_adr = y_adr;
			y_adr += (buf_stru.mc_buffer_size_h << 16);
		}
	}

	pic->mc_canvas_y = pic->index;
	pic->mc_canvas_u_v = pic->index;
	if (dw_mode & 0x10) {
		pic->mc_y_adr = y_adr;
		pic->mc_u_v_adr = y_adr +
			((buf_stru.mc_buffer_size_u_v_h << 16) << 1);
		pic->mc_canvas_y = (pic->index << 1);
		pic->mc_canvas_u_v = (pic->index << 1) + 1;

		pic->dw_y_adr = pic->mc_y_adr;
		pic->dw_u_v_adr = pic->mc_u_v_adr;

		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
			pic->ext_y_adr = pic->dw_u_v_adr + (buf_stru.mc_buffer_size_u_v_h << 16);
			pic->ext_uv_adr = pic->ext_y_adr + (buf_stru.mc_buffer_size_u_v_h << 16);
		}
	} else if (dw_mode && (dw_mode & 0x20) == 0) {
		pic->dw_y_adr = y_adr;
		pic->dw_u_v_adr = pic->dw_y_adr +
			((buf_stru.mc_buffer_size_u_v_h << 16) << 1);
	}

	if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"%s index %d BUF_index %d mc_y_adr %x\n",
		 __func__, pic->index,
		 pic->BUF_index, pic->mc_y_adr);
		if (hevc->mmu_enable && dw_mode)
			hevc_print(hevc, 0,
			"mmu double write  adr %ld\n",
			 pic->cma_alloc_addr);
	}
	ret = 0;

	return ret;
}
#endif
static void init_pic_list(struct hevc_state_s *hevc)
{
	int i;
	int dw_mode = get_double_write_mode(hevc);
	struct vdec_s *vdec = hw_to_vdec(hevc);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = &hevc->vvc_dec->pic_pool[i];
#if 0
		if (!pic) {
			pic = vmalloc(sizeof(struct PIC_s));
			if (pic == NULL) {
				hevc_print(hevc, 0,
					"%s: alloc pic %d fail!!!\n", __func__, i);
				break;
			}
			hevc->m_PIC[i] = pic;
		}
		memset(pic, 0, sizeof(struct PIC_s));
#endif
		pic->index = i;
		pic->BUF_index = -1;
		pic->mv_buf_index = -1;
		if (vdec->parallel_dec == 1) {
			pic->y_canvas_index = -1;
			pic->uv_canvas_index = -1;
		}

		pic->width = hevc->pic_w;
		pic->height = hevc->pic_h;
		pic->double_write_mode = dw_mode;
		pic->poc = INVALID_POC;
#if 0
		/*config canvas will be delay if work on v4l. */
		if (config_pic(hevc, pic) < 0) {
			if (get_dbg_flag(hevc))
				hevc_print(hevc, 0, "Config_pic %d fail\n", pic->index);
			pic->index = -1;
			i++;
			break;
		}
        pic->used = 0;

		if (pic->double_write_mode)
			set_canvas(hevc, pic);
#endif
	}
}

static void uninit_pic_list(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int i;
#ifndef MV_USE_FIXED_BUF
	dealloc_mv_bufs(hevc);
#endif
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		//struct PIC_s *pic = hevc->m_PIC[i];
		struct PIC_s *pic = &hevc->vvc_dec->pic_pool[i];
		//if (pic) {
			if (vdec->parallel_dec == 1) {
				vdec->free_canvas_ex(pic->y_canvas_index, vdec->id);
				vdec->free_canvas_ex(pic->uv_canvas_index, vdec->id);
			}
			release_aux_data(hevc, pic);
			//vfree(pic);
			//hevc->m_PIC[i] = NULL;
		//}
	}
    hevc->uninit_list_done = 1;
}

#ifdef LOSLESS_COMPRESS_MODE
static void init_decode_head_hw(struct hevc_state_s *hevc)
{

	struct BuffInfo_s *buf_spec = hevc->work_space_buf;
	unsigned int data32 = 0;

	int losless_comp_header_size =
		compute_losless_comp_header_size(hevc->pic_w,
			 hevc->pic_h);
	int losless_comp_body_size = compute_losless_comp_body_size(hevc,
		hevc->pic_w, hevc->pic_h, hevc->mem_saving_mode);

	hevc->losless_comp_body_size = losless_comp_body_size;


	if (hevc->mmu_enable) {
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);
	} else {
        WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0<<3)); // bit[3] smem mode
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
	}
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);

	if (hevc->mmu_enable) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR,
			buf_spec->mmu_vbh.buf_start +
			VBH_BUF_SIZE(buf_spec));
		data32 = READ_VREG(HEVC_SAO_CTRL9);
		data32 |= 0x1;
		WRITE_VREG(HEVC_SAO_CTRL9, data32);

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable) {
		u32 data_tmp;
		data_tmp = READ_VREG(HEVC_SAO_CTRL9);
		data_tmp |= (1 << 10);
		WRITE_VREG(HEVC_SAO_CTRL9, data_tmp);

		WRITE_VREG(HEVC_CM_BODY_LENGTH2,
			losless_comp_body_size);
		WRITE_VREG(HEVC_CM_HEADER_OFFSET2,
			losless_comp_body_size);
		WRITE_VREG(HEVC_CM_HEADER_LENGTH2,
			losless_comp_header_size);

		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR2,
			buf_spec->mmu_vbh_dw.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR2,
			buf_spec->mmu_vbh_dw.buf_start + DW_VBH_BUF_SIZE(buf_spec));
		WRITE_VREG(HEVC_DW_VH0_ADDDR,
			buf_spec->mmu_vbh_dw.buf_start + (2 * DW_VBH_BUF_SIZE(buf_spec)));
		WRITE_VREG(HEVC_DW_VH1_ADDDR,
			buf_spec->mmu_vbh_dw.buf_start + (3 * DW_VBH_BUF_SIZE(buf_spec)));
		/* use HEVC_CM_HEADER_START_ADDR */
		data32 |= (1 << 15);
	} else
		data32 &= ~(1 << 15);
	WRITE_VREG(HEVC_SAO_CTRL5, data32);
#endif
	if (!hevc->m_ins_flag)
		hevc_print(hevc, 0,
			"%s: (%d, %d) body_size 0x%x header_size 0x%x\n",
			__func__, hevc->pic_w, hevc->pic_h,
			losless_comp_body_size, losless_comp_header_size);

}
#endif
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464

static void init_pic_list_hw(struct hevc_state_s *hevc)
{
	int i;
	int cur_pic_num = MAX_REF_PIC_NUM;
	int dw_mode = get_double_write_mode(hevc);
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
		(0x1 << 1) | (0x1 << 2));
#ifdef USE_NV21_EXTRA_BUF
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR_EXTRA,
			(0x1 << 1) | (0x1 << 2));
	}
#endif

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
        //WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, (0x1 << 1) | (i << 8));

		if (hevc->vvc_dec->pic_pool[i].index == -1) {
			cur_pic_num = i;
			break;
		}
		if (hevc->mmu_enable && ((dw_mode & 0x10) == 0))
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->vvc_dec->pic_pool[i].header_adr>>5);
		else
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->vvc_dec->pic_pool[i].mc_y_adr >> 5);

		if (dw_mode & 0x10) {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
			hevc->vvc_dec->pic_pool[i].mc_u_v_adr >> 5);
		}
#ifdef USE_NV21_EXTRA_BUF
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA_EXTRA,
				hevc->vvc_dec->pic_pool[i].ext_y_adr >> 5);
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA_EXTRA,
				hevc->vvc_dec->pic_pool[i].ext_uv_adr >> 5);
		}
#endif
	}
	if (cur_pic_num == 0)
		return;

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);
#ifdef USE_NV21_EXTRA_BUF
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR_EXTRA, 0x1);
#endif

	/* Zero out canvas registers in IPP -- avoid simulation X */
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

#ifdef LOSLESS_COMPRESS_MODE
	if ((dw_mode & 0x10) == 0)
		init_decode_head_hw(hevc);
#endif

}

static void vh266_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vh266_vf_put(vf, vdec_ctx);
}

static void vh266_get_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	memcpy(vf, vh266_vf_get(vdec_ctx), sizeof(struct vframe_s));
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= vh266_get_video_frame,
	.put_vframe	= vh266_put_video_frame,
};

static int32_t config_mc_buffer(struct hevc_state_s *hevc)
{
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	int32_t i;
	int32_t long_term_flag = 0;
	vvc_frame_t *pic;
	vvc_frame_t *cur_pic = vvc_dec->cur_pic;
	DecLib *p_declib = &vvc_dec->m_decApp.m_cDecLib;
	Slice* slice;
	//Slice *slice = p_declib->m_apcSlicePilot;
	hevc_print(hevc, H266_DEBUG_REG_CFG,
		"config_mc_buffer entered (slice_type : %d) m_uiSliceSegmentIdx %d .....\n",
		cur_pic->slice_type, p_declib->m_uiSliceSegmentIdx-1);
	slice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx - 1];
	if (cur_pic->slice_type != I_SLICE) { //P and B pic
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
		hevc_print(hevc, H266_DEBUG_REG_CFG, "slice=%p, slice->m_aiNumRefIdx[REF_PIC_LIST_0] = %d\n",
			slice, slice->m_aiNumRefIdx[REF_PIC_LIST_0]);
		for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_0] && i < MAX_NUM_REF; i++) {
			if (slice->m_apcRefPicList[REF_PIC_LIST_0][i] == NULL) {
				hevc_print(hevc, 0, "Error %s, slice->m_apcRefPicList[REF_PIC_LIST_0][%d] is NULL\n", __func__, i);
				cur_pic->error_mark = 1;
				return -1;
			}
			pic = get_ref_pic_by_POC(vvc_dec,  slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc);
			if (pic) {
				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
					cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
					(pic->mc_canvas_u_v << 16) | (pic->mc_canvas_u_v << 8) | pic->mc_canvas_y);
			} else {
				cur_pic->error_mark = 1;
				hevc_print(hevc, 0, "Error %s, %dth poc (%d) of RPS is not in the pic list0\n",
				__func__, i, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
			}
			if (h266_is_long_term(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc))
				long_term_flag = long_term_flag | (1 << i);
		}

		for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_0] && i < MAX_NUM_REF; i++) {
			pic = get_ref_pic_by_POC(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc);
			if (pic)
				WRITE_VREG(VVC_MPP_REF0_POC_CFG, pic->poc);
		}
	}
	if (cur_pic->slice_type == B_SLICE) { //B pic
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (16 << 8) | (0 << 1) | 1);
		hevc_print(hevc, H266_DEBUG_REG_CFG, "slice=%p, slice->m_aiNumRefIdx[REF_PIC_LIST_1] = %d\n", slice, slice->m_aiNumRefIdx[REF_PIC_LIST_1]);
		for (i = 0; i  < slice->m_aiNumRefIdx[REF_PIC_LIST_1] && i < MAX_NUM_REF; i++) {
			if (slice->m_apcRefPicList[REF_PIC_LIST_1][i] == NULL) {
				hevc_print(hevc, 0, "Error %s, slice->m_apcRefPicList[REF_PIC_LIST_1][%d] is NULL\n", __func__, i);
				cur_pic->error_mark = 1;
				return -1;
			}
			pic = get_ref_pic_by_POC(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
			if (pic) {
				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
				cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
					(pic->mc_canvas_u_v << 16) | (pic->mc_canvas_u_v << 8) | pic->mc_canvas_y);
			} else {
				cur_pic->error_mark = 1;
				hevc_print(hevc, 0, "Error %s, %dth poc (%d) of RPS is not in the pic list1\n",
				__func__, i, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
			}
			if (h266_is_long_term(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc))
				long_term_flag = long_term_flag | (1 << (i + 16));
		}
		for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_1] && i < MAX_NUM_REF; i++) {
			pic = get_ref_pic_by_POC(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
			if (pic)
				WRITE_VREG(VVC_MPP_REF1_POC_CFG, pic->poc);
		}
	}

	WRITE_VREG(AV1D_MPP_ORDERHINT_CFG, cur_pic->poc);
	WRITE_VREG(VVC_MPP_REF_IS_LONGTERM, long_term_flag);
	return 0;
}

static unsigned HEVC_MPRED_L0_REF_POC_ADR[] = {
	HEVC_MPRED_L0_REF00_POC,
	HEVC_MPRED_L0_REF01_POC,
	HEVC_MPRED_L0_REF02_POC,
	HEVC_MPRED_L0_REF03_POC,
	HEVC_MPRED_L0_REF04_POC,
	HEVC_MPRED_L0_REF05_POC,
	HEVC_MPRED_L0_REF06_POC,
	HEVC_MPRED_L0_REF07_POC,
	HEVC_MPRED_L0_REF08_POC,
	HEVC_MPRED_L0_REF09_POC,
	HEVC_MPRED_L0_REF10_POC,
	HEVC_MPRED_L0_REF11_POC,
	HEVC_MPRED_L0_REF12_POC,
	HEVC_MPRED_L0_REF13_POC,
	HEVC_MPRED_L0_REF14_POC,
	HEVC_MPRED_L0_REF15_POC,
	HEVC_MPRED_POC24_CTRL0
};

static unsigned HEVC_MPRED_L1_REF_POC_ADR[] = {
	HEVC_MPRED_L1_REF00_POC,
	HEVC_MPRED_L1_REF01_POC,
	HEVC_MPRED_L1_REF02_POC,
	HEVC_MPRED_L1_REF03_POC,
	HEVC_MPRED_L1_REF04_POC,
	HEVC_MPRED_L1_REF05_POC,
	HEVC_MPRED_L1_REF06_POC,
	HEVC_MPRED_L1_REF07_POC,
	HEVC_MPRED_L1_REF08_POC,
	HEVC_MPRED_L1_REF09_POC,
	HEVC_MPRED_L1_REF10_POC,
	HEVC_MPRED_L1_REF11_POC,
	HEVC_MPRED_L1_REF12_POC,
	HEVC_MPRED_L1_REF13_POC,
	HEVC_MPRED_L1_REF14_POC,
	HEVC_MPRED_L1_REF15_POC,
	HEVC_MPRED_POC24_CTRL1
};

static uint32_t HEVC_MPRED_COL_REF_CANVAS_xx_POC_adr[] = {
	HEVC_MPRED_COL_REF_CANVAS_00_POC,
	HEVC_MPRED_COL_REF_CANVAS_01_POC,
	HEVC_MPRED_COL_REF_CANVAS_02_POC,
	HEVC_MPRED_COL_REF_CANVAS_03_POC,
	HEVC_MPRED_COL_REF_CANVAS_04_POC,
	HEVC_MPRED_COL_REF_CANVAS_05_POC,
	HEVC_MPRED_COL_REF_CANVAS_06_POC,
	HEVC_MPRED_COL_REF_CANVAS_07_POC,
	HEVC_MPRED_COL_REF_CANVAS_08_POC,
	HEVC_MPRED_COL_REF_CANVAS_09_POC,
	HEVC_MPRED_COL_REF_CANVAS_10_POC,
	HEVC_MPRED_COL_REF_CANVAS_11_POC,
	HEVC_MPRED_COL_REF_CANVAS_12_POC,
	HEVC_MPRED_COL_REF_CANVAS_13_POC,
	HEVC_MPRED_COL_REF_CANVAS_14_POC,
	HEVC_MPRED_COL_REF_CANVAS_15_POC,
	HEVC_MPRED_COL_REF_CANVAS_16_POC,
	HEVC_MPRED_COL_REF_CANVAS_17_POC,
	HEVC_MPRED_COL_REF_CANVAS_18_POC,
	HEVC_MPRED_COL_REF_CANVAS_19_POC,
	HEVC_MPRED_COL_REF_CANVAS_20_POC,
	HEVC_MPRED_COL_REF_CANVAS_21_POC,
	HEVC_MPRED_COL_REF_CANVAS_22_POC,
	HEVC_MPRED_COL_REF_CANVAS_23_POC,
	HEVC_MPRED_COL_REF_CANVAS_24_POC,
	HEVC_MPRED_COL_REF_CANVAS_25_POC,
	HEVC_MPRED_COL_REF_CANVAS_26_POC,
	HEVC_MPRED_COL_REF_CANVAS_27_POC,
	HEVC_MPRED_COL_REF_CANVAS_28_POC,
	HEVC_MPRED_COL_REF_CANVAS_29_POC,
	HEVC_MPRED_COL_REF_CANVAS_30_POC
};

static int config_mpred_hw(struct hevc_state_s *hevc, BuffInfo_t* buf_spec)
{
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	int32_t i, ii;
	uint32_t data32, data32_2;
	DecLib *p_declib = &vvc_dec->m_decApp.m_cDecLib;
	Slice* slice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx-1];
	vvc_frame_t *pic;
	vvc_frame_t *cur_pic = vvc_dec->cur_pic;
	vvc_frame_t *col_pic;
	//int32_t     AMVP_MAX_NUM_CANDS_MEM=3;
	//int32_t     AMVP_MAX_NUM_CANDS=2;
	//int32_t     NUM_CHROMA_MODE=5;
	//int32_t     DM_CHROMA_IDX=36;
	int32_t     above_ptr_ctrl =0;
	int32_t     buffer_linear =1;
	int32_t     cu_size_log2 =3;

	int32_t     mpred_mv_rd_start_addr ;
	int32_t     mpred_curr_lcu_x;
	int32_t     mpred_curr_lcu_y;
	int32_t     mpred_above_buf_start ;
	int32_t     mpred_mv_rd_ptr ;
	int32_t     mpred_mv_rd_ptr_p1 ;
	int32_t     mpred_mv_rd_end_addr;
	int32_t     MV_MEM_UNIT;
	int32_t     mpred_mv_wr_ptr ;
	//int32_t     *ref_poc_L0, *ref_poc_L1;

	int32_t     above_en;
	int32_t     mv_rd_en;
	int32_t     col_isIntra;

	Picture *col_picture;
	if (cur_pic == NULL) {
		cur_pic->error_mark = 1;
		hevc_print(hevc, 0, "Error %s, cur_pic is NULL\n", __func__);
		return -1;
	}
	if (slice == NULL) {
		cur_pic->error_mark = 1;
		hevc_print(hevc, 0, "Error %s, slice is NULL, m_uiSliceSegmentIdx-1 = %d\n",
			__func__, p_declib->m_uiSliceSegmentIdx-1);
		return -1;
	}
	col_picture = h266_get_col_picture(&vvc_dec->m_decApp, slice);
	if (col_picture)
		col_pic = col_picture->buf_cfg;
	else
		col_pic = cur_pic;

	if (col_pic == NULL) {
		cur_pic->error_mark = 1;
		hevc_print(hevc, 0, "Error %s, col_pic is NULL\n", __func__);
		return -1;
	}

	if (vvc_dec->slice_type != I_SLICE) {
		above_en=1;
		//mv_wr_en=1;
		//mv_rd_en=1;
		//col_isIntra=0;
	} else {
		above_en=1;
		//mv_wr_en=1;
		//mv_rd_en=0;
		//col_isIntra=0;
	}
#ifdef SAVE_NON_REF
	if (NON_REF_B) cur_pic->mv_wr_en = 0;
#endif
	if (vvc_dec->slice_type != I_SLICE || cur_pic->inter_slice_allowed_flag)
		cur_pic->mv_wr_en = 1;
	else
		cur_pic->mv_wr_en = 0;

	//if ((col_pic != cur_pic) && (col_pic->has_inter_slice
	//    || cur_pic->inter_slice_allowed_flag)) {
	if ((col_pic != cur_pic) && col_pic->mv_wr_en) {
		mv_rd_en = 1;
		col_isIntra = 0;
	} else {
		mv_rd_en = 0;
		col_isIntra = 1;
	}


	mpred_mv_rd_start_addr=col_pic->mpred_mv_wr_start_addr;
	data32 = READ_VREG(HEVC_MPRED_CURR_LCU);
	mpred_curr_lcu_x   = data32 & 0xffff;
	mpred_curr_lcu_y   = (data32 >> 16) & 0xffff;

	MV_MEM_UNIT = vvc_dec->lcu_size_log2 == 6 ? 0x200 : vvc_dec->lcu_size_log2 == 5 ? 0x80 : 0x20;
	mpred_mv_rd_ptr = mpred_mv_rd_start_addr  + (vvc_dec->slice_addr * MV_MEM_UNIT);

	mpred_mv_rd_ptr_p1  =mpred_mv_rd_ptr+MV_MEM_UNIT;
	mpred_mv_rd_end_addr=mpred_mv_rd_start_addr + ((vvc_dec->lcu_x_num*vvc_dec->lcu_y_num)*MV_MEM_UNIT);

	mpred_above_buf_start = buf_spec->mpred_above.buf_start;

	mpred_mv_wr_ptr = cur_pic->mpred_mv_wr_start_addr  + (vvc_dec->slice_addr*MV_MEM_UNIT);

	/*
	for (i = 0; i < PIC_POOL_SIZE; i++) {
		pic = &vvc_dec->pic_pool[i];
		if (pic->used > 0) {
			printk("WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_%02d_POC, %d)\n", pic->index, pic->poc);
			WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_xx_POC_adr[pic->index], pic->poc);
		}
	}*/
	for (i=0; i<col_pic->canvas_poc_list_size;i++) {
		hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_%02d_POC, %d)\n", i, col_pic->canvas_poc_list[i]);
		WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_xx_POC_adr[i], col_pic->canvas_poc_list[i]);
	}
	WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_LT, col_pic->canvas_lt_flag);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(HEVC_MPRED_COL_REF_CANVAS_LT, 0x%x)\n", col_pic->canvas_lt_flag);

	hevc_print(hevc, H266_DEBUG_REG_CFG, "%s: cur pic index %d poc %d slice addr 0x%x col pic index %d poc %d\n",
		__func__, cur_pic->index, cur_pic->poc, vvc_dec->slice_addr, col_pic->index, col_pic->poc);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "mv_wr_en %d mv_rd_en %d col_isIntra %d, slice type %d inter_slice_allowed_flag %d, col has_inter_slice %d, col inter_slice_allowed_flag %d\n",
		cur_pic->mv_wr_en, mv_rd_en, col_isIntra,
		vvc_dec->slice_type, cur_pic->inter_slice_allowed_flag,
		col_pic->has_inter_slice || cur_pic->inter_slice_allowed_flag);

	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR, cur_pic->mpred_mv_wr_start_addr);
	WRITE_VREG(VVC_MPP_AXI_CTL, 0x0100c100);
	WRITE_VREG(VVC_MPP_MV_WRPTR, cur_pic->mpred_mv_wr_start_addr);
	WRITE_VREG(VVC_MPP_LCU_INFO, ((vvc_dec->lcu_x_num)|(vvc_dec->lcu_y_num)<<16));
	data32 = (vvc_dec->vvc_monochrome << 6) | (vvc_dec->lcu_size_log2 << 2)|(cur_pic->mv_wr_en?0x0:0x2); // |vvc_dec->slice_type
	WRITE_VREG(VVC_MPP_SLICE_INFO, data32);
	WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR, mpred_mv_rd_start_addr);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "[MPRED CO_MV] write 0x%x  read 0x%x -- 0x%X\n", cur_pic->mpred_mv_wr_start_addr, mpred_mv_rd_start_addr, col_pic);
#if 0
	data32 = ((vvc_dec->lcu_x_num - hevc->tile_width_lcu)*MV_MEM_UNIT);
	WRITE_VREG(HEVC_MPRED_MV_WR_ROW_JUMP,data32);
	WRITE_VREG(HEVC_MPRED_MV_RD_ROW_JUMP,data32);
#endif

	data32 = READ_VREG(HEVC_MPRED_CTRL0);
	data32 = (
		vvc_dec->slice_type |
		/*hevc->new_pic<<2 |
		hevc->new_tile<<3|
		hevc->isNextSliceSegment<<4|
		hevc->TMVPFlag<<5|
		hevc->ColFromL0Flag<<7|*/
		slice->m_bCheckLDC<<6 |
		above_ptr_ctrl<<8 |
		above_en<<9|
		0<<10 | //mv_wr_en<<10|
		mv_rd_en<<11|
		col_isIntra<<12|
		buffer_linear<<13|
		/*hevc->LongTerm_Curr<<14|
		hevc->LongTerm_Col<<15|*/
		vvc_dec->lcu_size_log2<<16|
		cu_size_log2<<20 /*|
		hevc->plevel<<24*/
		);
	WRITE_VREG(HEVC_MPRED_CTRL0,data32);

	if (vvc_dec->lcu_size_log2 == 5) {
		WRITE_VREG(HEVC_MPRED_CTRL3,0x08080808);
	} else if (vvc_dec->lcu_size_log2 == 6) {
		WRITE_VREG(HEVC_MPRED_CTRL3,0x20102010);
	} else {
		WRITE_VREG(HEVC_MPRED_CTRL3,0x80208020);
	}

#if 0
	data32 = READ_VREG(HEVC_MPRED_CTRL1);
	data32  =   (
#ifdef DOS_PROJECT
		//no set in m8baby test1902
		(data32 & (0x1<<24)) |  // Don't override clk_forced_on ,
#endif
		hevc->MaxNumMergeCand |
		AMVP_MAX_NUM_CANDS<<4 |
		AMVP_MAX_NUM_CANDS_MEM<<8|
		NUM_CHROMA_MODE<<12|
		DM_CHROMA_IDX<<16
	);
	WRITE_VREG(HEVC_MPRED_CTRL1,data32);
#endif
	data32 = (
		cur_pic->width |
		cur_pic->height << 16
	);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE, data32);

	data32 = (
		(vvc_dec->lcu_x_num-1)   |
		(vvc_dec->lcu_y_num-1) << 16
	);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE_LCU,data32);

#if 0
	data32  =   (
		hevc->tile_start_lcu_x   |
		hevc->tile_start_lcu_y << 16
	);
	WRITE_VREG(HEVC_MPRED_TILE_START,data32);

	data32  =   (
		hevc->tile_width_lcu   |
		hevc->tile_height_lcu << 16
	);
	WRITE_VREG(HEVC_MPRED_TILE_SIZE_LCU,data32);
#endif
	data32 = (
		slice->m_aiNumRefIdx[REF_PIC_LIST_0]   |
		slice->m_aiNumRefIdx[REF_PIC_LIST_1]<<8|
		0
		//col_RefNum_L0<<16|
		//col_RefNum_L1<<24
	);
	WRITE_VREG(HEVC_MPRED_REF_NUM,data32);

#if 1 //def SUPPORT_LONG_TERM_RPS
	data32 = 0;
	data32_2 = 0;
	ii = 0;
	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_0] && i < MAX_NUM_REF; i++) {
		if (slice->m_apcRefPicList[REF_PIC_LIST_0][i] == NULL) {
			cur_pic->error_mark = 1;
			hevc_print(hevc, 0, "Error %s, slice->m_apcRefPicList[REF_PIC_LIST_0][%d] is NULL\n", __func__, i);
			return -1;
		}

		pic = get_ref_pic_by_POC(vvc_dec,  slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc);
		if (pic == NULL) {
			cur_pic->error_mark = 1;
			hevc_print(hevc, 0, "Error %s, no pic with poc %d in list 0\n", __func__, slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc);
			return -1;
		}
		if (h266_is_long_term(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc))
			data32 = data32 | (1 << i);

		hevc_print(hevc, H266_DEBUG_REG_CFG, "%s: ref0[%d]: poc %d, index %d\n",
			__func__, i, slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc, pic->index);
		data32_2 |= ((pic->index & 0x1f) << ii);
		ii += 5;
		if (ii >= 30) {
			data32_2 |= ((i / 6) << 30);
			hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(VVC_MPP_CANVAS_ID_L0, 0x%x)\n", data32_2);
			WRITE_VREG(VVC_MPP_CANVAS_ID_L0, data32_2);
			data32_2 = 0;
			ii = 0;
		}
	}
	if (ii != 0) {
		data32_2 |= ((i / 6) << 30);
		hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(VVC_MPP_CANVAS_ID_L0, 0x%x)\n", data32_2);
		WRITE_VREG(VVC_MPP_CANVAS_ID_L0, data32_2);
	}

	data32_2 = 0;
	ii = 0;
	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_1] && i < MAX_NUM_REF; i++) {
		if (slice->m_apcRefPicList[REF_PIC_LIST_1][i] == NULL) {
			cur_pic->error_mark = 1;
			hevc_print(hevc, 0, "Error %s, slice->m_apcRefPicList[REF_PIC_LIST_1][%d] is NULL\n", __func__, i);
			return -1;
		}
		pic = get_ref_pic_by_POC(vvc_dec,  slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
		if (pic == NULL) {
			cur_pic->error_mark = 1;
			hevc_print(hevc, 0, "Error %s, no pic with poc %d in list 1\n", __func__, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);
			return -1;
		}
		if (h266_is_long_term(vvc_dec, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc))
			data32 = data32 | (1 << (i + 16));

		hevc_print(hevc, H266_DEBUG_REG_CFG, "%s: ref1[%d]: poc %d, index %d\n",
			__func__, i, slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc, pic->index);
		data32_2 |= ((pic->index & 0x1f) << ii);
		ii += 5;
		if (ii >= 30) {
			data32_2 |= ((i / 6) << 30);
			hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(VVC_MPP_CANVAS_ID_L1, 0x%x)\n", data32_2);
			WRITE_VREG(VVC_MPP_CANVAS_ID_L1, data32_2);
			data32_2 = 0;
			ii = 0;
		}
	}
	if (ii != 0) {
		data32_2 |= ((i/6)<<30);
		hevc_print(hevc, H266_DEBUG_REG_CFG, "WRITE_VREG(VVC_MPP_CANVAS_ID_L1, 0x%x)\n", data32_2);
		WRITE_VREG(VVC_MPP_CANVAS_ID_L1, data32_2);
	}
	hevc_print(hevc, H266_DEBUG_REG_CFG, "LongTerm_Ref 0x%x\n", data32);
#endif
	WRITE_VREG(HEVC_MPRED_LT_REF,data32);


	data32 = 0;
	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_0] && i < MAX_NUM_REF; i++) data32 = data32 | (1 << i);
		WRITE_VREG(HEVC_MPRED_REF_EN_L0, data32);

	data32 = 0;
	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_1] && i < MAX_NUM_REF; i++) data32 = data32 | (1 << i);
		WRITE_VREG(HEVC_MPRED_REF_EN_L1, data32);


	WRITE_VREG(HEVC_MPRED_CUR_POC, cur_pic->poc);
#if 1
	//WRITE_VREG(HEVC_MPRED_COL_POC, hevc->Col_POC);
	WRITE_VREG(HEVC_MPRED_COL_POC, col_pic->poc);
#endif
	//below MPRED Ref_POC_xx_Lx registers must follow Ref_POC_xx_L0 -> Ref_POC_xx_L1 in pair write order!!!
	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_0] && i < MAX_NUM_REF; i++)
		WRITE_VREG(HEVC_MPRED_L0_REF_POC_ADR[i], slice->m_apcRefPicList[REF_PIC_LIST_0][i]->poc);

	for (i = 0; i < slice->m_aiNumRefIdx[REF_PIC_LIST_1] && i < MAX_NUM_REF; i++)
		WRITE_VREG(HEVC_MPRED_L1_REF_POC_ADR[i], slice->m_apcRefPicList[REF_PIC_LIST_1][i]->poc);

	//if (hevc->new_pic)
	//{
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, mpred_above_buf_start);
	WRITE_VREG(HEVC_MPRED_MV_WPTR, mpred_mv_wr_ptr);
	//WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_ptr);
	WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_start_addr);
	//}
	//else if (!hevc->isNextSliceSegment)
	//{
	//    //WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr_p1);
	//    WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr);
	//}

	WRITE_VREG(HEVC_MPRED_MV_RD_END_ADDR, mpred_mv_rd_end_addr);
	return 0;
}

static int config_scale_hw(struct hevc_state_s *hevc)
{
/*
`VVC_MPP_RPR_REFINFO    [31]         -- 0: refL0 1: refL1   TODO
    [30:27]   -- refIdx
    [26:24]   -- field config -- 000 : [14:0] refpicWidth
                                                001 : [14:0] refpicHeight
                                                010 : [15:0] scaleX( C model m_scalingRatio.x )
                                                011 : [15:0] scaleY( C model m_scalingRatio.y )
                                                100 : [18:0] ref_left_win_scaling_offset( C model refpic -> m_winLeftOffset )   bitwidth unsure
                                                101 : [18:0] ref_top_win_scaling_offset( C model refpic -> m_winTopOffset )
`VVC_MPP_SCALING_WIN_OFFSET [18:0]  -- currpic_scaling_win_offset( C model currpic -> m_winLeftOffset/m_winTopOffset )  TODO
    [19]      -- 0: left 1: top
`VVC_MPP_CHROMA_COLLOCATED_FLAG [0]        -- C model refpic -> m_horCollocatedChromaFlag   TODO
    [1]        -- C model refpic -> m_verCollocatedChromaFlag            For now, all refpics share the same chroma collocated flag ?
`VP9D_MPP_REF_SCALE_ENBL    [15:0]    -- refL0 Idx 0~15 is scaled   TODO
    [31 :16] -- refL1 Idx 0~15 is scaled
*/
	//int32_t i, ii;
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	vvc_frame_t *cur_pic = vvc_dec->cur_pic;
	int iRefList, iRefIndex;
	uint32_t data32, data32_2;
	DecLib *p_declib = &vvc_dec->m_decApp.m_cDecLib;
	param_t *param = &vvc_dec->param;
	Slice* slice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx-1];
	//vvc_frame_t *pic;
	//vvc_frame_t *cur_pic = vvc_dec->cur_pic;

	hevc_print(hevc, H266_DEBUG_REG_CFG, "%s\n", __func__);
	data32_2 = 0;
	if (cur_pic == NULL) {
		hevc_print(hevc, 0, "Error %s, cur_pic is NULL\n", __func__);
		return -1;
	}
	if (slice == NULL) {
		cur_pic->error_mark = 1;
		hevc_print(hevc, 0, "Error %s, slice is NULL\n", __func__);
		return -1;
	}
	for (iRefList = 0; iRefList < 2; iRefList++) {
		for (iRefIndex = 0; iRefIndex < slice->m_aiNumRefIdx[iRefList] && iRefIndex < MAX_NUM_REF; iRefIndex++) {
			ScaleRatio *scaleRatio = getScalingRatio(slice, iRefList, iRefIndex );
			Picture *refPic = slice->m_apcRefPicList[iRefList][iRefIndex]->unscaledPic;

			if (refPic == NULL) {
				cur_pic->error_mark = 1;
				hevc_print(hevc, 0, "Error %s, refPic is NULL\n", __func__);
				return -1;
			}
			if (refPic->buf_cfg == NULL) {
				cur_pic->error_mark = 1;
				hevc_print(hevc, 0, "Error %s, refPic->buf_cfg is NULL\n", __func__);
				return -1;
			}
			//data32_2 |= refPic->m_scalingWindow.m_enabledFlag << ((iRefList<<4)+iRefIndex);
			data32_2 |= h266_is_ref_scaled(slice, iRefList, iRefIndex) << ((iRefList<<4)+iRefIndex);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				0 << 24 |
				refPic->buf_cfg->width;
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				1 << 24 |
				refPic->buf_cfg->height;
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				2 << 24 |
				scaleRatio->first;
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				3 << 24 |
				scaleRatio->second;
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				4 << 24 |
				(refPic->m_scalingWindow.m_winLeftOffset & 0x7FFFF);
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);

			data32 = iRefList << 31 |
				iRefIndex << 27 |
				5 << 24 |
				(refPic->m_scalingWindow.m_winTopOffset & 0x7FFFF);
			WRITE_VREG(VVC_MPP_RPR_REFINFO, data32);
		}
	}
	WRITE_VREG(VP9D_MPP_REF_SCALE_ENBL, data32_2);

	data32 = 0 << 19 |
		(p_declib->m_pcPic->m_scalingWindow.m_winLeftOffset & 0x7FFFF);
	WRITE_VREG(VVC_MPP_SCALING_WIN_OFFSET, data32);

	data32 = 1 << 19 |
		(p_declib->m_pcPic->m_scalingWindow.m_winTopOffset & 0x7FFFF);
	WRITE_VREG(VVC_MPP_SCALING_WIN_OFFSET, data32);

	data32 = (((param->p.sps_decoding_flags_4 >> 7) & 0x1) << 1) | //VerCollocatedChromaFlag
		((param->p.sps_decoding_flags_4 >> 8) & 0x1); //HorCollocatedChromaFlag
	WRITE_VREG(VVC_MPP_CHROMA_COLLOCATED_CFG, data32);

	return 0;
}

static void config_sao_hw(struct hevc_state_s *hevc)
{
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	vvc_frame_t *pic = vvc_dec->cur_pic;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;
	//union param_u* params = &vvc_dec->param;
	int dw_mode = get_double_write_mode(hevc);
	uint32_t data32;
	int32_t pic_width = vvc_dec->cur_pic->width;
	int32_t pic_height = vvc_dec->cur_pic->height;
	int32_t lcu_size_log2 = vvc_dec->lcu_size_log2;
	int32_t lcu_size = 1<<lcu_size_log2;
	int32_t pic_width_lcu  = vvc_dec->lcu_x_num;
	int32_t pic_height_lcu = vvc_dec->lcu_y_num;
	int32_t lcu_total = vvc_dec->lcu_total;
	int32_t mc_buffer_size_u_v = lcu_total*lcu_size*lcu_size/2;
#ifdef P010_DW_ENABLE
	int32_t mc_buffer_size_u_v_h = ((mc_buffer_size_u_v + 0xffff) >> 16) * 2; //64k alignment and double for P010
#else
	int32_t mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16; //64k alignment
#endif
	hevc_print(hevc, H266_DEBUG_REG_CFG,
		"config_sao_hw: lcu_size_log2 %d, pic_width %d (pic_width_lcu %d) pic_height %d (pic_height_lcu %d)\n",
		lcu_size_log2, pic_width, pic_width_lcu, pic_height, pic_height_lcu);

#ifdef DW_NO_SCALE
	WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16) );
#endif
#ifdef DIFF_DW
	if (READ_VREG(HEVC_SAO_CTRL9) & (1<<10) == 0) { // dw cm_mode : 0=old compress mode, 1=mmu mode
		if (frame_width > 4096) {
			WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16) | (0x33 << 16));
		} else if (frame_width > 1920) {
			WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16) | (0xff << 16));
		} else if (frame_width > 1280) {
			WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16) | (0x33 << 16));
		} else {
			WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16) | (0x00 << 16));
			//WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff<<16) | (0x33<<16));
			//WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff<<16) | (0xff<<16));
		}
	} // only NV21 DW
#endif
	/*copy from h265*/
	if ((dw_mode & 0x10) == 0) {
			data32 = READ_VREG(HEVC_SAO_CTRL5);
			data32 &= (~(0xff << 16));

			if (((dw_mode & 0xf) == 8) ||
			((dw_mode & 0xf) == 9)) {
				data32 |= (0xff << 16);
				WRITE_VREG(HEVC_SAO_CTRL5, data32);
		} else {
			if ((dw_mode & 0xf) == 2 ||
			(dw_mode & 0xf) == 3)
				data32 |= (0xff<<16);
			else if ((dw_mode & 0xf) == 4 ||
			(dw_mode & 0xf) == 5)
				data32 |= (0x33<<16);
			WRITE_VREG(HEVC_SAO_CTRL5, data32);
		}
	}
	/**/
	data32 = READ_VREG(HEVC_SAO_CTRL3);
	data32 |= (0x1 << 0); /* vvc mode */
	WRITE_VREG(HEVC_SAO_CTRL3, data32);

	// TODO WARNING across_tile/slice setting
	data32 = READ_VREG(HEVC_SAO_CTRL0);
	data32 &= (~0xf);
	data32 |= vvc_dec->lcu_size_log2;
	WRITE_VREG(HEVC_SAO_CTRL0, data32);

	data32 = (
		pic_width |
		pic_height << 16
	);
	WRITE_VREG(HEVC_SAO_PIC_SIZE , data32);

	data32 = (
		(pic_width_lcu - 1) |
		(pic_height_lcu - 1) << 16
	);
	WRITE_VREG(HEVC_SAO_PIC_SIZE_LCU , data32);

	//if (hevc->new_pic) {
	if (vvc_dec->cur_pic->new_picture) {
		//if (1) { //TO_DO, to change ...
		WRITE_VREG(HEVC_SAO_Y_START_ADDR,0xffffffff);
#if 1 //def OW_TRIPLE_WRITE
		WRITE_VREG(HEVC_SAO_Y_START_ADDR3, 0xffffffff);
#endif
	}
	data32 = pic->mc_y_adr;
	//WRITE_VREG(HEVC_SAO_Y_START_ADDR,data32);
#ifdef LOSLESS_COMPRESS_MODE
	if (dw_mode && ((dw_mode & 0x20) == 0))
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, pic->dw_y_adr);
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable)
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0);
#endif
#ifdef OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_Y_START_ADDR3, TRIPLE_WRITE_YSTART);
#endif
	if ((dw_mode & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, data32);
#ifdef VVC_10B_MMU
	WRITE_VREG(HEVC_CM_HEADER_START_ADDR, pic->header_adr);
#endif
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable)
		WRITE_VREG(HEVC_CM_HEADER_START_ADDR2, pic->header_dw_adr);
#endif

#else
	WRITE_VREG(HEVC_SAO_Y_START_ADDR,data32);
#ifdef OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_Y_START_ADDR3, data32);
#endif
#endif

	data32 = (mc_buffer_size_u_v_h<<16)<<1;
	WRITE_VREG(HEVC_SAO_Y_LENGTH ,data32);
#if 1 //def OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_Y_LENGTH3 ,data32);
#endif

#ifndef LOSLESS_COMPRESS_MODE
	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_START_ADDR,data32);
#ifdef OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_C_START_ADDR3, data32);
#endif
#else
	if (dw_mode && ((dw_mode & 0x20) == 0))
		WRITE_VREG(HEVC_SAO_C_START_ADDR, pic->dw_u_v_adr);
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable)
		WRITE_VREG(HEVC_SAO_C_START_ADDR, 0);
#endif
#ifdef OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_C_START_ADDR3, TRIPLE_WRITE_CSTART);
#endif
#endif

	data32 = (mc_buffer_size_u_v_h<<16);
	WRITE_VREG(HEVC_SAO_C_LENGTH  ,data32);
#if 1 //def OW_TRIPLE_WRITE
	WRITE_VREG(HEVC_SAO_C_LENGTH3 ,data32);
#endif

#ifndef LOSLESS_COMPRESS_MODE
	/* multi tile to do... */
	data32 = cur_pic->mc_y_adr;
	WRITE_VREG(HEVC_SAO_Y_WPTR ,data32);

	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_WPTR ,data32);
#else
	if (dw_mode && ((dw_mode & 0x20) == 0)) {
		WRITE_VREG(HEVC_SAO_Y_WPTR, pic->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, pic->dw_u_v_adr);
	}
#endif

#if 0
#ifdef DOS_PROJECT
#ifdef VVC_10B_NV21
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (hevc->mem_map_mode << 12); // [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
#ifdef SAVE_NON_REF
	if (NON_REF_B) WRITE_VREG(HEVC_SAO_CTRL1, data32 | (0x1<<0)); // disable compress output
	else WRITE_VREG(HEVC_SAO_CTRL1, data32 & 0xfffffffe);
#endif

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (hevc->mem_map_mode << 4); // [5:4]    -- address_format 00:linear 01:32x32 10:64x32
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
#else
	// m8baby test1902
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (hevc->mem_map_mode << 12); // [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
	data32 &= (~0xff0);
	//data32 |= 0x670;  // Big-Endian per 64-bit
	data32 |= 0x880;  // Big-Endian per 64-bit
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
#ifdef SAVE_NON_REF
	if (NON_REF_B) WRITE_VREG(HEVC_SAO_CTRL1, data32 | (0x1<<0)); // disable compress output
	else WRITE_VREG(HEVC_SAO_CTRL1, data32 & 0xfffffffe);
#endif

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (hevc->mem_map_mode << 4); // [5:4]    -- address_format 00:linear 01:32x32 10:64x32
	data32 &= (~0xF);
	data32 |= 0x8;    // Big-Endian per 64-bit
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif

#else
	/*
	copy from h265:
	config HEVC_SAO_CTRL1 and HEVCD_IPP_AXIIF_CONFIG
	*/
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	/* [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32 */
	data32 |= (hevc->mem_map_mode << 12);
	data32 &= (~0xff0);
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable == 0)
		data32 |= ((hevc->endian >> 8) & 0xfff);
#else
	data32 |= ((hevc->endian >> 8) & 0xfff);    /* data32 |= 0x670; Big-Endian per 64-bit */
#endif
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
	if (dw_mode == 0)
		data32 |= 0x2; /*disable double write*/
	else if (dw_mode & 0x10)
		data32 |= 0x1; /*disable cm*/

	data32 &= (~(3 << 14));
	if (is_hevc_align32(hevc->mem_map_mode)) {
		data32 |= (1 << 14);
	} else {
		data32 |= (2 << 14);
	}
	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		data32 &= ~(1 << 8); /* NV21 */
	else
		data32 |= (1 << 8); /* NV12 */
	/*
	*  [31:24] ar_fifo1_axi_thread
	*  [23:16] ar_fifo0_axi_thread
	*  [15:14] axi_linealign, 0-16bytes, 1-32bytes, 2-64bytes
	*  [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
	*  [11:08] axi_lendian_C
	*  [07:04] axi_lendian_Y
	*  [3]     reserved
	*  [2]     clk_forceon
	*  [1]     dw_disable:disable double write output
	*  [0]     cm_disable:disable compress output
	*/
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
	if (dw_mode & 0x10) {
		/* [23:22] dw_v1_ctrl
		*[21:20] dw_v0_ctrl
		*[19:18] dw_h1_ctrl
		*[17:16] dw_h0_ctrl
		*/
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]    -- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (hevc->mem_map_mode << 4);
	data32 &= (~0xF);
	data32 |= (hevc->endian & 0xf);  /* valid only when double write only */

	data32 &= (~(3 << 8));
	if (is_hevc_align32(hevc->mem_map_mode)) {
		data32 |= (1 << 8);
	} else {
		data32 |= (2 << 8);
	}
	/*
	* [3:0]   little_endian
	* [5:4]   address_format 00:linear 01:32x32 10:64x32
	* [7:6]   reserved
	* [9:8]   Linear_LineAlignment 00:16byte 01:32byte 10:64byte
	* [11:10] reserved
	* [12]    CbCr_byte_swap
	* [31:13] reserved
	*/
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);


#endif

#if 0  // moved to with other VH setting
	//def VVC_10B_NV21
#ifdef VVC_10B_MMU_DW
	WRITE_VREG(HEVC_DW_VH0_ADDDR, DOUBLE_WRITE_VH0_TEMP);
	WRITE_VREG(HEVC_DW_VH1_ADDDR, DOUBLE_WRITE_VH1_TEMP);
#endif
#endif

	data32 = READ_VREG(HEVC_SAO_CTRL3);
	WRITE_VREG(HEVC_SAO_CTRL3, data32 | (0x1 << 2)); //enable triple write

	hevc_print(hevc, H266_DEBUG_REG_CFG, "[c] cfgSAO .done.\n");
}

/*
 * alf related functions
 */

static void config_alf_hw(struct hevc_state_s *hevc)
{
	/*
	* Picture level ALF parameter configuration here
	*/

	//struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	hevc_print(hevc, H266_DEBUG_REG_CFG, "[c] cfgALF .done.\n");
}

static void  config_mcrcc_axi_hw(struct hevc_state_s *hevc)
{
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	uint32_t rdata32;
	uint32_t rdata32_2;

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2); // reset mcrcc
	if (vvc_dec->slice_type == SLICE_I) {
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0); // remove reset -- disables clock
		return;
	}

#if 0
	mcrcc_get_hitrate();
	decomp_get_hitrate();
	decomp_get_comprate();
#endif
	if ((vvc_dec->slice_type == SLICE_B) || (vvc_dec->slice_type == SLICE_P)) {
		// Programme canvas0
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | ( rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		// Programme canvas1
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (16 << 8) | (1 << 1) | 0);
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | ( rdata32_2 << 16);
		if ( rdata32 == rdata32_2 ) {
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | ( rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else { // P-PIC
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (1 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | ( rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		// Programme canvas1
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | ( rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
	}

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0); // enable mcrcc progressive-mode
	return;
}

static void print_pic_pool(struct hevc_state_s *hevc, char *mark)
{
	vvc_frame_t * pic;
	int i;
	int used_count = 0;

	for (i = 0; i < PIC_POOL_SIZE; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic->used)
			used_count++;
	}
	hevc_print(hevc, 0, "%s----pic_pool (used %d, total %d):\n", mark, used_count, hevc->used_buf_num);
	for (i = 0; i < hevc->used_buf_num; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		hevc_print(hevc, 0, "%d(%d,%d): used %d, decode_idx %d, mv_idx %d, %spoc %d, referenced %d, decode_done %d, error_mark 0x%x, vf_ref %d, w/h/depth(%d,%d,%d), slicetype %d, mv_wr_start 0x%x cam addr:%lx\n",
			i, pic->index, pic->BUF_index, pic->used, pic->decode_idx,
#ifndef MV_USE_FIXED_BUF
			pic->mv_buf_index,
#else
			-1,
#endif
			pic->referenced && h266_is_long_term(hevc->vvc_dec, pic->poc)?"LT ":"",
			pic->poc, pic->referenced,
			pic->decode_done, pic->error_mark, pic->vf_ref,
			pic->width, pic->height, pic->depth,
			pic->slice_type,
			pic->mpred_mv_wr_start_addr,
			pic->cma_alloc_addr
			);
	}
}

#ifdef USE_OLD_CHIP
static void parser_cmd_write(void)
{
	u32 i;
	const unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
		0x0401, 0x8401, 0x0800, 0x0402, 0x9002, 0x1423,
		0x8CC3, 0x1423, 0x8804, 0x9825, 0x0800, 0x04FE,
		0x8406, 0x8411, 0x1800, 0x8408, 0x8409, 0x8C2A,
		0x9C2B, 0x1C00, 0x840F, 0x8407, 0x8000, 0x8408,
		0x2000, 0xA800, 0x8410, 0x04DE, 0x840C, 0x840D,
		0xAC00, 0xA000, 0x08C0, 0x08E0, 0xA40E, 0xFC00,
		0x7C00
	};
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
}
#endif
#ifndef USE_OLD_CHIP
static void lpf_init(struct hevc_state_s *hevc) // lpf initialization :: update for every bitstream
{
	uint32_t data32;

	data32 = READ_VREG(HEVC_DBLK_CFGB);
	data32 |= (7 << 0);
	WRITE_VREG(HEVC_DBLK_CFGB, data32); // [3:0] cfg_video_type -> H266/VVC

#ifdef LPF_LINEBUF_MODE_CTU_BASED //must define it for DUAL_CORE
	WRITE_VREG(HEVC_DBLK_CFG0, (0 << 18) |  // ctu based line buffer storage mode
							(1 << 17) |  // dblk cpi_cmn wrrsp mode
							(1 << 0));  // rst_sync(will be self cleared)
#else
	WRITE_VREG(HEVC_DBLK_CFG0, (1 << 18) |  // tile based line buffer storage mode
							(1 << 17) |  // dblk cpi_cmn wrrsp mode
							(1 << 0));  // rst_sync(will be self cleared)
#endif

	data32 = READ_VREG(HEVC_DBLK_CFG1) & ~(0x3ff << 20);
	WRITE_VREG(HEVC_DBLK_CFG1, data32 | (0x3 << 20)); // SPCC enable & using slice address from ucode

	WRITE_VREG(HEVC_DBLK_CFG3, 0x808040); // axi left address offset set for 8k, WARNING TODO TODO TODO REVIEW REVIEW REVIEW

	hevc_print(hevc, H266_DEBUG_REG_CFG, "cfgLPF::Bitstream Initialize ... Done\n");
}
#endif

static void config_lpf_hw(struct hevc_state_s *hevc, int32_t dbg_pic_cnt) // lpf settings :: updating per picture
{
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	union param_u *rpm_param = &vvc_dec->param;
	uint32_t data32;

	uint32_t pic_width = rpm_param->p.pic_width_in_luma_samples;
	uint32_t pic_height = rpm_param->p.pic_height_in_luma_samples;
	uint32_t lcu_size = 1 << (rpm_param->p.lcu_size);
	uint32_t bit_depth = 8 + (rpm_param->p.sps_bitdepth_minus8);
	// uint32_t loop_filter_disable = rpm_param->p.loop_filter_disable;
	//uint32_t misc_flag0 = vvc_dec->misc_flag0;

	// bit[3]  - SingleSlicePerSubPicFlag(pps_single_slice_per_subpic_flag)
	// bit[2]  - LoopFilterAcrossSlicesEnabledFlag(pps_loop_filter_across_slices_enabled_flag)
	// bit[1]  - LoopFilterAcrossTilesEnabledFlag(pps_loop_filter_across_tiles_enabled_flag)
	// bit[0]  - RectSliceFlag(pps_rect_slice_flag)
	uint32_t tile_flags_0 = rpm_param->p.tile_flags_0;

	// bit[15]     - IBCFlag (sps_ibc_enabled_flag)
	// bit[14:12]  - MaxNumIBCMergeCand (IBC_MRG_MAX_NUM_CANDS-sps_six_minus_max_num_ibc_merge_cand)
	// bit[11]     - LadfEnabled (sps_ladf_enabled_flag)
	// bit[10]     - ScalingListFlag (sps_explicit_scaling_list_enabled_flag)
	// bit[9]      - DisableScalingMatrixForLfnstBlks (sps_scaling_matrix_for_lfnst_disabled_flag)
	// bit[8]      - DepQuantEnabledFlag (sps_dep_quant_enabled_flag)
	// bit[7]      - SignDataHidingEnabledFlag (sps_sign_data_hiding_enabled_flag)
	// bit[6]      - VirtualBoundariesEnabledFlag (sps_virtual_boundaries_enabled_flag)
	// bit[5]      - GeneralHrdParametersPresentFlag (sps_timing_hrd_params_present_flag)
	// bit[4]      - FieldSeqFlag (sps_field_seq_flag)
	// bit[3]      - VuiParametersPresentFlag (sps_vui_parameters_present_flag)
	// bit[2]      - TSRCRicePresentFlag(extended_precision_processing_flag)
	// bit[1]      - VirtualBoundariesPresentFlag(sps_loop_filter_across_virtual_boundaries_present_flag)
	// bit[0]      - Reserved
	uint32_t sps_decoding_flags_5 = rpm_param->p.sps_decoding_flags_5;

	// bit[15] sps_virtual_boundaries_present_flag
	// bit[14:8] sps_num_hor_virtual_boundaries
	// bit[7:0] sps_num_ver_virtual_boundaries
	uint32_t sps_virtual_boundary_num            = rpm_param->p.sps_virtual_boundary_num           ;
	uint32_t sps_virtual_boundary_pos_x_minus1_0 = rpm_param->p.sps_virtual_boundary_pos_x_minus1_0;
	uint32_t sps_virtual_boundary_pos_x_minus1_1 = rpm_param->p.sps_virtual_boundary_pos_x_minus1_1;
	uint32_t sps_virtual_boundary_pos_x_minus1_2 = rpm_param->p.sps_virtual_boundary_pos_x_minus1_2;
	uint32_t sps_virtual_boundary_pos_y_minus1_0 = rpm_param->p.sps_virtual_boundary_pos_y_minus1_0;
	uint32_t sps_virtual_boundary_pos_y_minus1_1 = rpm_param->p.sps_virtual_boundary_pos_y_minus1_1;
	uint32_t sps_virtual_boundary_pos_y_minus1_2 = rpm_param->p.sps_virtual_boundary_pos_y_minus1_2;

	// bit[15] ph_virtual_boundaries_present_flag
	// bit[14:8] ph_num_hor_virtual_boundaries
	// bit[7:0] ph_num_ver_virtual_boundaries
	uint32_t ph_virtual_boundary_num            = rpm_param->p.ph_virtual_boundary_num           ;
	uint32_t ph_virtual_boundary_pos_x_minus1_0 = rpm_param->p.ph_virtual_boundary_pos_x_minus1_0;
	uint32_t ph_virtual_boundary_pos_x_minus1_1 = rpm_param->p.ph_virtual_boundary_pos_x_minus1_1;
	uint32_t ph_virtual_boundary_pos_x_minus1_2 = rpm_param->p.ph_virtual_boundary_pos_x_minus1_2;
	uint32_t ph_virtual_boundary_pos_y_minus1_0 = rpm_param->p.ph_virtual_boundary_pos_y_minus1_0;
	uint32_t ph_virtual_boundary_pos_y_minus1_1 = rpm_param->p.ph_virtual_boundary_pos_y_minus1_1;
	uint32_t ph_virtual_boundary_pos_y_minus1_2 = rpm_param->p.ph_virtual_boundary_pos_y_minus1_2;

	// bit[15]     - Rpl1IdxPresentFlag (pps_rpl1_idx_present_flag)
	// bit[14]     - UseWP (pps_weighted_pred_flag)
	// bit[13]     - WPBiPred (pps_weighted_bipred_flag)
	// bit[12]     - WrapAroundEnabledFlag (pps_ref_wraparound_enabled_flag)
	// bit[11]     - UseDQP (pps_cu_qp_delta_enabled_flag)
	// bit[10]     - PPSChromaToolFlag (pps_chroma_tool_offsets_present_flag)
	// bit[9]      - JointCbCrQpOffsetPresentFlag (pps_joint_cbcr_qp_offset_present_flag)
	// bit[8]      - SliceChromaQpFlag (pps_slice_chroma_qp_offsets_present_flag)
	// bit[7]      - ChromaQpOffsetListEnableFlag (pps_cu_chroma_qp_offset_list_enabled_flag)
	// bit[6]      - DeblockingFilterControlPresentFlag (pps_deblocking_filter_control_present_flag)
	// bit[5]      - PictureHeaderExtensionPresentFlag (pps_picture_header_extension_present_flag)
	// bit[4]      - SliceHeaderExtensionPresentFlag (pps_slice_header_extension_present_flag)
	// bit[3]      - PPSExtensionFlag (pps_extension_flag)
	// bit[2]      - Reserved -- RectSliceFlag(pps_rect_slice_flag)
	// bit[1:0]  reserved
	uint32_t pps_decoding_flags_1 = rpm_param->p.pps_decoding_flags_1;

	// bit[2] - DbfInfoInPhFlag(pps_dbf_info_in_ph_flag)
	// bit[1] - DeblockingFilterOverrideEnabledFlag(pps_deblocking_filter_override_enabled_flag)
	// bit[0] - PPSDeblockingFilterDisabledFlag(pps_deblocking_filter_disabled_flag)
	uint32_t pps_dbf_info = rpm_param->p.pps_dbf_info;

	// bit[1]      - DeblockingFilterOverrideFlag(ph_deblocking_params_present_flag)
	// bit[0]      - DeblockingFilterDisable(ph_deblocking_filter_disabled_flag)
	uint32_t ph_dbf_info = rpm_param->p.ph_dbf_info;

	// bit[1] - DeblockingFilterOverrideFlag(sh_deblocking_params_present_flag)
	// bit[0] - DeblockingFilterDisable(sh_deblocking_filter_disabled_flag)
	uint32_t sh_dbf_info = rpm_param->p.sh_dbf_info;

	int32_t beta_tc_offset_div2 = rpm_param->p.pps_beta_tc_offset_div2;
	int32_t cb_beta_tc_offset_div2 = ((pps_decoding_flags_1>>10)&1) ? rpm_param->p.pps_cb_beta_tc_offset_div2 :
	                                          rpm_param->p.pps_beta_tc_offset_div2;
	int32_t cr_beta_tc_offset_div2 = ((pps_decoding_flags_1>>10)&1) ? rpm_param->p.pps_cr_beta_tc_offset_div2 :
	                                          rpm_param->p.pps_beta_tc_offset_div2;

	int32_t picheader_dblk_override_flag = 0;
	int32_t picheader_dblk_disabled_flag;
	int32_t sliceheader_dblk_override_flag = 0;
	int32_t sliceheader_dblk_disabled_flag;
	if ((pps_decoding_flags_1 >> 6) & 1) { // pps_deblocking_filter_control_present_flag
		if ((pps_dbf_info >> 2) & 1) // pps_dbf_info_in_ph_flag
			picheader_dblk_override_flag = (ph_dbf_info >> 1) & 1; // DeblockingFilterOverrideFlag(ph_deblocking_params_present_flag)
		else
			picheader_dblk_override_flag = 0;

		if (picheader_dblk_override_flag) {
			if (!((pps_dbf_info >> 0) & 1)) // pps_deblocking_filter_disabled_flag
				picheader_dblk_disabled_flag = (ph_dbf_info >> 0) & 1; // DeblockingFilterDisable(ph_deblocking_filter_disabled_flag)
			else
				picheader_dblk_disabled_flag = 0;

			if (!picheader_dblk_disabled_flag) {
				beta_tc_offset_div2 = rpm_param->p.ph_beta_tc_offset_div2;
				if ((pps_decoding_flags_1 >> 10) & 1) { // pps_chroma_tool_offsets_present_flag
					cb_beta_tc_offset_div2 = rpm_param->p.ph_cb_beta_tc_offset_div2;
					cr_beta_tc_offset_div2 = rpm_param->p.ph_cr_beta_tc_offset_div2;
				} else {
					cb_beta_tc_offset_div2 = rpm_param->p.ph_beta_tc_offset_div2;
					cr_beta_tc_offset_div2 = rpm_param->p.ph_beta_tc_offset_div2;
				}
			}
		}
		else
			picheader_dblk_disabled_flag = (pps_dbf_info >> 0) & 1;
	} else {
		picheader_dblk_disabled_flag = 0;
		beta_tc_offset_div2 = 0;
		cb_beta_tc_offset_div2 = 0;
		cr_beta_tc_offset_div2 = 0;
	}


	if ((pps_decoding_flags_1 >> 6) & 1) { // pps_deblocking_filter_control_present_flag
		if (((pps_dbf_info >> 1) & 1) && !((pps_dbf_info >> 2) & 1)) // pps_deblocking_filter_override_enabled_flag & !pps_dbf_info_in_ph_flag
			sliceheader_dblk_override_flag = (sh_dbf_info >> 1) & 1; // DeblockingFilterOverrideFlag(sh_deblocking_params_present_flag)
		else
			sliceheader_dblk_override_flag = 0;

		if (sliceheader_dblk_override_flag) {
			if (!((pps_dbf_info >> 0) & 1)) // pps_deblocking_filter_disabled_flag
				sliceheader_dblk_disabled_flag = (sh_dbf_info >> 0) & 1; // DeblockingFilterDisable(sh_deblocking_filter_disabled_flag)
			else
				sliceheader_dblk_disabled_flag = 0;

			if (!sliceheader_dblk_disabled_flag) {
				beta_tc_offset_div2 = rpm_param->p.sh_beta_tc_offset_div2;
				if ((pps_decoding_flags_1 >> 10) & 1) { // pps_chroma_tool_offsets_present_flag
					cb_beta_tc_offset_div2 = rpm_param->p.sh_cb_beta_tc_offset_div2;
					cr_beta_tc_offset_div2 = rpm_param->p.sh_cr_beta_tc_offset_div2;
				} else {
					cb_beta_tc_offset_div2 = rpm_param->p.sh_beta_tc_offset_div2;
					cr_beta_tc_offset_div2 = rpm_param->p.sh_beta_tc_offset_div2;
				}
			}
		} else
			sliceheader_dblk_disabled_flag = picheader_dblk_disabled_flag;
	} else {
		sliceheader_dblk_disabled_flag = 0;
		beta_tc_offset_div2 = 0;
		cb_beta_tc_offset_div2 = 0;
		cr_beta_tc_offset_div2 = 0;
	}
	{
		int32_t virtualboundary_sps_enabled = (sps_decoding_flags_5 >> 6) & 0x1;
		int32_t virtualboundary_sps_present = (sps_decoding_flags_5 >> 1) & 0x1;
		int32_t virtualboundary_ph_present = (ph_virtual_boundary_num >> 15) & 0x1;
		int32_t virtualboundary_enabled = virtualboundary_sps_enabled & (virtualboundary_sps_present | virtualboundary_ph_present);
		int32_t virtualboundary_ver = virtualboundary_sps_present ? (((sps_virtual_boundary_num >> 0) & 0x3) | (((sps_virtual_boundary_pos_x_minus1_0 + 1) & 0xffff) << 16 )) :
		                                    ((( ph_virtual_boundary_num >> 0) & 0x3) | ((( ph_virtual_boundary_pos_x_minus1_0 + 1) & 0xffff) << 16 ));
		int32_t virtualboundary_ver1= virtualboundary_sps_present ? ((((sps_virtual_boundary_pos_x_minus1_1 + 1) & 0xffff) << 0) | (((sps_virtual_boundary_pos_x_minus1_2 + 1) & 0xffff) << 16)) :
		                                    (((( ph_virtual_boundary_pos_x_minus1_1 + 1) & 0xffff) << 0) | ((( ph_virtual_boundary_pos_x_minus1_2 + 1) & 0xffff) << 16));

		int32_t virtualboundary_hor = virtualboundary_sps_present ? (((sps_virtual_boundary_num >> 8) & 0x3) | (((sps_virtual_boundary_pos_y_minus1_0 + 1) & 0xffff) << 16 )) :
		                                    ((( ph_virtual_boundary_num >> 8) & 0x3) | ((( ph_virtual_boundary_pos_y_minus1_0 + 1) & 0xffff) << 16 ));
		int32_t virtualboundary_hor1= virtualboundary_sps_present ? ((((sps_virtual_boundary_pos_y_minus1_1 + 1) & 0xffff) << 0) | (((sps_virtual_boundary_pos_y_minus1_2 + 1) & 0xffff) << 16)) :
		                                    (((( ph_virtual_boundary_pos_y_minus1_1 + 1) & 0xffff) << 0) | ((( ph_virtual_boundary_pos_y_minus1_2 + 1) & 0xffff) << 16));

		int32_t sps_dualitree = (rpm_param->p.sps_decoding_flags_0 >> 14) & 0x1;
		int32_t slice_isinterb = (rpm_param->p.slice_type == 0); // 0:B 1:P 2:I

		int32_t deblock_enabled = sliceheader_dblk_disabled_flag ? 0 : 1;
		int32_t sao_enabled_y = (((rpm_param->p.sps_decoding_flags_1>>8)&1) &  ((rpm_param->p.pps_decoding_flags_2>>14)&1) & ((rpm_param->p.slice_ph_decoding_flags_1>>8)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>8)&1) & ~((rpm_param->p.pps_decoding_flags_2>>14)&1) & ((rpm_param->p.slice_decoding_flags_0>>9)&1));
		int32_t sao_enabled_c = (((rpm_param->p.sps_decoding_flags_1>>8)&1) &  ((rpm_param->p.pps_decoding_flags_2>>14)&1) & ((rpm_param->p.slice_ph_decoding_flags_1>>7)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>8)&1) & ~((rpm_param->p.pps_decoding_flags_2>>14)&1) & ((rpm_param->p.slice_decoding_flags_0>>8)&1));
		int32_t alf_enabled_lm = (((rpm_param->p.sps_decoding_flags_1>>7)&1) &  ((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.slice_ph_decoding_flags_0>>6)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>7)&1) & ~((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.sh_alf_info_hi>>15)&1));
		int32_t alf_enabled_cb = (((rpm_param->p.sps_decoding_flags_1>>7)&1) &  ((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.slice_ph_decoding_flags_0>>5)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>7)&1) & ~((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.sh_alf_info_lo>>6)&1));
		int32_t alf_enabled_cr = (((rpm_param->p.sps_decoding_flags_1>>7)&1) &  ((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.slice_ph_decoding_flags_0>>4)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>7)&1) & ~((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.sh_alf_info_lo>>5)&1));
		int32_t alf_enabled_ccb = (((rpm_param->p.sps_decoding_flags_1>>6)&1) &  ((rpm_param->p.pps_decoding_flags_2>>13)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>6)&1) & ~((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.sh_alf_cc_info>>7)&1));
		int32_t alf_enabled_ccr = (((rpm_param->p.sps_decoding_flags_1>>6)&1) &  ((rpm_param->p.pps_decoding_flags_2>>13)&1)) |
			(((rpm_param->p.sps_decoding_flags_1>>6)&1) & ~((rpm_param->p.pps_decoding_flags_2>>13)&1) & ((rpm_param->p.sh_alf_cc_info>>3)&1));

		// NOTE: SPECIAL for ladf_intv_num setting: (different from spec) 0:means ladf not enabled, 2~5 if ladf is enabled, other value is not allowed
		int32_t ladf_intv_num   = (rpm_param->p.ladf_info&0x1)?((rpm_param->p.ladf_info>>1)&0x7):0; // cfg: LADF range[2,5] (sps_num_ladf_intervals_minus2 + 2),
		int32_t ladf_qplow      = (rpm_param->p.ladf_info>>8)&0xff; // cfg: LADF range[-63,63] sps_ladf_lowest_interval_qp_offset
		int32_t ladf_qpoffset0  = rpm_param->p.ladf_qpoffset0; // cfg: LADF range[-63,63] sps_ladf_qp_offset[0]
		int32_t ladf_qpoffset1  = rpm_param->p.ladf_qpoffset1; // cfg: LADF range[-63,63] sps_ladf_qp_offset[1]
		int32_t ladf_qpoffset2  = rpm_param->p.ladf_qpoffset2; // cfg: LADF range[-63,63] sps_ladf_qp_offset[2]
		int32_t ladf_qpoffset3  = rpm_param->p.ladf_qpoffset3; // cfg: LADF range[-63,63] sps_ladf_qp_offset[3]
		int32_t ladf_deltathm10 = 0               + rpm_param->p.ladf_deltathm10 + 1; // lowerbound1 cfg: LADF range[0, 2^bitdepth - 3] sps_ladf_delta_threshold_minus1[0]
		int32_t ladf_deltathm11 = ladf_deltathm10 + rpm_param->p.ladf_deltathm11 + 1; // lowerbound2 cfg: LADF range[0, 2^bitdepth - 3] sps_ladf_delta_threshold_minus1[1]
		int32_t ladf_deltathm12 = ladf_deltathm11 + rpm_param->p.ladf_deltathm12 + 1; // lowerbound3 cfg: LADF range[0, 2^bitdepth - 3] sps_ladf_delta_threshold_minus1[2]
		int32_t ladf_deltathm13 = ladf_deltathm12 + rpm_param->p.ladf_deltathm13 + 1; // lowerbound4 cfg: LADF range[0, 2^bitdepth - 3] sps_ladf_delta_threshold_minus1[3]

		data32 = READ_VREG(HEVC_DBLK_CFG1);
		data32 = (((data32 >> 20) & 0xfff) << 20) |
			(((bit_depth == 10) ? 0xa : (bit_depth == 9) ? 0x5: 0x0) << 16) |                                // [16 +: 4]: {luma_bd[1:0],chroma_bd[1:0]}
			(((data32 >> 2) & 0x3fff) << 2) |
			(((lcu_size == 64) ? 0 : (lcu_size == 32) ? 1 : (lcu_size == 16) ? 2 : 3) << 0); // [ 0 +: 2]: lcu_size
		data32 &= ~(1 << 15);
		if (dbg_pic_cnt == 0 && sps_dualitree)
			data32 |= (1 << 15); // [15] cfg_dualtree only when I picture TODO
		WRITE_VREG(HEVC_DBLK_CFG1, data32);
		data32 = (pic_height<<16) | pic_width;
		WRITE_VREG(HEVC_DBLK_CFG2, data32);
		hevc_print(hevc, H266_DEBUG_REG_CFG,
			"cfgLPF: picSize(%d x %d), bitDepth(%d),lcu_size(%d),deblock(%d,%d,%d),sao(%d,%d),alf(%d,%d,%d,%d,%d)\n",
			pic_width, pic_height, bit_depth, lcu_size,
			deblock_enabled, picheader_dblk_override_flag, sliceheader_dblk_override_flag, sao_enabled_y, sao_enabled_c,
			alf_enabled_lm, alf_enabled_cb, alf_enabled_cr, alf_enabled_ccb, alf_enabled_ccr);

		data32 = READ_VREG(HEVC_DBLK_CFGB);
		data32 &= ~(1<<20); data32 |= (deblock_enabled & 0x1) << 20;
		data32 &= ~(1<<8); data32 |= (slice_isinterb & 0x1) << 8;
		data32 &= ~(1<<9); data32 |= (virtualboundary_enabled & 0x1) << 9;
		WRITE_VREG(HEVC_DBLK_CFGB, data32);

		WRITE_VREG(HEVC_DBLK_VBVER, virtualboundary_ver);
		WRITE_VREG(HEVC_DBLK_VBVER1, virtualboundary_ver1);
		WRITE_VREG(HEVC_DBLK_VBHOR, virtualboundary_hor);
		WRITE_VREG(HEVC_DBLK_VBHOR1, virtualboundary_hor1);

		data32 = ((ladf_intv_num   & 0x7  ) << 0 ) |
				((ladf_qplow      & 0x7f ) << 3 ) |
				((ladf_qpoffset0  & 0x7f ) << 10) |
				((ladf_qpoffset1  & 0x7f ) << 17) |
				((ladf_qpoffset2  & 0x7f ) << 24);
		WRITE_VREG(HEVC_DBLK_DBLK0, data32);
		data32 = ((ladf_qpoffset3  & 0x7f)  << 0 ) |
				((ladf_deltathm10 & 0xfff) << 7 ) |
				((ladf_deltathm11 & 0xfff) << 19);
		WRITE_VREG(HEVC_DBLK_DBLK1, data32);
		data32 = ((ladf_deltathm12 & 0xfff) << 0 ) |
				((ladf_deltathm13 & 0xfff) << 12 );
		WRITE_VREG(HEVC_DBLK_DBLK2, data32);

		data32 = (alf_enabled_lm<<0) |
				(alf_enabled_cb<<1) |
				(alf_enabled_cr<<2) |
				(alf_enabled_ccb<<3) |
				(alf_enabled_ccr<<4);
		WRITE_VREG(HEVC_DBLK_ALF0, data32);
		hevc_print(hevc, H266_DEBUG_REG_CFG, "cfgLPF: ALF0(%x)\n",data32);

		data32 = (( (tile_flags_0>>1) & 0x1) << 0) | // loopfilter_across_tiles_enable
				(( (tile_flags_0>>2) & 0x1) << 1) | // loopfilter_across_slices_enable
				(( (beta_tc_offset_div2>>0) & 0x1f) << 2) |
				(( (beta_tc_offset_div2>>5) & 0x1f) << 7) |
				(( (cb_beta_tc_offset_div2>>0) & 0x1f) << 12) |
				(( (cb_beta_tc_offset_div2>>5) & 0x1f) << 17) |
				(( (cr_beta_tc_offset_div2>>0) & 0x1f) << 22) |
				(( (cr_beta_tc_offset_div2>>5) & 0x1f) << 27);
		WRITE_VREG(HEVC_DBLK_CFG9, data32);
		hevc_print(hevc, H266_DEBUG_REG_CFG, " [DBLK DEBUG] HEVC1 CFG9 : 0x%x\n", data32);
	}
	hevc_print(hevc, H266_DEBUG_REG_CFG, "cfgLPF::Picture (DBLK_CFG1,DBLK_CFG2) ... Done\n");
}

static void hevc_config_work_space_hw(struct hevc_state_s *hevc)
{
	uint32_t data32;
	BuffInfo_t* buf_spec = hevc->work_space_buf;
	hevc_print(hevc, H266_DEBUG_REG_CFG, "%s %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", __func__,
		buf_spec->ipp.buf_start,
		buf_spec->start_adr,
		buf_spec->short_term_rps.buf_start,
		buf_spec->rcs.buf_start,
		buf_spec->sps.buf_start,
			buf_spec->pps.buf_start,
			buf_spec->entrop_context.buf_start,
			buf_spec->sbac_top.buf_start,
			buf_spec->sao_up.buf_start,
			buf_spec->swap_buf.buf_start,
		buf_spec->swap_buf2.buf_start,
		buf_spec->scalelut.buf_start,
		buf_spec->dblk_para.buf_start,
		buf_spec->dblk_data.buf_start);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE,buf_spec->ipp.buf_start);
	//WRITE_VREG(HEVC_RPM_BUFFER, buf_spec->rpm.buf_start);
	if ((get_dbg_flag(hevc) & H266_DEBUG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)hevc->rpm_phy_addr);
	WRITE_VREG(HEVC_REF_LIST_BUFFER, hevc->ref_list_buffer_phy_addr);
	WRITE_VREG(VVC_ALF_SWAP_BUFFER, buf_spec->short_term_rps.buf_start);
	WRITE_VREG(HEVC_RCS_BUFFER, buf_spec->rcs.buf_start);
	WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
	WRITE_VREG(AV1_GMC_PARAM_BUFF_ADDR, buf_spec->coeff_hold.buf_start);
	WRITE_VREG(VVC_CONTEXT_BUFF, buf_spec->entrop_context.buf_start);
	WRITE_VREG(VVC_SBAC_TOP_BUFFER, buf_spec->sbac_top.buf_start);
	WRITE_VREG(HEVC_SAO_UP, buf_spec->sao_up.buf_start);
#ifdef VVC_10B_MMU
	WRITE_VREG(H266_MMU_MAP_BUFFER,
		((hevc->frame_mmu_map_phy_addr + FRAME_MMU_MAP_ALIGNMENT_SIZE - 1)
		>> FRAME_MMU_MAP_ALIGNMENT_BITS) << FRAME_MMU_MAP_ALIGNMENT_BITS);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "write H266_MMU_MAP_BUFFER %x\n",
		READ_VREG(H266_MMU_MAP_BUFFER));
#else
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER, buf_spec->swap_buf.buf_start);
#endif
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable) {
		//WRITE_VREG(HEVC_ASSIST_MMU_MAP_ADDR2, hevc->frame_dw_mmu_map_phy_addr);
		WRITE_VREG(HEVC_SAO_MMU_DMA_CTRL2,
			((hevc->frame_dw_mmu_map_phy_addr + FRAME_MMU_MAP_ALIGNMENT_SIZE - 1)
			>> FRAME_MMU_MAP_ALIGNMENT_BITS) << FRAME_MMU_MAP_ALIGNMENT_BITS);
	}
#endif
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, buf_spec->swap_buf2.buf_start);
	WRITE_VREG(HEVC_SCALELUT, buf_spec->scalelut.buf_start);

	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);  // cfg_cpi_addr
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);  // cfg_xio_addr
	WRITE_VREG(HEVC_DBLK_CFGE, buf_spec->dblk_data2.buf_start); // cfg_adp_addr

	data32 = READ_VREG(HEVC_SAO_CTRL5);
#if 1
	data32 &= ~(1<<9);
#else
	if (params->p.bit_depth != 0x00)
		data32 &= ~(1<<9);
	else
		data32 |= (1<<9);
#endif
	WRITE_VREG(HEVC_SAO_CTRL5, data32);

	if (hevc->mmu_enable) {
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,(0x1<< 4)); // bit[4] : paged_mem_mode
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2,0x0);
	} else {
#if 1
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0<<3)); // bit[3] smem mode
#else
		if (params->p.bit_depth != 0x00) WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0<<3)); // bit[3] smem mode
		else WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (1<<3)); // bit[3] smem mdoe
#endif
	}
	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);  /*/Enable NV21 reference read mode for MC*/
	//WRITE_VREG(HEVCD_MPP_DECOMP_CTL2,(losless_comp_body_size >> 5));
	//WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,(0xff<<20) | (0xff<<10) | 0xff); //8-bit mode
#if 0
	WRITE_VREG(HEVC_CM_BODY_LENGTH,losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET,losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH,losless_comp_header_size);
#endif

	if (hevc->mmu_enable) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR, buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size/2);

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}
#ifdef VVC_10B_MMU_DW
#if 0
	WRITE_VREG(HEVC_CM_BODY_LENGTH2,losless_comp_body_size_dw);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET2,losless_comp_body_size_dw);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH2,losless_comp_header_size_dw);
#endif
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	if (hevc->dw_mmu_enable) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR2, buf_spec->mmu_vbh_dw.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR2, buf_spec->mmu_vbh_dw.buf_start + buf_spec->mmu_vbh_dw.buf_size/2);

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 |= (1<<15);
	} else
		data32 &= ~(1 << 15);
	WRITE_VREG(HEVC_SAO_CTRL5, data32);
#endif

	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, buf_spec->mpred_above.buf_start);
#ifdef CO_MV_COMPRESS
	data32 = READ_VREG(HEVC_MPRED_CTRL4);
	data32 |=  (1<<1);
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);
#endif
	data32 = READ_VREG(HEVC_MPRED_CTRL4);
	data32 |=  (1<<27); //enable VVC mode
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);
}

#ifndef USE_OLD_CHIP
static void hevc_init_decoder_hw(struct hevc_state_s *hevc)
{
	uint32_t data32;
	//int32_t i;

#if 0
	int32_t g_WqMDefault4x4[16] = {
	64,     64,     64,     68,
	64,     64,     68,     72,
	64,     68,     76,     80,
	72,     76,     84,     96
	};


	int32_t g_WqMDefault8x8[64] = {
	64,     64,     64,     64,     68,     68,     72,     76,
	64,     64,     64,     68,     72,     76,     84,     92,
	64,     64,     68,     72,     76,     80,     88,     100,
	64,     68,     72,     80,     84,     92,     100,    112,
	68,     72,     80,     84,     92,     104,    112,    128,
	76,     80,     84,     92,     104,    116,    132,    152,
	96,     100,    104,    116,    124,    140,    164,    188,
	104,    108,    116,    128,    152,    172,    192,    216
	};
#endif

	hevc_print(hevc, H266_DEBUG_REG_CFG, "Entering hevc_init_decoder_hw\n");
#if 0
	printk("[test.c] Test Parser Register Read/Write\n");
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001) { print_scratch_error(25); return; }
	WRITE_VREG(HEVC_PARSER_VERSION, 0x5a5a55aa);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x5a5a55aa) { print_scratch_error(26); return; }

	// test Parser Reset
	WRITE_VREG(DOS_SW_RESET1, (1<<3)); // reset_whole parser
	WRITE_VREG(DOS_SW_RESET1, 0); // reset_whole parser
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001) { print_scratch_error(27); return; }
#endif

#if 0 // JT
	printk("[test.c] Enable BitStream Fetch\n");
	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 |
	(1 << 0) // stream_fetch_enable
	;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) { print_scratch_error(29); return; }
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) { print_scratch_error(30); return; }
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) { print_scratch_error(31); return; }
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) { print_scratch_error(32); return; }
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);
#endif // JT

	hevc_print(hevc, H266_DEBUG_REG_CFG, "Enable HEVC Parser Interrupt\n");
	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 = data32 |
			(1 << 24) |  // stream_buffer_empty_int_amrisc_enable
			(1 << 22) |  // stream_fifo_empty_int_amrisc_enable
			(1 << 7) |  // dec_done_int_cpu_enable
			(1 << 4) |  // startcode_found_int_cpu_enable
			(0 << 3) |  // startcode_found_int_amrisc_enable
			(1 << 0)    // parser_int_enable
			;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	hevc_print(hevc, H266_DEBUG_REG_CFG, "Enable HEVC Parser Shift\n");

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 |
			(1 << 1) |  // emulation_check_on
			(1 << 0)    // startcode_check_on
			;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL,
			(0 << 14) | // disable_start_code_protect
			(3 << 6) | // sft_valid_wr_position
			(2 << 4) | // emulate_code_length_sub_1
			(2 << 1) | // start_code_length_sub_1
			(1 << 0)   // stream_shift_enable
			);

	WRITE_VREG(HEVC_SHIFT_LENGTH_PROTECT,
			(0 << 30) |   // data_protect_fill_00_enable
			(1 << 29)     // data_protect_fill_ff_enable
			);

	WRITE_VREG(HEVC_CABAC_CONTROL,
			(1 << 0)   // cabac_enable
			);

	WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
			(1 << 0)   // hevc_parser_core_clk_en
			);


	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

#if 0
	// Initial IQIT_SCALELUT memory -- just to avoid X in simulation
	printk("[test.c] Initial IQIT_SCALELUT memory -- just to avoid X in simulation...\n");
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0); // cfg_p_addr
	for (i = 0; i < 1024; i++) WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);
#endif


#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#else
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 0);
#endif

	//WRITE_VREG(HEVC_DECODE_PIC_BEGIN_REG, 0);
	//WRITE_VREG(HEVC_DECODE_PIC_NUM_REG, decode_pic_num);

#if 0  // No need any more
	// Send parser_cmd
	printk("[test.c] SEND Parser Command ...\n");
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1<<16) | (0<<0));
	for (i = 0; i < PARSER_CMD_NUMBER; i++) {
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
	}

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);
#endif


	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			(1 << 9) | // parser_alf_if_en
			//(1 << 8) | // sao_sw_pred_enable
			(1 << 5) | // parser_sao_if_en
			(1 << 2) | // parser_mpred_if_en
			(1 << 0) // parser_scaler_if_en
			);

#if 0
	//def MULTI_INSTANCE_SUPPORT
	// Begin of Multi-instance
	WRITE_VREG(HEVC_MPRED_INT_STATUS, (1<<31));

	WRITE_VREG(HEVC_PARSER_RESULT_3, 0xffffffff);

	data32 = READ_VREG(HEVC_MPRED_ABV_START_ADDR);
	WRITE_VREG(DOS_SW_RESET3, (1<<18)); // reset mpred
	WRITE_VREG(DOS_SW_RESET3, 0);
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, data32);

	// End of Multi-instance
#endif
	// Changed to Start MPRED in microcode
	/*
	printk("[test.c] Start MPRED\n");
	WRITE_VREG(HEVC_MPRED_INT_STATUS,
			(1<<31)
			);
	*/

#if 0
	// VVC default seq_wq_matrix config
	printk("[test.c] Config VVC default seq_wq_matrix ...\n");
	// 4x4
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 64); // default seq_wq_matrix_4x4 begin address
	for (i = 0; i < 16; i++) WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, g_WqMDefault4x4[i]);

	// 8x8
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0); // default seq_wq_matrix_8x8 begin address
	for (i = 0; i < 64; i++) WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, g_WqMDefault8x8[i]);
#endif

	hevc_print(hevc, H266_DEBUG_REG_CFG, "Reset IPP\n");
	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
			(0 << 1) | // enable ipp
			(1 << 0)   // software reset ipp and mpp
			);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
			(1 << 1) | // enable ipp
			(0 << 0)   // software reset ipp and mpp
			);

	// Initialize lpf
	lpf_init(hevc);

	// Initialize mcrcc and decomp perf counters
#if 0
	mcrcc_perfcount_reset();
	decomp_perfcount_reset();
#endif
	hevc_print(hevc, H266_DEBUG_REG_CFG, "Leaving hevc_init_decoder_hw\n");

	return;
}

static void save_register_context(struct hevc_state_s *hevc)
{
	int i;
	DecApp *pcDecApp = &hevc->vvc_dec->m_decApp;
	WRITE_VREG(HEVC_IQIT_QP_CHROMA_MAP_RADDR, 0);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "%s Read chromaQPmap:", __func__);
	for (i = 0; i < CHROMAQPMAP_SIZE; i++) {
		pcDecApp->chromaQPmap[i] = READ_VREG(HEVC_IQIT_QP_CHROMA_MAP_DATA);
		hevc_print_cont(hevc, H266_DEBUG_REG_CFG, "%x ", pcDecApp->chromaQPmap[i]);
	}
	hevc_print(hevc, H266_DEBUG_REG_CFG, "\n");
}

static void restore_register_context(struct hevc_state_s *hevc)
{
	int i;
	DecApp *pcDecApp = &hevc->vvc_dec->m_decApp;
	WRITE_VREG(HEVC_IQIT_QP_CHROMA_MAP_WADDR, 0);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "%s Write chromaQPmap:", __func__);
	for (i = 0; i < CHROMAQPMAP_SIZE; i++) {
		hevc_print_cont(hevc, H266_DEBUG_REG_CFG, "%x ", pcDecApp->chromaQPmap[i]);
		WRITE_VREG(HEVC_IQIT_QP_CHROMA_MAP_DATA, pcDecApp->chromaQPmap[i]);
	}
	hevc_print(hevc, H266_DEBUG_REG_CFG, "\n");
}

#else
static void hevc_init_decoder_hw(struct hevc_state_s *hevc)
{
	unsigned int data32;
	int i;
#if 0
	/* m8baby test1902 */
	if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR)
		hevc_print(hevc, 0, "%s\n", __func__);
	data32 = READ_VREG(HEVC_PARSER_VERSION);

	WRITE_VREG(HEVC_PARSER_VERSION, 0x5a5a55aa);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
#endif
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14));
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);
#if 0
	if (!hevc->m_ins_flag) {
		data32 = READ_VREG(HEVC_STREAM_CONTROL);
		data32 = data32 | (1 << 0);      /* stream_fetch_enable */
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
			data32 |= (0xf << 25); /*arwlen_axi_max*/
		WRITE_VREG(HEVC_STREAM_CONTROL, data32);
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);
#endif
	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 &= 0x03ffffff;
	data32 = data32 | (3 << 29) | (2 << 26) | (1 << 24)
			 |	/* stream_buffer_empty_int_amrisc_enable */
			 (1 << 22) |	/* stream_fifo_empty_int_amrisc_enable*/
			 (1 << 7) |	/* dec_done_int_cpu_enable */
			 (1 << 4) |	/* startcode_found_int_cpu_enable */
			 (0 << 3) |	/* startcode_found_int_amrisc_enable */
			 (1 << 0);	/* parser_int_enable */

	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 | (1 << 1) |	/* emulation_check_on */
			 (1 << 0);		/* startcode_check_on */

	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL, (3 << 6) |/* sft_valid_wr_position */
				(2 << 4) |	/* emulate_code_length_sub_1 */
				(2 << 1) |	/* start_code_length_sub_1 */
				(1 << 0));	/* stream_shift_enable */

	WRITE_VREG(HEVC_CABAC_CONTROL, (1 << 0));	/* cabac_enable */

	/* hevc_parser_core_clk_en */
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, (1 << 0));

	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

	/* Initial IQIT_SCALELUT memory -- just to avoid X in simulation */
	if (is_rdma_enable())
		rdma_back_end_work(hevc->rdma_phy_adr, RDMA_SIZE);
	else {
		WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);/*cfg_p_addr*/
		for (i = 0; i < 1024; i++)
			WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);
	}

#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#endif

	WRITE_VREG(HEVC_DECODE_SIZE, 0);

	/* Send parser_cmd */
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));

	parser_cmd_write();

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);

	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			/* (1 << 8) | // sao_sw_pred_enable */
			(1 << 5) |	/* parser_sao_if_en */
			(1 << 2) |	/* parser_mpred_if_en */
			(1 << 0));	/* parser_scaler_if_en */

	/* Changed to Start MPRED in microcode */

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) |	/* enable ipp */
			(1 << 0));/* software reset ipp and mpp */

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) |	/* enable ipp */
			(0 << 0));	/* software reset ipp and mpp */

	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);  /*/Enable NV21 reference read mode for MC*/

#if  0//def CO_MV_COMPRESS
	data32 = READ_VREG(HEVC_MPRED_CTRL4);
	data32 |=  (1<<1);
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);
#endif
}
#endif

#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_hevc_clk_forced_on(void)
{
	unsigned int rdata32;
	/* IQIT */
	rdata32 = READ_VREG(HEVC_IQIT_CLK_RST_CTRL);
	WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL, rdata32 | (0x1 << 2));

	/* DBLK */
	rdata32 = READ_VREG(HEVC_DBLK_CFG0);
	WRITE_VREG(HEVC_DBLK_CFG0, rdata32 | (0x1 << 2));

	/* SAO */
	rdata32 = READ_VREG(HEVC_SAO_CTRL1);
	WRITE_VREG(HEVC_SAO_CTRL1, rdata32 | (0x1 << 2));

	/* MPRED */
	rdata32 = READ_VREG(HEVC_MPRED_CTRL1);
	WRITE_VREG(HEVC_MPRED_CTRL1, rdata32 | (0x1 << 24));

	/* PARSER */
	rdata32 = READ_VREG(HEVC_STREAM_CONTROL);
	WRITE_VREG(HEVC_STREAM_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_SHIFT_CONTROL);
	WRITE_VREG(HEVC_SHIFT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_CABAC_CONTROL);
	WRITE_VREG(HEVC_CABAC_CONTROL, rdata32 | (0x1 << 13));
	rdata32 = READ_VREG(HEVC_PARSER_CORE_CONTROL);
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_IF_CONTROL);
	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   rdata32 | (0x3 << 5) | (0x3 << 2) | (0x3 << 0));

	/* IPP */
	rdata32 = READ_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG);
	WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG, rdata32 | 0xffffffff);

	/* MCRCC */
	rdata32 = READ_VREG(HEVCD_MCRCC_CTL1);
	WRITE_VREG(HEVCD_MCRCC_CTL1, rdata32 | (0x1 << 3));
}
#endif

static u32 init_aux_size;
static int aux_data_is_available(struct hevc_state_s *hevc)
{
	u32 reg_val;

	reg_val = READ_VREG(HEVC_AUX_DATA_SIZE);
	if (reg_val != 0 && reg_val != init_aux_size)
		return 1;
	else
		return 0;
}

void config_aux_buf(struct hevc_state_s *hevc)
{
	WRITE_VREG(HEVC_AUX_ADR, hevc->aux_phy_addr);
	init_aux_size = ((hevc->prefix_aux_size >> 4) << 16) |
		(hevc->suffix_aux_size >> 4);
	WRITE_VREG(HEVC_AUX_DATA_SIZE, init_aux_size);
}

static void v4l_crop_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int crop_w, crop_h;
	hevc->crop_w = pic->width;
	hevc->crop_h = pic->height;

	if (pic->conformance_window_flag &&
	(get_dbg_flag(hevc) &
	H266_DEBUG_IGNORE_CONFORMANCE_WINDOW) == 0) {
		unsigned int SubWidthC, SubHeightC;

		switch (pic->chroma_format_idc) {
		case 1:
			SubWidthC = 2;
			SubHeightC = 2;
			break;
		case 2:
			SubWidthC = 2;
			SubHeightC = 1;
			break;
		default:
			SubWidthC = 1;
			SubHeightC = 1;
			break;
		}
		crop_w = SubWidthC * (pic->conf_win_left_offset + pic->conf_win_right_offset);
		crop_h = SubHeightC * (pic->conf_win_top_offset + pic->conf_win_bottom_offset);

		if (crop_w < 0 || crop_h < 0 || pic->width <= crop_w || pic->height <= crop_h) {
			hevc_print(hevc, H266_DEBUG_BUFMGR,
				"%s invalid crop, crop_w:%d, crop_h:%d\n", __func__,crop_w, crop_h);
			pic->crop_w = pic->width;
			pic->crop_h = pic->height;
			return;
		}

		hevc->crop_w -= crop_w;
		hevc->crop_h -= crop_h;

		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR)
			hevc_print(hevc, 0,
			"conformance_window %d, %d, %d, %d, %d => cropped width %d, height %d, com_w %d com_h %d\n",
			pic->chroma_format_idc,
			pic->conf_win_left_offset,
			pic->conf_win_right_offset,
			pic->conf_win_top_offset,
			pic->conf_win_bottom_offset,
			hevc->crop_w, hevc->crop_h, pic->width, pic->height);
	}
	pic->crop_w = hevc->crop_w;
	pic->crop_h = hevc->crop_h;
}

static void flush_output(struct hevc_state_s *hevc)
{
	h266_bufmgr_code_process(&hevc->vvc_dec->m_decApp, SEQUENCE_END_CODE);
}

/*
* dv_meta_flag: 1, dolby meta only; 2, not include dolby meta
*/
static void set_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned char suffix_flag,
	unsigned char dv_meta_flag)
{
	int i;
	unsigned short *aux_adr;
	unsigned int size_reg_val =
		READ_VREG(HEVC_AUX_DATA_SIZE);
	unsigned int aux_count = 0;
	int aux_size = 0;
	if (pic == NULL || 0 == aux_data_is_available(hevc))
		return;

	if ((pic->BUF_index < 0) || (!hevc->m_BUF[pic->BUF_index].start_adr)) {
		hevc_print(hevc, 0,
			"%s, aux data buf should be released\n", __func__);
		return;
	}

	if (hevc->aux_data_dirty ||
		hevc->m_ins_flag == 0) {

		hevc->aux_data_dirty = 0;
	}

	if (suffix_flag) {
		aux_adr = (unsigned short *)(hevc->aux_addr + hevc->prefix_aux_size);
		aux_count = ((size_reg_val & 0xffff) << 4) >> 1;
		aux_size = hevc->suffix_aux_size;
	} else {
		aux_adr = (unsigned short *)hevc->aux_addr;
		aux_count = ((size_reg_val >> 16) << 4) >> 1;
		aux_size = hevc->prefix_aux_size;
	}
	if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI) {
		hevc_print(hevc, 0,
			"%s:pic 0x%p old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, pic, pic->aux_data_size,
			aux_count, suffix_flag, dv_meta_flag);
	}

	if ((aux_count > aux_size) ||
		(pic->aux_data_size + aux_count > AUX_DATA_SIZE1)) {
		hevc_print(hevc, 0,
			"%s:aux_count(%d) is over size\n", __func__, aux_count);
		aux_count = 0;
	}
	if (aux_size > 0 && aux_count > 0) {
		int heads_size = 0;

		for (i = 0; i < aux_count; i++) {
			unsigned char tag = aux_adr[i] >> 8;
			if (tag != 0 && tag != 0xff) {
				if (dv_meta_flag == 0)
					heads_size += 8;
				else if (dv_meta_flag == 1 && tag == 0x1)
					heads_size += 8;
				else if (dv_meta_flag == 2 && tag != 0x1)
					heads_size += 8;
			}
		}

		if (pic->aux_data_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h = pic->aux_data_buf + pic->aux_data_size;
			unsigned char *p = h + 8;
			int len = 0;
			int padding_len = 0;

			for (i = 0; i < aux_count; i += 4) {
				int ii;
				unsigned char tag = aux_adr[i + 3] >> 8;
				if (tag != 0 && tag != 0xff) {
					if (dv_meta_flag == 0)
						valid_tag = 1;
					else if (dv_meta_flag == 1 && tag == 0x1)
						valid_tag = 1;
					else if (dv_meta_flag == 2 && tag != 0x1)
						valid_tag = 1;
					else
						valid_tag = 0;
					if (valid_tag && len > 0) {
						if ((char *)p > (pic->aux_data_buf + AUX_DATA_SIZE1 - 11)) {
							hevc_print(hevc, 0, "%s, buf oversize risk %px %px\n",
								__func__, p, pic->aux_data_buf);
							break;
						}
						pic->aux_data_size += (len + 8);
						h[0] = (len >> 24) & 0xff;
						h[1] = (len >> 16) & 0xff;
						h[2] = (len >> 8) & 0xff;
						h[3] = (len >> 0) & 0xff;
						h[6] = (padding_len >> 8) & 0xff;
						h[7] = (padding_len) & 0xff;
						h += (len + 8);
						p += 8;
						len = 0;
						padding_len = 0;
					}
					if (valid_tag) {
						h[4] = tag;
						h[5] = 0;
						h[6] = 0;
						h[7] = 0;
					}
				}
				if (valid_tag) {
					for (ii = 0; ii < 4; ii++) {
						unsigned short aa = aux_adr[i + 3 - ii];
						*p = aa & 0xff;
						p++;
						len++;
					}
				}
			}
			if (len > 0) {
				pic->aux_data_size += (len + 8);
				h[0] = (len >> 24) & 0xff;
				h[1] = (len >> 16) & 0xff;
				h[2] = (len >> 8) & 0xff;
				h[3] = (len >> 0) & 0xff;
				h[6] = (padding_len >> 8) & 0xff;
				h[7] = (padding_len) & 0xff;
			}

			hevc_print(hevc, H266_DEBUG_PRINT_SEI, "%s: aux: (size %d) suffix_flag %d\n",
				__func__, pic->aux_data_size, suffix_flag);

			if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI) {
				for (i = 0; i < pic->aux_data_size; i++) {
					pr_info("%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						pr_info("\n");
				}
				pr_info("\n");
			}
		}
	}

}

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	pic->aux_data_size = 0;
}

static int recycle_mmu_buf_tail(struct hevc_state_s *hevc,
		bool check_dma)
{
	int index = hevc->vvc_dec->cur_pic->BUF_index;
	struct aml_buf *aml_buf;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

	if (index == INVALID_IDX) {
		hevc_print(hevc,
			0, "[ERR]%s buf has been recycled!\n", __func__);
		hevc->vvc_dec->cur_pic->scatter_alloc = 2;
		hevc->used_4k_num = -1;

		return 0;
	}

	aml_buf = index_to_afbc_aml_buf(hevc, index);
	hevc_print(hevc,
			H266_DEBUG_BUFMGR_MORE,
			"%s pic index %d scatter_alloc %d page_start %d fb %px\n",
			"decoder_mmu_box_free_idx_tail",
			hevc->vvc_dec->cur_pic->index,
			hevc->vvc_dec->cur_pic->scatter_alloc,
			hevc->used_4k_num,
			aml_buf);
	ctx->cal_compress_buff_info(hevc->used_4k_num, ctx);
	if (check_dma)
		hevc_mmu_dma_check(hw_to_vdec(hevc));

	if (aml_buf == NULL) {
		hevc_print(hevc, 0, "[ERR]%s index %d\n", __func__, index);
		hevc->vvc_dec->cur_pic->scatter_alloc = 2;
		hevc->used_4k_num = -1;

		return 0;
	}

	decoder_mmu_box_free_idx_tail(
		aml_buf->fbc->mmu,
		aml_buf->fbc->index,
		hevc->used_4k_num);

	hevc->vvc_dec->cur_pic->scatter_alloc = 2;
	hevc->used_4k_num = -1;
	return 0;
}

static void check_pic_decoded_error(struct hevc_state_s *hevc,
	int decoded_lcu)
	{
	if (hevc->vvc_dec->cur_pic == NULL)
		return;
	if (hevc->timeout_flag)
		hevc->vvc_dec->cur_pic->error_mark = 1;
}

/* only when we decoded one field or one frame,
we can call this function to get qos info*/
static void get_picture_qos_info(struct hevc_state_s *hevc)
{
	struct PIC_s *picture = hevc->vvc_dec->cur_pic;

	if (!hevc->vvc_dec->cur_pic)
		return;

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		data = READ_VREG(HEVC_MV_INFO);
		if (picture->slice_type == I_SLICE)
			data = 0;
		a[0] = data & 0xff;
		a[1] = (data >> 8) & 0xff;
		a[2] = (data >> 16) & 0xff;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_mv = a[2];
		picture->avg_mv = a[1];
		picture->min_mv = a[0];

		data = READ_VREG(HEVC_QP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_qp = a[2];
		picture->avg_qp = a[1];
		picture->min_qp = a[0];

		data = READ_VREG(HEVC_SKIP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_skip = a[2];
		picture->avg_skip = a[1];
		picture->min_skip = a[0];
	} else {
		uint32_t blk88_y_count;
		uint32_t blk88_c_count;
		uint32_t blk22_mv_count;
		uint32_t rdata32;
		int32_t mv_hi;
		int32_t mv_lo;
		uint32_t rdata32_l;
		uint32_t mvx_L0_hi;
		uint32_t mvy_L0_hi;
		uint32_t mvx_L1_hi;
		uint32_t mvy_L1_hi;
		int64_t value;
		uint64_t temp_value;

		picture->max_mv = 0;
		picture->avg_mv = 0;
		picture->min_mv = 0;

		picture->max_skip = 0;
		picture->avg_skip = 0;
		picture->min_skip = 0;

		picture->max_qp = 0;
		picture->avg_qp = 0;
		picture->min_qp = 0;

		/* set rd_idx to 0 */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, 0);

	    blk88_y_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_y_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_y_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		picture->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* skipped_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		picture->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* blk66_c_count */
	    blk88_c_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_c_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_c_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* intra_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* skipped_cu_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* coeff_non_zero_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		picture->min_qp = (rdata32>>0)&0xff;

		picture->max_qp = (rdata32>>8)&0xff;

		/* blk22_mv_count */
	    blk22_mv_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk22_mv_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    /* should all be 0x00 or 0xff */
	    mvx_L0_hi = ((rdata32>>0)&0xff);
	    mvy_L0_hi = ((rdata32>>8)&0xff);
	    mvx_L1_hi = ((rdata32>>16)&0xff);
	    mvy_L1_hi = ((rdata32>>24)&0xff);

		/* mvx_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvx_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
		 value = div_s64(value, blk22_mv_count);

		picture->avg_mv = value;

		/* mvy_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* mvx_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* mvy_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		picture->max_mv = mv_hi;

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

		picture->min_mv = mv_lo;

	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_CTRL);

		/* reset all counts */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
	}
}


/* return page number */
static int hevc_mmu_page_num(struct hevc_state_s *hevc,
		int w, int h, bool is_bit_depth_10)
{
	int picture_size;
	int page_num;
	int max_frame_num;

	picture_size = compute_losless_comp_body_size(hevc, w,
				h, is_bit_depth_10);
	page_num = ((picture_size + PAGE_SIZE - 1) >> PAGE_SHIFT);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TXHD2))
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;

	if (page_num > max_frame_num) {
		hevc_print(hevc, 0, "over max !! 0x%x width %d height %d\n",
			page_num, w, h);
		return -1;
	}
	return page_num;
}

int H266_alloc_mmu(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	int ret;
	struct aml_buf *aml_buf =
			index_to_afbc_aml_buf(hevc, new_pic->BUF_index);
	if (get_double_write_mode(hevc) == 0x10)
		return 0;

	if (aml_buf->fbc->frame_size < 0)
		return -1;

	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_START);
	ret = decoder_mmu_box_alloc_idx(
			aml_buf->fbc->mmu,
			aml_buf->fbc->index,
			aml_buf->fbc->frame_size,
			mmu_index_adr);

	if (!ret)
		aml_buf->fbc->used[aml_buf->fbc->index] |= 1;

	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_END);

	new_pic->scatter_alloc = 1;

	hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
		"%s pic index %d mmu_4k_number %d cur fb idx mmu %d dma 0x%lx ret =%d\n",
		__func__, new_pic->index,
		aml_buf->fbc->frame_size, aml_buf->fbc->index, aml_buf->planes[0].addr, ret);
	return ret;
}
#ifdef VVC_10B_MMU_DW
int H266_alloc_mmu_dw(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	int is_bit_depth_10 = (bit_depth != 0x00);
	int cur_mmu_4k_number;
	int ret;
	struct aml_buf *aml_buf =
			index_to_afbc_aml_buf(hevc, new_pic->BUF_index);

	if (aml_buf->fbc->frame_size < 0) {
		hevc_print(hevc, 0,
			"%s, error no mmu box dw!\n", __func__);
		return -1;
	}

	if (get_double_write_mode(hevc) == 0x10)
		return 0;

	cur_mmu_4k_number = hevc_mmu_page_num(hevc, new_pic->width,
			new_pic->height, is_bit_depth_10);
	if (cur_mmu_4k_number < 0)
		return -1;

	ret = decoder_mmu_box_alloc_idx(
			aml_buf->fbc->mmu_dw,
			aml_buf->fbc->index,
			aml_buf->fbc->frame_size,
			mmu_index_adr);

	if (!ret)
		aml_buf->fbc->used[aml_buf->fbc->index] |= 1;

	hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,
		"%s pic index %d mmu_4k_number %d cur fb idx mmu %d dma 0x%lx ret =%d\n",
		__func__, new_pic->index,
		aml_buf->fbc->frame_size, aml_buf->fbc->index, aml_buf->planes[0].addr, ret);
	return ret;
}
#endif

static void vh266_report_err_timestamp_for_decoded_frames(struct aml_vcodec_ctx *ctx,
	u64 timestamp)
{
	u64 timestamp_bak;
	timestamp_bak = ctx->current_timestamp;
	ctx->current_timestamp = timestamp;
	vdec_v4l_post_error_frame_event(ctx);
	ctx->decoder_status_info.decoder_error_count++;
	vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
	ctx->current_timestamp = timestamp_bak;
}

static int get_idle_pos(struct hevc_state_s *hevc)
{
	int i;

	for (i = 0; i < hevc->used_buf_num; ++i) {
		if (/*(hevc->vvc_dec->pic_pool[i].referenced == 0) &&
			(hevc->vvc_dec->pic_pool[i].vf_ref == 0) &&*/
			(hevc->vvc_dec->pic_pool[i].used == 0) &&
			(!hevc->vvc_dec->pic_pool[i].cma_alloc_addr)) {
			break;
		}
	}

	return (i != hevc->used_buf_num) ? i : INVALID_IDX;
}

static int v4l_alloc_and_config_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int i = pic->index;
	struct aml_buf *aml_buf = hevc->aml_buf;
	int dw_mode = get_double_write_mode(hevc);

	if (!aml_buf) {
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"%s aml_buf is NULL\n",
			__func__);
		return -1;
	}

	if (hevc->mmu_enable) {
		hevc->m_BUF[i].header_addr = aml_buf->fbc->haddr;
	}

	hevc->m_BUF[i].used_flag	= 1;
	hevc->m_BUF[i].v4l_ref_buf_addr	= (ulong)aml_buf;
	pic->cma_alloc_addr             = aml_buf->planes[0].addr;

	if (hevc->mmu_enable &&
		!hevc->afbc_buf_table[aml_buf->fbc->index].used) {
		if (!vdec_secure(hw_to_vdec(hevc)))
			vdec_mm_dma_flush(aml_buf->fbc->haddr,
					aml_buf->fbc->hsize);

		hevc->afbc_buf_table[aml_buf->fbc->index].fb = hevc->m_BUF[i].v4l_ref_buf_addr;
		hevc->afbc_buf_table[aml_buf->fbc->index].used = 1;
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS, "%s afbc_index %d, i: %d, fb 0x%lx\n",
			__func__, aml_buf->fbc->index, i, hevc->afbc_buf_table[aml_buf->fbc->index].fb);
	}
	if (aml_buf->num_planes == 1) {
		hevc->m_BUF[i].start_adr = aml_buf->planes[0].addr;
		hevc->m_BUF[i].luma_size = aml_buf->planes[0].offset;
		hevc->m_BUF[i].size = aml_buf->planes[0].length;
		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
		pic->dw_y_adr = hevc->m_BUF[i].start_adr;
		pic->dw_u_v_adr = pic->dw_y_adr + hevc->m_BUF[i].luma_size;
		pic->luma_size = aml_buf->planes[0].offset;
		pic->chroma_size = aml_buf->planes[0].length - aml_buf->planes[0].offset;
	} else if (aml_buf->num_planes == 2) {
		hevc->m_BUF[i].start_adr = aml_buf->planes[0].addr;
		hevc->m_BUF[i].luma_size = aml_buf->planes[0].length;
		hevc->m_BUF[i].chroma_addr = aml_buf->planes[1].addr;
		hevc->m_BUF[i].chroma_size = aml_buf->planes[1].length;
		hevc->m_BUF[i].size = aml_buf->planes[0].length + aml_buf->planes[1].length;
		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
		aml_buf->planes[1].bytes_used = aml_buf->planes[1].length;
		pic->dw_y_adr = hevc->m_BUF[i].start_adr;
		pic->dw_u_v_adr = hevc->m_BUF[i].chroma_addr;
		pic->luma_size = aml_buf->planes[0].length;
		pic->chroma_size = aml_buf->planes[1].length;
	}

	if (hevc->mmu_enable)
		pic->header_adr = hevc->m_BUF[i].header_addr;

	pic->BUF_index		= i;
	pic->poc		= INVALID_POC;
	pic->mc_canvas_y	= pic->index;
	pic->mc_canvas_u_v	= pic->index;
	pic->double_write_mode = dw_mode;

	if (dw_mode & 0x10) {
		pic->mc_canvas_y = (pic->index << 1);
		pic->mc_canvas_u_v = (pic->index << 1) + 1;
		pic->mc_y_adr = pic->dw_y_adr;
		pic->mc_u_v_adr = pic->dw_u_v_adr;
	}

	pic->hevc = hevc;
	pic->has_inter_slice = 0;
	pic->new_picture = 1;

	return 0;
}

static struct PIC_s *v4l_get_new_pic(struct hevc_state_s *hevc,
		union param_u *rpm_param)
{
	int ret;
	struct aml_vcodec_ctx * v4l = hevc->v4l2_ctx;
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic = NULL;
	struct vvc_decoder *vvc_dec = hevc->vvc_dec;
	DecLib *p_declib = &vvc_dec->m_decApp.m_cDecLib;
	int i = 0;

	if (new_pic == NULL) {
		int pos = get_idle_pos(hevc);

		if (pos < 0) {
			hevc_print(hevc, 0,
				"No idle pos!\n");
			return NULL;
		}

		pic = &hevc->vvc_dec->pic_pool[pos];
		if (pic && v4l_alloc_and_config_pic(hevc, pic)) {
			hevc_print(hevc, H266_DEBUG_BUFMGR, "%s pic %px or v4l_alloc_buf fail!\n",
			__func__, pic);
			return NULL;
		}

		pic->used = 1;
		pic->BUF_index	= pos;
		pic->width	= hevc->pic_w;
		pic->height	= hevc->pic_h;
		new_pic		= pic;
		hevc->cur_idx = pos;
		init_pic_list_hw(hevc);
	}

	/* for notify eos. */
	if (!rpm_param)
		return new_pic;

	pic->width = rpm_param->p.pic_width_in_luma_samples;
	pic->height = rpm_param->p.pic_height_in_luma_samples;
	pic->slice_type = rpm_param->p.slice_type;
	pic->depth = rpm_param->p.sps_bitdepth_minus8 + 8;
	pic->poc = p_declib->m_pcPic->poc;
	pic->inter_slice_allowed_flag = (rpm_param->p.slice_ph_decoding_flags_0 >> 12) & 0x1;

	vvc_dec->vvc_monochrome = (rpm_param->p.sps_chroma_format_idc == 0);
	vvc_dec->lcu_size_log2 = rpm_param->p.lcu_size;
	vvc_dec->lcu_size = 1 << vvc_dec->lcu_size_log2;
	vvc_dec->lcu_x_num	= (pic->width % vvc_dec->lcu_size) ?
		pic->width / vvc_dec->lcu_size + 1 : pic->width / vvc_dec->lcu_size;
	vvc_dec->lcu_y_num = (pic->height % vvc_dec->lcu_size ) ?
		pic->height/vvc_dec->lcu_size + 1 : pic->height / vvc_dec->lcu_size;
	vvc_dec->lcu_total = vvc_dec->lcu_x_num * vvc_dec->lcu_y_num;
	vvc_dec->slice_type = pic->slice_type;
	vvc_dec->slice_addr = rpm_param->p.sliceAddr;

	if (vvc_dec->slice_type != I_SLICE)
		pic->has_inter_slice = 1;

	p_declib->m_pcPic->buf_cfg = pic;
	pic->canvas_lt_flag = 0;
	for (i = 0; i < PIC_POOL_SIZE; i++) {
		if (vvc_dec->pic_pool[i].used <= 0) {
			pic->canvas_poc_list_size = i;
			break;
		}
		pic->canvas_poc_list[i] = vvc_dec->pic_pool[i].poc;

		if (vvc_dec->pic_pool[i].referenced && h266_is_long_term(vvc_dec, vvc_dec->pic_pool[i].poc))
			pic->canvas_lt_flag |= (1 << i);
	}

	if (new_pic->double_write_mode)
		set_canvas(hevc, new_pic);

	if (get_mv_buf(hevc, new_pic) < 0)
		return NULL;

	if (hevc->mmu_enable) {
		ret = H266_alloc_mmu(hevc, new_pic,
			rpm_param->p.sps_bitdepth_minus8,
			hevc->frame_mmu_map_addr);
		if (ret != 0) {
			put_mv_buf(new_pic);
			hevc_print(hevc, 0,
				"can't alloc need mmu1,idx %d ret =%d\n",
				new_pic->decode_idx, ret);
			vdec_v4l_post_error_event(v4l, DECODER_ERROR_ALLOC_BUFFER_FAIL);
			return NULL;
		}
	}

#if 0
	new_pic->decode_idx = hevc->decode_idx;
	new_pic->slice_idx = 0;
	new_pic->referenced = 1;
	//new_pic->output_mark = 0;
	//new_pic->recon_mark = 0;
	new_pic->error_mark = 0;
	//new_pic->dis_mark = 0;
	//new_pic->nodisp_mark = 0;
	new_pic->sei_present_flag = 0;
	//new_pic->num_reorder_pic = rpm_param->p.sps_num_reorder_pics_0;
	new_pic->ip_mode = hevc->low_latency_flag ? true :
			(!new_pic->num_reorder_pic &&
			!(vdec->slave || vdec->master) &&
			!disable_ip_mode) ? true : false;
	new_pic->losless_comp_body_size = hevc->losless_comp_body_size;
	new_pic->POC = hevc->curr_POC;
	new_pic->pic_struct = hevc->curr_pic_struct;
#endif

	hevc->new_pic = 1;
	new_pic->hevc = hevc;
	new_pic->new_picture = 1;
	new_pic->mmu_alloc_flag = 1;
	new_pic->decode_idx = hevc->decode_idx;
	new_pic->slice_idx = 0;
	new_pic->referenced = 1;
	new_pic->decode_done = 0;
	new_pic->error_mark = 0;
	new_pic->sei_present_flag = 0;
#if 0
	new_pic->num_reorder_pic = rpm_param->p.sps_num_reorder_pics_0;
	new_pic->ip_mode = (!new_pic->num_reorder_pic &&
	!(vdec->slave || vdec->master) &&
	!disable_ip_mode) ? true : false;
#endif
	new_pic->losless_comp_body_size = hevc->losless_comp_body_size;
	//new_pic->POC = hevc->curr_POC;
	new_pic->pic_struct = hevc->curr_pic_struct;
	//if (new_pic->aux_data_buf)
	//	release_aux_data(hevc, new_pic);
	new_pic->mem_saving_mode = hevc->mem_saving_mode;
	new_pic->bit_depth_luma = hevc->bit_depth_luma;
	new_pic->bit_depth_chroma = hevc->bit_depth_chroma;
	new_pic->video_signal_type = hevc->video_signal_type;
#if 0
	new_pic->conformance_window_flag =
	hevc->param.p.conformance_window_flag;
	new_pic->conf_win_left_offset =
	hevc->param.p.conf_win_left_offset;
	new_pic->conf_win_right_offset =
	hevc->param.p.conf_win_right_offset;
	new_pic->conf_win_top_offset =
	hevc->param.p.conf_win_top_offset;
	new_pic->conf_win_bottom_offset =
	hevc->param.p.conf_win_bottom_offset;
	new_pic->chroma_format_idc =
	hevc->param.p.chroma_format_idc;
#endif

	if (new_pic) {
		struct aml_buf *aml_buf =
			(struct aml_buf *)hevc->m_BUF[pic->index].v4l_ref_buf_addr;

		aml_buf->state = FB_ST_DECODER;

		aml_buf_get_ref(&v4l->bm, aml_buf);
		if (v4l->vpp_is_need || v4l->enable_di_post) {
			if (new_pic->pic_struct == 3 || new_pic->pic_struct == 4)
				aml_buf_get_ref(&v4l->bm, aml_buf);
			if (new_pic->pic_struct == 5 || new_pic->pic_struct == 6) {
				aml_buf_get_ref(&v4l->bm, aml_buf);
				aml_buf_get_ref(&v4l->bm, aml_buf);
			}
		}

		if (hevc->chunk) {
			new_pic->timestamp = hevc->chunk->timestamp;
		}

		hevc->aml_buf = NULL;
	}
	v4l_crop_pic(hevc, new_pic);

	hevc_print(hevc, H266_DEBUG_BUFMGR,
		"%s: index %d, buf_idx %d, decode_idx %d, POC %d\n",
		__func__, new_pic->index,
		new_pic->BUF_index, new_pic->decode_idx,
		new_pic->poc);

	return new_pic;
}

/*
 *************************************************
 *
 *h266 buffer management end
 *
 **************************************************
 */
static struct hevc_state_s *gHevc;

static void hevc_local_uninit(struct hevc_state_s *hevc)
{
	hevc->rpm_ptr = NULL;
	hevc->lmem_ptr = NULL;

	if (hevc->uninit_list_done == 0)
		uninit_pic_list(hevc);
#ifdef SWAP_HEVC_UCODE
	if (hevc->is_swap) {
		if (hevc->mc_cpu_addr != NULL) {
			decoder_dma_free_coherent(hevc->mc_cpu_handle,
				hevc->swap_size, hevc->mc_cpu_addr,
				hevc->mc_dma_handle);
				hevc->mc_cpu_addr = NULL;
		}

	}
#endif
	if (hevc->aux_addr) {
		decoder_dma_free_coherent(hevc->aux_mem_handle,
				hevc->prefix_aux_size + hevc->suffix_aux_size, hevc->aux_addr,
				hevc->aux_phy_addr);
		hevc->aux_addr = NULL;
	}
	if (hevc->rpm_addr) {
		decoder_dma_free_coherent(hevc->rpm_mem_handle,
				RPM_BUF_SIZE, hevc->rpm_addr,
					hevc->rpm_phy_addr);
		hevc->rpm_addr = NULL;
	}
	if (hevc->lmem_addr) {
		decoder_dma_free_coherent(hevc->lmem_phy_handle,
				RPM_BUF_SIZE, hevc->lmem_addr,
					hevc->lmem_phy_addr);
		hevc->lmem_addr = NULL;
	}
	if (hevc->ref_list_buffer_addr) {
		decoder_dma_free_coherent(hevc->ref_list_buffer_phy_handle,
			REF_LIST_BUF_SIZE, hevc->ref_list_buffer_addr,
			hevc->ref_list_buffer_phy_addr);
		hevc->ref_list_buffer_addr = NULL;
	}

	if (hevc->mmu_enable && hevc->frame_mmu_map_addr) {
		if (hevc->frame_mmu_map_phy_addr)
			decoder_dma_free_coherent(hevc->frame_mmu_map_handle,
				get_frame_mmu_map_size(), hevc->frame_mmu_map_addr,
					hevc->frame_mmu_map_phy_addr);

		hevc->frame_mmu_map_addr = NULL;
	}
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable && hevc->frame_dw_mmu_map_addr) {
		if (hevc->frame_dw_mmu_map_phy_addr)
			decoder_dma_free_coherent(hevc->frame_dw_mmu_map_handle,
				get_frame_mmu_map_size(), hevc->frame_dw_mmu_map_addr,
					hevc->frame_dw_mmu_map_phy_addr);

		hevc->frame_dw_mmu_map_addr = NULL;
	}
#endif
}

static int hevc_local_init(struct hevc_state_s *hevc)
{
	int ret = -1;
	struct BuffInfo_s *cur_buf_info = NULL;

	//memset(&hevc->param, 0, sizeof(union param_o_u));

	cur_buf_info = &hevc->work_space_buf_store;

	if (force_bufspec) {
		memcpy(cur_buf_info, &amvh266_workbuff_spec[force_bufspec & 0xf],
		sizeof(struct BuffInfo_s));
		pr_info("force buffer spec %d\n", force_bufspec & 0xf);
	} else {
		if (hevc_is_support_4k()) {
			if (1)
				memcpy(cur_buf_info, &amvh266_workbuff_spec[2],	/* 4k */
				sizeof(struct BuffInfo_s));
			else
				memcpy(cur_buf_info, &amvh266_workbuff_spec[1],	/* 8k */
				sizeof(struct BuffInfo_s));
		} else {
			memcpy(cur_buf_info, &amvh266_workbuff_spec[0],	/* 1080p */
			sizeof(struct BuffInfo_s));
		}
	}

	cur_buf_info->start_adr = hevc->buf_start;
	if (init_buff_spec(hevc, cur_buf_info) < 0)
		return -1;

	hevc_init_stru(hevc, cur_buf_info);
	init_pic_buf_cfg_list(hevc);
	h266_init_decode(&hevc->vvc_dec->m_decApp);
	hevc->vvc_dec->m_decApp.m_cDecLib.vvc_dec = hevc->vvc_dec;
	hevc->vvc_dec->m_decApp.m_cDecLib.hw = hevc;

	hevc->bit_depth_luma = 8;
	hevc->bit_depth_chroma = 8;
	hevc->video_signal_type = 0;
	hevc->video_signal_type_debug = 0;
	bit_depth_luma = hevc->bit_depth_luma;
	bit_depth_chroma = hevc->bit_depth_chroma;
	video_signal_type = hevc->video_signal_type;

	if ((get_dbg_flag(hevc) & H266_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		hevc->rpm_addr = decoder_dma_alloc_coherent(&hevc->rpm_mem_handle,
				RPM_BUF_SIZE, &hevc->rpm_phy_addr, "H.266_PRM_BUF");
		if (hevc->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}
		hevc->rpm_ptr = hevc->rpm_addr;
	}

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;

		hevc->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hevc->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hevc->prefix_aux_size + hevc->suffix_aux_size;
		hevc->aux_addr = decoder_dma_alloc_coherent(&hevc->aux_mem_handle,
				aux_buf_size, &hevc->aux_phy_addr, "H.266_AUX_BUF");
		if (hevc->aux_addr == NULL) {
			pr_err("%s: failed to alloc aux buffer\n", __func__);
			return -1;
		}
	}

	hevc->lmem_addr = decoder_dma_alloc_coherent(&hevc->lmem_phy_handle,
				LMEM_BUF_SIZE, &hevc->lmem_phy_addr, "H.266_LMEM_BUF");
	if (hevc->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	hevc->lmem_ptr = hevc->lmem_addr;

	hevc->ref_list_buffer_addr = decoder_dma_alloc_coherent(&hevc->ref_list_buffer_phy_handle,
		REF_LIST_BUF_SIZE, &hevc->ref_list_buffer_phy_addr, "H.266_REF_LIST_BUFFER");
	if (hevc->ref_list_buffer_addr == NULL) {
		pr_err("%s: failed to alloc ref_list_buffer buffer\n", __func__);
		return -1;
	}
	memset(hevc->ref_list_buffer_addr, 0, REF_LIST_BUF_SIZE);
	hevc_print(hevc, H266_DEBUG_REG_CFG, "ref_list_buffer_addr %p size 0x%x phy_addr 0x%x\n",
		hevc->ref_list_buffer_addr, REF_LIST_BUF_SIZE, hevc->ref_list_buffer_phy_addr);

	if (hevc->mmu_enable) {
		hevc->frame_mmu_map_addr =
				decoder_dma_alloc_coherent(&hevc->frame_mmu_map_handle,
				get_frame_mmu_map_size(),
				&hevc->frame_mmu_map_phy_addr, "H.266_MMU_MAP");
		if (hevc->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		hevc_print(hevc, H266_DEBUG_REG_CFG, "frame_mmu_map_phy_addr %x size %x\n",
			hevc->frame_mmu_map_phy_addr, get_frame_mmu_map_size());
		memset(hevc->frame_mmu_map_addr, 0, get_frame_mmu_map_size());
	}
#ifdef VVC_10B_MMU_DW
	if (hevc->dw_mmu_enable) {
		hevc->frame_dw_mmu_map_addr =
				decoder_dma_alloc_coherent(&hevc->frame_dw_mmu_map_handle,
				get_frame_mmu_map_size(),
				&hevc->frame_dw_mmu_map_phy_addr, "H.266_DWMMU_MAP");
		if (hevc->frame_dw_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hevc->frame_dw_mmu_map_addr, 0, get_frame_mmu_map_size());
	}
#endif
	ret = 0;
	return ret;
}

/*
 *******************************************
 *  Mailbox command
 *******************************************
 */
#define CMD_FINISHED               0
#define CMD_ALLOC_VIEW             1
#define CMD_FRAME_DISPLAY          3
#define CMD_DEBUG                  10


#define DECODE_BUFFER_NUM_MAX    32
#define DISPLAY_BUFFER_NUM       6

#define video_domain_addr(adr) (adr&0x7fffffff)
#define DECODER_WORK_SPACE_SIZE 0x800000

#define spec2canvas(x)  \
	(((x)->uv_canvas_index << 16) | \
	 ((x)->uv_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))


static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int canvas_w = ALIGN(pic->width, 64)/4;
	int canvas_h = ALIGN(pic->height, 32)/4;
	int blkmode = hevc->mem_map_mode;
	u32 canvas_endian = 0;

	/*CANVAS_BLKMODE_64X32*/
#ifdef SUPPORT_10BIT
	if (pic->double_write_mode &&
		((pic->double_write_mode & 0x20) == 0)) {
		canvas_w = pic->width /
			get_double_write_ratio(pic->double_write_mode & 0xf);
		canvas_h = pic->height /
			get_double_write_ratio(pic->double_write_mode & 0xf);

		if (is_hevc_align32(hevc->mem_map_mode)) {
			canvas_w = ALIGN(canvas_w, 32);
		} else {
			canvas_w = ALIGN(canvas_w, 64);
		}
		canvas_h = ALIGN(canvas_h, 32);

		if (vdec->parallel_dec == 1) {
			if (pic->y_canvas_index == -1)
				pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
			if (pic->uv_canvas_index == -1)
				pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		} else {
			pic->y_canvas_index = 128 + pic->index * 2;
			pic->uv_canvas_index = 128 + pic->index * 2 + 1;
		}

		config_cav_lut_ex(pic->y_canvas_index,
			pic->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);
		config_cav_lut_ex(pic->uv_canvas_index, pic->dw_u_v_adr,
			canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);
#ifdef MULTI_INSTANCE_SUPPORT
		pic->canvas_config[0].phy_addr =
				pic->dw_y_adr;
		pic->canvas_config[0].width =
				canvas_w;
		pic->canvas_config[0].height =
				canvas_h;
		pic->canvas_config[0].block_mode =
				blkmode;
		pic->canvas_config[0].endian =
				canvas_endian;

		pic->canvas_config[1].phy_addr =
				pic->dw_u_v_adr;
		pic->canvas_config[1].width =
				canvas_w;
		pic->canvas_config[1].height =
				canvas_h;
		pic->canvas_config[1].block_mode =
				blkmode;
		pic->canvas_config[1].endian =
				canvas_endian;

		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, pic->canvas_config[0].phy_addr);
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,"%s(canvas0 addr:0x%x)\n",
			__func__, pic->canvas_config[0].phy_addr);
#else
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,"%s(canvas0 addr:0x%x)\n",
			__func__, spec2canvas(pic));
#endif
	} else {
		if (!hevc->mmu_enable) {
			/* to change after 10bit VPU is ready ... */
			if (vdec->parallel_dec == 1) {
				if (pic->y_canvas_index == -1)
					pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
				pic->uv_canvas_index = pic->y_canvas_index;
			} else {
				pic->y_canvas_index = 128 + pic->index;
				pic->uv_canvas_index = 128 + pic->index;
			}

			config_cav_lut_ex(pic->y_canvas_index,
				pic->mc_y_adr, canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);
			config_cav_lut_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
				canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);
		}
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
		hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,"%s(canvas0 addr:0x%x)\n",
			__func__, spec2canvas(pic));
	}
#else
	if (vdec->parallel_dec == 1) {
		if (pic->y_canvas_index == -1)
			pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		if (pic->uv_canvas_index == -1)
			pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
	} else {
		pic->y_canvas_index = 128 + pic->index * 2;
		pic->uv_canvas_index = 128 + pic->index * 2 + 1;
	}


	config_cav_lut_ex(pic->y_canvas_index, pic->mc_y_adr, canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);
	config_cav_lut_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
		canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, canvas_endian, VDEC_HEVC);

	ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
	hevc_print(hevc, H266_DEBUG_BUFMGR_MORE,"%s(canvas0 addr:0x%x)\n",
		__func__, spec2canvas(pic));
#endif
}

#ifdef H266_USERDATA_ENABLE

static void vh266_destroy_userdata_manager(struct hevc_state_s *hevc)
{
	if (hevc) {
		memset(&hevc->userdata_info, 0,
			sizeof(struct h266_userdata_info_t));
	}
}

static void vh266_reset_udr_mgr(struct hevc_state_s *hevc)
{
	hevc->wait_for_udr_send = 0;
	hevc->sei_itu_data_len = 0;
	memset(&hevc->ud_record, 0, sizeof(hevc->ud_record));
}

static int vh266_crate_userdata_manager(struct hevc_state_s *hevc,
	char *userdata_buf, u32 buflen)
{
	if (hevc == NULL)
		return -1;

	mutex_init(&hevc->userdata_mutex);

	memset(&hevc->userdata_info, 0, sizeof(struct h266_userdata_info_t));
	hevc->userdata_info.data_buf = userdata_buf;
	hevc->userdata_info.buf_len = buflen;
	hevc->userdata_info.data_buf_end = userdata_buf + buflen;

	vh266_reset_udr_mgr(hevc);

	return 0;
}

static int vh266_user_data_read(struct vdec_s *vdec,
			struct userdata_param_t *puserdata_para)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	int rec_ri, rec_wi;
	struct h266_userdata_record_t *rec;
	int rec_len;
	void *rec_start;
	void *dest_buf;
	unsigned long res;
	u32 data_size;
	int copy_ok = 0;

#if 0
	uint32_t version;
	uint32_t instance_id; /*input, 0~9*/
	uint32_t buf_len; /*input*/
	uint32_t data_size; /*output*/
	void *pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
#endif

	mutex_lock(&hevc->userdata_mutex);
	rec_ri = hevc->userdata_info.read_index;
	rec_wi = hevc->userdata_info.write_index;

	if (rec_ri == rec_wi) {
		mutex_unlock(&hevc->userdata_mutex);
		return 0;
	}

	rec = &hevc->userdata_info.records[rec_ri];
	rec_start = hevc->userdata_info.data_buf + rec->rec_start;
	rec_len = rec->rec_len;

	dest_buf = puserdata_para->pbuf_addr;
	data_size = rec_len;


	/* pr_info("%s, ready to copy. size 0x%x, bufsize 0x%x, start %p, end %p\n",
		__func__, data_size, puserdata_para->buf_len, rec_start, hevc->userdata_info.data_buf_end); */

	if (rec_len <= puserdata_para->buf_len) {
		if ((u8 *)(rec_start + rec_len) > hevc->userdata_info.data_buf_end) {
			u32 first_len = hevc->userdata_info.data_buf_end - (u8 *)rec_start;

			res = copy_to_user(dest_buf, rec_start, first_len);
			copy_ok = 1;
			if (res) {
				pr_info("%s[1], res %ld, request %d\n", __func__, res, first_len);
				copy_ok = 0;
				rec->rec_len -= (first_len - res);
				rec->rec_start += (first_len - res);
				puserdata_para->data_size += (first_len - res);
			} else {
				res = copy_to_user(dest_buf + first_len,
					hevc->userdata_info.data_buf, data_size - first_len);
				if (res) {
					pr_info("%s[2], res %ld, request %d\n", __func__, res, data_size);
					copy_ok = 0;
				}
				rec->rec_len -= (data_size - res);
				rec->rec_start = (data_size - first_len - res);
				puserdata_para->data_size = (data_size - res);
			}
		} else {
			res = copy_to_user(dest_buf, rec_start, data_size);
			if (res) {
				pr_info("%s[3], res %ld, request %d\n", __func__, res, data_size);
				copy_ok = 0;
			}
			rec->rec_len -= (data_size - res);
			rec->rec_start += (data_size - res);
			puserdata_para->data_size = (data_size - res);
			copy_ok = 1;

		}

		if (copy_ok) {
			hevc->userdata_info.read_index++;
			if (hevc->userdata_info.read_index >= USERDATA_FIFO_NUM)
				hevc->userdata_info.read_index = 0;
		}
	} else {
		res = (u32)copy_to_user(dest_buf,
							(void *)rec_start,
							data_size);
		if (res) {
			pr_info("%s[4], res %ld, request %d\n",
				__func__, res, data_size);
			copy_ok = 0;
		}

		rec->rec_len -= data_size - res;
		rec->rec_start += data_size - res;
		puserdata_para->data_size = data_size - res;
	}

	puserdata_para->meta_info = rec->meta_info;

	if (hevc->userdata_info.read_index <= hevc->userdata_info.write_index)
		puserdata_para->meta_info.records_in_que =
			hevc->userdata_info.write_index -
			hevc->userdata_info.read_index;
	else
		puserdata_para->meta_info.records_in_que =
			hevc->userdata_info.write_index +
			USERDATA_FIFO_NUM - hevc->userdata_info.read_index;

	puserdata_para->version = (0<<24|0<<16|0<<8|1);

	mutex_unlock(&hevc->userdata_mutex);

	return 1;
}

void vh266_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;

	if (hevc) {
		mutex_lock(&hevc->userdata_mutex);
		pr_info("%s, bInit %d, ri %d, wi %d\n", __func__,
			bInit, hevc->userdata_info.read_index,
			hevc->userdata_info.write_index);

		hevc->userdata_info.read_index = 0;
		hevc->userdata_info.write_index = 0;

		if (bInit)
			hevc->userdata_info.last_wp = 0;
		mutex_unlock(&hevc->userdata_mutex);
	}
}

static void vh266_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

static void vh266_userdata_fill_vpts(struct hevc_state_s *hevc,
	u32 vpts, int pts_valid, u32 poc)
{
	u8 *pdata;
	u8 *pmax_sei_data_buffer;
	u8 *sei_data_buf;
	int i;
	int wp;
	int data_length;
	struct h266_userdata_record_t *p_rec;
	struct userdata_meta_info_t meta_info;

	if (hevc->sei_itu_data_len <= 0)
		return;
	sei_data_buf = hevc->sei_itu_data_buf;
	pdata = hevc->sei_user_data_buffer + hevc->sei_user_data_wp;
	pmax_sei_data_buffer = hevc->sei_user_data_buffer + USER_DATA_SIZE;
	memset(&meta_info, 0, sizeof(meta_info));

	for (i = 0; i < hevc->sei_itu_data_len; i++) {
		*pdata++ = sei_data_buf[i];
		if (pdata >= pmax_sei_data_buffer)
			pdata = hevc->sei_user_data_buffer;
	}

	hevc->sei_user_data_wp = (hevc->sei_user_data_wp
		+ hevc->sei_itu_data_len) % USER_DATA_SIZE;
	hevc->sei_itu_data_len = 0;

	meta_info.duration = hevc->frame_dur;
	meta_info.flags |= (VFORMAT_H266 << 3);
	meta_info.flags |= (hevc->vvc_dec->cur_pic->pic_struct << 12);
	meta_info.vpts = vpts;
	meta_info.vpts_valid = pts_valid;
	meta_info.poc_number = poc;

	/*pr_info("one record ready pts %d, poc %d\n", vpts, meta_info.poc_number);*/
	wp = hevc->sei_user_data_wp;
	if (hevc->sei_user_data_wp > hevc->userdata_info.last_wp)
		data_length = wp - hevc->userdata_info.last_wp;
	else
		data_length = wp + hevc->userdata_info.buf_len
			- hevc->userdata_info.last_wp;

	if (data_length & 0x7)
		data_length = (((data_length + 8) >> 3) << 3);

	p_rec = &hevc->ud_record;
	p_rec->meta_info = meta_info;
	p_rec->rec_start = hevc->userdata_info.last_wp;
	p_rec->rec_len = data_length;
	hevc->userdata_info.last_wp = wp;

	hevc->wait_for_udr_send = 1;

	/* notify userdata ready */
	mutex_lock(&hevc->userdata_mutex);
	hevc->userdata_info.records[hevc->userdata_info.write_index]
		= hevc->ud_record;
	hevc->userdata_info.write_index++;
	if (hevc->userdata_info.write_index >= USERDATA_FIFO_NUM)
		hevc->userdata_info.write_index = 0;
	mutex_unlock(&hevc->userdata_mutex);

	vdec_wakeup_userdata_poll(hw_to_vdec(hevc));
	hevc->wait_for_udr_send = 0;
}

#endif

static void set_frame_info(struct hevc_state_s *hevc, struct vframe_s *vf,
			struct PIC_s *pic)
{
	unsigned int ar;
	int i, j;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	struct vframe_master_display_colour_s *vf_dp
		= &vf->prop.master_display_colour;

	vf->width = pic->width /
		get_double_write_ratio(pic->double_write_mode);
	vf->height = pic->height /
		get_double_write_ratio(pic->double_write_mode);

	vf->duration = hevc->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	ar = min_t(u32, hevc->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	hevc->ratio_control = vf->ratio_control;
	if (pic->aux_data_buf
		&& pic->aux_data_size) {
		/* parser sei */
		p = pic->aux_data_buf;
		while (p < pic->aux_data_buf
			+ pic->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				//parse_sei(hevc, pic, p, size);
			}
			p += size;
		}
	}
	if (hevc->video_signal_type & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK) {
		vf->signal_type = pic->video_signal_type;

		vf->ext_signal_type = 0;
		/* When the matrix_coeffiecents, transfer_characteristics and colour_primaries
		 * syntax elements are absent, their values shall be presumed to be equal to 2
		 */
		if ((vf->signal_type & 0x1000000) == 0) {
			vf->signal_type = vf->signal_type & 0xff000000;
			vf->signal_type = vf->signal_type | 0x20202;
		}
		if (pic->sei_present_flag & SEI_HDR10PLUS_MASK) {
			u32 data;
			data = vf->signal_type;
			data = data & 0xFFFF00FF;
			data = data | (0x30<<8);
			vf->signal_type = data;
		}

		if (pic->sei_present_flag & SEI_HDR_CUVA_MASK) {
			u32 data;
			data = vf->signal_type;
			data = data & 0x7FFFFFFF;
			data = data | (1<<31);
			vf->signal_type = data;
		}

		if (pic->sei_present_flag & SEI_HDR_FMM_MASK) {
			u32 data;
			data = vf->ext_signal_type;
			data = data & 0xFFFFFFFE;
			data = data | (1<<0);
			vf->ext_signal_type = data;
		}
	}
	else {
		vf->signal_type = 0;
		vf->ext_signal_type = 0;
	}

	hevc->video_signal_type_debug = vf->signal_type;

	/* master_display_colour */
	if (hevc->sei_hdr10_flag & SEI_MASTER_DISPLAY_COLOR_MASK) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++)
				vf_dp->primaries[i][j] = hevc->primaries[i][j];
		for (i = 0; i < 2; i++) {
			vf_dp->white_point[i] = hevc->white_point[i];
			vf_dp->luminance[i]
				= hevc->luminance[i];
		}
		vf_dp->present_flag = 1;
	} else
		vf_dp->present_flag = 0;

	/* content_light_level */
	if (hevc->sei_hdr10_flag & SEI_CONTENT_LIGHT_LEVEL_MASK) {
		vf_dp->content_light_level.max_content
			= hevc->content_light_level[0];
		vf_dp->content_light_level.max_pic_average
			= hevc->content_light_level[1];
		vf_dp->content_light_level.present_flag = 1;
	} else
		vf_dp->content_light_level.present_flag = 0;

	vf->hdr10p_data_size = pic->hdr10p_data_size;
	vf->hdr10p_data_buf = pic->hdr10p_data_buf;

	if (hevc->dv_profile == 4)
		vf->ext_signal_type |= (1 << 1);
	else if (hevc->dv_profile == 7)
		vf->ext_signal_type |= (1 << 2);

	vf->sidebind_type = hevc->sidebind_type;
	vf->sidebind_channel_id = hevc->sidebind_channel_id;
	vf->codec_vfmt = VFORMAT_H266;
}

static int vh266_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	spin_lock_irqsave(&h266_lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hevc->newframe_q);
	states->buf_avail_num = kfifo_len(&hevc->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	spin_unlock_irqrestore(&h266_lock, flags);
	return 0;
}

static struct vframe_s *vh266_vf_peek(void *op_arg)
{
	struct vframe_s *vf[2] = {0, 0};
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;

	if (force_disp_pic_index & 0x100) {
		if (force_disp_pic_index & 0x200)
			return NULL;
		return &hevc->vframe_dummy;
	}

	if (kfifo_len(&hevc->display_q) > VF_POOL_SIZE) {
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"kfifo len:%d invalid, peek error\n",
			kfifo_len(&hevc->display_q));
		return NULL;
	}

	if (kfifo_out_peek(&hevc->display_q, (void *)&vf, 2)) {
		if (vf[1]) {
			vf[0]->next_vf_pts_valid = true;
			vf[0]->next_vf_pts = vf[1]->pts;
		} else
			vf[0]->next_vf_pts_valid = false;
		return vf[0];
	}

	return NULL;
}

static struct vframe_s *vh266_vf_get(void *op_arg)
{
	struct vframe_s *vf;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;
	else if (step == 1)
		step = 2;

	if (kfifo_get(&hevc->display_q, &vf)) {
		struct vframe_s *next_vf = NULL;

		ATRACE_COUNTER(hevc->trace.vf_get_name, (long)vf);
		ATRACE_COUNTER(hevc->trace.disp_q_name, kfifo_len(&hevc->display_q));
#ifdef MULTI_INSTANCE_SUPPORT
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, vf->canvas0_config[0].phy_addr);
#else
		ATRACE_COUNTER(hevc->trace.get_canvas0_addr, vf->canvas0Addr);
#endif
		if (hevc->discard_dv_data || (vdec_stream_based(vdec) && (vf->type & VIDTYPE_INTERLACE))) {
			vf->discard_dv_data = true;
		}

		if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS) {
			hevc_print(hevc, 0,
				"%s(vf 0x%p type %x index 0x%x poc %d/%d) pts(%d,%d) dur %d, discard_dv:%d\n",
				__func__, vf, vf->type, vf->index,
				get_pic_poc(hevc, vf->index & 0xff),
				get_pic_poc(hevc, (vf->index >> 8) & 0xff),
				vf->pts, vf->pts_us64,
				vf->duration, vf->discard_dv_data);
#ifdef MULTI_INSTANCE_SUPPORT
			hevc_print(hevc, 0, "get canvas0 addr:0x%x\n", vf->canvas0_config[0].phy_addr);
#else
			hevc_print(hevc, 0, "get canvas0 addr:0x%x\n", vf->canvas0Addr);
#endif
		}
		if (get_dbg_flag(hevc) & H266_DEBUG_DV) {
			struct PIC_s *pic = &hevc->vvc_dec->pic_pool[vf->index & 0xff];
			hevc_print(hevc, 0, "pic 0x%p aux size %d:\n",
					pic, pic->aux_data_size);
			if (pic->aux_data_buf && pic->aux_data_size > 0) {
				int i;
				for (i = 0; i < pic->aux_data_size; i++) {
					hevc_print_cont(hevc, 0,
						"%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}
				hevc_print_cont(hevc, 0, "\n");
			}
		}
		hevc->show_frame_num++;
		vf->index_disp = atomic_read(&hevc->vf_get_count);
		vf->omx_index = atomic_read(&hevc->vf_get_count);
		atomic_add(1, &hevc->vf_get_count);

		vf->vf_ud_param.magic_code = UD_MAGIC_CODE;
		vf->vf_ud_param.ud_param.buf_len = 0;
		vf->vf_ud_param.ud_param.pbuf_addr = NULL;
		vf->vf_ud_param.ud_param.instance_id = vdec->afd_video_id;

		vf->vf_ud_param.ud_param.meta_info.duration = vf->duration;
		vf->vf_ud_param.ud_param.meta_info.flags = (VFORMAT_H266 << 3);
		vf->vf_ud_param.ud_param.meta_info.vpts = vf->pts;
		if (vf->pts)
			vf->vf_ud_param.ud_param.meta_info.vpts_valid = 1;

		if (kfifo_peek(&hevc->display_q, &next_vf) && next_vf) {
			vf->next_vf_pts_valid = true;
			vf->next_vf_pts = next_vf->pts;
		} else
			vf->next_vf_pts_valid = false;

		kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
		return vf;
	}

	return NULL;
}
static bool vf_valid_check(struct vframe_s *vf, struct hevc_state_s *hevc) {
	int i;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vf == &hevc->vfpool[i]  || vf == &hevc->vframe_dummy)
			return true;
	}
	hevc_print(hevc, 0," h266 invalid vf been put, vf = %p\n", vf);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,"valid vf[%d]= %p \n", i, &hevc->vfpool[i]);
	}
	return false;
}

static void vh266_recycle_dec_resource(void *priv,
						struct aml_buf *aml_buf)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)priv;
	struct vframe_s *vf = &aml_buf->vframe;
	unsigned long flags;

	if (hevc->enable_fence && vf->fence) {
		int ret, i;

		mutex_lock(&hevc->fence_mutex);
		ret = dma_fence_get_status(vf->fence);
		if (ret == 0) {
			for (i = 0; i < VF_POOL_SIZE; i++) {
				if (hevc->fence_vf_s.fence_vf[i] == NULL) {
					hevc->fence_vf_s.fence_vf[i] = vf;
					hevc->fence_vf_s.used_size++;
					mutex_unlock(&hevc->fence_mutex);
					return;
				}
			}
		}
		mutex_unlock(&hevc->fence_mutex);
	}

	spin_lock_irqsave(&h266_lock, flags);
	if (hevc->enable_fence && vf->fence) {
		vdec_fence_put(vf->fence);
		vf->fence = NULL;
	}
	spin_unlock_irqrestore(&h266_lock, flags);

	return;
}

static void vh266_vf_put(struct vframe_s *vf, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	unsigned char index_top;
	unsigned char index_bot;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	struct aml_buf *aml_buf;

	if (!vf)
		return;
	if (vf == (&hevc->vframe_dummy))
		return;
	if (vf && (vf_valid_check(vf, hevc) == false))
		return;

	if (hevc->enable_fence && vf->fence) {
		int ret, i;

		mutex_lock(&hevc->fence_mutex);
		ret = dma_fence_get_status(vf->fence);
		if (ret == 0) {
			for (i = 0; i < VF_POOL_SIZE; i++) {
				if (hevc->fence_vf_s.fence_vf[i] == NULL) {
					hevc->fence_vf_s.fence_vf[i] = vf;
					hevc->fence_vf_s.used_size++;
					mutex_unlock(&hevc->fence_mutex);
					return;
				}
			}
		}
		mutex_unlock(&hevc->fence_mutex);
	}

	ATRACE_COUNTER(hevc->trace.vf_put_name, (long)vf);
#ifdef MULTI_INSTANCE_SUPPORT
	ATRACE_COUNTER(hevc->trace.put_canvas0_addr, vf->canvas0_config[0].phy_addr);
#else
	ATRACE_COUNTER(hevc->trace.put_canvas0_addr, vf->canvas0Addr);
#endif
	index_top = vf->index & 0xff;
	index_bot = (vf->index >> 8) & 0xff;
	if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
		hevc_print(hevc, 0,
			"%s(vf 0x%p type %d index 0x%x put canvas0 addr:0x%x)\n",
			__func__, vf, vf->type, vf->index
#ifdef MULTI_INSTANCE_SUPPORT
			, vf->canvas0_config[0].phy_addr
#else
			, vf->canvas0Addr
#endif
			);
	atomic_add(1, &hevc->vf_put_count);
	spin_lock_irqsave(&h266_lock, flags);
	kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hevc->trace.new_q_name, kfifo_len(&hevc->newframe_q));
	if (hevc->enable_fence && vf->fence) {
		vdec_fence_put(vf->fence);
		vf->fence = NULL;
	}
	spin_unlock_irqrestore(&h266_lock, flags);

	aml_buf = (struct aml_buf *)vf->v4l_mem_handle;
	aml_buf_put_ref(&ctx->bm, aml_buf);
	vh266_recycle_dec_resource(hevc, aml_buf);
#ifdef MULTI_INSTANCE_SUPPORT
	vdec_up(vdec);
#endif
}

static int vh266_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	if (type & VFRAME_EVENT_RECEIVER_RESET) {

	} else if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;

		if (!req->vf) {
			req->aux_size = atomic_read(&hevc->vf_put_count);
			return 0;
		}

		if (((req->vf->signal_type & 0x80000000) == 0) &&
			(((req->vf->signal_type >> 8)  & 0xff) != 0x30) &&
			req->vf->discard_dv_data) {
			req->aux_size = atomic_read(&hevc->vf_put_count);
			return 0;
		}
		spin_lock_irqsave(&h266_lock, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		req->format = VFORMAT_H266;
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff
			&& index < MAX_REF_PIC_NUM) {
			req->aux_buf = hevc->vvc_dec->pic_pool[index].aux_data_buf;
			req->aux_size = hevc->vvc_dec->pic_pool[index].aux_data_size;
			req->dv_enhance_exist = 0;
		}
		spin_unlock_irqrestore(&h266_lock, flags);

		if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
			hevc_print(hevc, 0,
			"%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
			__func__, type, index, req->aux_size);
	}
	else if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static bool v4l_output_dw_with_compress(struct hevc_state_s *hevc, int dw)
{
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

	if ((dw == 0x10) ||
		IS_8K_SIZE(hevc->frame_width, hevc->frame_height))
		return false;
	if (hevc->interlace_flag &&
		(!(is_support_interlace_avbc() &&
		(ctx->vpp_cfg.enable_nr == 1) &&
		(ctx->vpp_cfg.enable_local_buf == 1))))
		return false;

	return true;
}

#ifdef HEVC_PIC_STRUCT_SUPPORT
static int recycle_pending_vframe(struct hevc_state_s *hevc, struct vframe_s *vf)
{
	unsigned long flags;
	int index1;
	int index2;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	u64 timestamp_back = ctx->current_timestamp;

	if (v4l_output_dw_with_compress(hevc, hevc->double_write_mode)) {
		vf->type |= VIDTYPE_COMPRESS;
		if (hevc->mmu_enable)
			vf->type |= VIDTYPE_SCATTER;
	}

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s warning(1), vf=>newframe_q: (index 0x%x), vf 0x%px\n",
		__func__, vf->index, vf);
	hevc->vf_pre_count++;
	spin_lock_irqsave(&h266_lock, flags);
	kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
	index1 = vf->index & 0xff;
	index2 = (vf->index >> 8) & 0xff;
	if (index1 >= MAX_REF_PIC_NUM &&
		index2 >= MAX_REF_PIC_NUM) {
		spin_unlock_irqrestore(&h266_lock, flags);
		return -1;
	}

	if (index1 < MAX_REF_PIC_NUM) {
		hevc->vvc_dec->pic_pool[index1].vf_ref = 0;
		hevc->vvc_dec->pic_pool[index1]->error_mark = 1;
		ctx->current_timestamp = hevc->vvc_dec->pic_pool[index1]->timestamp;
		vdec_v4l_post_error_frame_event(ctx);
	}
	if (index2 < MAX_REF_PIC_NUM) {
		hevc->vvc_dec->pic_pool[index2].vf_ref = 0;
		hevc->vvc_dec->pic_pool[index2]->error_mark = 1;
		ctx->current_timestamp = hevc->vvc_dec->pic_pool[index2]->timestamp;
		vdec_v4l_post_error_frame_event(ctx);
	}

	ctx->current_timestamp = timestamp_back;
	if (hevc->wait_buf != 0)
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
			0x1);
	spin_unlock_irqrestore(&h266_lock, flags);
	return 0;
}

static int process_pending_vframe(struct hevc_state_s *hevc,
	struct PIC_s *pair_pic, unsigned char pair_frame_top_flag)
{
	struct vframe_s *vf;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

	if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
		hevc_print(hevc, 0,
			"%s: pair_pic index 0x%x %s\n",
			__func__,  pair_pic ? pair_pic->index : -1,
			pair_frame_top_flag ?
			"top" : "bot");

	if (kfifo_len(&hevc->pending_q) > 1) {
		/* do not pending more than 1 frame */
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
			return -1;
		}
		if (recycle_pending_vframe(hevc, vf))
			return -1;
	}

	if (kfifo_peek(&hevc->pending_q, &vf)) {
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
		}
		if (vf == NULL)
			return -1;

		if (pair_pic == NULL || pair_pic->vf_ref <= 0) {
			/*
			 *if pair_pic is recycled (pair_pic->vf_ref <= 0),
			 *do not use it
			 */
			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"%s warning(2), vf=>newframe_q: (index 0x%x)\n",
					__func__, vf->index);
			if (recycle_pending_vframe(hevc, vf))
				return -1;
		} else if ((!pair_frame_top_flag) &&
			(((vf->index >> 8) & 0xff) == 0xff)) {
			if (!ctx->no_fbc_output &&
				v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}
			vf->index &= 0xff;
			vf->index |= (pair_pic->index << 8);
			pair_pic->vf_ref++;
			hevc->pre_bot_pic = NULL;
			vdec_vframe_ready(hw_to_vdec(hevc), vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(hevc->trace.pts_name, vf->pts);
			hevc->vf_pre_count++;
			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
		} else if (pair_frame_top_flag &&
			((vf->index & 0xff) == 0xff)) {
			if (!ctx->no_fbc_output &&
				v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}
			vf->index &= 0xff00;
			vf->index |= pair_pic->index;
			pair_pic->vf_ref++;
			hevc->pre_top_pic = NULL;
			vdec_vframe_ready(hw_to_vdec(hevc), vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(hevc->trace.pts_name, vf->pts);
			hevc->vf_pre_count++;
			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
		} else {
			if (recycle_pending_vframe(hevc, vf))
				return -1;
		}
	}
	return 0;
}
#endif

#if 0
static void update_vf_memhandle(struct hevc_state_s *hevc,
	struct vframe_s *vf, struct PIC_s *pic)
{
	vf->mem_handle = NULL;
	vf->mem_head_handle = NULL;

	if (vf->type & VIDTYPE_SCATTER) {
#ifdef VVC_10B_MMU_DW
		if (hevc->dw_mmu_enable) {
			vf->mem_handle =
				decoder_mmu_box_get_mem_handle(
					hevc->mmu_box_dw, pic->index);
			vf->mem_head_handle =
				decoder_bmmu_box_get_mem_handle(
					hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
		} else

#endif
		{
			vf->mem_handle =
				decoder_mmu_box_get_mem_handle(
					hevc->mmu_box, pic->index);
			vf->mem_head_handle =
				decoder_bmmu_box_get_mem_handle(
					hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
		}
	} else {
		vf->mem_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
		vf->mem_head_handle = NULL;
	}
	return;
}
#endif

static void fill_frame_info(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned int framesize, unsigned int pts)
{
	struct vframe_qos_s *vframe_qos = &hevc->vframe_qos;
	/*if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR)
		vframe_qos->type = 4;
	else*/ if (pic->slice_type == I_SLICE)
		vframe_qos->type = 1;
	else if (pic->slice_type == P_SLICE)
		vframe_qos->type = 2;
	else if (pic->slice_type == B_SLICE)
		vframe_qos->type = 3;

	if (input_frame_based(hw_to_vdec(hevc)))
		vframe_qos->size = pic->frame_size;
	else
		vframe_qos->size = framesize;
	vframe_qos->pts = pts;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "slice:%d, poc:%d\n", pic->slice_type, pic->poc);
#endif

	vframe_qos->max_mv = pic->max_mv;
	vframe_qos->avg_mv = pic->avg_mv;
	vframe_qos->min_mv = pic->min_mv;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "mv: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_mv,
			vframe_qos->avg_mv,
			vframe_qos->min_mv);
#endif

	vframe_qos->max_qp = pic->max_qp;
	vframe_qos->avg_qp = pic->avg_qp;
	vframe_qos->min_qp = pic->min_qp;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "qp: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_qp,
			vframe_qos->avg_qp,
			vframe_qos->min_qp);
#endif

	vframe_qos->max_skip = pic->max_skip;
	vframe_qos->avg_skip = pic->avg_skip;
	vframe_qos->min_skip = pic->min_skip;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "skip: max:%d,	avg:%d, min:%d\n",
			vframe_qos->max_skip,
			vframe_qos->avg_skip,
			vframe_qos->min_skip);
#endif

	vframe_qos->num++;

}

static inline void hevc_update_gvs(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	if (hevc->gvs->frame_height != pic->height) {
		hevc->gvs->frame_width = pic->width;
		hevc->gvs->frame_height = pic->height;
	}
	if (hevc->gvs->frame_dur != hevc->frame_dur) {
		hevc->gvs->frame_dur = hevc->frame_dur;
		if (hevc->frame_dur != 0)
			hevc->gvs->frame_rate = ((96000 * 10 / hevc->frame_dur) % 10) < 5 ?
					96000 / hevc->frame_dur : (96000 / hevc->frame_dur +1);
		else
			hevc->gvs->frame_rate = -1;
	}
	hevc->gvs->error_count = hevc->gvs->error_frame_count;
	hevc->gvs->status = hevc->stat | hevc->fatal_error;
	if (hevc->gvs->ratio_control != hevc->ratio_control)
		hevc->gvs->ratio_control = hevc->ratio_control;
}

static void put_vf_to_display_q(struct hevc_state_s *hevc, struct vframe_s *vf)
{
	hevc->vf_pre_count++;
	decoder_do_frame_check(hw_to_vdec(hevc), vf);
	vdec_vframe_ready(hw_to_vdec(hevc), vf);
	kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hevc->trace.pts_name, vf->pts);
}

static int post_prepare_process(struct vdec_s *vdec, struct PIC_s *frame)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;

	if (force_disp_pic_index & 0x100) {
		/*recycle directly*/
		frame->show_frame = false;
		hevc_print(hevc, 0, "discard show frame.\n");
		return 0;
	}

	frame->show_frame = true;

	return 0;
}

static void v4l_hevc_update_frame_info(struct hevc_state_s *hevc, struct vframe_s *vf,
	struct PIC_s *pic)
{
	struct aml_vcodec_ctx *ctx = hevc->v4l2_ctx;
	struct dec_frame_info_s frm_info = {0};

	memcpy(&(frm_info.qos), &(hevc->vframe_qos), sizeof(struct vframe_qos_s));

	frm_info.frame_size = pic->frame_size;
	frm_info.offset = hevc->curr_pic_offset;
	frm_info.frame_poc = pic->poc;
	frm_info.type = pic->slice_type;
	frm_info.error_flag = pic->error_mark;
	frm_info.decode_time_cost = pic->hw_decode_time;
	frm_info.num = pic->decode_idx;
	frm_info.pic_height = pic->height;
	frm_info.pic_width = pic->width;
	frm_info.signal_type = pic->video_signal_type;

	if (hevc->gvs) {
		frm_info.bitrate = hevc->gvs->bit_rate;
		frm_info.status = hevc->gvs->status;
		frm_info.ratio_control = hevc->gvs->ratio_control;
	}
	if (vf) {
		frm_info.ext_signal_type = vf->ext_signal_type;
		frm_info.vf_type = vf->type;
		frm_info.timestamp = vf->timestamp;
		frm_info.pts = vf->pts;
		frm_info.pts_us64 = vf->pts_us64;
	}
	ctx->dec_intf.decinfo_event_report(ctx, AML_DECINFO_EVENT_FRAME, &frm_info);
}

static int post_video_frame(struct vdec_s *vdec, struct PIC_s *pic)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	struct vframe_s *vf = NULL;
	unsigned int stream_offset = pic->stream_offset;
	unsigned short slice_type = pic->slice_type;
	ulong nv_order = VIDTYPE_VIU_NV21;
	u32 frame_size = 0;
	struct vdec_info tmp4x;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;
	struct aml_buf *aml_buf = NULL;
	int index;

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

	if (kfifo_get(&hevc->newframe_q, &vf) == 0) {
		hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
		return -1;
	}

	if (vf) {
		vf->frame_type = 0;
		vf->src_fmt.dv_id = v4l2_ctx->dv_id;

		if (pic->error_mark) {
			vf->frame_type |= V4L2_BUF_FLAG_ERROR;
		}

		vf->v4l_mem_handle = hevc->m_BUF[pic->index].v4l_ref_buf_addr;
		aml_buf = (struct aml_buf *)vf->v4l_mem_handle;

		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: pic index 0x%x error_mark %d dma addr: 0x%lx\n",
			__func__, pic->index, pic->error_mark,
			pic->cma_alloc_addr);

		if (!vf->v4l_mem_handle) {
			kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
			hevc_print(hevc, 0,
			"[ERR]: v4l_ref_buf_addr(index: %d) is NULL!\n",
			pic->index);

			return -1;
		}

		if (hevc->enable_fence) {
			/* fill fence information. */
			if (hevc->fence_usage == FENCE_USE_FOR_DRIVER)
				vf->fence	= pic->fence;
		}

#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(vdec)) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
			vf->timestamp = pic->timestamp;
		} else {
#endif
			hevc_print(hevc, H266_DEBUG_OUT_PTS,
				"call pts_lookup_offset_us64(0x%x)\n",
				stream_offset);
			if (vdec->vbuf.use_ptsserv == SINGLE_PTS_SERVER_DECODER_LOOKUP) {
				if (pts_lookup_offset_us64
					(PTS_TYPE_VIDEO, stream_offset, &vf->pts,
					&frame_size, 0,
					 &vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
					hevc->pts_missed++;
#endif
					vf->pts = 0;
					vf->pts_us64 = 0;
				} else {
#ifdef DEBUG_PTS
					hevc->pts_hit++;
#endif
				}
			}
#ifdef MULTI_INSTANCE_SUPPORT
		}
#endif
		if (pts_unstable && (hevc->frame_dur > 0))
			hevc->pts_mode = PTS_NONE_REF_USE_DURATION;

		fill_frame_info(hevc, pic, frame_size, vf->pts);

		v4l_hevc_update_frame_info(hevc, vf, pic);
		if (vf->pts != 0)
			hevc->last_lookup_pts = vf->pts;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2))
			vf->pts = hevc->last_pts + DUR2PTS(hevc->frame_dur);
		hevc->last_pts = vf->pts;

		if (vf->pts_us64 != 0)
			hevc->last_lookup_pts_us64 = vf->pts_us64;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2)) {
			vf->pts_us64 = hevc->last_pts_us64 +
				(DUR2PTS(hevc->frame_dur) * 100 / 9);
		}
		hevc->last_pts_us64 = vf->pts_us64;
		if ((get_dbg_flag(hevc) & H266_DEBUG_OUT_PTS) != 0) {
			hevc_print(hevc, 0,
			"H266 dec out pts: vf->pts=%d, vf->pts_us64 = %lld, ts: %llu\n",
			 vf->pts, vf->pts_us64, vf->timestamp);
		}

		/*
		 *vf->index:
		 *(1) vf->type is VIDTYPE_PROGRESSIVE
		 *	and vf->canvas0Addr !=  vf->canvas1Addr,
		 *	vf->index[7:0] is the index of top pic
		 *	vf->index[15:8] is the index of bot pic
		 *(2) other cases,
		 *	only vf->index[7:0] is used
		 *	vf->index[15:8] == 0xff
		 */
		vf->index = 0xff00 | pic->index;

/*SUPPORT_10BIT*/
		if (pic->double_write_mode & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
#ifdef VVC_10B_MMU_DW
			vf->dwBodyAddr = 0;
			vf->dwHeadAddr = 0;
#endif
		} else {
			if (hevc->mmu_enable) {
				vf->compBodyAddr = 0;
				vf->compHeadAddr = pic->header_adr;
#ifdef VVC_10B_MMU_DW
				vf->dwBodyAddr = 0;
				vf->dwHeadAddr = 0;
				if (pic->double_write_mode & 0x20) {
					u32 mode = pic->double_write_mode & 0xf;
					if (mode == 5 || mode == 3)
						vf->dwHeadAddr = pic->header_dw_adr;
					else if ((mode == 1 || mode == 2 || mode == 4)
					&& (debug & H266_DEBUG_OUT_PTS) == 0) {
						vf->compHeadAddr = pic->header_dw_adr;
						pr_debug("Use dw mmu for display\n");
					}
				}
#endif
			} else {
				vf->compBodyAddr = pic->mc_y_adr; /*body adr*/
				vf->compHeadAddr = pic->mc_y_adr +
							pic->losless_comp_body_size;
				vf->mem_head_handle = NULL;
			}
			/*head adr*/
			vf->canvas0Addr = vf->canvas1Addr = 0;
		}

		if (pic->double_write_mode) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			vf->type |= nv_order;

			if (!v4l2_ctx->no_fbc_output) {
				if (v4l_output_dw_with_compress(hevc, pic->double_write_mode)) {
					vf->type |= VIDTYPE_COMPRESS;
					if (hevc->mmu_enable)
						vf->type |= VIDTYPE_SCATTER;
				}
			}

#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag &&
				(get_dbg_flag(hevc)
				& H266_CFG_CANVAS_IN_DECODE) == 0) {
					vf->canvas0Addr = vf->canvas1Addr = -1;
					vf->plane_num = 2;
					vf->canvas0_config[0] = pic->canvas_config[0];
					vf->canvas0_config[1] = pic->canvas_config[1];
					vf->canvas1_config[0] = pic->canvas_config[0];
					vf->canvas1_config[1] = pic->canvas_config[1];
			} else
#endif
				vf->canvas0Addr = vf->canvas1Addr
				= spec2canvas(pic);
		} else {
			vf->canvas0Addr = vf->canvas1Addr = 0;
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			if (hevc->mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}

		if (hevc->mmu_enable &&
			(pic->width != pic->crop_w || pic->height != pic->crop_h)) {
			vf->src_crop.magic_code = SRC_CROP_MAGIC_CODE;
			vf->src_crop.bottom = pic->height - pic->crop_h;
			vf->src_crop.right = pic->width - pic->crop_w;
			vf->src_crop.top = 0;
			vf->src_crop.left = 0;
		}

		vf->compWidth = pic->width;
		vf->compHeight = pic->height;
		switch (pic->bit_depth_luma) {
		case 9:
			vf->bitdepth = BITDEPTH_Y9;
			break;
		case 10:
			vf->bitdepth = BITDEPTH_Y10;
			break;
		default:
			vf->bitdepth = BITDEPTH_Y8;
			break;
		}
		switch (pic->bit_depth_chroma) {
		case 9:
			vf->bitdepth |= (BITDEPTH_U9 | BITDEPTH_V9);
			break;
		case 10:
			vf->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
			break;
		default:
			vf->bitdepth |= (BITDEPTH_U8 | BITDEPTH_V8);
			break;
		}
		if ((vf->type & VIDTYPE_COMPRESS) == 0)
			vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (pic->mem_saving_mode == 1)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;

		set_frame_info(hevc, vf, pic);

		if (hevc->high_bandwidth_flag) {
			vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;
		}

		vf->width = pic->crop_w;
		vf->height = pic->crop_h;

		if (force_w_h != 0) {
			vf->width = (force_w_h >> 16) & 0xffff;
			vf->height = force_w_h & 0xffff;
		}
		if (force_fps & 0x100) {
			u32 rate = force_fps & 0xff;

			if (rate)
				vf->duration = 96000/rate;
			else
				vf->duration = 0;
		}
		if (force_fps & 0x200) {
			vf->pts = 0;
			vf->pts_us64 = 0;
		}

#if 0
		if ((vdec->vbuf.use_ptsserv == MULTI_PTS_SERVER_UPPER_LOOKUP) && vdec_stream_based(vdec)) {
			u64 frame_type = 0;
			if (pic->slice_type == I_SLICE)
				frame_type = KEYFRAME_FLAG;
			else if (pic->slice_type == P_SLICE)
				frame_type = PFRAME_FLAG;
			else
				frame_type = BFRAME_FLAG;

			vf->pts_us64 = (((u64)vf->duration << 32 | (frame_type << 62)) & 0xffffffff00000000)
				| stream_offset;
			vf->pts = 0;
		} else if (vdec->vbuf.use_ptsserv == MULTI_PTS_SERVER_DECODER_LOOKUP) {
			u64 frame_type = 0;
			checkout_pts_offset pts_info;
			if (pic->slice_type == I_SLICE)
				frame_type = KEYFRAME_FLAG;
			else if (pic->slice_type == P_SLICE)
				frame_type = PFRAME_FLAG;
			else
				frame_type = BFRAME_FLAG;

			pts_info.offset = (((u64)vf->duration << 32 | (frame_type << 62)) & 0xffffffff00000000)
				| stream_offset;
			if (!ptsserver_checkout_pts_offset((vdec->pts_server_id & 0xff), &pts_info)) {
				vf->pts = pts_info.pts;
				vf->pts_us64 = pts_info.pts_64;
			} else {
				vf->pts = 0;
				vf->pts_us64 = 0;
			}
		}
#endif
		if (hevc->vvc_dec->cur_pic != NULL) {
			vf->sar_width = hevc->vvc_dec->cur_pic->sar_width;
			vf->sar_height = hevc->vvc_dec->cur_pic->sar_height;
		}

		vf->src_fmt.play_id = vdec->inst_cnt;

		vf->width = vf->width /
			get_double_write_ratio(pic->double_write_mode & 0xf);
		vf->height = vf->height /
			get_double_write_ratio(pic->double_write_mode & 0xf);

#if 0
#ifdef VVC_10B_MMU_DW
		if ((pic->double_write_mode & 0x20) &&
			((pic->double_write_mode & 0xf) == 2 ||
			(pic->double_write_mode & 0xf) == 4)) {
			vf->compWidth = vf->width;
			vf->compHeight = vf->height;
		}
#endif
#endif

		if (vdec->prog_only ||
			!(v4l2_ctx->vpp_is_need || v4l2_ctx->enable_di_post))
			pic->pic_struct = 0;

		vf->height <<= hevc->interlace_flag;
		vf->compHeight <<= hevc->interlace_flag;
		vf->canvas0_config[0].height <<= hevc->interlace_flag;
		vf->canvas0_config[1].height <<= hevc->interlace_flag;
#ifdef HEVC_PIC_STRUCT_SUPPORT
		if (pic->pic_struct == 3 || pic->pic_struct == 4) {
			struct vframe_s *vf2;

			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
					"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 2;
			vf->duration = vf->duration>>1;
			memcpy(vf2, vf, sizeof(struct vframe_s));

			if (v4l2_ctx->second_field_pts_mode) {
				vf2->timestamp = 0;
			}
			if (pic->pic_struct == 3) {
				vf->type = VIDTYPE_INTERLACE_TOP | nv_order;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM | nv_order;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM | nv_order;
				vf2->type = VIDTYPE_INTERLACE_TOP | nv_order;
			}
			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
				hevc->vf_pre_count++;
				vdec_vframe_ready(hw_to_vdec(hevc), vf2);
				kfifo_put(&hevc->display_q,(const struct vframe_s *)vf2);
				ATRACE_COUNTER(hevc->trace.pts_name, vf2->timestamp);
			} else {
				vh266_vf_put(vf, vdec);
				vh266_vf_put(vf2, vdec);
				atomic_add(2, &hevc->vf_get_count);
				hevc->vf_pre_count += 2;
				return 0;
			}
		} else if (pic->pic_struct == 5
			|| pic->pic_struct == 6) {
			struct vframe_s *vf2, *vf3;

			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (kfifo_get(&hevc->newframe_q, &vf3) == 0) {
				hevc_print(hevc, 0,
					"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 3;
			vf->duration = vf->duration/3;
			memcpy(vf2, vf, sizeof(struct vframe_s));
			memcpy(vf3, vf, sizeof(struct vframe_s));

			if (v4l2_ctx->second_field_pts_mode) {
				vf2->timestamp = 0;
				vf3->timestamp = 0;
			}

			if (pic->pic_struct == 5) {
				vf->type = VIDTYPE_INTERLACE_TOP | nv_order;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM | nv_order;
				vf3->type = VIDTYPE_INTERLACE_TOP | nv_order;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM | nv_order;
				vf2->type = VIDTYPE_INTERLACE_TOP | nv_order;
				vf3->type = VIDTYPE_INTERLACE_BOTTOM | nv_order;
			}
			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
				hevc->vf_pre_count++;
				vdec_vframe_ready(hw_to_vdec(hevc), vf2);
				kfifo_put(&hevc->display_q, (const struct vframe_s *)vf2);
				ATRACE_COUNTER(hevc->trace.pts_name, vf2->timestamp);
				hevc->vf_pre_count++;
				vdec_vframe_ready(hw_to_vdec(hevc), vf3);
				kfifo_put(&hevc->display_q, (const struct vframe_s *)vf3);
				ATRACE_COUNTER(hevc->trace.pts_name, vf3->timestamp);
			} else {
				vh266_vf_put(vf, vdec);
				vh266_vf_put(vf2, vdec);
				vh266_vf_put(vf3, vdec);
				atomic_add(3, &hevc->vf_get_count);
				hevc->vf_pre_count += 3;;
				return 0;
			}
		} else if (pic->pic_struct == 9
			|| pic->pic_struct == 10) {
			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc,
			pic, (pic->pic_struct == 9));
			//vf->height <<= 1;
			if (pic->show_frame) {
				decoder_do_frame_check(vdec, vf);
				vdec_vframe_ready(vdec, vf);
				/* process current vf */
				kfifo_put(&hevc->pending_q, (const struct vframe_s *)vf);
				if (pic->pic_struct == 9) {
					vf->type = VIDTYPE_INTERLACE_TOP | nv_order | VIDTYPE_VIU_FIELD;
					process_pending_vframe(hevc, hevc->pre_bot_pic, 0);
				} else {
					vf->type = VIDTYPE_INTERLACE_BOTTOM | nv_order | VIDTYPE_VIU_FIELD;
					vf->index = (pic->index << 8) | 0xff;
					process_pending_vframe(hevc, hevc->pre_top_pic, 1);
				}

				if (hevc->vf_pre_count == 0)
					hevc->vf_pre_count++;
			} else {
				vh266_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
				hevc->vf_pre_count++;
				return 0;
			}
		} else if (pic->pic_struct == 11
		    || pic->pic_struct == 12) {
			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);
			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc, pic, (pic->pic_struct == 11));

			/* put current into pending q */
			vf->height <<= 1;
			if (pic->pic_struct == 11)
				vf->type = VIDTYPE_INTERLACE_TOP | nv_order | VIDTYPE_VIU_FIELD;
			else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM | nv_order | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
			}
			if (pic->show_frame) {
				decoder_do_frame_check(vdec, vf);
				vdec_vframe_ready(vdec, vf);
				kfifo_put(&hevc->pending_q, (const struct vframe_s *)vf);
				if (hevc->vf_pre_count == 0)
					hevc->vf_pre_count++;

				/**/
				if (pic->pic_struct == 11)
					hevc->pre_top_pic = pic;
				else
					hevc->pre_bot_pic = pic;
			} else {
				vh266_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				hevc->vf_pre_count++;
				return 0;
			}
		} else {
			pic->vf_ref = 1;

			if (get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			switch (pic->pic_struct) {
			case 7:
				vf->duration <<= 1;
				break;
			case 8:
				vf->duration = vf->duration * 3;
				break;
			case 1:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_TOP | nv_order | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 1);
				hevc->pre_top_pic = pic;
				break;
			case 2:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_BOTTOM | nv_order | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 0);
				hevc->pre_bot_pic = pic;
				break;
			}
			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
			} else {
				vh266_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				hevc->vf_pre_count++;
				return 0;
			}
		}
#else
		vf->type_original = vf->type;
		pic->vf_ref = 1;
		put_vf_to_display_q(hevc, vf);
#endif
		ATRACE_COUNTER(hevc->trace.new_q_name, kfifo_len(&hevc->newframe_q));
		ATRACE_COUNTER(hevc->trace.disp_q_name, kfifo_len(&hevc->display_q));

		hevc_update_gvs(hevc, pic);
		memcpy(&tmp4x, hevc->gvs, sizeof(struct vdec_info));
		tmp4x.bit_depth_luma = pic->bit_depth_luma;
		tmp4x.bit_depth_chroma = pic->bit_depth_chroma;
		tmp4x.double_write_mode = pic->double_write_mode;
		vdec_fill_vdec_frame(vdec, &hevc->vframe_qos, &tmp4x, vf, pic->hw_decode_time);
		vdec->vdec_fps_detec(vdec->id);
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"%s(type %d index 0x%x poc %d/%d) pts(%d,%lld(0x%llx)) dur %d, video_id %d\n",
			__func__, vf->type, vf->index,
			get_pic_poc(hevc, vf->index & 0xff),
			get_pic_poc(hevc, (vf->index >> 8) & 0xff),
			vf->pts, vf->pts_us64, vf->pts_us64,
			vf->duration,
			vdec->video_id);

		if (pic->pic_struct == 10 || pic->pic_struct == 12) {
			index = (vf->index >> 8) & 0xff;
		} else {
			index = vf->index & 0xff;
		}

#ifdef AUX_DATA_CRC
		if (index < MAX_REF_PIC_NUM)
			decoder_do_aux_data_check(vdec, hevc->vvc_dec->pic_pool[index].aux_data_buf,
				hevc->vvc_dec->pic_pool[index].aux_data_size);
#endif

		hevc_print(hevc, H266_DEBUG_PRINT_SEI,
			"aux_data_size:%d signal_type:0x%x ext_signal_type:0x%x sei_present_flag:%d/%d inst_cnt:%d vf:%p\n",
			hevc->vvc_dec->pic_pool[index].aux_data_size, vf->signal_type, vf->ext_signal_type,
			pic->sei_present_flag, hevc->sei_hdr10_flag, vdec->inst_cnt, vf);

		if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI) {
			int i = 0;
			PR_INIT(128);
			for (i = 0; i < hevc->vvc_dec->pic_pool[index].aux_data_size; i++) {
				PR_FILL("%02x ", hevc->vvc_dec->pic_pool[index].aux_data_buf[i]);
				if (((i + 1) & 0xf) == 0)
					PR_INFO(hevc->index);
			}
			PR_INFO(hevc->index);
		}

		if (hevc->kpi_first_i_decoded == 0) {
			hevc->kpi_first_i_decoded = 1;
			pr_debug("[vdec_kpi][%s] First I frame decoded.\n",
				__func__);
		}

		if (without_display_mode == 0) {
			if (v4l2_ctx->is_stream_off) {
				vh266_vf_put(vh266_vf_get(vdec), vdec);
			} else {
				aml_buf_done(&v4l2_ctx->bm, aml_buf, BUF_USER_DEC);
			}
		} else
			vh266_vf_put(vh266_vf_get(vdec), vdec);
	}

	return 0;
}
#if 0
static int post_picture_early(struct vdec_s *vdec, int index)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	struct PIC_s *pic = hevc->m_PIC[index];

	if (!hevc->enable_fence)
		return 0;

	/* create fence for each buffers. */
	if (vdec_timeline_create_fence(vdec->sync))
		return -1;

	pic->fence		= vdec->sync->fence;
	pic->stream_offset	= READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	if (hevc->chunk) {
		pic->pts	= hevc->chunk->pts;
		pic->pts64	= hevc->chunk->pts64;
		pic->timestamp	= hevc->chunk->timestamp;
	}
	pic->show_frame = true;
	post_video_frame(vdec, pic);

	display_frame_count[hevc->index]++;

	return 0;
}
#endif
static int prepare_display_buf(void *hw, struct PIC_s *frame)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)hw;
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (frame) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s(poc %d pic_struct %d error_mark %d) enable_fence %d\n",
			__func__, frame->poc, frame->pic_struct, frame->error_mark, hevc->enable_fence);
#ifndef HEVC_PIC_STRUCT_SUPPORT
		if (frame->pic_struct)
			hevc_print(hevc, 0, "Error %s, pic_struct is not 0, need #define HEVC_PIC_STRUCT_SUPPORT\n",
				__func__);
#endif
	} else {
		hevc_print(hevc, 0, "Error %s, frame is NULL\n", __func__);
		return -1;
	}
	if (frame->error_mark &&
	((hevc->ignore_bufmgr_error &
	0x2) == 0)) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s error_mark = 1, skip displaying\n", __func__);
		vh266_report_err_timestamp_for_decoded_frames(hevc->v4l2_ctx, frame->timestamp);
		return 0;
	}

	if (hevc->enable_fence) {
		int i, j, used_size, ret;
		int signed_count = 0;
		struct vframe_s *signed_fence[VF_POOL_SIZE];

		post_prepare_process(vdec, frame);

		if (!frame->show_frame)
			pr_info("do not display.\n");

		frame->vf_ref = 1;

		/* notify signal to wake up wq of fence. */
		vdec_timeline_increase(vdec->sync, 1);
		mutex_lock(&hevc->fence_mutex);
		used_size = hevc->fence_vf_s.used_size;
		if (used_size) {
			for (i = 0, j = 0; i < VF_POOL_SIZE && j < used_size; i++) {
				if (hevc->fence_vf_s.fence_vf[i] != NULL) {
					ret = dma_fence_get_status(hevc->fence_vf_s.fence_vf[i]->fence);
					if (ret == 1) {
						signed_fence[signed_count] = hevc->fence_vf_s.fence_vf[i];
						hevc->fence_vf_s.fence_vf[i] = NULL;
						hevc->fence_vf_s.used_size--;
						signed_count++;
					}
					j++;
				}
			}
		}
		mutex_unlock(&hevc->fence_mutex);
		if (signed_count != 0) {
			for (i = 0; i < signed_count; i++)
				vh266_vf_put(signed_fence[i], vdec);
		}

		return 0;
	}

	if (post_prepare_process(vdec, frame))
		return -1;

	if (post_video_frame(vdec, frame))
		return -1;

	display_frame_count[hevc->index]++;
	return 0;
}

static int h266_recycle_frame_buffer(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	struct aml_buf *aml_buf;
	struct PIC_s *pic = NULL;
	ulong flags;
	int i;

	for (i = 0; i < hevc->used_buf_num && i < PIC_POOL_SIZE; ++i) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic == NULL)
			continue;

		if ((pic->used == 0) &&
			(pic->vf_ref || pic->error_mark) &&
			pic->cma_alloc_addr) {

			if ((ctx->vpp_is_need || ctx->enable_di_post) &&
				!(pic->error_mark && (hevc->nal_skip_policy & 0x2))) {
				if (pic->pic_struct == 3 || pic->pic_struct == 4 ||
					pic->pic_struct == 9 || pic->pic_struct == 10 ||
					pic->pic_struct == 11 || pic->pic_struct == 12) {
					if (pic->vf_ref < 2)
						continue;
				} else if (pic->pic_struct == 5 || pic->pic_struct == 6) {
					if (pic->vf_ref < 3)
						continue;
				}
			}

			aml_buf = (struct aml_buf *)hevc->m_BUF[pic->index].v4l_ref_buf_addr;

			hevc_print(hevc, H266_DEBUG_BUFMGR,
				"%s buf idx: %d pic index: %d dma addr: 0x%lx vb idx: %d vf_ref %d error_mark %d\n",
				__func__, i, pic->index, pic->cma_alloc_addr,
				aml_buf->index, pic->vf_ref, pic->error_mark);
			aml_buf_put_ref(&ctx->bm, aml_buf);
			if ((hevc->nal_skip_policy & 0x2) && pic->error_mark &&
				!pic->vf_ref) {
				aml_buf_put_ref(&ctx->bm, aml_buf);
				if (ctx->vpp_is_need || ctx->enable_di_post) {
					if (pic->pic_struct == 3 || pic->pic_struct == 4)
						aml_buf_put_ref(&ctx->bm, aml_buf);
					if (pic->pic_struct == 5 || pic->pic_struct == 6) {
						aml_buf_put_ref(&ctx->bm, aml_buf);
						aml_buf_put_ref(&ctx->bm, aml_buf);
					}
				}
			} else if (pic->error_mark && pic->vf_ref)
				hevc_print(hevc, H266_DEBUG_BUFMGR,
					"%s error pic conflict!\n", __func__);

			if (ctx->no_fbc_output && pic->vf_ref) {
				if (aml_buf->fbc->used[aml_buf->fbc->index] & 1) {
					decoder_mmu_box_free_idx(aml_buf->fbc->mmu,
								aml_buf->fbc->index);
					aml_buf->fbc->used[aml_buf->fbc->index] &= ~0x1;
					hevc_print(hevc, H266_DEBUG_BUFMGR,
						"free mmu buffer frame idx %d afbc_index: %d, dma addr: 0x%lx\n",
						pic->index,
						aml_buf->fbc->index,
						pic->cma_alloc_addr);
				}
			}

			spin_lock_irqsave(&h266_lock, flags);

			while (pic->vf_ref) {
				atomic_add(1, &hevc->vf_put_count);
				pic->vf_ref--;
			}

			pic->show_frame = false;
			pic->cma_alloc_addr = 0;
			pic->vf_ref = 0;
			hevc->m_BUF[pic->index].v4l_ref_buf_addr = 0;

			spin_unlock_irqrestore(&h266_lock, flags);

			break;
		}
	}

	return 0;
}

static bool is_available_buffer(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	struct PIC_s *pic = NULL;
	int i, free_count = 0;
	int free_slot = 0;

	/* Ignore the buffer available check until the head parse done. */
	if (!hevc->v4l_params_parsed) {
		/*
		 * If a resolution change and eos are detected, decoding will
		 * wait until the first valid buffer queue in driver
		 * before scheduling continues.
		 */
		if (ctx->v4l_resolution_change) {
			if (hevc->eos)
				return false;

			/* Wait for buffers ready. */
			if (!ctx->dst_queue_streaming)
				return false;
		} else {
			return true;
		}
	}

	/* Wait for the buffer number negotiation to complete. */
	if (hevc->used_buf_num == 0) {
		struct vdec_pic_info pic;

		vdec_v4l_get_pic_info(ctx, &pic);
		hevc->used_buf_num = pic.dpb_frames + pic.dpb_margin;

		if (hevc->used_buf_num > MAX_BUF_NUM)
			hevc->used_buf_num = MAX_BUF_NUM;
		if (hevc->used_buf_num == 0)
			return false;
	}

	h266_recycle_frame_buffer(hevc);

	for (i = 0; i < hevc->used_buf_num; ++i) {
		if ((hevc->vvc_dec->pic_pool[i].index != -1) &&
			//(hevc->vvc_dec->pic_pool[i].referenced == 0) &&
			(hevc->vvc_dec->pic_pool[i].vf_ref == 0) &&
			(hevc->vvc_dec->pic_pool[i].used == 0) &&
			(!hevc->vvc_dec->pic_pool[i].cma_alloc_addr)) {
			free_slot++;
		}
	}

	if (!free_slot) {
		hevc_print(hevc, H266_DEBUG_BUFMGR, "%s not enough free_slot %d!\n", __func__, free_slot);
		for (i = 0; i < hevc->used_buf_num; ++i) {
			pic = &hevc->vvc_dec->pic_pool[i];
		if (pic == NULL)
			continue;
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"%s buf idx:%d index:%d referenced:%d vf_ref:%d used:%d dma addr:0x%lx\n",
			__func__, i, pic->index,
			pic->referenced,
			pic->vf_ref,
			pic->used,
			pic->cma_alloc_addr);
		}
		return false;
	}

	if ((hevc->interlace_flag &&
		atomic_read(&ctx->vpp_cache_num) > 1) ||
		atomic_read(&ctx->vpp_cache_num) >= MAX_VPP_BUFFER_CACHE_NUM) {
		hevc_print(hevc, H266_DEBUG_DETAIL,
			"%s vpp cache: %d full!\n",
			__func__, atomic_read(&ctx->vpp_cache_num));
		return false;
	}

	if (!hevc->aml_buf && !aml_buf_empty(&ctx->bm)) {
		hevc->aml_buf = aml_buf_get(&ctx->bm, BUF_USER_DEC, false);
		if (!hevc->aml_buf) {
			return false;
		}
		hevc->aml_buf->task->attach(hevc->aml_buf->task, &task_dec_ops, hw_to_vdec(hevc));
		hevc->aml_buf->state = FB_ST_DECODER;
	}

	if (hevc->aml_buf) {
		free_count++;
		free_count += aml_buf_ready_num(&ctx->bm);
		hevc_print(hevc, H266_DEBUG_BUFMGR, "%s get fb: 0x%lx fb idx: %d\n",
			__func__, hevc->aml_buf, hevc->aml_buf->index);
	}

	return free_count >= run_ready_min_buf_num ? 1 : 0;
}

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct hevc_state_s *hw = (struct hevc_state_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct aml_buf *aml_buf = NULL;
	static struct PIC_s *pic = NULL;
	ulong expires;

	expires = jiffies + msecs_to_jiffies(2000);
	while (!is_available_buffer(hw)) {
		if (time_after(jiffies, expires)) {
			pr_err("[%d] H266 isn't enough buff for notify eos.\n", ctx->id);
			return 0;
		}
		usleep_range(500, 1000);
	}

	hw->eos = true;

	pic = v4l_get_new_pic(hw, NULL);
	if (NULL == pic) {
		pr_err("[%d] H266 EOS get free buff fail.\n", ctx->id);
		return 0;
	}

	aml_buf = (struct aml_buf *)hw->m_BUF[pic->index].v4l_ref_buf_addr;

	vf->type		|= VIDTYPE_V4L_EOS;
	vf->timestamp		= ULONG_MAX;
	vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
	vf->v4l_mem_handle	= (ulong)aml_buf;

	vdec_vframe_ready(vdec, vf);
	aml_buf_set_vframe(aml_buf, vf);
	kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

	aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);

	pr_info("[%d] H266 EOS notify.\n", vdec->id);


	return 0;
}

static void process_nal_sei(struct hevc_state_s *hevc,
	int payload_type, int payload_size)
{
	unsigned short data;

	if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI)
		hevc_print(hevc, 0,
			"\tsei message: payload_type = 0x%02x, payload_size = 0x%02x\n",
		payload_type, payload_size);

	if (payload_type == 137) {
		int i, j;
		/* MASTERING_DISPLAY_COLOUR_VOLUME */
		if (payload_size >= 24) {
			if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI)
				hevc_print(hevc, 0,
					"\tsei MASTERING_DISPLAY_COLOUR_VOLUME available\n");
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->primaries[i][j] = data;
					WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
					if (get_dbg_flag(hevc) &
						H266_DEBUG_PRINT_SEI)
						hevc_print(hevc, 0,
							"\t\tprimaries[%1d][%1d] = %04x\n",
						i, j, hevc->primaries[i][j]);
				}
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->white_point[i] = data;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\twhite_point[%1d] = %04x\n",
						i, hevc->white_point[i]);
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] = data << 16;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] |= data;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				if (get_dbg_flag(hevc) &
					H266_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\tluminance[%1d] = %08x\n",
						i, hevc->luminance[i]);
			}
			hevc->sei_hdr10_flag |= SEI_MASTER_DISPLAY_COLOR_MASK;
		}
		payload_size -= 24;
		while (payload_size > 0) {
			data = (READ_HREG(HEVC_SHIFTED_DATA) >> 24);
			payload_size--;
			WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|8);
			hevc_print(hevc, 0, "\t\tskip byte %02x\n", data);
		}
	}
}

static void dump_aux_buf(struct hevc_state_s *hevc)
{
	int i;
	unsigned short *aux_adr =
		(unsigned short *)hevc->aux_addr;
	unsigned int aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE) >> 16) << 4;

	if (hevc->prefix_aux_size > 0) {
		hevc_print(hevc, 0,
			"prefix aux: (size %d)\n", aux_size);
		if (aux_size > hevc->prefix_aux_size) {
			hevc_print(hevc, 0,
				"%s:aux_size(%d) is over size\n", __func__, aux_size);
			return ;
		}
		for (i = 0; i < (aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0, "%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}
	}
	if (hevc->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr + hevc->prefix_aux_size);
		aux_size = (READ_VREG(HEVC_AUX_DATA_SIZE) & 0xffff) << 4;
		hevc_print(hevc, 0, "suffix aux: (size %d)\n", aux_size);
		if (aux_size > hevc->suffix_aux_size) {
			hevc_print(hevc, 0,
				"%s:aux_size(%d) is over size\n", __func__, aux_size);
			return ;
		}
		for (i = 0; i <
		(aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0, "%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}
	}
}


static void read_decode_info(struct hevc_state_s *hevc)
{
#if 0
	uint32_t decode_info =
		READ_HREG(HEVC_DECODE_INFO);
	hevc->start_decoding_flag |=
		(decode_info & 0xff);
	hevc->rps_set_id = (decode_info >> 8) & 0xff;
#endif
}

#ifdef H266_USERDATA_ENABLE
static int userdata_prepare(struct hevc_state_s *hevc)
{
	struct PIC_s *pic = hevc->vvc_dec->cur_pic;
	char *p;
	u32 size;
	int type;
	u32 vpts = 0;
	u64 pts64 = 0;
	int pts_valid = 0;
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (!itu_t_t35_enable || pic == NULL)
		return 0;

	if (pic->aux_data_buf
	&& pic->aux_data_size) {
		/* parser sei */
		p = pic->aux_data_buf;
		while (p < pic->aux_data_buf
			+ pic->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				//hevc_print(hevc, 0, "sei(%d)\n", size);
				//parse_sei(hevc, pic, p, size);
			}
			p += size;
		}
		if (vdec_frame_based(hw_to_vdec(hevc))) {
			if (hevc->chunk) {
				vpts = hevc->chunk->pts;
				pts_valid = hevc->chunk->pts_valid;
			}
		} else {
			checkout_pts_offset pts_info;
			if (vdec->pts_server_id == 0) {
				if (pts_pickout_offset_us64(PTS_TYPE_VIDEO,
					pic->stream_offset, &vpts, 0, &pts64)) {
					vpts = 0;
					pts_valid = 0;
				} else {
					pts_valid = 1;
				}
			} else {
				pts_info.offset = (((u64)hevc->frame_dur << 32) & 0xffffffff00000000) | pic->stream_offset;
				if (!ptsserver_peek_pts_offset((vdec->pts_server_id & 0xff), &pts_info)) {
					vpts = pts_info.pts;
					pts_valid = 1;
				}
			}
		}
		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"%s: id = %x, offset: %x, vpts: %d, pts_valid: %d\n",
			__func__, vdec->pts_server_id, pic->stream_offset, vpts, pts_valid);
		vh266_userdata_fill_vpts(hevc, vpts, pts_valid, pic->poc);
	}

	return 0;
}
#endif

#if 0
static int is_interlace(struct hevc_state_s *hevc)
{

	int pic_struct = (hevc->vvc_dec->param.p.sei_frame_field_info >> 3) & 0xf;
	int frame_field_info_present_flag =
			(hevc->vvc_dec->param.p.sei_frame_field_info >> 8) & 0x1;

	if ((hevc->vvc_dec->param.p.profile_etc &  0x4)
		&& (frame_field_info_present_flag
		&& (pic_struct == 0
		|| pic_struct == 7
		|| pic_struct == 8)))
		return 0;
	else if (hevc->vvc_dec->param.p.profile_etc & 0x4)
		return 1;

	return 0;
}
#endif

static void hevc_interlace_check(struct hevc_state_s *hevc,
	union param_u *rpm_param)
{
#if 0
	int w, h;

	w = hevc->vvc_dec->param.p.pic_width_in_luma_samples;
	h = hevc->vvc_dec->param.p.pic_height_in_luma_samples;
	/* interlace check, 4k force no interlace */
	if ((interlace_enable != 0) &&
		(!IS_4K_SIZE(w, h * 2)) &&
		(is_interlace(hevc))) {
		hevc->interlace_flag = 1;
		hevc->frame_ar = (hevc->pic_h * 0x100 / hevc->pic_w) * 2;
		hevc_print(hevc, 0,
			"interlace (%d, %d), profile_etc %x, ar 0x%x, dw %d\n",
			hevc->pic_w, hevc->pic_h, hevc->param.p.profile_etc, hevc->frame_ar,
			get_double_write_mode(hevc));
	}
#endif
}

static int v4l_parser_work_pic_num(struct hevc_state_s *hevc)
{
	int used_buf_num = 0;
#if 0
	pr_debug("margin = %d, sps_max_dec_pic_buffering_minus1_0 = %d\n",
		get_dynamic_buf_num_margin(hevc),
		hevc->param.p.sps_max_dec_pic_buffering_minus1_0);

	used_buf_num = hevc->param.p.sps_max_dec_pic_buffering_minus1_0 + 1;
	/*
	1. need one more for multi instance, as apply_ref_pic_set()
	   has no chanch to run to clear referenced flag in some case
	2. for eos add more buffer to flush.
	*/
	used_buf_num += 1;
	if (!save_buffer)
		used_buf_num += 1;

	if (hevc->save_buffer_mode)
		hevc_print(hevc, 0,
			"save buf _mode : dynamic_buf_num_margin %d ----> %d \n",
			dynamic_buf_num_margin,  hevc->dynamic_buf_num_margin);
#endif
	used_buf_num = PIC_POOL_SIZE;
	if (used_buf_num > max_buf_num)
		used_buf_num = max_buf_num;
	return used_buf_num;
}

static int vh266_get_ps_info(struct hevc_state_s *hevc,
			     union param_u *rpm_param,
			     struct aml_vdec_ps_infos *ps)
{
	//u32 SubWidthC, SubHeightC;
	u32 width = rpm_param->p.pic_width_in_luma_samples;
	u32 height = rpm_param->p.pic_height_in_luma_samples;
	u32 coded_width = width;
	u32 coded_height = height;

#if 0
	switch (rpm_param->p.chroma_format_idc) {
	case 1:
		SubWidthC = 2;
		SubHeightC = 2;
		break;
	case 2:
		SubWidthC = 2;
		SubHeightC = 1;
		break;
	default:
		SubWidthC = 1;
		SubHeightC = 1;
		break;
	}

	width -= SubWidthC *
		(rpm_param->p.conf_win_left_offset +
		rpm_param->p.conf_win_right_offset);
	height -= SubHeightC *
		(rpm_param->p.conf_win_top_offset +
		rpm_param->p.conf_win_bottom_offset);
#endif

	hevc->last_width = rpm_param->p.pic_width_in_luma_samples;
	hevc->last_height = rpm_param->p.pic_height_in_luma_samples;

	height <<= hevc->interlace_flag;
	coded_height <<= hevc->interlace_flag;
	ps->visible_width 	= width;
	ps->visible_height 	= height;
	ps->coded_width 	= ALIGN(coded_width, is_hevc_align32(0) ? 32 : 64);
	ps->coded_height 	= ALIGN(coded_height, 64);
	ps->field 		= hevc->interlace_flag ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
	ps->dpb_frames		= v4l_parser_work_pic_num(hevc);
	ps->dpb_margin		= get_dynamic_buf_num_margin(hevc);
	ps->bitdepth		= (rpm_param->p.sps_bitdepth_minus8 & 0xf) + 8;

#if 0
	if (!ctx->is_multiplanar &&
		hevc->interlace_flag && (ps->bitdepth == 8)) {
		struct aml_vdec_cfg_infos cfg_info = { 0 };
		if (vh266_clear_mmu_config(hevc)) {
			hevc_print(hevc, 0,
				"vh266 mmu clear ERROR! \n");
			return -1;
		}
		hevc->double_write_mode = DM_YUV_ONLY;
		hevc_print(hevc, H266_DEBUG_DETAIL, "h266 8bit interlace, mmu force disable\n");
		vdec_v4l_get_cfg_infos(ctx, &cfg_info);
		cfg_info.double_write_mode = DM_YUV_ONLY;
		vdec_v4l_set_cfg_infos(ctx, &cfg_info);
	}
#endif

	hevc_print(hevc, H266_DEBUG_DETAIL,
		"%s mmu_enable %d double_write_mode 0x%x\n",
		__func__, hevc->mmu_enable, hevc->double_write_mode);

	return 0;
}

static void get_comp_buf_info(struct hevc_state_s *hevc,
		struct vdec_comp_buf_info *info)
{
	u16 bit_depth = hevc->vvc_dec->param.p.sps_bitdepth_minus8;
	int w = hevc->vvc_dec->param.p.pic_width_in_luma_samples;
	int h = hevc->vvc_dec->param.p.pic_height_in_luma_samples;
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

	info->max_size = hevc_max_mmu_buf_size(
			hevc->max_pic_w,
			hevc->max_pic_h);
	info->header_size = hevc_get_header_size(w,h);
	info->frame_buffer_size = hevc_mmu_page_num(
			hevc, w, h,	bit_depth != 0x00);
	if (info->frame_buffer_size < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
	}

	pr_info("hevc get comp info: %d %d %d\n",
			info->max_size, info->header_size,
			info->frame_buffer_size);
}

static int v4l_res_change(struct hevc_state_s *hevc, union param_u *rpm_param)
{
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	int ret = 0;

	if (ctx->param_sets_from_ucode) {
		struct aml_vdec_ps_infos ps;
		int width = rpm_param->p.pic_width_in_luma_samples;
		int height = rpm_param->p.pic_height_in_luma_samples;

		if ((hevc->last_width != 0 &&
			hevc->last_height != 0) &&
			(hevc->last_width != width ||
			hevc->last_height != height)) {
			int new_size;
			hevc_print(hevc, 0,
				"v4l_res_change Pic Width/Height Change (%d,%d)=>(%d,%d), interlace %d\n",
				hevc->last_width, hevc->last_height,
				width,
				height,
				hevc->interlace_flag);

			if (IS_8K_SIZE(hevc->pic_w, hevc->pic_h))
				new_size = MPRED_8K_MV_BUF_SIZE;
			else if (IS_4K_SIZE(hevc->pic_w, hevc->pic_h))
				new_size = MPRED_4K_MV_BUF_SIZE; /*0x120000*/
			else
				new_size = MPRED_MV_BUF_SIZE;

			if (new_size != hevc->mv_buf_size) {
				dealloc_mv_bufs(hevc);
				hevc->mv_buf_size = new_size;
			}

			if (get_valid_double_write_mode(hevc) != 16) {
				struct vdec_comp_buf_info info;

				get_comp_buf_info(hevc, &info);
				vdec_v4l_set_comp_buf_info(ctx, &info);
			}
			vh266_get_ps_info(hevc, &hevc->vvc_dec->param, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hevc->v4l_params_parsed = false;
			ctx->v4l_resolution_change = 1;
			hevc->resolution_change = true;
			flush_output(hevc);
			notify_v4l_eos(hw_to_vdec(hevc));
			ret = 1;
		}
	}

	return ret;
}

static void vh266_buf_ref_process_for_exception(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

	if (hevc->cur_idx != INVALID_IDX) {
		struct PIC_s *pic = &hevc->vvc_dec->pic_pool[hevc->cur_idx];
		struct aml_buf *aml_buf =
		(struct aml_buf *)hevc->m_BUF[pic->index].v4l_ref_buf_addr;

		hevc_print(hevc, H266_DEBUG_BUFMGR,
			"%s: dma addr(0x%lx)\n", __func__, pic->cma_alloc_addr);

		if (aml_buf == NULL)
			return;

		if (pic->pic_struct == 3 || pic->pic_struct == 4)
			aml_buf_put_ref(&ctx->bm, aml_buf);

		if (pic->pic_struct == 5 || pic->pic_struct == 6) {
			aml_buf_put_ref(&ctx->bm, aml_buf);
			aml_buf_put_ref(&ctx->bm, aml_buf);
		}

		aml_buf_put_ref(&ctx->bm, aml_buf);
		aml_buf_put_ref(&ctx->bm, aml_buf);
		pic->cma_alloc_addr = 0;
		hevc->m_BUF[pic->index].v4l_ref_buf_addr =0;
		pic->referenced = 0;
		pic->BUF_index = -1;
		pic->poc = INVALID_POC;
		hevc->cur_idx = INVALID_IDX;
	}
}

static irqreturn_t vh266_isr_thread_fn(int irq, void *data)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *) data;
	struct aml_vcodec_ctx *v4l2_ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	unsigned int dec_status = hevc->dec_status;
	int i, ret;
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (dec_status == VVC_HEAD_SLICE_INFO_READY) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_START);
	}
	else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_PIC_DONE_START);
	}

	if (hevc->eos)
		return IRQ_HANDLED;

	if (!hevc->m_ins_flag) {
		i = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if ((hevc->shift_byte_count_lo & (1 << 31))
			&& ((i & (1 << 31)) == 0))
			hevc->shift_byte_count_hi++;
		hevc->shift_byte_count_lo = i;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	/*
	mutex_lock(&hevc->chunks_mutex);
	if ((dec_status == HEVC_DECPIC_DATA_DONE)
		&& (hevc->chunk)) {
		hevc->vvc_dec->cur_pic->pts = hevc->chunk->pts;
		hevc->vvc_dec->cur_pic->pts64 = hevc->chunk->pts64;
		hevc->vvc_dec->cur_pic->timestamp = hevc->chunk->timestamp;
	}
	mutex_unlock(&hevc->chunks_mutex);
	*/
	if (dec_status == VVC_HEAD_SEQ_READY) {
		hevc_print(hevc, H266_DEBUG_BUFMGR, " ==== VVC_HEAD_SPS_READY ====\r\n");
		hevc->vvc_dec->seq_change_flag = 1;
		WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_ACTION_DONE);
	} else if (dec_status == VVC_HEAD_PIC_READY) {
		hevc_print(hevc, H266_DEBUG_BUFMGR, " ==== VVC_HEAD_PPS_READY ====\r\n");
		WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_ACTION_DONE);
	} else if (dec_status == VVC_HEAD_SEQ_END_READY) {
		hevc_print(hevc, H266_DEBUG_BUFMGR, " ==== VVC_HEAD_SEQ_END_READY ====\r\n");
		h266_bufmgr_code_process(&hevc->vvc_dec->m_decApp, SEQUENCE_END_CODE);
#if 0
		WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_ACTION_DONE);
#else
		hevc->dec_result = DEC_RESULT_DONE;
		hevc->process_state = PROCESS_STATE_INIT;
		amhevc_stop();
		vdec_schedule_work(&hevc->work);
		return IRQ_HANDLED;
#endif
	} else if (dec_status == VVC_STARTCODE_SEARCH_DONE) {
		hevc_print(hevc, H266_DEBUG_BUFMGR, " ==== VVC_STARTCODE_SEARCH_DONE ==== 0x%x\r\n", READ_VREG(CUR_NAL_UNIT_TYPE));
	} else if (dec_status == VVC_DECODE_BUFEMPTY) {
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				hevc->empty_flag = 1;
				/*suffix sei or dv meta*/
				set_aux_data(hevc, hevc->vvc_dec->cur_pic, 1, 0);
				goto pic_done;
			} else {
				vh266_buf_ref_process_for_exception(hevc);
				if (
					(data_resend_policy & 0x1)) {
					hevc->dec_result = DEC_RESULT_AGAIN;
					amhevc_stop();
					set_decode_again_state(hevc);
				} else
					hevc->dec_result = DEC_RESULT_GET_DATA;
			}
			vdec_schedule_work(&hevc->work);
		}
		return IRQ_HANDLED;
	} else if ((dec_status == HEVC_SEARCH_BUFEMPTY) ||
		(dec_status == HEVC_NAL_DECODE_DONE)) {
		if (hevc->m_ins_flag) {
			/*to review ... */
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				/* Ucode multiplexes HEVC_ASSIST_SCRATCH_4 to output dual layer flags.
				 * In the decoder driver, the bit0 of the register is read to
				 * determine whether the DV stream is a dual layer stream
				 */
				bool dv_duallayer = READ_VREG(HEVC_ASSIST_SCRATCH_4) & 0x1;
				if ((!hevc->discard_dv_data) && (!hevc->dv_duallayer)
					&& (dv_duallayer)) {
					hevc->dv_duallayer = true;
					hevc_print(hevc, 0, "dv dual layer\n");
				}
				hevc->empty_flag = 1;
				/*suffix sei or dv meta*/
				set_aux_data(hevc, hevc->vvc_dec->cur_pic, 1, 0);
				if (frmbase_muti_slice == 1)
					goto muti_output;
				else
					goto pic_done;
			} else {
				vh266_buf_ref_process_for_exception(hevc);
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				set_decode_again_state(hevc);
			}

			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
	} else if (dec_status == HEVC_DECODE_PARAMS_ERR) {
		hevc_print(hevc, 0, "hevc decode params err !!\n");
		if (hevc->m_ins_flag) {
			hevc->dec_result = DEC_RESULT_ERROR_DATA;
			amhevc_stop();
			vdec_schedule_work(&hevc->work);
		}
		return IRQ_HANDLED;
	} else if (dec_status == VVC_DECODE_TIMEOUT) {
		hevc->timeout_flag = 1;
#ifdef USE_OLD_CHIP
		goto pic_done;
#else
		vh266_report_err_timestamp_for_decoded_frames(v4l2_ctx,
			hevc->chunk ? hevc->chunk->timestamp : -1);
		hevc->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hevc->work);
		return IRQ_HANDLED;
#endif

	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		if (hevc->m_ins_flag) {
			int ii;
			if (vdec->mvfrm)
				vdec->mvfrm->hw_decode_time =
				local_clock() - vdec->mvfrm->hw_decode_start;
			hevc->empty_flag = 0;
			vdec_profile(hw_to_vdec(hevc), VDEC_PROFILE_DECODED_FRAME, CORE_MASK_HEVC);
pic_done:
			hevc->process_state = PROCESS_STATE_INIT;
			if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
					"==> dec idx %d, %d %s, poc %d, struct %d interlace %d pic idx %d HEVC_SAO_CRC 0x%x HEVC_SAO_CRC_3 0x%x\n",
					hevc->decode_idx,
					dec_status,
					dec_status ==  HEVC_DECPIC_DATA_DONE ? "HEVC_DECPIC_DATA_DONE":
						(dec_status == VVC_DECODE_BUFEMPTY ? "VVC_DECODE_BUFEMPTY":
						(dec_status == VVC_DECODE_TIMEOUT ? "VVC_DECODE_TIMEOUT" : "")),
					hevc->vvc_dec->cur_pic? hevc->vvc_dec->cur_pic->poc: INVALID_POC,
					hevc->curr_pic_struct,
					hevc->interlace_flag,
					hevc->vvc_dec->cur_pic? hevc->vvc_dec->cur_pic->index : -1,
					READ_VREG(HEVC_SAO_CRC),
					READ_VREG(HEVC_SAO_CRC_3));
			}
            hevc->decode_idx += 1;

			if (hevc->empty_flag == 0) {
				hevc->over_decode = (READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
				if (hevc->over_decode)
					hevc_print(hevc, 0, "!!!Over decode %d\n", __LINE__);
			}
			if (input_frame_based(hw_to_vdec(hevc)) &&
				frmbase_cont_bitlevel != 0 &&
				(hevc->decode_size > READ_VREG(HEVC_SHIFT_BYTE_COUNT)) &&
				(hevc->decode_size - (READ_VREG(HEVC_SHIFT_BYTE_COUNT))
				 > frmbase_cont_bitlevel)) {
				check_pic_decoded_error(hevc, READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff);
				/*handle the case: multi pictures in one packet*/
				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
					"%s  has more data index= %d, size=0x%x shiftcnt=0x%x)\n",
					__func__,
					hevc->decode_idx, hevc->decode_size,
					READ_VREG(HEVC_SHIFT_BYTE_COUNT));
				WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
				start_process_time(hevc);
				return IRQ_HANDLED;
			}

			read_decode_info(hevc);
			get_picture_qos_info(hevc);
			//hevc->decoded_poc = hevc->curr_POC;

#ifdef H266_USERDATA_ENABLE
			userdata_prepare(hevc);
#endif

			amhevc_stop();
#ifndef USE_OLD_CHIP
			save_register_context(hevc);
#endif
			if (hevc->vvc_dec->cur_pic) {
				hevc->vvc_dec->cur_pic->decode_done = 1;
			}
			if (dec_status == HEVC_DECPIC_DATA_DONE)
				h266_bufmgr_post_process(&hevc->vvc_dec->m_decApp);
			else
				vh266_report_err_timestamp_for_decoded_frames(v4l2_ctx,
					hevc->chunk ? hevc->chunk->timestamp : -1);
#ifdef VVC_10B_MMU
			for (ii = 0; ii < PIC_POOL_SIZE; ii++) {
				vvc_frame_t *pic = &hevc->vvc_dec->pic_pool[ii];
				if (pic->used == 1 && pic->referenced == 0) {
					pic_buf_cfg_free(pic);
				}
			}
#endif
			hevc->vvc_dec->cur_pic = NULL;
			if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR_MORE)
				print_pic_pool(hevc, "after bufmgr_post_process");

muti_output:
			if (vdec_frame_based(hw_to_vdec(hevc)) &&
				(READ_VREG(HEVC_SHIFT_BYTE_COUNT) + 4 < hevc->data_size)
				 && (frmbase_muti_slice == 1)) {
				hevc->consume_byte = READ_VREG(HEVC_SHIFT_BYTE_COUNT) - 8;
				hevc->dec_result = DEC_RESULT_UNFINISH;
			} else {
				hevc->data_size = 0;
				hevc->data_offset = 0;
				hevc->dec_result = DEC_RESULT_DONE;
			}

			ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
	} else if (dec_status == HEVC_OVER_DECODE) {
		hevc->process_state = PROCESS_STATE_INIT;
        hevc->over_decode = 1;
		hevc->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hevc->work);
		return IRQ_HANDLED;
	}

#endif

	if (dec_status == HEVC_SEI_DAT) {
		if (!hevc->m_ins_flag) {
			int payload_type =
				READ_HREG(CUR_NAL_UNIT_TYPE) & 0xffff;
			int payload_size =
				(READ_HREG(CUR_NAL_UNIT_TYPE) >> 16) & 0xffff;
				process_nal_sei(hevc, payload_type, payload_size);
		}
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_SEI_DAT_DONE);
	} else if (dec_status == VVC_HEAD_SLICE_INFO_READY) {
		struct vvc_decoder *vvc_dec = hevc->vvc_dec;
		DecLib *p_declib = &vvc_dec->m_decApp.m_cDecLib;
		BuffInfo_t* buf_spec = hevc->work_space_buf;
		union param_u *param = &hevc->vvc_dec->param;
		uint32_t data32;
		Slice* slice = NULL;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
		}
#endif
		if (hevc->start_decoding_time > 0) {
		u32 process_time = 1000 * (jiffies - hevc->start_decoding_time)/HZ;
			if (process_time > max_decoding_time)
				max_decoding_time = process_time;
		}

		if (hevc->process_state == PROCESS_STATE_DECODE_AGAIN)
			goto dec_cont;
		else
			hevc->process_state = PROCESS_STATE_DECODING;

		if (get_dbg_flag(hevc) & H266_DEBUG_SEND_PARAM_WITH_REG)
			get_rpm_param(&hevc->vvc_dec->param);
		else {
			ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_START);
			for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
				int ii;

				for (ii = 0; ii < 4; ii++) {
					hevc->vvc_dec->param.l.data[i + ii] = hevc->rpm_ptr[i + 3 - ii];
				}
			}
			ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_END);
#ifdef SEND_LMEM_WITH_RPM
			check_head_error(hevc);
#endif
		}
		//get_ref_set(&vvc_dec->m_decApp, hevc->vvc_dec->ref_list_buf_v);
		get_ref_set_fast(&vvc_dec->m_decApp, hevc->vvc_dec->ref_list_buf_v,
			param->p.sps_seq_parameter_set_id, param->p.RPLidx & 0xff, (param->p.RPLidx >> 8) & 0xff);
#if 1 //def FOR_STANDALONE_BUFMGR_DEBUG
		if (get_dbg_flag(hevc) & H266_DEBUG_DUMP_REF_LIST_BUF) {
			hevc_print(hevc, H266_DEBUG_DUMP_REF_LIST_BUF, "ref_list_buf_v begin:\n");
			for (i = 0; i < REF_LIST_BUF_SIZE; i++) {
				hevc_print_cont(hevc, 0, "%02x ", hevc->vvc_dec->ref_list_buf_v[i]);
				if (((i + 1) & 0xf) == 0)
					hevc_print_cont(hevc, 0, "\n");
			}
			hevc_print(hevc, H266_DEBUG_DUMP_REF_LIST_BUF, "ref_list_buf_v end\n");
		}
#endif

		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR_MORE) {
			hevc_print(hevc, 0, "rpm_param: (%d)\n", hevc->slice_idx);
			hevc->slice_idx++;
			for (i = 0; i < (RPM_END - RPM_BEGIN); i++) {
				hevc_print_cont(hevc, 0, "%04x ", hevc->vvc_dec->param.l.data[i]);
				if (((i + 1) & 0xf) == 0)
					hevc_print_cont(hevc, 0, "\n");
			}
		}
		if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR_MORE)
			print_param(&vvc_dec->param);

		if (aux_data_is_available(hevc)) {
			if (get_dbg_flag(hevc) & H266_DEBUG_PRINT_SEI)
				dump_aux_buf(hevc);
		}

		if (is_oversize(hevc->vvc_dec->param.p.pic_width_in_luma_samples,
			hevc->vvc_dec->param.p.pic_height_in_luma_samples)) {
			hevc_print(hevc, 0,"is_oversize w:%d h:%d\n",
				hevc->vvc_dec->param.p.pic_width_in_luma_samples,
				hevc->vvc_dec->param.p.pic_height_in_luma_samples);
			hevc->dec_result = DEC_RESULT_ERROR_DATA;
			if (vdec_frame_based(hw_to_vdec(hevc)))
				vh266_report_err_timestamp_for_decoded_frames(v4l2_ctx,
					hevc->chunk ? hevc->chunk->timestamp : -1);
			amhevc_stop();
			vdec_schedule_work(&hevc->work);
			return IRQ_HANDLED;
		}

		if (!v4l_res_change(hevc, &hevc->vvc_dec->param)) {
			if (v4l2_ctx->param_sets_from_ucode && !hevc->v4l_params_parsed) {
				struct aml_vdec_ps_infos ps;

				hevc->pic_w = hevc->vvc_dec->param.p.pic_width_in_luma_samples;
				hevc->pic_h = hevc->vvc_dec->param.p.pic_height_in_luma_samples;
				hevc->lcu_size_log2 = hevc->vvc_dec->param.p.lcu_size;
				hevc->lcu_size = 1 << hevc->lcu_size_log2;

				pr_debug("set ucode parse\n");
				hevc_interlace_check(hevc, &hevc->vvc_dec->param);
				if (get_valid_double_write_mode(hevc) != 16) {
					struct vdec_comp_buf_info info;

					get_comp_buf_info(hevc, &info);
					vdec_v4l_set_comp_buf_info(v4l2_ctx, &info);
				}
				vh266_get_ps_info(hevc, &hevc->vvc_dec->param, &ps);
				/*notice the v4l2 codec.*/
				vdec_v4l_set_ps_infos(v4l2_ctx, &ps);
				v4l2_ctx->decoder_status_info.frame_height = ps.visible_height;
				v4l2_ctx->decoder_status_info.frame_width = ps.visible_width;
				//v4l_hevc_collect_stream_info(vdec, hevc);
				//ctx->dec_intf.decinfo_event_report(ctx, AML_DECINFO_EVENT_STATISTIC, NULL);
				hevc->v4l_params_parsed = true;
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				//restore_decode_state(hevc);
				vdec_schedule_work(&hevc->work);
				//ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
				return IRQ_HANDLED;
			} else {
				struct vdec_pic_info pic;

				vdec_v4l_get_pic_info(v4l2_ctx, &pic);
				hevc->used_buf_num = pic.dpb_frames +
					pic.dpb_margin;
				if (hevc->used_buf_num > MAX_BUF_NUM)
					hevc->used_buf_num = MAX_BUF_NUM;
			}
		} else {
			hevc->dec_result = DEC_RESULT_AGAIN;
			amhevc_stop();
			//restore_decode_state(hevc);
			vdec_schedule_work(&hevc->work);
			return IRQ_HANDLED;
		}
#if 1
		vvc_dec->img.width = param->p.pic_width_in_luma_samples;
		vvc_dec->img.height = param->p.pic_height_in_luma_samples;
		vvc_dec->lcu_size_log2 = param->p.lcu_size;

		hevc->pic_w = hevc->vvc_dec->param.p.pic_width_in_luma_samples;
		hevc->pic_h = hevc->vvc_dec->param.p.pic_height_in_luma_samples;
		if (hevc->frame_width == 0 || hevc->frame_height == 0) {
			hevc->frame_width = hevc->pic_w;
			hevc->frame_height = hevc->pic_h;
		}
		hevc->lcu_size_log2 = hevc->vvc_dec->param.p.lcu_size;
		hevc->lcu_size = 1 << hevc->lcu_size_log2;
		hevc->crop_w = hevc->pic_w;
		hevc->crop_h = hevc->pic_h;
		hevc->bit_depth_luma = hevc->vvc_dec->param.p.sps_bitdepth_minus8 + 8;
		hevc->bit_depth_chroma = hevc->bit_depth_luma;
#endif
		if (vvc_dec->init_hw_flag == 0) {
			init_pic_list(hevc); //init_pic_list_hw(vvc_dec, buf_spec, mc_buf_spec);
			init_pic_list_hw(hevc);
			vvc_dec->init_hw_flag = 1;
		}

		//new picture: vvc_dec->cur_pic == NULL (don't use param->p.sliceAddr == 0)
		ret = h266_bufmgr_process(&vvc_dec->m_decApp, param, vvc_dec->cur_pic == NULL);
		h266_recycle_frame_buffer(hevc);
		if (ret < 0) {
			if (hevc->m_ins_flag) {
				vh266_buf_ref_process_for_exception(hevc);
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				vdec_schedule_work(&hevc->work);
				return IRQ_HANDLED;
			}
		} else if (ret == 0) {
			if (p_declib->m_pcPic) {
				slice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx - 1];
				if (vvc_dec->cur_pic == NULL) {
					vvc_dec->cur_pic = v4l_get_new_pic(hevc, param);
					if (vvc_dec->cur_pic == NULL) {
						hevc_print(hevc, 0, "Error, v4l_get_new_pic err");
						//WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_SKIP_DECODING);
						hevc->dec_result = DEC_RESULT_DONE;
						amhevc_stop();
						vdec_schedule_work(&hevc->work);
						return IRQ_HANDLED;
					}
					hevc_print(hevc, H266_DEBUG_BUFMGR, "--------------- new pic --------------slice addr 0x%x\n", param->p.sliceAddr);
				} else
					vvc_dec->cur_pic->new_picture = 0;
dec_cont:
				config_mc_buffer(hevc);
				config_mcrcc_axi_hw(hevc);
				config_mpred_hw(hevc, buf_spec);
				config_scale_hw(hevc);
				config_lpf_hw(hevc, hevc->decode_idx);
				config_sao_hw(hevc);
				config_alf_hw(hevc);
				if (slice) {
					hevc_print(hevc, H266_DEBUG_REG_CFG, "Send m_symRefIdx to Parser : (%d, %d)\n", slice->m_symRefIdx[0], slice->m_symRefIdx[1]);
					data32 = READ_VREG(HEVC_PARSER_CORE_CONTROL);
					data32 = (data32 & (~(0x3ff << 5))) |
					((slice->m_symRefIdx[1]&0xf) << 11) |
					((slice->m_symRefIdx[0]&0xf) << 7) |
					(slice->m_bCheckLDC << 6) |
					(slice->m_biDirPred << 5);
#ifndef USE_OLD_CHIP
					WRITE_VREG(HEVC_PARSER_CORE_CONTROL, data32);
#endif
				}
			}

#ifdef USE_OLD_CHIP
			goto pic_done;
#endif
			if (is_skip_decoding(hevc, vvc_dec->cur_pic)
#if (defined BUFMGR_ONLY) || (defined USE_OLD_CHIP)
			|| (decode_timeout_val == 0)
#endif
			) {
				WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_SKIP_DECODING);
			} else {
				// HEVC_PARSER_HEADER_INFO :
				// bit[30]    --  vvc_bi_mid_ptr  // (ctx->ptr - ctx->refp[0][REFP_0].ptr == ctx->refp[0][REFP_1].ptr - ctx->ptr)
				// bit[21:16] --  ctx->dpm.num_refp[REFP_1]
				// bit[15:10] --  ctx->dpm.num_refp[REFP_0]
				//vvc_bi_mid_ptr = (ctx->ptr - ctx->refp[0][REFP_0].ptr == ctx->refp[0][REFP_1].ptr - ctx->ptr);
				//WRITE_VREG(HEVC_PARSER_HEADER_INFO, (ctx->dpm.num_refp[REFP_0]<<10) | (ctx->dpm.num_refp[REFP_1]<<16) | (vvc_bi_mid_ptr<<30));

				WRITE_VREG(NAL_SEARCH_CTL, 0);

				WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_DECODE_SLICE);
			}
			hevc->start_decoding_time = jiffies;
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag)
				start_process_time(hevc);
#endif
			if ((hevc->new_pic) && (hevc->vvc_dec->cur_pic != NULL)) {
				hevc->slice_count++;
			}
		} else {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s, bufmgr ret %d skip, DEC_RESULT_DONE\n",
			__func__, ret);

			vh266_report_err_timestamp_for_decoded_frames(v4l2_ctx,
				hevc->chunk ? hevc->chunk->timestamp : -1);

			hevc->dec_result = DEC_RESULT_DONE;
			if ((hevc->new_pic) && (hevc->vvc_dec->cur_pic != NULL)) {
				hevc->slice_count++;
				hevc->gvs->drop_frame_count++;
				if (hevc->vvc_dec->cur_pic->slice_type == I_SLICE) {
					hevc->gvs->i_lost_frames++;
				} else if (hevc->vvc_dec->cur_pic->slice_type == P_SLICE) {
					hevc->gvs->p_lost_frames++;
				} else if (hevc->vvc_dec->cur_pic->slice_type == B_SLICE) {
					hevc->gvs->b_lost_frames++;
				}
			}
			amhevc_stop();
			vdec_schedule_work(&hevc->work);
		}

		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
		vdec_profile(hw_to_vdec(hevc), VDEC_PROFILE_DECODER_START, CORE_MASK_HEVC);
	} else if (dec_status == HEVC_DECODE_OVER_SIZE) {
		hevc_print(hevc, 0 , "hevc  decode oversize !!\n");
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hevc->m_ins_flag)
			debug |= (H266_DEBUG_DIS_LOC_ERROR_PROC |
				H266_DEBUG_DIS_SYS_ERROR_PROC);
#endif
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		vh266_buf_ref_process_for_exception(hevc);

	}
	return IRQ_HANDLED;
}

static void wait_hevc_search_done(struct hevc_state_s *hevc)
{
	int count = 0;
	WRITE_VREG(HEVC_SHIFT_STATUS, 0);
	while (READ_VREG(HEVC_STREAM_CONTROL) & 0x2) {
		msleep(2);
		count++;
		if (count > 100) {
			hevc_print(hevc, 0, "%s timeout\n", __func__);
			break;
		}
	}
}
static irqreturn_t vh266_isr(int irq, void *data)
{
	int i, temp;
	unsigned int dec_status;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;
	u32 debug_tag;

	if (hevc->m_ins_flag)
		reset_process_time(hevc);

	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);

	if (dec_status == VVC_HEAD_SLICE_INFO_READY) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_HEAD_DONE);
	}
	else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_PIC_DONE);
		vdec_profile(hw_to_vdec(hevc), VDEC_PROFILE_DECODER_END, CORE_MASK_HEVC);
	}

	if (hevc->init_flag == 0) {
		start_process_time(hevc);
		return IRQ_HANDLED;
	}

	hevc->dec_status = dec_status;
	if (is_log_enable(hevc))
		add_log(hevc,
			"isr: status = 0x%x dec info 0x%x lcu 0x%x shiftbyte 0x%x shiftstatus 0x%x",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_MPRED_CURR_LCU),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"266 isr dec status = 0x%x dec info 0x%x shiftbyte 0x%x shiftstatus 0x%x\n",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	debug_tag = READ_HREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		hevc_print(hevc, 0,
			"LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));

		if (hevc->mmu_enable)
			temp = 0x500;
		else
			temp = 0x400;
		for (i = 0; i < temp; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				hevc_print_cont(hevc, 0, "%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				hevc_print_cont(hevc, 0, "%04x ", hevc->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}

		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		else if (debug_tag & 0x20000)
			hevc->ucode_pause_pos = 0xffffffff;
		if (!hevc->ucode_pause_pos) {
			start_process_time(hevc);
			WRITE_HREG(DEBUG_REG1, 0);
		}
	} else if (debug_tag != 0) {
		hevc_print(hevc, 0,
			"dbg%x: %x lcu %x stream crc %x shiftbyte %x l/w/r %x %x %x SAO_CRC %x\n", READ_HREG(DEBUG_REG1),
			READ_HREG(DEBUG_REG2),
			READ_VREG(HEVC_PARSER_LCU_START),
			READ_VREG(HEVC_STREAM_CRC),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_SAO_CRC));
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		if (!hevc->ucode_pause_pos) {
			start_process_time(hevc);
			WRITE_HREG(DEBUG_REG1, 0);
		}
		return IRQ_HANDLED;
	}


	if (!hevc->m_ins_flag) {
		if (dec_status == HEVC_OVER_DECODE) {
			hevc->over_decode = 1;
			hevc_print(hevc, 0,
				"isr: over decode\n"),
				WRITE_VREG(HEVC_DEC_STATUS_REG, 0);
			return IRQ_HANDLED;
		}
	}
	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_END);
	return IRQ_WAKE_THREAD;
}

static void vh266_set_clk(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, set_clk_work);

		int fps = 96000 / hevc->frame_dur;

		if (hevc_source_changed(VFORMAT_H266,
			hevc->frame_width, hevc->frame_height, fps) > 0)
			hevc->saved_resolution = hevc->frame_width *
			hevc->frame_height * fps;
}

static void vh266_check_timer_func(struct timer_list *timer)
{
	struct hevc_state_s *hevc = container_of(timer,
		 struct hevc_state_s, timer);
	unsigned char empty_flag;
	//unsigned int buf_level;

	if (hevc->init_flag == 0) {
		if (hevc->stat & STAT_TIMER_ARM) {
			mod_timer(&hevc->timer, jiffies + PUT_INTERVAL);
		}
		return;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag &&
		(get_dbg_flag(hevc) &
		H266_DEBUG_WAIT_DECODE_DONE_WHEN_STOP) == 0 &&
		hw_to_vdec(hevc)->next_status ==
		VDEC_STATUS_DISCONNECTED) {
		hevc->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hevc->work);
		hevc_print(hevc,
			0, "vdec requested to be disconnected\n");
		return;
	}
	if (hevc->m_ins_flag) {
		if ((decode_timeout_val > 0) &&
			(hevc->start_process_time > 0) &&
			((1000 * (jiffies - hevc->start_process_time) / HZ)
				> decode_timeout_val)) {
			u32 dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
			int current_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff;
			if (dec_status == VVC_SLICE_DECODING) {
				if (hevc->last_lcu_idx == current_lcu_idx) {
					if (hevc->decode_timeout_count > 0)
						hevc->decode_timeout_count--;
					if (hevc->decode_timeout_count == 0)
						timeout_process(hevc);
				} else
					restart_process_time(hevc);
				hevc->last_lcu_idx = current_lcu_idx;
			} else {
				if ((dec_status != VVC_HEAD_SLICE_INFO_READY) &&
					(dec_status != HEVC_DECPIC_DATA_DONE)) {
					hevc->pic_decoded_lcu_idx = current_lcu_idx;
					timeout_process(hevc);
				}
			}
		}
	} else {
#endif

	empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
	/* error watchdog */
#ifdef MULTI_INSTANCE_SUPPORT
	}
#endif
	if ((hevc->ucode_pause_pos != 0) &&
		(hevc->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hevc->ucode_pause_pos) {
		hevc->ucode_pause_pos = 0;
		WRITE_HREG(DEBUG_REG1, 0);
	}

	if (get_dbg_flag(hevc) & H266_DEBUG_DUMP_PIC_LIST) {
		print_pic_pool(hevc, "");
		debug &= ~H266_DEBUG_DUMP_PIC_LIST;
	}
	if (get_dbg_flag(hevc) & H266_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		debug &= ~H266_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}

	if (get_dbg_flag(hevc) & H266_DEBUG_HW_RESET) {
		hevc->error_skip_nal_count = error_skip_nal_count;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

		debug &= ~H266_DEBUG_HW_RESET;
	}

#ifdef ERROR_HANDLE_DEBUG
	if ((dbg_nal_skip_count > 0) && ((dbg_nal_skip_count & 0x10000) != 0)) {
		hevc->error_skip_nal_count = dbg_nal_skip_count & 0xffff;
		dbg_nal_skip_count &= ~0x10000;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
	}
#endif

	if (radr != 0) {
#ifdef SUPPORT_LONG_TERM_RPS
		if ((radr >> 24) != 0) {
			int count = radr >> 24;
			int adr = radr & 0xffffff;
			int i;
			for (i = 0; i < count; i++)
				pr_info("READ_VREG(%x)=%x\n", adr+i, READ_VREG(adr+i));
		} else
#endif
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			pr_info("WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			pr_info("READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}
#if 0
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			u32 disp_laddr;

			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB &&
				get_double_write_mode(hevc) == 0) {
				disp_laddr = READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;

				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0) & 0xff),
					&cur_canvas);
				disp_laddr = cur_canvas.addr;
			}
			hevc_print(hevc, 0,
				"current displayed buffer address %x\r\n",
				disp_laddr);
		}
		dbg_cmd = 0;
	}
#endif
	/*don't changed at start.*/
	if (hevc->m_ins_flag == 0 &&
		hevc->get_frame_dur && hevc->show_frame_num > 60 &&
		hevc->frame_dur > 0 && hevc->saved_resolution !=
			hevc->frame_width * hevc->frame_height * (96000 / hevc->frame_dur))
		vdec_schedule_work(&hevc->set_clk_work);

	mod_timer(timer, jiffies + PUT_INTERVAL);
}

void vh266_free_cmabuf(void)
{
	struct hevc_state_s *hevc = gHevc;

	mutex_lock(&vh266_mutex);

	if (hevc->init_flag) {
		mutex_unlock(&vh266_mutex);
		return;
	}

	mutex_unlock(&vh266_mutex);
}

#ifdef MULTI_INSTANCE_SUPPORT
int vh266_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
#else
int vh266_dec_status(struct vdec_info *vstatus)
#endif
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = gHevc;
#endif
	struct vdec_info_statistic_s *vstatistic = container_of(
		vstatus, struct vdec_info_statistic_s, vstatus);
	if (!hevc || !vstatistic) {
		pr_info("param invalid!\n");
		return -1;
	}

	vstatus->frame_width = hevc->crop_w;
	/* for hevc interlace for disp height x2 */
	vstatus->frame_height =
		(hevc->crop_h << hevc->interlace_flag);
	if (hevc->frame_dur != 0)
		vstatus->frame_rate = ((96000 * 10 / hevc->frame_dur) % 10) < 5 ?
				96000 / hevc->frame_dur : (96000 / hevc->frame_dur +1);
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = hevc->gvs->error_frame_count;
	vstatus->status = hevc->stat | hevc->fatal_error;
	if (!hevc_is_support_4k() &&
		(IS_4K_SIZE(vstatus->frame_width, vstatus->frame_height)) &&
		(vstatus->frame_width <= 4096 && vstatus->frame_height <= 2304)) {
		vstatus->status |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	}

	vstatus->bit_rate = hevc->gvs->bit_rate;
	vstatus->frame_dur = hevc->frame_dur;
	if (hevc->gvs) {
		vstatus->bit_rate = hevc->gvs->bit_rate;
		vstatus->frame_data = hevc->gvs->frame_data;
		vstatus->total_data = hevc->gvs->total_data;
		vstatus->frame_count = hevc->gvs->frame_count;
		vstatus->error_frame_count = hevc->gvs->error_frame_count;
		vstatus->drop_frame_count = hevc->gvs->drop_frame_count;
		vstatus->i_decoded_frames = hevc->gvs->i_decoded_frames;
		vstatus->i_lost_frames = hevc->gvs->i_lost_frames;
		vstatus->i_concealed_frames = hevc->gvs->i_concealed_frames;
		vstatus->p_decoded_frames = hevc->gvs->p_decoded_frames;
		vstatus->p_lost_frames = hevc->gvs->p_lost_frames;
		vstatus->p_concealed_frames = hevc->gvs->p_concealed_frames;
		vstatus->b_decoded_frames = hevc->gvs->b_decoded_frames;
		vstatus->b_lost_frames = hevc->gvs->b_lost_frames;
		vstatus->b_concealed_frames = hevc->gvs->b_concealed_frames;
		vstatus->samp_cnt = hevc->gvs->samp_cnt;
		vstatus->offset = hevc->gvs->offset;
	}

	vstatistic->aspect_ratio.sar_width = hevc->sar_width;
	vstatistic->aspect_ratio.sar_height = hevc->sar_height;
	vstatistic->aspect_ratio.dar_width = -1;
	vstatistic->aspect_ratio.dar_height = -1;
	vstatistic->ext_info_valid = 1;

	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name), "%s", DRIVER_NAME);
	vstatus->ratio_control = hevc->ratio_control;
	return 0;
}

int vh266_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static int vh266_vdec_info_init(struct hevc_state_s  *hevc)
{
	hevc->gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == hevc->gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	vdec_set_vframe_comm(hw_to_vdec(hevc), DRIVER_NAME);
	return 0;
}

int vh266_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	hevc_print(hevc, 0,	"[%s %d] trickmode:%lu\n", __func__, __LINE__, trickmode);

	if (trickmode == TRICKMODE_I) {
		trickmode_i = 1;
		i_only_flag = 0x1;
	} else if (trickmode == TRICKMODE_NONE) {
		trickmode_i = 0;
		i_only_flag = 0x0;
	} else if (trickmode == 0x02) {
		trickmode_i = 0;
		i_only_flag = 0x02;
	} else if (trickmode == 0x03) {
		trickmode_i = 1;
		i_only_flag = 0x03;
	} else if (trickmode == 0x07) {
		trickmode_i = 1;
		i_only_flag = 0x07;
	}

	return 0;
}

static void config_decode_mode(struct hevc_state_s *hevc)
{
	unsigned decode_mode;
	if (!hevc->m_ins_flag)
		decode_mode = DECODE_MODE_SINGLE;
	else if (vdec_frame_based(hw_to_vdec(hevc)))
		decode_mode =
			DECODE_MODE_MULTI_FRAMEBASE;
	else
		decode_mode =
			DECODE_MODE_MULTI_STREAMBASE;

	if (hevc->decode_idx > 0)
		decode_mode |= (3<< 16); //start_decoding_flag
	/* set MBX0 interrupt flag */
	decode_mode |= (0x80 << 24);
	WRITE_VREG(HEVC_DECODE_MODE, decode_mode);
	WRITE_VREG(HEVC_DECODE_MODE2,
		hevc->rps_set_id);
}

int32_t vvc_hw_init(struct hevc_state_s *hevc)
{
    uint32_t data32;
    hevc_config_work_space_hw(hevc);

//Start JT
#if 1
    hevc_print(hevc, H266_DEBUG_REG_CFG, "Enable BitStream Fetch\n");
    data32 = READ_VREG(HEVC_STREAM_CONTROL);
    data32 = data32 |
             (1 << 0) // stream_fetch_enable
             ;
    WRITE_VREG(HEVC_STREAM_CONTROL, data32);

    data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
    //if (data32 != 0x00000100) { print_scratch_error(29); return; }
    /*data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
    if (data32 != 0x00000300) { print_scratch_error(30); return; }*/
    WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
    WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
    data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
    //if (data32 != 0x12345678) { print_scratch_error(31); return; }
    data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
    //if (data32 != 0x9abcdef0) { print_scratch_error(32); return; }
    WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
    WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300); // 0x000003 emulate code for VVC (same as H266)
#endif
// End JT

    hevc_init_decoder_hw(hevc);

// Set MCR fetch priorities
    data32 = 0x1 | (0x1 << 2) | (0x1 <<3) | (24 << 4) | (32 << 11) | (24 << 18) | (32 << 25);
    WRITE_VREG(HEVCD_MPP_DECOMP_AXIURG_CTL, data32);

#if 1 // JT

#if 0 //def SIMULATION
    if (decode_pic_begin == 0)
        WRITE_VREG(HEVC_WAIT_FLAG, 1);
    else
        WRITE_VREG(HEVC_WAIT_FLAG, 0);
#else
    WRITE_VREG(HEVC_WAIT_FLAG, 1);
#endif

    /* disable PSCALE for hardware sharing */
#ifdef DOS_PROJECT
#else
    WRITE_VREG(HEVC_PSCALE_CTRL, 0);
#endif

    WRITE_VREG(DEBUG_REG1, 0x0);  //no debug
    WRITE_VREG(NAL_SEARCH_CTL, 0x8); //check SEQUENCE/I_PICTURE_START in ucode
    WRITE_VREG(DECODE_STOP_POS, udebug_flag);
#ifdef MULTI_INSTANCE_SUPPORT
    WRITE_VREG(DECODE_MODE, 1); //DECODE_MODE_MULTI_STREAMBASE
    WRITE_VREG(HEVC_DECODE_SIZE, 0xffffffff);
#endif

    //WRITE_VREG(XIF_DOS_SCRATCH31, 0x0);
    //WRITE_VREG(HEVC_MPSR, 1);
    hevc_print(hevc, H266_DEBUG_REG_CFG, "P_HEVC_MPSR!\n");
#endif

   return 0;
}

static void vh266_prot_init(struct hevc_state_s *hevc)
{
#ifndef USE_OLD_CHIP
	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);

	if (hevc->decode_idx > 0)
		restore_register_context(hevc);

	vvc_hw_init(hevc);
    config_decode_mode(hevc);
    hevc->vvc_dec->ref_list_buf_v = hevc->ref_list_buffer_addr;
#else
    hevc->vvc_dec->ref_list_buf_v = hevc->ref_list_buffer_addr;
	/* H266_DECODE_INIT(); */

	hevc_config_work_space_hw(hevc);

	hevc_init_decoder_hw(hevc);

	//WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* WRITE_VREG(HEVC_MPSR, 1); */

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	WRITE_VREG(DEBUG_REG1, 0x0 | (dump_nal << 8));

#if 0
	if ((get_dbg_flag(hevc) &
		(H266_DEBUG_MAN_SKIP_NAL |
		H266_DEBUG_MAN_SEARCH_NAL))) {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);	/* manual parser NAL */
	} else
#endif
	{
		/* check vps/sps/pps/i-slice in ucode */
		unsigned ctl_val = 0x8;
		if (hevc->PB_skip_mode == 0)
			ctl_val = 0x4;	/* check vps/sps/pps only in ucode */
		else if (hevc->PB_skip_mode == 3)
			ctl_val = 0x0;	/* check vps/sps/pps/idr in ucode */
		WRITE_VREG(NAL_SEARCH_CTL, ctl_val);
	}
	if ((get_dbg_flag(hevc) & H266_DEBUG_NO_EOS_SEARCH_DONE)
		)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));

	WRITE_VREG(DECODE_STOP_POS, udebug_flag);
#endif

	config_decode_mode(hevc);
	config_aux_buf(hevc);
#ifdef SWAP_HEVC_UCODE
	if (!fw_tee_enabled() && hevc->is_swap) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
		/*pr_info("write swap buffer %x\n", (u32)(hevc->mc_dma_handle));*/
	}
#endif
}

static int vh266_local_init(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;
	struct vdec_s *vdec = hw_to_vdec(hevc);

#ifdef DEBUG_PTS
	hevc->pts_missed = 0;
	hevc->pts_hit = 0;
#endif
	hevc->saved_resolution = 0;
	hevc->get_frame_dur = false;
	hevc->frame_width = hevc->vh266_amstream_dec_info.width;
	hevc->frame_height = hevc->vh266_amstream_dec_info.height;
	hevc->dec_again_cnt = 0;

	if (hevc->max_pic_w && hevc->max_pic_h) {
		hevc->is_4k = !(hevc->max_pic_w && hevc->max_pic_h) ||
			((hevc->max_pic_w * hevc->max_pic_h) >
			1920 * 1088) ? true : false;
	} else {
		hevc->is_4k = !(hevc->frame_width && hevc->frame_height) ||
			((hevc->frame_width * hevc->frame_height) >
			1920 * 1088) ? true : false;
	}

	hevc->frame_dur =
		(hevc->vh266_amstream_dec_info.rate == 0) ?
		3600 : hevc->vh266_amstream_dec_info.rate;
	if (hevc->frame_width && hevc->frame_height)
		hevc->frame_ar = hevc->frame_height * 0x100 / hevc->frame_width;

	if (i_only_flag)
		hevc->i_only = i_only_flag & 0xff;
	else if ((unsigned long) hevc->vh266_amstream_dec_info.param & 0x08)
		hevc->i_only = 0x7;
	else
		hevc->i_only = 0x0;
	hevc->sei_hdr10_flag = 0;
	if (vdec->sys_info)
		pts_unstable = ((unsigned long)vdec->sys_info->param & 0x40) >> 6;
	hevc_print(hevc, 0,
		"h266:pts_unstable=%d\n", pts_unstable);
/*
 *TODO:FOR VERSION
 */
	hevc_print(hevc, 0,
		"h266: ver (%d,%d) decinfo: %dx%d rate=%d\n", h266_version,
		0, hevc->frame_width, hevc->frame_height, hevc->frame_dur);

	if (hevc->frame_dur == 0)
		hevc->frame_dur = 96000 / 24;

	INIT_KFIFO(hevc->display_q);
	INIT_KFIFO(hevc->newframe_q);
	INIT_KFIFO(hevc->pending_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hevc->vfpool[i];

		hevc->vfpool[i].index = -1;
		kfifo_put(&hevc->newframe_q, vf);
	}

	if (!hevc->resolution_change)
		ret = hevc_local_init(hevc);
	else
		ret = 0;

	return ret;
}
#ifdef MULTI_INSTANCE_SUPPORT
static s32 vh266_init(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
static s32 vh266_init(struct hevc_state_s *hevc)
{
#endif
	int ret = -1, size = -1;
	int fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;

	timer_setup(&hevc->timer, vh266_check_timer_func, 0);

	hevc->stat |= STAT_TIMER_INIT;

	if (hevc->m_ins_flag) {
		INIT_WORK(&hevc->work, vh266_work);
		INIT_WORK(&hevc->timeout_work, vh266_timeout_work);
	}

	if (vh266_local_init(hevc) < 0)
		return -EBUSY;

	mutex_init(&hevc->chunks_mutex);
	INIT_WORK(&hevc->notify_work, vh266_notify_work);
	INIT_WORK(&hevc->set_clk_work, vh266_set_clk);

	if ((get_decoder_firmware_version() <= UCODE_SWAP_VERSION) &&
		(get_decoder_firmware_submit_count() < UCODE_SWAP_SUBMIT_COUNT)) {
		hevc->enable_ucode_swap = false;
		if ((get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) && (!hevc->is_4k)) {
			hevc->enable_ucode_swap = true;
		}
	} else {
		/* no h266 swap ucode no */
		if (enable_swap)
			hevc->enable_ucode_swap = true;
		else
			hevc->enable_ucode_swap = false;
	}

	pr_debug("ucode version %d.%d, swap enable %d\n",
		get_decoder_firmware_version(), get_decoder_firmware_submit_count(),
		hevc->enable_ucode_swap);

	fw = fw_firmare_s_creat(fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	if (hevc->enable_ucode_swap) {
		size = get_firmware_data(VIDEO_DEC_HEVC_MMU_SWAP, fw->data);
		if (size < 0) {
			pr_info("hevc can not get swap fw code\n");
			size = get_firmware_data(VIDEO_DEC_H266_MMU, fw->data);
			hevc->enable_ucode_swap = false;
			hevc->is_swap = false;
		} else if (size)
			hevc->is_swap = true;	//local fw swap
		pr_info("get_firmware_data swap size %d swap %d\n", size, hevc->is_swap);
	} else {
		size = get_firmware_data(VIDEO_DEC_H266_MMU, fw->data);
		pr_info("get_firmware_data size %d\n", size);
	}

	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		return -1;
	}

	fw->len = size;

#ifdef SWAP_HEVC_UCODE
	if (!fw_tee_enabled() && hevc->is_swap) {
		hevc->swap_size = (4 * (4 * SZ_1K)); /*max 4 swap code, each 0x400*/
		hevc->mc_cpu_addr =
			decoder_dma_alloc_coherent(&hevc->mc_cpu_handle,
				hevc->swap_size,
				&hevc->mc_dma_handle, "H.266_MC_CPU_BUF");
		if (!hevc->mc_cpu_addr) {
			amhevc_disable();
			pr_info("vh266 mmu swap ucode loaded fail.\n");
			return -ENOMEM;
		}

		memcpy((u8 *) hevc->mc_cpu_addr, fw->data + SWAP_HEVC_OFFSET,
			hevc->swap_size);

		hevc_print(hevc, 0,
			"vh266 mmu ucode swap loaded %x\n", hevc->mc_dma_handle);
	}
#endif

#ifdef H266_USERDATA_ENABLE
			hevc->sei_itu_data_buf = kmalloc(SEI_ITU_DATA_SIZE, GFP_KERNEL);
			if (hevc->sei_itu_data_buf == NULL) {
				pr_err("%s: failed to alloc sei itu data buffer\n",
					__func__);
				return -1;
			} else if (NULL == hevc->sei_user_data_buffer) {
				hevc->sei_user_data_buffer = kmalloc(USER_DATA_SIZE, GFP_KERNEL);
				if (!hevc->sei_user_data_buffer) {
					pr_info("%s: Can not allocate sei_data_buffer\n", __func__);
					kfree(hevc->sei_itu_data_buf);
					hevc->sei_itu_data_buf = NULL;
				}
				hevc->sei_user_data_wp = 0;
			}
#endif

#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		hevc->timer.expires = jiffies + PUT_INTERVAL;

		hevc->fw = fw;
		hevc->init_flag = 1;

		return 0;
	}
#endif
	amhevc_enable();

	if (hevc->mmu_enable) {
		if (hevc->enable_ucode_swap) {
			//ret = amhevc_loadmc_ex(VFORMAT_H266, "hevc_mmu_swap", fw->data);
			if (ret < 0)
				ret = amhevc_loadmc_ex(VFORMAT_H266, "h266_mmu", fw->data);
			else
				hevc->is_swap = true;
		} else {
			ret = amhevc_loadmc_ex(VFORMAT_H266, "h266_mmu", fw->data);
		}
	} else {
		ret = amhevc_loadmc_ex(VFORMAT_H266, NULL, fw->data);
		hevc->is_swap = true;
	}
	if (ret < 0) {
		amhevc_disable();
		vfree(fw);
		pr_err("H266: the %s fw loading failed, err: %x\n",
			fw_tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}
	vfree(fw);

	hevc->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vh266_prot_init(hevc);

	if (vdec_request_threaded_irq(VDEC_IRQ_0, vh266_isr,
		vh266_isr_thread_fn,
		IRQF_ONESHOT,/*run thread on this irq disabled*/
		"vh266-irq", (void *)hevc)) {
		hevc_print(hevc, 0, "vh266 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	hevc->stat |= STAT_ISR_REG;
	hevc->provider_name = PROVIDER_NAME;

#ifdef MULTI_INSTANCE_SUPPORT
	vf_provider_init(&vh266_vf_prov, hevc->provider_name,
				&vh266_vf_provider, vdec);
	vf_reg_provider(&vh266_vf_prov);
	vf_notify_receiver(hevc->provider_name, VFRAME_EVENT_PROVIDER_START,
				NULL);
	if (hevc->frame_dur != 0) {
		if (!is_reset) {
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
			fr_hint_status = VDEC_HINTED;
		}
	} else
		fr_hint_status = VDEC_NEED_HINT;
#else
	vf_provider_init(&vh266_vf_prov, PROVIDER_NAME, &vh266_vf_provider,
					 hevc);
	vf_reg_provider(&vh266_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	if (hevc->frame_dur != 0) {
		vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)
				((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	} else
		fr_hint_status = VDEC_NEED_HINT;
#endif
	hevc->stat |= STAT_VF_HOOK;

	hevc->timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_ARM;

#ifdef SWAP_HEVC_UCODE
	if (!fw_tee_enabled() && hevc->is_swap) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
	}
#endif

#ifndef MULTI_INSTANCE_SUPPORT
	set_vdec_func(&vh266_dec_status);
#endif
	amhevc_start();

	WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);

	hevc->stat |= STAT_VDEC_RUN;
	hevc->init_flag = 1;
	error_handle_threshold = 30;

	return 0;
}

static int check_dirty_data(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)(vdec->private);
	struct vdec_input_s *input = &vdec->input;
	u32 wp, rp, level;
	u32 rp_set;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = hevc->pre_parser_wr_ptr;

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp;

	if (level > 0x100000) {
		u32 skip_size = ((level >> 1) >> 19) << 19;
		if (!vdec->input.swap_valid) {
			hevc_print(hevc , 0, "h266 start data discard level 0x%x, buffer level 0x%x, RP 0x%x, WP 0x%x\n",
				((level >> 1) >> 19) << 19, level, rp, wp);
			if (wp >= rp) {
				rp_set = rp + skip_size;
			}
			else if ((rp + skip_size) < (input->start + input->size)) {
				rp_set = rp + skip_size;
			} else {
				rp_set = rp + skip_size - input->size;
			}
			STBUF_WRITE(&vdec->vbuf, set_rp, rp_set);
			vdec->discard_start_data_flag = 1;
			vdec->input.stream_cookie += skip_size;
			hevc->dirty_shift_flag = 1;
		}
		return 1;
	}
	return 0;
}

static int check_data_size(struct vdec_s *vdec)
{
	struct hevc_state_s *hw =
		(struct hevc_state_s *)(vdec->private);
	u32 wp, rp, level;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = STBUF_READ(&vdec->vbuf, get_wp);

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp ;

	if (level > (vdec->input.size / 2))
		hw->dec_again_cnt++;

	if (hw->dec_again_cnt > dirty_again_threshold) {
		hevc_print(hw, 0, "h266 data skipped %x\n", level);
		hw->dec_again_cnt = 0;
		return 1;
	}
	return 0;
}

static int vh266_stop(struct hevc_state_s *hevc)
{
	if (get_dbg_flag(hevc) &
		H266_DEBUG_WAIT_DECODE_DONE_WHEN_STOP) {
		int wait_timeout_count = 0;

		while (READ_VREG(HEVC_DEC_STATUS_REG) ==
				VVC_SLICE_DECODING &&
				wait_timeout_count < 10) {
			wait_timeout_count++;
			msleep(20);
		}
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_ISR_REG) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hevc->m_ins_flag)
#endif
			WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
		vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	hevc->stat &= ~STAT_TIMER_INIT;
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED) {
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
		}
		fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vh266_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	cancel_work_sync(&hevc->notify_work);
	cancel_work_sync(&hevc->set_clk_work);
	uninit_mmu_buffers(hevc);
	amhevc_disable();

	if (hevc->gvs)
		kfree(hevc->gvs);
	hevc->gvs = NULL;

	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static void reset_process_time(struct hevc_state_s *hevc)
{
	if (hevc->start_process_time) {
		unsigned int process_time =
			1000 * (jiffies - hevc->start_process_time) / HZ;
		hevc->start_process_time = 0;
		if (process_time > max_process_time[hevc->index])
			max_process_time[hevc->index] = process_time;
	}
}

static void start_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
	hevc->last_lcu_idx = 0;
}

static void restart_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
}

static void timeout_process(struct hevc_state_s *hevc)
{
	/*
	 * In this very timeout point,the vh266_work arrives,
	 * or in some cases the system become slow,  then come
	 * this second timeout. In both cases we return.
	 */
	if (work_pending(&hevc->work) ||
	    work_busy(&hevc->work) ||
	    work_busy(&hevc->timeout_work) ||
	    work_pending(&hevc->timeout_work)) {
		pr_err("%s h266[%d] work pending, do nothing.\n",__func__, hevc->index);
		return;
	}

	hevc->timeout_num++;
	hevc_print(hevc, 0, "%s decoder timeout\n", __func__);
    WRITE_VREG(HEVC_DEC_STATUS_REG, VVC_DECODE_TIMEOUT);
    WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
        0x1);

}

#ifdef CONSTRAIN_MAX_BUF_NUM
static int get_vf_ref_only_buf_count(struct hevc_state_s *hevc)
{
	struct PIC_s *pic;
	int i;
	int count = 0;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic->index == -1)
			continue;
		if (pic->used && pic->referenced == 0
			&& pic->vf_ref > 0)
			count++;
	}

	return count;
}

static int get_used_buf_count(struct hevc_state_s *hevc)
{
	struct PIC_s *pic;
	int i;
	int count = 0;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic->index == -1)
			continue;
		if (pic->used)
			count++;
	}

	return count;
}
#endif

static unsigned char is_new_pic_available(struct hevc_state_s *hevc)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	/* recycle un-used pic */
	int i;
	unsigned long flags;
	/*return 1 if pic_list is not initialized yet*/
	if (hevc->vvc_dec->init_hw_flag == 0)
		return 1;
	spin_lock_irqsave(&h266_lock, flags);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic->index == -1)
			continue;
		if (pic->used == 0) {
            new_pic = pic;
		}
	}
	if (new_pic == NULL) {
		int decode_count = 0;

		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = &hevc->vvc_dec->pic_pool[i];
			if (pic->index == -1)
				continue;
			if (pic->decode_done)
				decode_count++;
		}
#if 0
		if (decode_count >=
				hevc->param.p.sps_max_dec_pic_buffering_minus1_0 + detect_stuck_buffer_margin) {
			if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR_MORE)
				print_pic_pool(hevc, "to flush dpb");
			if (!(error_handle_policy & 0x400)) {
				spin_unlock_irqrestore(&h266_lock, flags);
				flush_output(hevc);
				hevc_print(hevc, H266_DEBUG_BUFMGR, "flush dpb, ref_error_count %d, sps_max_dec_pic_buffering_minus1_0 %d\n",
						decode_count, hevc->param.p.sps_max_dec_pic_buffering_minus1_0);
				return 1;
			}
		}
#endif
	}
	spin_unlock_irqrestore(&h266_lock, flags);

	return (new_pic != NULL) ? 1 : 0;
}

static int vmh266_stop(struct hevc_state_s *hevc)
{
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}
	if (hevc->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED)
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
		fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vh266_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	if (use_cma) {
		reset_process_time(hevc);
		hevc->dec_result = DEC_RESULT_FREE_CANVAS;
		vdec_schedule_work(&hevc->work);
		flush_work(&hevc->work);
	}
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	cancel_work_sync(&hevc->notify_work);
	cancel_work_sync(&hevc->set_clk_work);
	cancel_work_sync(&hevc->timeout_work);
	cancel_work_sync(&hevc->work);
	uninit_mmu_buffers(hevc);
#ifdef H266_USERDATA_ENABLE
	if (hevc->sei_itu_data_buf) {
		kfree(hevc->sei_itu_data_buf);
		hevc->sei_itu_data_buf = NULL;
	}
	if (hevc->sei_user_data_buffer) {
		kfree(hevc->sei_user_data_buffer);
		hevc->sei_user_data_buffer = NULL;
	}
#endif

	if (hevc->gvs)
		kfree(hevc->gvs);
	hevc->gvs = NULL;

	vfree(hevc->fw);
	hevc->fw = NULL;

	dump_log(hevc);
	return 0;
}

static unsigned char get_data_check_sum
	(struct hevc_state_s *hevc, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!hevc->chunk->block->is_mapped)
		data = codec_mm_vmap(hevc->chunk->block->start +
			hevc->data_offset, size);
	else
		data = ((u8 *)hevc->chunk->block->start_virt) +
			hevc->data_offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
		__func__, size, sum,
		data[0], data[1], data[2], data[3],
		data[4], data[5], data[size - 4],
		data[size - 3], data[size - 2],
		data[size - 1]);

	if (!hevc->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void vh266_notify_work(struct work_struct *work)
{
	struct hevc_state_s *hevc =
		container_of(work, struct hevc_state_s, notify_work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

#ifdef MULTI_INSTANCE_SUPPORT
	if (vdec->fr_hint_state == VDEC_NEED_HINT) {
		vf_notify_receiver(hevc->provider_name,
			VFRAME_EVENT_PROVIDER_FR_HINT,
			(void *)((unsigned long)hevc->frame_dur));
		vdec->fr_hint_state = VDEC_HINTED;
	} else if (fr_hint_status == VDEC_NEED_HINT) {
		vf_notify_receiver(hevc->provider_name,
			VFRAME_EVENT_PROVIDER_FR_HINT,
			(void *)((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	}
#else
	if (fr_hint_status == VDEC_NEED_HINT)
		vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	}
#endif

	return;
}

static void vh266_work_implement(struct hevc_state_s *hevc,
	struct vdec_s *vdec,int from)
{
	if (hevc->dec_result == DEC_RESULT_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_START);
	} else if (hevc->dec_result == DEC_RESULT_AGAIN)
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_AGAIN);

	if (hevc->dec_result == DEC_RESULT_FREE_CANVAS &&
		hevc->uninit_list_done == 0) {
		/*USE_BUF_BLOCK*/
		uninit_pic_list(hevc);
		return;
	}

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hevc->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	if (((hevc->dec_result == DEC_RESULT_GET_DATA) ||
		(hevc->dec_result == DEC_RESULT_GET_DATA_RETRY))
		&& (hw_to_vdec(hevc)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
		if (!input_frame_based(vdec)) {
			int r = vdec_sync_input(vdec);
			if (r >= 0x200) {
				WRITE_VREG(HEVC_DECODE_SIZE,
					READ_VREG(HEVC_DECODE_SIZE) + r);

				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
					"%s DEC_RESULT_GET_DATA %x %x %x mpc %x size 0x%x\n",
					__func__,
					READ_VREG(HEVC_STREAM_LEVEL),
					READ_VREG(HEVC_STREAM_WR_PTR),
					READ_VREG(HEVC_STREAM_RD_PTR),
					READ_VREG(HEVC_MPC_E), r);

				start_process_time(hevc);
				WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DEC_CONT);
			} else {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;
				vdec_schedule_work(&hevc->work);
			}
			return;
		}

		/*below for frame_base*/
		if (hevc->dec_result == DEC_RESULT_GET_DATA) {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x mpc %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				READ_VREG(HEVC_MPC_E));
			mutex_lock(&hevc->chunks_mutex);
			vdec_vframe_dirty(vdec, hevc->chunk);
			hevc->chunk = NULL;
			mutex_unlock(&hevc->chunks_mutex);
			vdec_clean_input(vdec);
		}

		/*if (is_new_pic_available(hevc)) {*/
		if (run_ready(vdec, VDEC_HEVC)) {
			int r;
			int decode_size;
			r = vdec_prepare_input(vdec, &hevc->chunk);
			if (r < 0) {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

				hevc_print(hevc,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh266: Insufficient data\n");

				vdec_schedule_work(&hevc->work);
				return;
			}
			hevc->dec_result = DEC_RESULT_NONE;
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x mpc %x\n",
				__func__, r,
				(get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(hevc, r) : 0,
				READ_VREG(HEVC_MPC_E));

			if (get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) {
				int jj;
				u8 *data = NULL;

				if (!hevc->chunk->block->is_mapped)
					data = codec_mm_vmap(
						hevc->chunk->block->start +
						hevc->data_offset, r);
				else
					data = ((u8 *)
						hevc->chunk->block->start_virt)
						+ hevc->data_offset;

				for (jj = 0; jj < r; jj++) {
					if ((jj & 0xf) == 0)
						hevc_print(hevc,
						PRINT_FRAMEBASE_DATA, "%06x:", jj);
					hevc_print_cont(hevc,
					PRINT_FRAMEBASE_DATA, "%02x ", data[jj]);
					if (((jj + 1) & 0xf) == 0)
						hevc_print_cont(hevc,
						PRINT_FRAMEBASE_DATA, "\n");
				}

				if (!hevc->chunk->block->is_mapped)
					codec_mm_unmap_phyaddr(data);
			}

			decode_size = hevc->data_size +
				(hevc->data_offset & (VDEC_FIFO_ALIGN - 1));
			WRITE_VREG(HEVC_DECODE_SIZE,
				READ_VREG(HEVC_DECODE_SIZE) + decode_size);

			vdec_enable_input(vdec);

			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: mpc %x\n",
				__func__, READ_VREG(HEVC_MPC_E));

			start_process_time(hevc);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		} else{
			hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

			/*hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			 *	"amvdec_vh266: Insufficient data\n");
			 */

			vdec_schedule_work(&hevc->work);
		}
		return;
	} else if (hevc->dec_result == DEC_RESULT_DONE) {
		/* if (!hevc->ctx_valid)
			hevc->ctx_valid = 1; */
			int i;
		hevc->dec_again_cnt = 0;
		decode_frame_count[hevc->index]++;
		if (hevc->muti_frame_flag)
			goto done_end;

		if (hevc->mmu_enable && ((hevc->double_write_mode & 0x10) == 0)) {
			hevc->used_4k_num =
				READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;
			if (hevc->used_4k_num >= 0 &&
				hevc->vvc_dec->cur_pic &&
				hevc->vvc_dec->cur_pic->scatter_alloc
				== 1)
				recycle_mmu_buf_tail(hevc, hevc->m_ins_flag);
		}
		hevc->pic_decoded_lcu_idx =
			READ_VREG(HEVC_PARSER_LCU_START)
			& 0xffffff;

		if (hevc->empty_flag == 0) {
			hevc->over_decode =
				(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
			if (hevc->over_decode)
				hevc_print(hevc, 0,
					"!!!Over decode\n");
		}

		if (is_log_enable(hevc))
			add_log(hevc,
				"%s DEC_RESULT_DONE %d lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x",
				__func__,
				hevc->pic_decoded_lcu_idx,
				hevc->used_4k_num,
				READ_VREG(HEVC_SHIFT_BYTE_COUNT),
				READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
				hevc->start_shift_bytes
				);

		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s DEC_RESULT_DONE lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes
			);

		hevc->used_4k_num = -1;

		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);

		if ((hevc->vvc_dec->cur_pic != NULL) && (hevc->vvc_dec->cur_pic->error_mark)) {
			hevc->gvs->error_frame_count++;
			if (hevc->vvc_dec->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_concealed_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->p_concealed_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->b_concealed_frames++;
			}
		}

		hevc->gvs->frame_count += hevc->slice_count;
		if ((hevc->slice_count != 0) && (hevc->vvc_dec->cur_pic != NULL)) {
			if (hevc->vvc_dec->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_decoded_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->p_decoded_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->b_decoded_frames++;
			}
		}

		if ((error_handle_policy & 0x100) == 0 && hevc->vvc_dec->cur_pic) {
			for (i = 0; i < MAX_REF_PIC_NUM; i++) {
				struct PIC_s *pic;
				pic = &hevc->vvc_dec->pic_pool[i];
				if (pic->used == 0 || pic->index == -1)
					continue;
				if ((hevc->vvc_dec->cur_pic->poc + poc_num_margin < pic->poc) && (pic->referenced == 0) &&
					(pic->decode_done == 1)) {
					hevc->poc_error_count++;
					break;
				}
			}
			if (i == MAX_REF_PIC_NUM)
				hevc->poc_error_count = 0;
			if (hevc->poc_error_count >= poc_error_limit) {
				for (i = 0; i < MAX_REF_PIC_NUM; i++) {
					struct PIC_s *pic;
					pic = &hevc->vvc_dec->pic_pool[i];
					if (pic->used == 0 || pic->index == -1)
						continue;
					if ((hevc->vvc_dec->cur_pic->poc + poc_num_margin < pic->poc) && (pic->referenced == 0) &&
						(pic->decode_done == 1)) {
						//pic->decode_done = 0;
						hevc_print(hevc, 0, "DPB poc error, remove error frame, to do ...\n");
					}
				}
			}
		}

done_end:
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
#ifdef AGAIN_HAS_THRESHOLD
		hevc->next_again_flag = 1;
#endif
		if (input_stream_based(vdec)) {
			if (!(error_handle_policy & 0x400) && check_data_size(vdec)) {
				hevc->dec_result = DEC_RESULT_DONE;
				vdec_schedule_work(&hevc->work);
				return;
			} else if ((((error_handle_policy & 0x200) == 0) &&
						(hevc->vvc_dec->init_hw_flag == 0))) {
				check_dirty_data(vdec);
			}
		}
	} else if (hevc->dec_result == DEC_RESULT_EOS) {
		hevc->eos = 1;
		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS, "%s: end of stream\n", __func__);
		flush_output(hevc);
		/* dummy vf with eos flag to backend */
		notify_v4l_eos(hw_to_vdec(hevc));
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_FORCE_EXIT) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n",
			__func__);
		if (hevc->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			hevc->stat &= ~STAT_VDEC_RUN;
		}
		if (hevc->stat & STAT_ISR_REG) {
				WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
			vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
			hevc->stat &= ~STAT_ISR_REG;
		}
		hevc_print(hevc, 0, "%s: force exit end\n",
			__func__);
	} else if (hevc->dec_result == DEC_RESULT_ERROR_DATA) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s DEC_RESULT_ERROR_DATA lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes);
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_UNFINISH) {
		/* if (!hevc->ctx_valid)
			hevc->ctx_valid = 1; */
			int i;
		hevc->dec_again_cnt = 0;
		decode_frame_count[hevc->index]++;

		if (hevc->mmu_enable && ((hevc->double_write_mode & 0x10) == 0)) {
			hevc->used_4k_num =
				READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;
			if (hevc->used_4k_num >= 0 &&
				hevc->vvc_dec->cur_pic &&
				hevc->vvc_dec->cur_pic->scatter_alloc
				== 1)
				recycle_mmu_buf_tail(hevc, hevc->m_ins_flag);
		}
		hevc->pic_decoded_lcu_idx =
			READ_VREG(HEVC_PARSER_LCU_START)
			& 0xffffff;

		if (hevc->empty_flag == 0) {
			hevc->over_decode =
				(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
			if (hevc->over_decode)
				hevc_print(hevc, 0,
					"!!!Over decode\n");
		}

		if (is_log_enable(hevc))
			add_log(hevc,
				"%s DEC_RESULT_UNFINISH lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x",
				__func__,
				hevc->pic_decoded_lcu_idx,
				hevc->used_4k_num,
				READ_VREG(HEVC_SHIFT_BYTE_COUNT),
				READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
				hevc->start_shift_bytes
				);

		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s DEC_RESULT_UNFINISH lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes
			);

		hevc->used_4k_num = -1;

		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);

		if ((hevc->vvc_dec->cur_pic != NULL) && (hevc->vvc_dec->cur_pic->error_mark)) {
			hevc->gvs->error_frame_count++;
			if (hevc->vvc_dec->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_concealed_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->p_concealed_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->b_concealed_frames++;
			}
		}

		hevc->gvs->frame_count += hevc->slice_count;
		if ((hevc->slice_count != 0) && (hevc->vvc_dec->cur_pic != NULL)) {
			if (hevc->vvc_dec->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_decoded_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->p_decoded_frames++;
			} else if (hevc->vvc_dec->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->b_decoded_frames++;
			}
		}

		if ((error_handle_policy & 0x100) == 0 && hevc->vvc_dec->cur_pic) {
			for (i = 0; i < MAX_REF_PIC_NUM; i++) {
				struct PIC_s *pic;
				pic = &hevc->vvc_dec->pic_pool[i];
				if (pic->used == 0 || pic->index == -1)
					continue;
				if ((hevc->vvc_dec->cur_pic->poc + poc_num_margin < pic->poc) && (pic->referenced == 0) &&
					(pic->decode_done == 1)) {
					hevc->poc_error_count++;
					break;
				}
			}
			if (i == MAX_REF_PIC_NUM)
				hevc->poc_error_count = 0;
			if (hevc->poc_error_count >= poc_error_limit) {
				for (i = 0; i < MAX_REF_PIC_NUM; i++) {
					struct PIC_s *pic;
					pic = &hevc->vvc_dec->pic_pool[i];
					if (pic->used == 0 || pic->index == -1)
						continue;
					if ((hevc->vvc_dec->cur_pic->poc + poc_num_margin < pic->poc) && (pic->referenced == 0) &&
						(pic->decode_done == 1)) {
						//pic->decode_done = 0;
						hevc_print(hevc, 0, "DPB poc error, remove error frame, to do ...\n");
					}
				}
			}
		}
	}

	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	ATRACE_COUNTER(hevc->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_START);
	wait_hevc_search_done(hevc);
	ATRACE_COUNTER(hevc->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_END);

	if (from == 1) {
		/* This is a timeout work */
		if (work_pending(&hevc->work)) {
			/*
			 * The vh266_work arrives at the last second,
			 * give it a chance to handle the scenario.
			 */
			return;
			//cancel_work_sync(&hevc->work);//reserved for future consideration
		}
	}
	if (hevc->dec_result == DEC_RESULT_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_END);
	}

	if (get_dbg_flag(hevc) & H266_DEBUG_DETAIL) {
		hevc_print(hevc, 0, "%s:frame_count %d, drop_frame_count %d, error_frame_count %d\n",
			__func__, hevc->gvs->frame_count, hevc->gvs->drop_frame_count, hevc->gvs->error_frame_count);
		hevc_print(hevc, 0, "i decoded_frames %d, lost_frames %d, concealed_frames %d\n",
			hevc->gvs->i_decoded_frames, hevc->gvs->i_lost_frames, hevc->gvs->i_concealed_frames);
		hevc_print(hevc, 0, "p decoded_frames %d, lost_frames %d, concealed_frames %d\n",
			hevc->gvs->p_decoded_frames, hevc->gvs->p_lost_frames, hevc->gvs->p_concealed_frames);
		hevc_print(hevc, 0, "b decoded_frames %d, lost_frames %d, concealed_frames %d\n",
			hevc->gvs->b_decoded_frames, hevc->gvs->b_lost_frames, hevc->gvs->b_concealed_frames);
	}

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (hevc->is_used_v4l) {
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

		if (ctx->param_sets_from_ucode &&
			!hevc->v4l_params_parsed)
			vdec_v4l_write_frame_sync(ctx);
	}

	if (from == 1)
		hevc->timeout_processing = 0;

	if (hevc->vdec_cb)
		hevc->vdec_cb(hw_to_vdec(hevc), hevc->vdec_cb_arg, CORE_MASK_HEVC);
}

static void vh266_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
			struct hevc_state_s, work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	vh266_work_implement(hevc, vdec, 0);
}

static void vh266_timeout_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, timeout_work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (work_pending(&hevc->work))
		return;
	hevc->timeout_processing = 1;
	vh266_work_implement(hevc, vdec, 1);
}


static int vh266_hw_ctx_restore(struct hevc_state_s *hevc)
{
	vh266_prot_init(hevc);
	return 0;
}
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int tvp = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	bool ret = 0;
	if (step == 0x12)
		return 0;
	else if (step == 0x11)
		step = 0x12;

	if (hevc->fatal_error & DECODER_FATAL_ERROR_NO_MEM)
		return 0;

	if (hevc->eos)
		return 0;
	if (hevc->timeout_processing &&
	    (work_pending(&hevc->work) ||
	    work_busy(&hevc->work) ||
	    work_busy(&hevc->timeout_work) ||
	    work_pending(&hevc->timeout_work))) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			   "h266 work pending,not ready for run.\n");
		return 0;
	}
	hevc->timeout_processing = 0;
	if (!hevc->first_sc_checked && hevc->mmu_enable) {
		int size;
		struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
		void * mmu_box = ctx->bm.mmu;;

		size = decoder_mmu_box_sc_check(mmu_box, tvp);
		hevc->first_sc_checked =1;
		hevc_print(hevc, 0,
			"vh266 cached=%d  need_size=%d speed= %d ms\n",
			size, (hevc->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - hevc->sc_start_time) * 1000/HZ);
	}
	if (vdec_stream_based(vdec) && (hevc->init_flag == 0)
			&& pre_decode_buf_level != 0) {
			u32 rp, wp, level;

			rp = STBUF_READ(&vdec->vbuf, get_rp);
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = vdec->input.size + wp - rp;
			else
				level = wp - rp;

			if (level < pre_decode_buf_level)
				return PRE_LEVEL_NOT_ENOUGH;
	}

#ifdef AGAIN_HAS_THRESHOLD
	if (hevc->next_again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		if (parser_wr_ptr >= hevc->pre_parser_wr_ptr &&
			(parser_wr_ptr - hevc->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			hevc_print(hevc,
				PRINT_FLAG_VDEC_DETAIL, "%s buf level:%x\n",  __func__, r);
			return 0;
		}
	}
#endif

	if (disp_vframe_valve_level &&
		kfifo_len(&hevc->display_q) >=
		disp_vframe_valve_level) {
		hevc->valve_count--;
		if (hevc->valve_count <= 0)
			hevc->valve_count = 2;
		else
			return 0;
	}

	/*
	ret = is_new_pic_available(hevc);
	if (!ret) {
		hevc_print(hevc,
		PRINT_FLAG_VDEC_DETAIL, "%s=>%d\r\n",
		__func__, ret);
	}
	*/
#ifdef CONSTRAIN_MAX_BUF_NUM
	if (hevc->vvc_dec->init_hw_flag && !hevc->is_used_v4l) {
		if (run_ready_max_vf_only_num > 0 &&
			get_vf_ref_only_buf_count(hevc) >=
			run_ready_max_vf_only_num
			)
			ret = 0;
		if (run_ready_display_q_num > 0 &&
			kfifo_len(&hevc->display_q) >=
			run_ready_display_q_num)
			ret = 0;

		if (run_ready_max_buf_num &&
			get_used_buf_count(hevc) >=
			run_ready_max_buf_num)
			ret = 0;
	}
#endif

	ret = is_available_buffer(hevc);

	if (ret)
		not_run_ready[hevc->index] = 0;
	else
		not_run_ready[hevc->index]++;
	if (vdec->parallel_dec == 1)
		return ret ? (CORE_MASK_HEVC) : 0;
	else
		return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *, int), void *arg)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int r, loadr = 0;
	unsigned char check_sum = 0;

	if (hevc->process_state != PROCESS_STATE_HEAD_AGAIN &&
		hevc->process_state != PROCESS_STATE_DECODE_AGAIN)
		hevc->process_state = PROCESS_STATE_INIT;
	run_count[hevc->index]++;
	hevc->vdec_cb_arg = arg;
	hevc->vdec_cb = callback;
	hevc->aux_data_dirty = 1;
	hevc->timeout_flag = 0;
	if (i_only_flag)
		hevc->i_only = i_only_flag & 0xff;

	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_RUN_START);
	hevc_reset_core(vdec);

#ifdef AGAIN_HAS_THRESHOLD
	if (vdec_stream_based(vdec)) {
		hevc->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		hevc->next_again_flag = 0;
	}
#endif

	if ((vdec_frame_based(vdec)) &&
		(hevc->dec_result == DEC_RESULT_UNFINISH)) {
		u32 res_byte = hevc->data_size - hevc->consume_byte;

		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			"%s before, consume 0x%x, size 0x%x, offset 0x%x, res 0x%x\n", __func__,
			hevc->consume_byte, hevc->data_size, hevc->data_offset + hevc->consume_byte, res_byte);

		hevc->data_invalid = vdec_offset_prepare_input(vdec, hevc->consume_byte, hevc->data_offset, hevc->data_size);
		hevc->data_offset -= (hevc->data_invalid - hevc->consume_byte);
		hevc->data_size += (hevc->data_invalid - hevc->consume_byte);
		r = hevc->data_size;
		if ((r < 0) || (hevc->chunk == NULL)) {
			input_empty[hevc->index]++;
			hevc->dec_result = DEC_RESULT_AGAIN;
			hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
				"%s: Insufficient data, r %d, hevc->chunk %p\n", __func__, r, hevc->chunk);

			vdec_schedule_work(&hevc->work);
			return;
		}
		hevc->muti_frame_flag = 1;
		WRITE_VREG(HEVC_WAIT_FLAG, hevc->data_invalid);

		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			"%s after, consume 0x%x, size 0x%x, offset 0x%x, invalid 0x%x, res 0x%x\n", __func__,
			hevc->consume_byte, hevc->data_size, hevc->data_offset, hevc->data_invalid, res_byte);
	} else {
		r = vdec_prepare_input(vdec, &hevc->chunk);
		if (r < 0) {
			input_empty[hevc->index]++;
			hevc->dec_result = DEC_RESULT_AGAIN;
			hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
				"ammvdec_vh266: Insufficient data\n");

			vdec_schedule_work(&hevc->work);
			return;
		}
		if ((vdec_frame_based(vdec)) &&
			(hevc->chunk != NULL)) {
			hevc->data_offset = hevc->chunk->offset;
			hevc->data_size = r;
		}
		hevc->muti_frame_flag = 0;
		WRITE_VREG(HEVC_WAIT_FLAG, 0);
	}

	input_empty[hevc->index] = 0;
	hevc->dec_result = DEC_RESULT_NONE;
	if (vdec_frame_based(vdec) &&
		((get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
		|| is_log_enable(hevc)) &&
		!vdec_secure(vdec))
		check_sum = get_data_check_sum(hevc, r);

	if (is_log_enable(hevc))
		add_log(hevc,
			"%s: size 0x%x sum 0x%x shiftbyte 0x%x",
			__func__, r,
			check_sum,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT)
			);
	if ((hevc->dirty_shift_flag == 1) && !(vdec->input.swap_valid)) {
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, vdec->input.stream_cookie);
	}
	hevc->start_shift_bytes = READ_VREG(HEVC_SHIFT_BYTE_COUNT);

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x process_state %d, (%x %x %x %x %x) byte count %x\n",
		__func__, r,
		check_sum,
		hevc->process_state,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR),
		STBUF_READ(&vdec->vbuf, get_rp),
		STBUF_READ(&vdec->vbuf, get_wp),
		hevc->start_shift_bytes
		);
	if ((get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) &&
		input_frame_based(vdec) &&
		!vdec_secure(vdec)) {
		int jj;
		u8 *data = NULL;
		if (!hevc->chunk->block->is_mapped)
			data = codec_mm_vmap(hevc->chunk->block->start +
				hevc->data_offset, r);
		else
			data = ((u8 *)hevc->chunk->block->start_virt)
				+ hevc->data_offset;

		for (jj = 0; jj < r; jj++) {
			if ((jj & 0xf) == 0)
				hevc_print(hevc, PRINT_FRAMEBASE_DATA,
					"%06x:", jj);
			hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
				"%02x ", data[jj]);
			if (((jj + 1) & 0xf) == 0)
				hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
					"\n");
		}

		if (!hevc->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	}
	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_START);
	if (vdec->mc_loaded) {
		/*firmware have load before,
		  and not changes to another.
		  ignore reload.
		*/
		if (fw_tee_enabled() && hevc->is_swap)
			WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->swap_addr);
	} else {
#ifdef USE_OLD_CHIP
		if (hevc->mmu_enable) {
			if (hevc->enable_ucode_swap) {
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"hevc_mmu_swap", hevc->fw->data);
				if (loadr < 0) {
					loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
							"h265_mmu", hevc->fw->data);
					hevc->enable_ucode_swap = false;
				} else
				hevc->is_swap = true;
			} else {
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"h265_mmu", hevc->fw->data);
			}
		} else {
			loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec, NULL, hevc->fw->data);
			hevc->is_swap = true;
		}
#else
		if (hevc->enable_ucode_swap) {
			loadr = amhevc_vdec_loadmc_ex(VFORMAT_H266, vdec,
					"h266_mmu_swap", hevc->fw->data);
			if (loadr < 0) {
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_H266, vdec,
					"h266_mmu", hevc->fw->data);
				hevc->enable_ucode_swap = false;
			} else
				hevc->is_swap = true;
		} else {
			loadr = amhevc_vdec_loadmc_ex(VFORMAT_H266, vdec,
					"h266_mmu", hevc->fw->data);
		}
#endif
		if (loadr < 0) {
			amhevc_disable();
			hevc_print(hevc, 0, "H266: the %s fw loading failed, err: %x\n",
				fw_tee_enabled() ? "TEE" : "local", loadr);
			hevc->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hevc->work);
			return;
		}

		if (fw_tee_enabled() && hevc->is_swap)
			hevc->swap_addr = READ_VREG(HEVC_STREAM_SWAP_BUFFER2);
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_H266;
	}

	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_END);

	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_START);
	if (vh266_hw_ctx_restore(hevc) < 0) {
		vdec_schedule_work(&hevc->work);
		return;
	}
	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_END);
	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec)) {
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
		r = hevc->data_size +
			(hevc->data_offset & (VDEC_FIFO_ALIGN - 1));
		hevc->decode_size = r;
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = hevc->data_size;
	}
	WRITE_VREG(HEVC_DECODE_SIZE, r);
	hevc->init_flag = 1;
	hevc->cur_idx = INVALID_IDX;

	if (hevc->vvc_dec->init_hw_flag)
		init_pic_list_hw(hevc);
	if (get_dbg_flag(hevc) & H266_DEBUG_BUFMGR_MORE)
		print_pic_pool(hevc, "before start ucode");

	start_process_time(hevc);
	mod_timer(&hevc->timer, jiffies);
	hevc->stat |= STAT_TIMER_ARM;
	hevc->stat |= STAT_ISR_REG;
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
#ifdef DYN_CACHE
	hevc_print(hevc, H266_DEBUG_REG_CFG, "HEVC DYN MCRCC\n");
	WRITE_VREG(HEVCD_IPP_DYN_CACHE,0x2b);//enable new mcrcc
#endif
	amhevc_start();
	hevc->stat |= STAT_VDEC_RUN;
	hevc->slice_count = 0;
	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_RUN_END);
}

static void aml_free_canvas(struct vdec_s *vdec)
{
	int i;
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = &hevc->vvc_dec->pic_pool[i];

		if (pic) {
			if (vdec->parallel_dec == 1) {
				vdec->free_canvas_ex(pic->y_canvas_index, vdec->id);
				vdec->free_canvas_ex(pic->uv_canvas_index, vdec->id);
			}
		}
		hevc->buffer_wrap[i] = i;
	}
}

static int vh266_reset_frame_buffer(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	struct aml_buf *aml_buf;
	struct PIC_s *pic = NULL;
	ulong flags;
	int i;

	for (i = 0; i < hevc->used_buf_num; ++i) {
		pic = &hevc->vvc_dec->pic_pool[i];
		if (pic == NULL)
			continue;
		if (pic->cma_alloc_addr) {
			aml_buf = (struct aml_buf *)hevc->m_BUF[pic->index].v4l_ref_buf_addr;

			hevc_print(hevc, H266_DEBUG_BUFMGR,
				"%s buf idx: %d dma addr: 0x%lx vb idx: %d vf_ref %d\n",
				__func__, i, pic->cma_alloc_addr,
				aml_buf->index,
				pic->vf_ref);

			if (!pic->vf_ref &&
				!(hevc->vframe_dummy.type & VIDTYPE_V4L_EOS)) {
				aml_buf_put_ref(&ctx->bm, aml_buf);
				aml_buf_put_ref(&ctx->bm, aml_buf);
			} else
				aml_buf_put_ref(&ctx->bm, aml_buf);

			spin_lock_irqsave(&h266_lock, flags);
			while (pic->vf_ref) {
				atomic_add(1, &hevc->vf_put_count);
				pic->vf_ref--;
			}

			pic->show_frame = false;
			pic->cma_alloc_addr = 0;
			pic->vf_ref = 0;
			pic->referenced = 0;
			hevc->m_BUF[pic->index].v4l_ref_buf_addr =0;

			spin_unlock_irqrestore(&h266_lock, flags);
		}
	}

	return 0;
}

static void  h266_decode_ctx_reset(struct hevc_state_s *hevc)
{
	int i;

	for (i = 0; i < PIC_POOL_SIZE; ++i) {
		hevc->vvc_dec->pic_pool[i].vf_ref = 0;
		hevc->vvc_dec->pic_pool[i].cma_alloc_addr = 0;
		hevc->vvc_dec->pic_pool[i].index = i;
		hevc->vvc_dec->pic_pool[i].BUF_index = i;
		hevc->vvc_dec->pic_pool[i].used = 0;
	}

	for (i = 0; i < MAX_REF_PIC_NUM; ++i) {
		hevc->m_mv_BUF[i].used_flag = 0;
	}

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		hevc->m_BUF[i].start_adr = 0;
	}

	for (i = 0; i < BUF_FBC_NUM_MAX; i++) {
		if (hevc->afbc_buf_table[i].used)
			hevc->afbc_buf_table[i].used = 0;
	}

	hevc->dec_result = DEC_RESULT_NONE;
	hevc->timeout_flag = 0;
	hevc->vvc_dec->init_hw_flag = 0;
	hevc->init_flag		= 0;
	hevc->first_sc_checked	= 0;
	hevc->fatal_error		= 0;
	hevc->show_frame_num	= 0;
	hevc->process_state	= 0;
	hevc->eos = false;
	hevc->resolution_change = false;
	hevc->aml_buf = NULL;

	hevc->vf_pre_count = 0;
	atomic_set(&hevc->vf_get_count, 0);
	atomic_set(&hevc->vf_put_count, 0);
}

static void reset(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	cancel_work_sync(&hevc->work);
	cancel_work_sync(&hevc->notify_work);
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	reset_process_time(hevc);
	dealloc_mv_bufs(hevc);
	vh266_reset_frame_buffer(hevc);
	aml_free_canvas(vdec);
	if (!hevc->resolution_change) {
		hevc_local_uninit(hevc);
		h266_decode_ctx_reset(hevc);
	}

	if (vh266_local_init(hevc) < 0)
		pr_debug(" %s local init fail\n", __func__);

	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
}

static irqreturn_t vh266_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh266_isr(0, hevc);
}

static irqreturn_t vh266_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh266_isr_thread_fn(0, hevc);
}
#endif

static int amvdec_h266_probe(struct platform_device *pdev)
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
#else
	struct vdec_dev_reg_s *pdata =
		(struct vdec_dev_reg_s *)pdev->dev.platform_data;
#endif
	char *tmpbuf;
	int ret;
	struct hevc_state_s *hevc;

	hevc = vmalloc(sizeof(struct hevc_state_s));
	if (hevc == NULL) {
		hevc_print(hevc, 0, "%s vmalloc hevc failed\r\n", __func__);
		return -ENOMEM;
	}
	gHevc = hevc;
	if ((debug & H266_NO_CHANG_DEBUG_FLAG_IN_CODE) == 0)
			debug &= (~(H266_DEBUG_DIS_LOC_ERROR_PROC |
					H266_DEBUG_DIS_SYS_ERROR_PROC));
	memset(hevc, 0, sizeof(struct hevc_state_s));
	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);
	mutex_lock(&vh266_mutex);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7; /*old 1*/
	hevc->m_ins_flag = 0;
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	hevc->uninit_list_done = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;
	hevc->frameinfo_enable = 1;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->platform_dev = pdev;
	platform_set_drvdata(pdev, pdata);
#endif

	if (pdata == NULL) {
		hevc_print(hevc, 0,
			"\namvdec_h266 memory resource undefined.\n");
		vfree(hevc);
		mutex_unlock(&vh266_mutex);
		return -EFAULT;
	}
	if (mmu_enable_force == 0) {
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL
			|| double_write_mode == 0x10)
			hevc->mmu_enable = 0;
		else
			hevc->mmu_enable = 1;
	}
#ifdef VVC_10B_MMU_DW
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T5D) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TXHD2)) {
		hevc->dw_mmu_enable =
			get_double_write_mode(hevc) & 0x20 ? 1 : 0;
	} else {
		hevc->dw_mmu_enable = 0;
	}
#endif
	if (init_mmu_buffers(hevc, 1)) {
		hevc_print(hevc, 0,
			"\n 266 mmu init failed!\n");
		vfree(hevc);
		mutex_unlock(&vh266_mutex);
		return -EFAULT;
	}

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box, BMMU_WORKSPACE_ID,
			work_buf_size, DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		vfree(hevc);
		mutex_unlock(&vh266_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;
	hevc_print(hevc, 0, "hevc->buf_start %x work_buf_size %x\n", hevc->buf_start, work_buf_size);

	if (!vdec_secure(pdata)) {
			tmpbuf = (char *)codec_mm_phys_to_virt(hevc->buf_start);
			if (tmpbuf) {
					memset(tmpbuf, 0, work_buf_size);
					dma_sync_single_for_device(amports_get_dma_device(),
							hevc->buf_start,
							work_buf_size, DMA_TO_DEVICE);
			} else {
					tmpbuf = codec_mm_vmap(hevc->buf_start,
							work_buf_size);
					if (tmpbuf) {
							memset(tmpbuf, 0, work_buf_size);
							dma_sync_single_for_device(
									amports_get_dma_device(),
									hevc->buf_start,
									work_buf_size,
									DMA_TO_DEVICE);
							codec_mm_unmap_phyaddr(tmpbuf);
					}
			}
	}

	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.266 decoder mem resource 0x%lx size 0x%x\n",
			hevc->buf_start, hevc->buf_size);
	}

	if (pdata->sys_info)
		hevc->vh266_amstream_dec_info = *pdata->sys_info;
	else {
		hevc->vh266_amstream_dec_info.width = 0;
		hevc->vh266_amstream_dec_info.height = 0;
		hevc->vh266_amstream_dec_info.rate = 30;
	}

	hevc->endian = HEVC_CONFIG_LITTLE_ENDIAN;

	if (endian)
		hevc->endian = endian;

#ifndef MULTI_INSTANCE_SUPPORT
	if (pdata->flag & DEC_FLAG_HEVC_WORKAROUND) {
		workaround_enable |= 3;
		hevc_print(hevc, 0,
			"amvdec_h266 HEVC_WORKAROUND flag set.\n");
	} else
		workaround_enable &= ~3;
#endif
	hevc->cma_dev = pdata->cma_dev;
	vh266_vdec_info_init(hevc);

#ifdef MULTI_INSTANCE_SUPPORT
	pdata->private = hevc;
	pdata->dec_status = vh266_dec_status;
	pdata->set_trickmode = vh266_set_trickmode;
	pdata->set_isreset = vh266_set_isreset;
	is_reset = 0;
	if (vh266_init(pdata) < 0) {
#else
	if (vh266_init(hevc) < 0) {
#endif
		hevc_print(hevc, 0,
			"\namvdec_h266 init failed.\n");
		hevc_local_uninit(hevc);
		if (hevc->gvs)
			kfree(hevc->gvs);
		hevc->gvs = NULL;
		uninit_mmu_buffers(hevc);
		vfree(hevc);
		pdata->dec_status = NULL;
		mutex_unlock(&vh266_mutex);
		return -ENODEV;
	}
	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_H266,
			3840, 2160, 60);
	mutex_unlock(&vh266_mutex);

	return 0;
}

static int amvdec_h266_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc = gHevc;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	mutex_lock(&vh266_mutex);

	vh266_stop(hevc);

	hevc_source_changed(VFORMAT_H266, 0, 0, 0);


#ifdef DEBUG_PTS
	hevc_print(hevc, 0,
		"pts missed %ld, pts hit %ld, duration %d\n",
		hevc->pts_missed, hevc->pts_hit, hevc->frame_dur);
#endif

	vfree(hevc);
	hevc = NULL;
	gHevc = NULL;

	mutex_unlock(&vh266_mutex);

	return 0;
}
/****************************************/
#ifdef CONFIG_PM
static int h266_suspend(struct device *dev)
{
	amhevc_suspend(to_platform_device(dev), dev->power.power_state);
	return 0;
}

static int h266_resume(struct device *dev)
{
	amhevc_resume(to_platform_device(dev));
	return 0;
}

static const struct dev_pm_ops h266_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(h266_suspend, h266_resume)
};
#endif

static struct platform_driver amvdec_h266_driver = {
	.probe = amvdec_h266_probe,
	.remove = amvdec_h266_remove,
	.driver = {
		.name = DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &h266_pm_ops,
#endif
	}
};

#ifdef MULTI_INSTANCE_SUPPORT
static void vh266_dump_state(struct vdec_s *vdec)
{
	int i;
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	hevc_print(hevc, 0,
		"====== %s\n", __func__);

	hevc_print(hevc, 0,
		"width/height (%d/%d), reorder_pic_num %d ip_mode %d buf count(bufspec size) %d, video_signal_type 0x%x, is_swap %d i_only 0x%x\n",
		hevc->frame_width,
		hevc->frame_height,
		hevc->sps_num_reorder_pics_0,
		hevc->ip_mode,
		hevc->used_buf_num,
		hevc->video_signal_type_debug,
		hevc->is_swap,
		hevc->i_only
		);

	hevc_print(hevc, 0,
		"is_framebase(%d), eos %d, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d error_frame_count %d drop_frame_count %d\n",
		input_frame_based(vdec),
		hevc->eos,
		hevc->dec_result,
		decode_frame_count[hevc->index],
		display_frame_count[hevc->index],
		run_count[hevc->index],
		not_run_ready[hevc->index],
		input_empty[hevc->index],
		hevc->gvs->error_frame_count,
		hevc->gvs->drop_frame_count
		);

	if (hevc->is_used_v4l && vf_get_receiver(vdec->vf_provider_name)) {
		enum receiver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		hevc_print(hevc, 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}

	hevc_print(hevc, 0,
	"%s, newq(%d/%d), dispq(%d/%d), vf prepare/get/put (%d/%d/%d), init_hw_flag(%d), is_new_pic_available(%d), use count(%d) pic_num(%d)\n",
	__func__,
	kfifo_len(&hevc->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hevc->display_q),
	VF_POOL_SIZE,
	hevc->vf_pre_count,
	hevc->vf_get_count,
	hevc->vf_put_count,
	hevc->vvc_dec->init_hw_flag,
	is_new_pic_available(hevc),
	get_used_buf_count(hevc),
	get_work_pic_num(hevc));

	print_pic_pool(hevc, "");

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		hevc_print(hevc, 0,
			"Buf(%d) start_adr 0x%x header_addr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_BUF[i].start_adr,
			hevc->m_BUF[i].header_addr,
			hevc->m_BUF[i].size,
			hevc->m_BUF[i].used_flag);
	}

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		hevc_print(hevc, 0,
			"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_mv_BUF[i].start_adr,
			hevc->m_mv_BUF[i].size,
			hevc->m_mv_BUF[i].used_flag);
	}

	hevc_print(hevc, 0,
		"HEVC_DEC_STATUS_REG=0x%x\n",
		READ_VREG(HEVC_DEC_STATUS_REG));
	hevc_print(hevc, 0,
		"HEVC_MPC_E=0x%x\n",
		READ_VREG(HEVC_MPC_E));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE2=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE2));
	hevc_print(hevc, 0,
		"NAL_SEARCH_CTL=0x%x\n",
		READ_VREG(NAL_SEARCH_CTL));
	hevc_print(hevc, 0,
		"HEVC_PARSER_LCU_START=0x%x\n",
		READ_VREG(HEVC_PARSER_LCU_START));
	hevc_print(hevc, 0,
		"HEVC_DECODE_SIZE=0x%x\n",
		READ_VREG(HEVC_DECODE_SIZE));
	hevc_print(hevc, 0,
		"HEVC_SHIFT_BYTE_COUNT=0x%x\n",
		READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	hevc_print(hevc, 0,
		"HEVC_STREAM_START_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_START_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_END_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_END_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_LEVEL=0x%x\n",
		READ_VREG(HEVC_STREAM_LEVEL));
	hevc_print(hevc, 0,
		"HEVC_STREAM_WR_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_WR_PTR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_RD_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_RD_PTR));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (input_frame_based(vdec) &&
		(get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA)
		) {
		int jj;
		if (hevc->chunk && hevc->chunk->block &&
			hevc->data_size > 0) {
			u8 *data = NULL;
			if (!hevc->chunk->block->is_mapped)
				data = codec_mm_vmap(hevc->chunk->block->start +
					hevc->data_offset, hevc->data_size);
			else
				data = ((u8 *)hevc->chunk->block->start_virt)
					+ hevc->data_offset;
			hevc_print(hevc, 0,
				"frame data size 0x%x\n",
				hevc->data_size);
			for (jj = 0; jj < hevc->data_size; jj++) {
				if ((jj & 0xf) == 0)
					hevc_print(hevc,
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				hevc_print_cont(hevc,
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					hevc_print_cont(hevc,
					PRINT_FRAMEBASE_DATA,
						"\n");
			}

			if (!hevc->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}


static int ammvdec_h266_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct hevc_state_s *hevc = NULL;
	int ret;
	int i;
	struct aml_vcodec_ctx *ctx = NULL;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	int config_val;
#endif
	static struct vframe_operations_s vf_tmp_ops;

	if (pdata == NULL) {
		pr_info("\nammvdec_h266 memory resource undefined.\n");
		return -EFAULT;
	}

	hevc = vmalloc(sizeof(struct hevc_state_s));
	if (hevc == NULL) {
		pr_info("\nammvdec_h266 device data allocation failed\n");
		return -ENOMEM;
	}
	memset(hevc, 0, sizeof(struct hevc_state_s));

	/* the ctx from v4l2 driver. */
	hevc->v4l2_ctx = pdata->private;
	ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	ctx->vdec_recycle_dec_resource = vh266_recycle_dec_resource;

	pdata->private = hevc;
	pdata->dec_status = vh266_dec_status;
	pdata->set_trickmode = vh266_set_trickmode;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh266_irq_cb;
	pdata->threaded_irq_handler = vh266_threaded_irq_cb;
	pdata->dump_state = vh266_dump_state;
#ifdef H266_USERDATA_ENABLE
	pdata->wakeup_userdata_poll = vh266_wakeup_userdata_poll;
	pdata->user_data_read = vh266_user_data_read;
	pdata->reset_userdata_fifo = vh266_reset_userdata_fifo;
#else
	pdata->wakeup_userdata_poll = NULL;
	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;
#endif

	hevc->index = pdev->id;
	hevc->m_ins_flag = 1;

	if (is_rdma_enable()) {
		hevc->rdma_adr = decoder_dma_alloc_coherent(&hevc->rdma_mem_handle,
			RDMA_SIZE, &hevc->rdma_phy_adr, "H.266_RDMA_BUF");
		for (i = 0; i < SCALELUT_DATA_WRITE_NUM; i++) {
			hevc->rdma_adr[i * 4] = HEVC_IQIT_SCALELUT_WR_ADDR & 0xfff;
			hevc->rdma_adr[i * 4 + 1] = i;
			hevc->rdma_adr[i * 4 + 2] = HEVC_IQIT_SCALELUT_DATA & 0xfff;
			hevc->rdma_adr[i * 4 + 3] = 0;
			if (i == SCALELUT_DATA_WRITE_NUM - 1) {
				hevc->rdma_adr[i * 4 + 2] = (HEVC_IQIT_SCALELUT_DATA & 0xfff) | 0x20000;
			}
		}
	}
	snprintf(hevc->trace.vdec_name, sizeof(hevc->trace.vdec_name),
		"h266-%d", hevc->index);
	snprintf(hevc->trace.pts_name, sizeof(hevc->trace.pts_name),
		"%s-pts", hevc->trace.vdec_name);
	snprintf(hevc->trace.vf_get_name, sizeof(hevc->trace.vf_get_name),
		"%s-vf_get", hevc->trace.vdec_name);
	snprintf(hevc->trace.vf_put_name, sizeof(hevc->trace.vf_put_name),
		"%s-vf_put", hevc->trace.vdec_name);
	snprintf(hevc->trace.set_canvas0_addr, sizeof(hevc->trace.set_canvas0_addr),
		"%s-set_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.get_canvas0_addr, sizeof(hevc->trace.get_canvas0_addr),
		"%s-get_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.put_canvas0_addr, sizeof(hevc->trace.put_canvas0_addr),
		"%s-put_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.new_q_name, sizeof(hevc->trace.new_q_name),
		"%s-newframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.disp_q_name, sizeof(hevc->trace.disp_q_name),
		"%s-dispframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.decode_time_name, sizeof(hevc->trace.decode_time_name),
		"decoder_time%d", pdev->id);
	snprintf(hevc->trace.decode_run_time_name, sizeof(hevc->trace.decode_run_time_name),
		"decoder_run_time%d", pdev->id);
	snprintf(hevc->trace.decode_header_memory_time_name, sizeof(hevc->trace.decode_header_memory_time_name),
		"decoder_header_time%d", pdev->id);
	snprintf(hevc->trace.decode_work_time_name, sizeof(hevc->trace.decode_work_time_name),
		"decoder_work_time%d", pdev->id);

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name,
		VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
		hevc->frameinfo_enable = 1;
	} else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	hevc->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	hevc->platform_dev = pdev;

	if (((get_dbg_flag(hevc) & IGNORE_PARAM_FROM_CONFIG) == 0) &&
			pdata->config_len) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		/*use ptr config for double_write_mode, etc*/
		hevc_print(hevc, 0, "pdata->config=%s\n", pdata->config);

		if (get_config_int(pdata->config, "h266_double_write_mode",
				&config_val) == 0)
			hevc->double_write_mode = config_val;
		else
			hevc->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config, "save_buffer_mode",
				&config_val) == 0)
			hevc->save_buffer_mode = config_val;
		else
			hevc->save_buffer_mode = 0;

		/*use ptr config for max_pic_w, etc*/
		if (get_config_int(pdata->config, "hevc_buf_width",
				&config_val) == 0) {
				hevc->max_pic_w = config_val;
		}
		if (get_config_int(pdata->config, "hevc_buf_height",
				&config_val) == 0) {
				hevc->max_pic_h = config_val;
		}
		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hevc->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hevc->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hevc->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_buffer_margin",
			&config_val) == 0)
			hevc->dynamic_buf_num_margin = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hevc->mem_map_mode = config_val;

		if (get_config_int(pdata->config, "negative_dv",
			&config_val) == 0) {
			hevc->discard_dv_data = config_val;
			if (hevc->discard_dv_data)
			    hevc_print(hevc, 0, "discard dv data\n");
		}

		if (get_config_int(pdata->config, "dv_duallayer",
			&config_val) == 0) {
			hevc->dv_duallayer = config_val;
			hevc_print(hevc, 0, "dv dual layer\n");
		}

		if (get_config_int(pdata->config, "parm_metadata_config_flag",
			&config_val) == 0) {
			hevc->high_bandwidth_flag = config_val & VDEC_CFG_FLAG_HIGH_BANDWIDTH;
			if (hevc->high_bandwidth_flag)
				hevc_print(hevc, 0, "high bandwidth\n");
		}

		if (get_config_int(pdata->config,
			"dv_profile", &config_val) == 0) {
			hevc->dv_profile = config_val;
			hevc_print(hevc, 0, "dv_profile: %d\n", config_val);
		}

		if (get_config_int(pdata->config,
			"parm_enable_fence",
			&config_val) == 0)
			hevc->enable_fence = config_val;

		if (get_config_int(pdata->config,
			"parm_fence_usage",
			&config_val) == 0)
			hevc->fence_usage = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_low_latency_mode",
			&config_val) == 0)
			hevc->low_latency_flag = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0) {
			hevc->metadata_config_flag = config_val;
			hevc->discard_dv_data = hevc->metadata_config_flag & VDEC_CFG_FLAG_DV_NEGATIVE;
			hevc->dv_duallayer = hevc->metadata_config_flag & VDEC_CFG_FLAG_DV_TWOLAYER;
			if (hevc->discard_dv_data)
				hevc_print(hevc, 0, "discard dv data\n");
			if (hevc->dv_duallayer)
				hevc_print(hevc, 0, "dv_duallayer\n");
		}
		if (get_config_int(pdata->config,
			"api_error_policy", &config_val) == 0) {
			if (config_val == 0) {
				hevc->nal_skip_policy = HEVC_ERROR_FRAME_DISPLAY;
			} else if (config_val == 1) {
				hevc->nal_skip_policy = HEVC_ERROR_FRAME_DROP;
			} else {
				hevc->nal_skip_policy = nal_skip_policy;
			}
		} else {
			hevc->nal_skip_policy  = nal_skip_policy;
		}
#endif
	} else {
		if (pdata->sys_info)
			hevc->vh266_amstream_dec_info = *pdata->sys_info;
		else {
			hevc->vh266_amstream_dec_info.width = 0;
			hevc->vh266_amstream_dec_info.height = 0;
			hevc->vh266_amstream_dec_info.rate = 30;
		}
		hevc->double_write_mode = double_write_mode;
		hevc->nal_skip_policy = nal_skip_policy;
	}

	if (nal_skip_policy & 0x80000000)
		hevc->nal_skip_policy = nal_skip_policy & 0x7fffffff;

	memcpy(&vf_tmp_ops, &vh266_vf_provider, sizeof(struct vframe_operations_s));
	if (without_display_mode == 1) {
		vf_tmp_ops.get = NULL;
	}
	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_tmp_ops, pdata);

	if (force_config_fence) {
		hevc->enable_fence = true;
		hevc->fence_usage = (force_config_fence >> 4) & 0xf;
		if (force_config_fence & 0x2)
			hevc->enable_fence = false;
		hevc_print(hevc, 0,
			"enable fence: %d, fence usage: %d\n",
			hevc->enable_fence, hevc->fence_usage);
	}

	if (hevc->save_buffer_mode && dynamic_buf_num_margin > 2)
		hevc->dynamic_buf_num_margin = dynamic_buf_num_margin -2;
	else
		hevc->dynamic_buf_num_margin = dynamic_buf_num_margin;

	hevc->mem_map_mode = mem_map_mode;

	hevc->endian = HEVC_CONFIG_LITTLE_ENDIAN;
	if (is_support_vdec_canvas())
		hevc->endian = HEVC_CONFIG_BIG_ENDIAN;
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A)
		hevc->endian = 0;
	if (endian)
		hevc->endian = endian;

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5) &&
			(hevc->double_write_mode == 3))
		hevc->double_write_mode = 0x1000;

	if (mmu_enable_force) {
		hevc->mmu_enable = 1;
	} else {
		if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL) ||
			(hevc->double_write_mode & 0x10))
			hevc->mmu_enable = 0;
		else
			hevc->mmu_enable = 1;
	}
	if (hevc->double_write_mode & 0x10)
		hevc->mmu_enable = 0;
	else
		hevc->mmu_enable = 1;
#ifdef VVC_10B_MMU_DW
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T5D) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TXHD2)) {
		hevc->dw_mmu_enable =
			get_double_write_mode(hevc) & 0x20 ? 1 : 0;
	} else {
		hevc->dw_mmu_enable = 0;
	}
#endif

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
		hevc->mmu_enable = 0;
		hevc->dw_mmu_enable = 0;
		hevc->double_write_mode = 0x10;
	}

	if (init_mmu_buffers(hevc, 1) < 0) {
		hevc_print(hevc, 0,
			"\n 266 mmu init failed!\n");
		mutex_unlock(&vh266_mutex);
		/* devm_kfree(&pdev->dev, (void *)hevc);*/
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -EFAULT;
	}
#if 0
	hevc->buf_start = pdata->mem_start;
	hevc->buf_size = pdata->mem_end - pdata->mem_start + 1;
#else

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box,
			BMMU_WORKSPACE_ID, work_buf_size,
			DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		mutex_unlock(&vh266_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;
#endif
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7;
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	hevc->uninit_list_done = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;

	/*
	 *hevc->mc_buf_spec.buf_end = pdata->mem_end + 1;
	 *for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
	 *	amvh266_workbuff_spec[i].start_adr = pdata->mem_start;
	 */
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.266 decoder mem resource 0x%lx size 0x%x\n",
			   hevc->buf_start, hevc->buf_size);
	}

	hevc_print(hevc, 0,
		"dynamic_buf_num_margin=%d\n",
		hevc->dynamic_buf_num_margin);
	hevc_print(hevc, 0,
		"double_write_mode=%d\n",
		hevc->double_write_mode);

	hevc->cma_dev = pdata->cma_dev;
	vh266_vdec_info_init(hevc);

	if (vh266_init(pdata) < 0) {
		hevc_print(hevc, 0,
			"\namvdec_h266 init failed.\n");
		hevc_local_uninit(hevc);
		if (hevc->gvs)
			kfree(hevc->gvs);
		hevc->gvs = NULL;
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -ENODEV;
	}

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_init(pdata);
#endif

#ifdef H266_USERDATA_ENABLE
	vh266_crate_userdata_manager(hevc, hevc->sei_user_data_buffer, USER_DATA_SIZE);
#endif

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_H266,
			3840, 2160, 60);
	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_HEVC);
	else
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);

	mutex_init(&hevc->fence_mutex);
	if (hevc->enable_fence) {
		pdata->sync = vdec_sync_get();
		if (!pdata->sync) {
			hevc_print(hevc, 0, "alloc fence timeline error\n");
			hevc_local_uninit(hevc);
			if (hevc->gvs)
				kfree(hevc->gvs);
			hevc->gvs = NULL;
			uninit_mmu_buffers(hevc);
			/* devm_kfree(&pdev->dev, (void *)hevc); */
			if (hevc)
				vfree((void *)hevc);
			pdata->dec_status = NULL;
			return -ENODEV;
		}
		pdata->sync->usage = hevc->fence_usage;
		/* creat timeline. */
		vdec_timeline_create(pdata->sync, DRIVER_NAME);
	}

	return 0;
}

static void vdec_fence_release(struct hevc_state_s *hw,
			       struct vdec_sync *sync)
{
	ulong expires;

	/* notify signal to wake up all fences. */
	vdec_timeline_increase(sync, VF_POOL_SIZE);

	expires = jiffies + msecs_to_jiffies(2000);
	while (!check_objs_all_signaled(sync)) {
		if (time_after(jiffies, expires)) {
			pr_err("wait fence signaled timeout.\n");
			break;
		}
	}

	/* decreases refcnt of timeline. */
	vdec_timeline_put(sync);
}

static int ammvdec_h266_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec;

	if (hevc == NULL)
		return 0;
	vdec = hw_to_vdec(hevc);

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_exit(vdec);
#endif

	//pr_err("%s [pid=%d,tgid=%d]\n", __func__, current->pid, current->tgid);
	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	vmh266_stop(hevc);
#ifdef H266_USERDATA_ENABLE
	vh266_destroy_userdata_manager(hevc);
#endif

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */
	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);
	else
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(hevc), VDEC_STATUS_DISCONNECTED);

	if (hevc->enable_fence)
		vdec_fence_release(hevc, vdec->sync);
	if (is_rdma_enable())
		decoder_dma_free_coherent(hevc->rdma_mem_handle,
			RDMA_SIZE, hevc->rdma_adr, hevc->rdma_phy_adr);
	vfree((void *)hevc);

	return 0;
}

static struct platform_driver ammvdec_h266_driver = {
	.probe = ammvdec_h266_probe,
	.remove = ammvdec_h266_remove,
	.driver = {
		.name = MULTI_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &h266_pm_ops,
#endif
	}
};
#endif

static struct codec_profile_t amvdec_h266_profile = {
	.name = "H266-V4L",
	.profile = ""
};

static struct codec_profile_t amvdec_h266_profile_single,
		amvdec_h266_profile_mult;

static struct mconfig h266_configs[] = {
	MC_PU32("use_cma", &use_cma),
	MC_PU32("bit_depth_luma", &bit_depth_luma),
	MC_PU32("bit_depth_chroma", &bit_depth_chroma),
	MC_PU32("video_signal_type", &video_signal_type),
#ifdef ERROR_HANDLE_DEBUG
	MC_PU32("dbg_nal_skip_flag", &dbg_nal_skip_flag),
	MC_PU32("dbg_nal_skip_count", &dbg_nal_skip_count),
#endif
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("dbg_cmd", &dbg_cmd),
	MC_PU32("dbg_skip_decode_index", &dbg_skip_decode_index),
	MC_PU32("endian", &endian),
	MC_PU32("step", &step),
	MC_PU32("udebug_flag", &udebug_flag),
	MC_PU32("decode_pic_begin", &decode_pic_begin),
	MC_PU32("slice_parse_begin", &slice_parse_begin),
	MC_PU32("nal_skip_policy", &nal_skip_policy),
	MC_PU32("i_only_flag", &i_only_flag),
	MC_PU32("error_handle_policy", &error_handle_policy),
	MC_PU32("error_handle_threshold", &error_handle_threshold),
	MC_PU32("error_handle_nal_skip_threshold",
		&error_handle_nal_skip_threshold),
	MC_PU32("error_handle_system_threshold",
		&error_handle_system_threshold),
	MC_PU32("error_skip_nal_count", &error_skip_nal_count),
	MC_PU32("debug", &debug),
	MC_PU32("debug_mask", &debug_mask),
	MC_PU32("buffer_mode", &buffer_mode),
	MC_PU32("double_write_mode", &double_write_mode),
	MC_PU32("buf_alloc_width", &buf_alloc_width),
	MC_PU32("buf_alloc_height", &buf_alloc_height),
	MC_PU32("dynamic_buf_num_margin", &dynamic_buf_num_margin),
	MC_PU32("max_buf_num", &max_buf_num),
	MC_PU32("buf_alloc_size", &buf_alloc_size),
	MC_PU32("buffer_mode_dbg", &buffer_mode_dbg),
	MC_PU32("mem_map_mode", &mem_map_mode),
	MC_PU32("enable_mem_saving", &enable_mem_saving),
	MC_PU32("force_w_h", &force_w_h),
	MC_PU32("force_fps", &force_fps),
	MC_PU32("max_decoding_time", &max_decoding_time),
	MC_PU32("prefix_aux_buf_size", &prefix_aux_buf_size),
	MC_PU32("suffix_aux_buf_size", &suffix_aux_buf_size),
	MC_PU32("interlace_enable", &interlace_enable),
	MC_PU32("pts_unstable", &pts_unstable),
	MC_PU32("parser_sei_enable", &parser_sei_enable),
	MC_PU32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
	MC_PU32("parser_dolby_vision_enable", &parser_dolby_vision_enable),
};

static struct mconfig_node decoder_266_node;

static int __init amvdec_h266_driver_init_module(void)
{
	struct BuffInfo_s *p_buf_info;

	if (hevc_is_support_4k()) {
		if (0)
			p_buf_info = &amvh266_workbuff_spec[2]; //4k
		else
			p_buf_info = &amvh266_workbuff_spec[1]; //8k
	} else
		p_buf_info = &amvh266_workbuff_spec[0]; //1080p

	init_buff_spec(NULL, p_buf_info);
	work_buf_size =
		(p_buf_info->end_adr - p_buf_info->start_adr
		 + 0xffff) & (~0xffff);

	if (debug & PRINT_FLAG_VDEC_DETAIL)
		pr_info("amvdec_h266_v4l module init\n");

	error_handle_policy = 0;

#ifdef ERROR_HANDLE_DEBUG
	dbg_nal_skip_flag = 0;
	dbg_nal_skip_count = 0;
#endif
	udebug_flag = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

#ifdef MULTI_INSTANCE_SUPPORT
	if (platform_driver_register(&ammvdec_h266_driver))
		pr_err("failed to register ammvdec_h266 driver\n");

#endif
	if (platform_driver_register(&amvdec_h266_driver)) {
		pr_err("failed to register amvdec_h266 driver\n");
		return -ENODEV;
	}

#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8*/
	if (!has_hevc_vdec()) {
		/* not support hevc */
		amvdec_h266_profile.name = "hevc_unsupport";
	}
	if (hevc_is_support_4k()) {
		if (is_meson_m8m2_cpu()) {
			/* m8m2 support 4k */
			amvdec_h266_profile.profile = "4k";
		} else if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
					(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5M) &&
					(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TXHD2)) {
			amvdec_h266_profile.profile =
				"8k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, v4l-uvm, multi_frame_dv";
		}else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB) {
			amvdec_h266_profile.profile =
				"4k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, v4l-uvm, multi_frame_dv";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_MG9TV)
			amvdec_h266_profile.profile = "4k";
	} else {
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D || is_cpu_s4_s805x2()) {
				amvdec_h266_profile.profile =
					"8bit, 10bit, dwrite, compressed, frame_dv, v4l, multi_frame_dv";
		} else if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
				amvdec_h266_profile.profile = "8bit, 10bit";
		} else {
				amvdec_h266_profile.profile =
					"8bit, 10bit, dwrite, compressed, v4l";
		}
	}
#endif
	if ((codec_mm_get_total_size() < 80 * SZ_1M) &&
		(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S1A)) {
		pr_info("amvdec_h266 default mmu enabled.\n");
		mmu_enable = 1;
	}

	vcodec_profile_register(&amvdec_h266_profile);
	amvdec_h266_profile_single = amvdec_h266_profile;
	amvdec_h266_profile_single.name = "h266";
	vcodec_profile_register(&amvdec_h266_profile_single);
	amvdec_h266_profile_mult = amvdec_h266_profile;
	amvdec_h266_profile_mult.name = "mh266";
	vcodec_profile_register(&amvdec_h266_profile_mult);
	INIT_REG_NODE_CONFIGS("media.decoder", &decoder_266_node,
		"h266-v4l", h266_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_H266, 1);

	return 0;
}

static void __exit amvdec_h266_driver_remove_module(void)
{
	pr_debug("amvdec_h266_v4l module remove.\n");

#ifdef MULTI_INSTANCE_SUPPORT
	platform_driver_unregister(&ammvdec_h266_driver);
#endif
	platform_driver_unregister(&amvdec_h266_driver);
}

/****************************************/
/*
 *module_param(stat, uint, 0664);
 *MODULE_PARM_DESC(stat, "\n amvdec_h266 stat\n");
 */
module_param(use_cma, uint, 0664);
MODULE_PARM_DESC(use_cma, "\n amvdec_h266 use_cma\n");

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_h266 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_h266 bit_depth_chroma\n");

module_param(video_signal_type, uint, 0664);
MODULE_PARM_DESC(video_signal_type, "\n amvdec_h266 video_signal_type\n");

#ifdef ERROR_HANDLE_DEBUG
module_param(dbg_nal_skip_flag, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_flag, "\n amvdec_h266 dbg_nal_skip_flag\n");

module_param(dbg_nal_skip_count, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_count, "\n amvdec_h266 dbg_nal_skip_count\n");
#endif

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\n radr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\n rval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\n dbg_cmd\n");

module_param(dump_nal, uint, 0664);
MODULE_PARM_DESC(dump_nal, "\n dump_nal\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\n dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\n rval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_h266 step\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_h266 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_h266 slice_parse_begin\n");

module_param(nal_skip_policy, uint, 0664);
MODULE_PARM_DESC(nal_skip_policy, "\n amvdec_h266 nal_skip_policy\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_h266 i_only_flag\n");

module_param(fast_output_enable, uint, 0664);
MODULE_PARM_DESC(fast_output_enable, "\n amvdec_h266 fast_output_enable\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_h266 error_handle_policy\n");

module_param(error_handle_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_threshold,
		"\n amvdec_h266 error_handle_threshold\n");

module_param(error_handle_nal_skip_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_nal_skip_threshold,
		"\n amvdec_h266 error_handle_nal_skip_threshold\n");

module_param(error_handle_system_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_system_threshold,
		"\n amvdec_h266 error_handle_system_threshold\n");

module_param(error_skip_nal_count, uint, 0664);
MODULE_PARM_DESC(error_skip_nal_count,
				 "\n amvdec_h266 error_skip_nal_count\n");

module_param(skip_nal_count, uint, 0664);
MODULE_PARM_DESC(skip_nal_count, "\n skip_nal_count\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_h266 debug\n");

module_param(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n amvdec_h266 debug mask\n");

module_param(log_mask, uint, 0664);
MODULE_PARM_DESC(log_mask, "\n amvdec_h266 log_mask\n");

module_param(buffer_mode, uint, 0664);
MODULE_PARM_DESC(buffer_mode, "\n buffer_mode\n");

module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(buf_alloc_width, uint, 0664);
MODULE_PARM_DESC(buf_alloc_width, "\n buf_alloc_width\n");

module_param(buf_alloc_height, uint, 0664);
MODULE_PARM_DESC(buf_alloc_height, "\n buf_alloc_height\n");

module_param(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n dynamic_buf_num_margin\n");

module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "\n max_buf_num\n");

module_param(buf_alloc_size, uint, 0664);
MODULE_PARM_DESC(buf_alloc_size, "\n buf_alloc_size\n");

#ifdef CONSTRAIN_MAX_BUF_NUM
module_param(run_ready_max_vf_only_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_vf_only_num, "\n run_ready_max_vf_only_num\n");

module_param(run_ready_display_q_num, uint, 0664);
MODULE_PARM_DESC(run_ready_display_q_num, "\n run_ready_display_q_num\n");

module_param(run_ready_max_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_buf_num, "\n run_ready_max_buf_num\n");
#endif

#if 0
module_param(re_config_pic_flag, uint, 0664);
MODULE_PARM_DESC(re_config_pic_flag, "\n re_config_pic_flag\n");
#endif

module_param(buffer_mode_dbg, uint, 0664);
MODULE_PARM_DESC(buffer_mode_dbg, "\n buffer_mode_dbg\n");

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

module_param(enable_mem_saving, uint, 0664);
MODULE_PARM_DESC(enable_mem_saving, "\n enable_mem_saving\n");

#ifndef MULTI_INSTANCE_SUPPORT
module_param(workaround_enable, uint, 0664);
MODULE_PARM_DESC(workaround_enable, "\n force_w_h\n");
#endif

module_param(force_w_h, uint, 0664);
MODULE_PARM_DESC(force_w_h, "\n force_w_h\n");

module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

module_param(max_decoding_time, uint, 0664);
MODULE_PARM_DESC(max_decoding_time, "\n max_decoding_time\n");

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

module_param(interlace_enable, uint, 0664);
MODULE_PARM_DESC(interlace_enable, "\n interlace_enable\n");
module_param(pts_unstable, uint, 0664);
MODULE_PARM_DESC(pts_unstable, "\n amvdec_h266 pts_unstable\n");
module_param(parser_sei_enable, uint, 0664);
MODULE_PARM_DESC(parser_sei_enable, "\n parser_sei_enable\n");

module_param(parser_dolby_vision_enable, uint, 0664);
MODULE_PARM_DESC(parser_dolby_vision_enable,
	"\n parser_dolby_vision_enable\n");

module_param(mmu_enable, uint, 0664);
MODULE_PARM_DESC(mmu_enable, "\n mmu_enable\n");

module_param(mmu_enable_force, uint, 0664);
MODULE_PARM_DESC(mmu_enable_force, "\n mmu_enable_force\n");

#ifdef MULTI_INSTANCE_SUPPORT
module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n h266 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n h266 decode_timeout_val\n");

module_param(print_lcu_error, uint, 0664);
MODULE_PARM_DESC(print_lcu_error,
	"\n h266 print_lcu_error\n");

module_param(data_resend_policy, uint, 0664);
MODULE_PARM_DESC(data_resend_policy,
	"\n h266 data_resend_policy\n");

module_param(poc_num_margin, int, 0664);
MODULE_PARM_DESC(poc_num_margin,
	"\n h266 poc_num_margin\n");

module_param(poc_error_limit, int, 0664);
MODULE_PARM_DESC(poc_error_limit,
	"\n h266 poc_error_limit\n");

module_param_array(decode_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(display_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_process_time, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval,
	uint, &max_decode_instance_num, 0664);

module_param_array(run_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(input_empty, uint,
	&max_decode_instance_num, 0664);

module_param_array(not_run_ready, uint,
	&max_decode_instance_num, 0664);

module_param_array(ref_frame_mark_flag, uint,
	&max_decode_instance_num, 0664);

#endif

module_param(itu_t_t35_enable, int, 0664);
MODULE_PARM_DESC(itu_t_t35_enable, "\n amvdec_h266 itu_t_t35_enable\n");

#ifdef AGAIN_HAS_THRESHOLD
module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");
#endif

module_param(force_disp_pic_index, int, 0664);
MODULE_PARM_DESC(force_disp_pic_index,
	"\n amvdec_h266 force_disp_pic_index\n");

module_param(frmbase_cont_bitlevel, uint, 0664);
MODULE_PARM_DESC(frmbase_cont_bitlevel,	"\n frmbase_cont_bitlevel\n");

module_param(force_bufspec, uint, 0664);
MODULE_PARM_DESC(force_bufspec, "\n amvdec_h266 force_bufspec\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h266 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(enable_swap, uint, 0664);
MODULE_PARM_DESC(enable_swap, "\n enable_swap\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level, "\n ammvdec_h264 pre_decode_buf_level\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(disp_vframe_valve_level, uint, 0664);
MODULE_PARM_DESC(disp_vframe_valve_level, "\n disp_vframe_valve_level\n");

module_param(pic_list_debug, uint, 0664);
MODULE_PARM_DESC(pic_list_debug, "\n pic_list_debug\n");

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n amvdec_h266 without_display_mode\n");

#ifdef HEVC_8K_LFTOFFSET_FIX
module_param(performance_profile, uint, 0664);
MODULE_PARM_DESC(performance_profile, "\n amvdec_h266 performance_profile\n");
#endif
module_param(disable_ip_mode, uint, 0664);
MODULE_PARM_DESC(disable_ip_mode, "\n amvdec_h266 disable ip_mode\n");

module_param(dirty_again_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_again_threshold, "\n dirty_again_threshold\n");

module_param(dirty_buffersize_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_buffersize_threshold, "\n dirty_buffersize_threshold\n");

module_param(force_config_fence, uint, 0664);
MODULE_PARM_DESC(force_config_fence, "\n force enable fence\n");

module_param(mv_buf_dynamic_alloc, uint, 0664);
MODULE_PARM_DESC(mv_buf_dynamic_alloc, "\n mv_buf_dynamic_alloc\n");

module_param(detect_stuck_buffer_margin, uint, 0664);
MODULE_PARM_DESC(detect_stuck_buffer_margin, "\n detect_stuck_buffer_margin\n");

module_param(frmbase_muti_slice, uint, 0664);
MODULE_PARM_DESC(frmbase_muti_slice,	"\n amvdec_h266 frmbase_muti_slice\n");

module_init(amvdec_h266_driver_init_module);
module_exit(amvdec_h266_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC h266 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
