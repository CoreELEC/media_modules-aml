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
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>

#include <uapi/linux/tee.h>
#include <media/v4l2-mem2mem.h>

#include "../../../stream_input/amports/amports_priv.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../decoder/utils/vdec_input.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/vdec_feature.h"
#include "../../decoder/utils/aml_buf_helper.h"
#include "../../decoder/utils/vdec_profile.h"

#include "../../decoder/utils/vdec_ge2d_utils.h"
#include "../../decoder/utils/scatter_input.h"
#define MEM_NAME "codec_mmjpeg"

#define DRIVER_NAME "ammvdec_mjpeg_v4l"
#define CHECK_INTERVAL        (HZ/100)

/* protocol register usage
 *    AV_SCRATCH_4 : decode buffer spec
 *    AV_SCRATCH_5 : decode buffer index
 */

#define MREG_DECODE_PARAM   AV_SCRATCH_2	/* bit 0-3: pico_addr_mode */
/* bit 15-4: reference height */
#define MREG_TO_AMRISC      AV_SCRATCH_8
#define MREG_FROM_AMRISC    AV_SCRATCH_9
#define MREG_FRAME_OFFSET   AV_SCRATCH_A
#define DEC_STATUS_REG      AV_SCRATCH_F
#define MREG_PIC_WIDTH      AV_SCRATCH_B
#define MREG_PIC_HEIGHT     AV_SCRATCH_C
#define DECODE_STOP_POS     AV_SCRATCH_K
#define PSCALE_CANADDR_TMP  AV_SCRATCH_N

#define PICINFO_BUF_IDX_MASK        0x0007
#define PICINFO_AVI1                0x0080
#define PICINFO_INTERLACE           0x0020
#define PICINFO_INTERLACE_AVI1_BOT  0x0010
#define PICINFO_INTERLACE_FIRST     0x0010

#define VF_POOL_SIZE          64
#define DECODE_BUFFER_NUM_MAX		16
#define DECODE_BUFFER_NUM_DEF		1
#define MAX_BMMU_BUFFER_NUM		DECODE_BUFFER_NUM_MAX

#define DEFAULT_MEM_SIZE	(32*SZ_1M)
#define RP_WORKAROUND_SIZE  SZ_4K

#define INVALID_IDX 		(-1)  /* Invalid buffer index.*/

static int debug_enable;
static u32 udebug_flag;
#define DECODE_ID(hw) (hw_to_vdec(hw)->id)

static unsigned int radr;
static unsigned int rval;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
static unsigned int max_decode_instance_num = MAX_INSTANCE_MUN;
#endif
static unsigned int max_process_time[MAX_INSTANCE_MUN];
static unsigned int decode_timeout_val = 200;
static struct vframe_s *vmjpeg_vf_peek(void *);
static struct vframe_s *vmjpeg_vf_get(void *);
static void vmjpeg_vf_put(struct vframe_s *, void *);
static int vmjpeg_vf_states(struct vframe_states *states, void *);
static int vmjpeg_event_cb(int type, void *data, void *private_data);
static void vmjpeg_work(struct work_struct *work);
static int notify_v4l_eos(struct vdec_s *vdec);
static int pre_decode_buf_level = 0x800;
static int start_decode_buf_level = 0x2000;
static u32 without_display_mode;
static u32 dynamic_buf_num_margin = 6;
static u32 run_ready_min_buf_num = 1;
static u32 out_height;
static u32 out_width;
static u32 enable_jpeg;
static u32 pscale_h = 0;
static u32 pscale_w = 0;
static u32 org_pic_type_test = 0;
static u32 buf_number = 2;

#define MAX_SCALE 16

#undef pr_info
#define pr_info pr_cont
unsigned int mmjpeg_debug_mask = 0xff;
#define PRINT_FLAG_ERROR              0x0
#define PRINT_FLAG_RUN_FLOW           0X0001
#define PRINT_FLAG_TIMEINFO           0x0002
#define PRINT_FLAG_UCODE_DETAIL		  0x0004
#define PRINT_FLAG_VLD_DETAIL         0x0008
#define PRINT_FLAG_DEC_DETAIL         0x0010
#define PRINT_FLAG_BUFFER_DETAIL      0x0020
#define PRINT_FLAG_RESTORE            0x0040
#define PRINT_FRAME_NUM               0x0080
#define PRINT_FLAG_FORCE_DONE         0x0100
#define PRINT_FRAMEBASE_DATA          0x0400
#define PRINT_FLAG_TIMEOUT_STATUS     0x1000
#define PRINT_FLAG_V4L_DETAIL         0x8000
#define IGNORE_PARAM_FROM_CONFIG      0x8000000

int mmjpeg_debug_print(int index, int debug_flag, const char *fmt, ...)
{
	if (((debug_enable & debug_flag) &&
		((1 << index) & mmjpeg_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
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

static const char vmjpeg_dec_id[] = "vmmjpeg-dev";

#define PROVIDER_NAME   "vdec.mjpeg"
static const struct vframe_operations_s vf_provider_ops = {
	.peek = vmjpeg_vf_peek,
	.get = vmjpeg_vf_get,
	.put = vmjpeg_vf_put,
	.event_cb = vmjpeg_event_cb,
	.vf_states = vmjpeg_vf_states,
};

#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_ERROR            3
#define DEC_RESULT_FORCE_EXIT       4
#define DEC_RESULT_EOS              5
#define DEC_DECODE_TIMEOUT         0x21

/*Send by DEC_STATUS_REG*/
#define MJPEG_CONFIG_REQUEST   1
#define MJPEG_DATA_EMPTY      2

struct buffer_spec_s {
	dos_addr_t y_addr;
	dos_addr_t u_addr;
	dos_addr_t v_addr;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;

	struct canvas_config_s canvas_config[3];
	dos_addr_t cma_alloc_addr;
	int cma_alloc_count;
	dos_addr_t buf_adr;
	ulong v4l_ref_buf_addr;
};

#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

struct vdec_mjpeg_hw_s {
	spinlock_t lock;
	struct mutex vmjpeg_mutex;

	struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);

	struct vframe_s vfpool[VF_POOL_SIZE];
	struct vframe_s vframe_dummy;

	struct buffer_spec_s buffer_spec[DECODE_BUFFER_NUM_MAX];
	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];

	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 saved_resolution;
	u8 init_flag;
	u32 stat;
	u32 dec_result;
	unsigned long buf_start;
	u32 buf_size;
	void *mm_blk_handle;
	struct dec_sysinfo vmjpeg_amstream_dec_info;

	struct vframe_chunk_s *chunk;
	struct work_struct work;
	void (*vdec_cb)(struct vdec_s *, void *, int);
	void *vdec_cb_arg;
	struct firmware_s *fw;
	struct timer_list check_timer;
	u32 decode_timeout_count;
	ulong start_process_time;
	u32 last_vld_level;
	bool eos;
	u32 frame_num;
	u32 run_count;
	u32	not_run_ready;
	u32 buffer_not_ready;
	u32	input_empty;
	atomic_t peek_num;
	atomic_t get_num;
	atomic_t put_num;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	int buf_num;
	int dynamic_buf_num_margin;
	int sidebind_type;
	int sidebind_channel_id;
	u32 res_ch_flag;
	u32 canvas_mode;
	u32 canvas_endian;
	struct aml_buf *aml_buf;
	char vdec_name[32];
	char pts_name[32];
	char new_q_name[32];
	char disp_q_name[32];
	int force_recycle;
	bool run_flag;
	s32 cur_idx;
	int v4l_duration;
	int vdec_pg_enable_flag;
	ulong decbuf_start;
	u32 out_width;
	u32 out_height;
	bool jpeg_flag;
	struct vdec_ge2d *ge2d;
	struct buffer_spec_s jpeg_buffer;
	int hv_subsample;
	u32 last_dur;
	u32 metadata_config_flag;
};

static void reset_process_time(struct vdec_mjpeg_hw_s *hw);
static int mjpeg_recycle_frame_buffer(struct vdec_mjpeg_hw_s *hw,
							int force_recycle);
static int mjpeg_reset_frame_buffer(struct vdec_mjpeg_hw_s *hw);

static u32 mjpeg_get_endian(struct vdec_mjpeg_hw_s *hw)
{
	u32 endian;

	/* mjpeg convert endian to match display. */
	if ((is_cpu_t7()) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5W) ||
		is_mjpeg_endian_rematch()) {
		endian = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	} else {
		endian = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 0 : 7;
	}

	return endian;
}

static void v4l_mjpeg_collect_stream_info(struct vdec_s *vdec,
	struct vdec_mjpeg_hw_s *hw, struct aml_vdec_ps_infos *ps)
{
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;
	struct dec_stream_info_s *str_info = NULL;
	int hv_subsample = READ_VREG(AV_SCRATCH_M)  & 0xff;

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
	str_info->profile_idc = hv_subsample;
	str_info->level_idc = 0;
	str_info->filed_flag = 0;
	if (ps != NULL) {
		str_info->frame_width= ps->visible_width;
		str_info->frame_height = ps->visible_height;
		str_info->dpb_num = ps->dpb_frames;
		str_info->margin_num = ps->dpb_margin;
	}
	str_info->crop_top = 0;
	str_info->crop_bottom = 0;
	str_info->crop_left= 0;
	str_info->crop_right = 0;
	str_info->double_write_mode = 0;
	str_info->error_handle_policy = 0;
	str_info->bit_depth = 8;

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

static void set_frame_info(struct vdec_mjpeg_hw_s *hw, struct vframe_s *vf)
{
	u32 width, height;
	u32 temp_endian;
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int vf_dur = ctx->duration;

	width = READ_VREG(MREG_PIC_WIDTH);
	height = READ_VREG(MREG_PIC_HEIGHT);
	vf->width = hw->frame_width = ((width > 4096) ? 4096 : width);
	vf->height = hw->frame_height = ((height > 2304) ? 2304 : height);

	if (width < height) {
		vf->width = hw->frame_width = ((width > 2304) ? 2304 : width);
		vf->height = hw->frame_height = ((height > 4096) ? 4096 : height);
	}

	if (hw->jpeg_flag) {
		vf->width = hw->out_width;
		vf->height = hw->out_height;
	}

	if (hw->last_dur != hw->frame_dur) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
			"decoder duration change old: %d new: %d\n", hw->last_dur, hw->frame_dur);
		hw->last_dur = hw->frame_dur;
		v4l_mjpeg_collect_stream_info(hw_to_vdec(hw), hw, NULL);
	}

	vf->duration = vf_dur ? vf_dur : hw->frame_dur;
	vf->ratio_control = DISP_RATIO_ASPECT_RATIO_MAX << DISP_RATIO_ASPECT_RATIO_BIT;
	vf->sar_width = 1;
	vf->sar_height = 1;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 3;

	if (hw->jpeg_flag) {
		vf->canvas0_config[0] = hw->jpeg_buffer.canvas_config[0];
		vf->canvas0_config[1] = hw->jpeg_buffer.canvas_config[1];
		vf->canvas0_config[2] = hw->jpeg_buffer.canvas_config[2];

		vf->canvas1_config[0] = hw->jpeg_buffer.canvas_config[0];
		vf->canvas1_config[1] = hw->jpeg_buffer.canvas_config[1];
		vf->canvas1_config[2] = hw->jpeg_buffer.canvas_config[2];
	} else {
		vf->canvas0_config[0] = hw->buffer_spec[vf->index].canvas_config[0];
		vf->canvas0_config[1] = hw->buffer_spec[vf->index].canvas_config[1];
		vf->canvas0_config[2] = hw->buffer_spec[vf->index].canvas_config[2];

		vf->canvas1_config[0] = hw->buffer_spec[vf->index].canvas_config[0];
		vf->canvas1_config[1] = hw->buffer_spec[vf->index].canvas_config[1];
		vf->canvas1_config[2] = hw->buffer_spec[vf->index].canvas_config[2];
	}

	temp_endian = mjpeg_get_endian(hw);
	vf->canvas0_config[0].endian = temp_endian;
	vf->canvas0_config[1].endian = temp_endian;
	vf->canvas0_config[2].endian = temp_endian;
	vf->canvas1_config[0].endian = temp_endian;
	vf->canvas1_config[1].endian = temp_endian;
	vf->canvas1_config[2].endian = temp_endian;

	vf->sidebind_type = hw->sidebind_type;
	vf->sidebind_channel_id = hw->sidebind_channel_id;
	if (hw->jpeg_flag)
		vf->codec_vfmt = VFORMAT_JPEG;
	else
		vf->codec_vfmt = VFORMAT_MJPEG;
}

static irqreturn_t vmjpeg_isr(struct vdec_s *vdec, int irq)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)(vdec->private);

	if (!hw)
		return IRQ_HANDLED;

	if (hw->eos)
		return IRQ_HANDLED;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
}

static int vmjpeg_get_ps_info(struct vdec_mjpeg_hw_s *hw, int width, int height, int hv_subsample, struct aml_vdec_ps_infos *ps)
{
	u32 display_width = 1920;
	u32 display_height = 1080;

	if (vdec_is_support_4k()) {
		display_width = 3840;
		display_height = 2160;
	}

	if (hw->jpeg_flag) {
		if ((hw->out_width == 0) || (hw->out_height == 0)) {
			int rate = 1;
			int i = 0;
			int j = 0;

			if ((width > display_width) || (height > display_height)) {
				for (i = 2; i < MAX_SCALE; i++) {
					if ((width / i) <= display_width)
						break;
				}

				for (j = 2; j < MAX_SCALE; j++) {
					if ((height / j) <= display_height)
						break;
				}

				rate = max(i, j);
			}

			hw->out_width = width / rate;
			hw->out_height = height / rate;

			if (hw->out_width & 1)
				hw->out_width -= 1;
			if (hw->out_height & 1)
				hw->out_height -= 1;
		}

		hw->frame_height = height;
		hw->frame_width = width;
	}

	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"out_width %d out_height %d, width %d height %d\n",
		hw->out_width, hw->out_height, width, height);

	if (hw->jpeg_flag) {
		ps->visible_width	= hw->out_width;
		ps->visible_height	= hw->out_height;
		ps->coded_width		= ALIGN(hw->out_width, 64);
		ps->coded_height 	= ALIGN(hw->out_height, 64);
		ps->dpb_size 		= 0;
		ps->dpb_frames		= buf_number;
		ps->dpb_margin		= 0;
	} else {
		/* decode mjpeg will get I420 (3 planes),
		 * ge2d requires width 64 align on old chips, such as TXHD2;
		 * ge2d requires width 32 align on new chips, such as T6W
		 */
		ps->visible_width	= width;
		ps->visible_height	= height;
		ps->coded_width 	= ALIGN(width, 64);
		ps->coded_height	= ALIGN(height, 64);
		ps->dpb_size 		= hw->buf_num;
		ps->dpb_frames		= DECODE_BUFFER_NUM_DEF;
		ps->dpb_margin		= hw->dynamic_buf_num_margin;
	}

	ps->dpb_size 		= hw->buf_num;
	ps->field		= V4L2_FIELD_NONE;
	ps->profile		= hv_subsample;
	hw->hv_subsample = hv_subsample;

	return 0;
}

static int v4l_res_change(struct vdec_mjpeg_hw_s *hw, int width, int height, int hv_subsample)
{
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int ret = 0;

	if (ctx->param_sets_from_ucode &&
		hw->res_ch_flag == 0) {
		struct aml_vdec_ps_infos ps;

		if ((hw->frame_width != 0 &&
			hw->frame_height != 0) &&
			(hw->frame_width != width ||
			hw->frame_height != height) &&
			!hw->jpeg_flag) {
			mmjpeg_debug_print(DECODE_ID(hw), 0,
				"v4l_res_change Pic Width/Height Change (%d,%d)=>(%d,%d)\n",
				hw->frame_width, hw->frame_height,
				width,
				height);
			vmjpeg_get_ps_info(hw, width, height, hv_subsample, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hw->v4l_params_parsed = false;
			hw->res_ch_flag = 1;
			ctx->v4l_resolution_change = 1;
			vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_4, __LINE__);
			notify_v4l_eos(hw_to_vdec(hw));
			vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_4, 0);

			ret = 1;
		}
	}

	return ret;
}

static irqreturn_t vmjpeg_isr_thread_fn(struct vdec_s *vdec, int irq)
{
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)(vdec->private);
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	struct aml_buf *aml_buf = NULL;
	u32 reg;
	struct vframe_s *vf = NULL;
	u32 index, offset = 0, pts;
	u64 pts_us64;
	u32 frame_size;
	unsigned int dec_status = READ_VREG(DEC_STATUS_REG);

	if (READ_VREG(AV_SCRATCH_D) != 0 &&
		(debug_enable & PRINT_FLAG_UCODE_DETAIL)) {
		pr_info("dbg%x: %x\n", READ_VREG(AV_SCRATCH_D),
		READ_VREG(AV_SCRATCH_E));
		WRITE_VREG(AV_SCRATCH_D, 0);
		return IRQ_HANDLED;
	}

	if (dec_status == MJPEG_CONFIG_REQUEST) {
		int frame_width = READ_VREG(MREG_PIC_WIDTH);
		int frame_height = READ_VREG(MREG_PIC_HEIGHT);
		int hv_subsample = READ_VREG(AV_SCRATCH_M)  & 0xff;

		if (frame_width & 1)
			frame_width -= 1;
		if (frame_height & 1)
			frame_height -= 1;

		if (hw->metadata_config_flag & THUMBNAIL_MODE_FLAG) {
			int dec_res_available = -1;
			dec_res_available = check_dec_res_available(vdec, hw->chunk->size, frame_width, frame_height);
			if (dec_res_available != true) {
				mmjpeg_debug_print(DECODE_ID(hw), 0, "i_oversize, post unsupport\n");
				vdec_v4l_post_error_event(v4l2_ctx, DECODER_EMERGENCY_UNSUPPORT);
				hw->dec_result = DEC_RESULT_DONE;
				vdec_schedule_work(&hw->work);
				return IRQ_HANDLED;
			}
		}

		if (!v4l_res_change(hw, frame_width, frame_height, hv_subsample)) {
			struct aml_vcodec_ctx *ctx =
				(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
			if (ctx->param_sets_from_ucode && !hw->v4l_params_parsed) {
				struct aml_vdec_ps_infos ps;

				vmjpeg_get_ps_info(hw, frame_width, frame_height, hv_subsample, &ps);
				hw->v4l_params_parsed = true;
				vdec_v4l_set_ps_infos(ctx, &ps);
				ctx->decoder_status_info.frame_height = ps.visible_height;
				ctx->decoder_status_info.frame_width = ps.visible_width;
				v4l_mjpeg_collect_stream_info(vdec, hw, &ps);
				reset_process_time(hw);
				hw->dec_result = DEC_RESULT_AGAIN;
				vdec_schedule_work(&hw->work);
			} else {
				struct vdec_pic_info pic = { 0 };

				if (!hw->buf_num) {
					vdec_v4l_get_pic_info(ctx, &pic);
					hw->buf_num = pic.dpb_frames +
						pic.dpb_margin;
					if (hw->buf_num > DECODE_BUFFER_NUM_MAX)
						hw->buf_num = DECODE_BUFFER_NUM_MAX;
				}

				WRITE_VREG(DEC_STATUS_REG, 0);

				hw->res_ch_flag = 1;
			}
		} else {
			reset_process_time(hw);
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
		}
		return IRQ_HANDLED;
	} else if (dec_status == MJPEG_DATA_EMPTY) {
		/*timeout when decoding next frame*/
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
			"%s: Insufficient data, lvl=%x ctrl=%x bcnt=%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_CONTROL),
			READ_VREG(VIFF_BIT_CNT));

		if (vdec_frame_based(vdec)) {
			struct aml_vcodec_ctx *ctx =
				(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

			hw->vfbuf_use[hw->cur_idx]++;
			vdec_v4l_post_error_frame_event(ctx);
			vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
			mjpeg_reset_frame_buffer(hw);
			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
		} else {
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
			reset_process_time(hw);
		}
		return IRQ_HANDLED;
	}
	reset_process_time(hw);

	reg = READ_VREG(MREG_FROM_AMRISC);
	index = READ_VREG(AV_SCRATCH_5) & 0xffffff;

	if (index >= hw->buf_num) {
		pr_err("fatal error, invalid buffer index.");
		return IRQ_HANDLED;
	}

	if (kfifo_get(&hw->newframe_q, &vf) == 0) {
		pr_info(
		"fatal error, no available buffer slot.");
		return IRQ_HANDLED;
	}

	vdec_profile(vdec, VDEC_PROFILE_DECODED_FRAME, CORE_MASK_VDEC_1);
	vdec_profile(vdec, VDEC_PROFILE_DECODER_PIC_END, CORE_MASK_VDEC_1);
	/*
	 * Index is not illegal, line 478 has been determined.
	 */
	/* coverity[overrun-local] */
	vf->v4l_mem_handle
		= hw->buffer_spec[index].v4l_ref_buf_addr;
	aml_buf = (struct aml_buf *)vf->v4l_mem_handle;
	vf->src_fmt.dv_id = v4l2_ctx->dv_id;
	vf->decoder_instid = v4l2_ctx->id;
	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), v4l mem handle: 0x%lx\n",
		((struct aml_vcodec_ctx *)(hw->v4l2_ctx))->id,
		__func__, vf->v4l_mem_handle);

	vf->index = index;
	set_frame_info(hw, vf);

	vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;

	if (hw->chunk) {
		vf->pts = hw->chunk->pts;
		vf->pts_us64 = hw->chunk->pts64;
		vf->timestamp = hw->chunk->timestamp;
	} else {
		offset = READ_VREG(MREG_FRAME_OFFSET);
		if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
			if (pts_lookup_offset_us64
				(PTS_TYPE_VIDEO, offset, &pts,
				&frame_size, 3000,
				&pts_us64) == 0) {
				vf->pts = pts;
				vf->pts_us64 = pts_us64;
			} else {
				vf->pts = 0;
				vf->pts_us64 = 0;
			}
		}
		if (!vdec->vbuf.use_ptsserv && vdec_stream_based(vdec)) {
			vf->pts_us64 = offset;
			vf->pts = 0;
		}
	}
	vf->orientation = 0;
	hw->vfbuf_use[index]++;

	decoder_do_frame_check(vdec, vf);
	vdec_vframe_ready(vdec, vf);
	aml_buf_set_vframe(aml_buf, vf);

	if (hw->jpeg_flag) {
		struct vdec_ge2d_info ge2d_info = { 0 };
		struct buffer_spec_s *dst_pic = &hw->buffer_spec[index];
		struct buffer_spec_s *src_pic = &hw->jpeg_buffer;

		vf->plane_num = 2;
		vf->canvas0Addr = vf->canvas1Addr = -1;
		vf->canvas0_config[0] = dst_pic->canvas_config[0];
		vf->canvas0_config[1] = dst_pic->canvas_config[1];
		vf->canvas0_config[2] = dst_pic->canvas_config[2];
		vf->canvas1_config[0] = dst_pic->canvas_config[0];
		vf->canvas1_config[1] = dst_pic->canvas_config[1];
		vf->canvas1_config[2] = dst_pic->canvas_config[2];

		ge2d_info.src_canvas0Addr = ge2d_info.src_canvas1Addr = -1;
		ge2d_info.src_canvas0_config[0] = src_pic->canvas_config[0];
		ge2d_info.src_canvas0_config[1] = src_pic->canvas_config[1];
		ge2d_info.src_canvas0_config[2] = src_pic->canvas_config[2];
		ge2d_info.src_canvas1_config[0] = src_pic->canvas_config[0];
		ge2d_info.src_canvas1_config[1] = src_pic->canvas_config[1];
		ge2d_info.src_canvas1_config[2] = src_pic->canvas_config[2];

		ge2d_info.dst_vf = vf;

		if (hw->hv_subsample == 0) {
			ulong decbuf_start = 0;
			int tmp_size = hw->jpeg_buffer.canvas_config[1].width * hw->jpeg_buffer.canvas_config[1].height;

			decbuf_start = hw->jpeg_buffer.u_addr;
			if (!vdec_secure(vdec))
				codec_mm_memset(decbuf_start, 128, tmp_size);

			decbuf_start = hw->jpeg_buffer.v_addr;

			if (!vdec_secure(vdec))
				codec_mm_memset(decbuf_start, 128, tmp_size);
		}

		if (!hw->ge2d) {
			int mode = GE2D_MODE_CONVERT_LE;

			if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV16M) ||
				(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV16)) {
				mode |= GE2D_MODE_CONVERT_NV16;
			} else if (v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_RGBA32) {
				mode |= GE2D_MODE_CONVERT_RGBA;
			} else if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M) ||
				(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12)) {
				mode |= GE2D_MODE_CONVERT_NV12;
			} else if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M) ||
				(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21)) {
				mode |= GE2D_MODE_CONVERT_NV21;
			}

			vdec_ge2d_init(&hw->ge2d,  mode);
		}
		vdec_ge2d_copy_data(hw->ge2d, &ge2d_info);
	}

	kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hw->pts_name, vf->timestamp);
	ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
	ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));
	hw->frame_num++;
	v4l2_ctx->decoder_status_info.decoder_count++;
	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"%s:frame num:%d, pts=%d,pts64=%lld. dur=%d width %d, height %d\n",
		__func__, hw->frame_num,
		vf->pts, vf->pts_us64, vf->duration, vf->width, vf->height);
	vdec->vdec_fps_detec(vdec->id);
	if (without_display_mode == 0) {
		if (v4l2_ctx->is_stream_off) {
			vmjpeg_vf_put(vmjpeg_vf_get(vdec), vdec);
		} else {
			if (v4l2_ctx->enable_di_post)
				v4l2_ctx->fbc_transcode_and_set_vf(v4l2_ctx,
					aml_buf, vf);
			aml_buf_set_vframe(aml_buf, vf);
			vdec_tracing(&v4l2_ctx->vtr, VTRACE_DEC_PIC_0, aml_buf->index);
			aml_buf_done(&v4l2_ctx->bm, aml_buf, BUF_USER_DEC);
		}
	} else
		vmjpeg_vf_put(vmjpeg_vf_get(vdec), vdec);

	hw->dec_result = DEC_RESULT_DONE;
	vdec_schedule_work(&hw->work);

	mjpeg_recycle_frame_buffer(hw, hw->force_recycle);

	return IRQ_HANDLED;
}

static int valid_vf_check(struct vframe_s *vf, struct vdec_mjpeg_hw_s *hw)
{
	int i;

	if (!vf || (vf->index == -1))
		return 0;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vf == &hw->vfpool[i])
			return 1;
	}

	return 0;
}

static struct vframe_s *vmjpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	if (!hw)
		return NULL;
	atomic_add(1, &hw->peek_num);
	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmjpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_get(&hw->display_q, &vf)) {
		vf->index_disp = atomic_read(&hw->get_num);
		atomic_add(1, &hw->get_num);
		ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));

		//hw->vfbuf_use[vf->index]--;

		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
		atomic_add(1, &hw->put_num);

		return vf;
	}
	return NULL;
}

static void vmjpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	if (!valid_vf_check(vf, hw)) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"invalid vf: %lx\n", (ulong)vf);
		return ;
	}

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"%s:put_num:%d\n", __func__, hw->put_num);

	//hw->vfbuf_use[vf->index]--;

	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
	atomic_add(1, &hw->put_num);
	vdec_up(vdec);
}

static int vmjpeg_event_cb(int type, void *data, void *op_arg)
{
	struct vdec_s *vdec = op_arg;

	if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static int vmjpeg_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	states->buf_recycle_num = 0;

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}

static int vmjpeg_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;
	int vf_dur = vdec_get_vf_dur();

	if (!hw)
		return -1;

	vstatus->frame_width = hw->frame_width;
	vstatus->frame_height = hw->frame_height;
	vf_dur = vf_dur ? vf_dur : hw->frame_dur;
	if (0 != vf_dur)
		vstatus->frame_rate = 96000 / vf_dur;
	else
		vstatus->frame_rate = 96000;
	vstatus->error_count = 0;
	vstatus->status = hw->stat;
	vdec->vdec_info_statistic.bit_depth = 8; //Only supports 8 bit
	vdec->vdec_info_statistic.is_interlace = false; //Only supports progressive
	return 0;
}

static void init_scaler(struct vdec_mjpeg_hw_s *hw)
{
	u32 endian = mjpeg_get_endian(hw);

	/* 4 point triangle */
	const unsigned int filt_coef[] = {
		0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
		0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
		0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
		0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
		0x18382808, 0x18382808, 0x17372909, 0x17372909,
		0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
		0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
		0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
		0x10303010
	};
	int i;

	/* pscale enable, PSCALE cbus bmem enable */
	WRITE_VREG(PSCALE_CTRL, 0xc000);

	/* write filter coefs */
	WRITE_VREG(PSCALE_BMEM_ADDR, 0);
	for (i = 0; i < 33; i++) {
		WRITE_VREG(PSCALE_BMEM_DAT, 0);
		WRITE_VREG(PSCALE_BMEM_DAT, filt_coef[i]);
	}

	/* Y horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 37 * 2);
	/* [35]: buf repeat pix0,
	 * [34:29] => buf receive num,
	 * [28:16] => buf blk x,
	 * [15:0] => buf phase
	 */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 41 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 39 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 43 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 36 * 2 + 1);
	/* [19:0] => Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);
	/* C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 40 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);

	/* Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 38 * 2 + 1);
	/* [19:0] => Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);
	/* C vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 42 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x10000);

	/* reset pscaler */
	WRITE_VREG(DOS_SW_RESET0, (1 << 10));
	WRITE_VREG(DOS_SW_RESET0, 0);

	if (is_mjpeg_endian_rematch()) {
		if (endian == 7)
			WRITE_VREG(PSCALE_CTRL2, (0x1ff << 16) | READ_VREG(PSCALE_CTRL2));
		else
			WRITE_VREG(PSCALE_CTRL2, (0 << 16) | READ_VREG(PSCALE_CTRL2));
	}

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SC2) {
		READ_RESET_REG(RESET2_REGISTER);
		READ_RESET_REG(RESET2_REGISTER);
		READ_RESET_REG(RESET2_REGISTER);
	}
	WRITE_VREG(PSCALE_RST, 0x7);
	WRITE_VREG(PSCALE_RST, 0x0);
}

static void init_scaler_jpeg(struct vdec_mjpeg_hw_s *hw)
{
	int				org_pic_w = hw->frame_width, org_pic_h = hw->frame_height;
	int				res_pic_w = hw->out_width, res_pic_h = hw->out_height;
	unsigned int	horz_step, vert_step;
	unsigned int	horz_ini_phase, vert_ini_phase;
	unsigned char	filt0_y_horz_ratio;
	unsigned char	filt0_y_vert_ratio;
	unsigned int	pico_addr_mode;
	unsigned char	org_pic_type;
	unsigned int	pico_start_x, pico_start_y;
	unsigned int	data16;
	u32 endian = mjpeg_get_endian(hw);
	const unsigned int* filt_coef = NULL;

	/* 4 point triangle */
	const unsigned int filt_coef1[] = {
		0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
		0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
		0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
		0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
		0x18382808, 0x18382808, 0x17372909, 0x17372909,
		0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
		0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
		0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
		0x10303010
	};

	const unsigned int filt_coef2[] =  //2 point bilinear
		{
			0x00800000,
			0x007e0200,
			0x007c0400,
			0x007a0600,
			0x00780800,
			0x00760a00,
			0x00740c00,
			0x00720e00,
			0x00701000,
			0x006e1200,
			0x006c1400,
			0x006a1600,
			0x00681800,
			0x00661a00,
			0x00641c00,
			0x00621e00,
			0x00602000,
			0x005e2200,
			0x005c2400,
			0x005a2600,
			0x00582800,
			0x00562a00,
			0x00542c00,
			0x00522e00,
			0x00503000,
			0x004e3200,
			0x004c3400,
			0x004a3600,
			0x00483800,
			0x00463a00,
			0x00443c00,
			0x00423e00,
			0x00404000
		};

	const unsigned int filt_coef3[] =  // Nearest neighbor
	{
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000, 0x00800000, 0x00800000, 0x00800000,
		0x00800000
	};

	int i;
	int tmp_w = 0, tmp_h = 0;

	tmp_w = org_pic_w / res_pic_w;
	if (org_pic_w % res_pic_w)
		tmp_w += 1;

	tmp_h = org_pic_h / res_pic_h;
	if (org_pic_h % res_pic_h)
		tmp_h += 1;

	if (tmp_w >= 8) {
		filt0_y_horz_ratio = 0x2;
	} else if (tmp_w >= 4) {
		filt0_y_horz_ratio = 0x1;
	} else {
		filt0_y_horz_ratio = 0x0;
	}

	if (tmp_h >= 8) {
		filt0_y_vert_ratio = 0x2;
	} else if (tmp_h >= 4) {
		filt0_y_vert_ratio = 0x1;
	} else {
		filt0_y_vert_ratio = 0x0;
	}

	if (pscale_w & 0xf0)
		filt0_y_horz_ratio = pscale_w & 0xf;

	if (pscale_h & 0xf0)
		filt0_y_vert_ratio = pscale_h & 0xf;

	if ((filt0_y_vert_ratio == 0) &&
		(filt0_y_horz_ratio == 0)) {
		filt_coef = filt_coef3;
	} else if ((filt0_y_vert_ratio == 2) &&
		(filt0_y_horz_ratio == 2)) {
		filt_coef = filt_coef1;
	} else {
		filt_coef = filt_coef2;
	}

	horz_ini_phase = 0x8000;
	vert_ini_phase = 0x8000;

	pico_start_x = 0;
	pico_start_y = 0;
	pico_addr_mode = 0; 		//left right swap, upside down
#if 0
	org_pic_type = 0x1 << 2 |	//x direction, y:c= 1 or 2 or 4 or 8
				   0;	//y direction, y:c = 1 or 2 or 4 or 8
#else
	org_pic_type = 0;
#endif
	if (org_pic_type_test & 0xf0)
		org_pic_type = org_pic_type_test & 0xf;

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"%s: filt0_y_horz_ratio %d, filt0_y_vert_ratio %d, org_pic_type %d\n",
		__func__, filt0_y_horz_ratio, filt0_y_vert_ratio, org_pic_type);

	horz_step = ((org_pic_w >> filt0_y_horz_ratio) * (1 << 16) / res_pic_w) & 0x7ffff;
	vert_step = ((org_pic_h >> filt0_y_vert_ratio) * (1 << 16) / res_pic_h) & 0x7ffff;

	data16 = filt0_y_horz_ratio << 10 |  //round0
			filt0_y_vert_ratio << 8  |	 //round0
			pico_addr_mode << 4 |
			org_pic_type;

	if ((hw->frame_width == hw->out_width) &&
		(hw->frame_height == hw->out_height)) {
		horz_ini_phase = 0;
		vert_ini_phase = 0;
		horz_step = 0x10000;
		vert_step = 0x10000;
	}

	/* pscale enable, PSCALE cbus bmem enable */
	WRITE_VREG(PSCALE_CTRL, 0xc000);

	/* write filter coefs */
	WRITE_VREG(PSCALE_BMEM_ADDR, 0);
	for (i = 0; i < 33; i++) {
		WRITE_VREG(PSCALE_BMEM_DAT, 0);
		WRITE_VREG(PSCALE_BMEM_DAT, filt_coef[i]);
	}

	/* Y horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 37 * 2);
	/* [35]: buf repeat pix0,
	 * [34:29] => buf receive num,
	 * [28:16] => buf blk x,
	 * [15:0] => buf phase
	 */
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, (0x6000 << 16) | horz_ini_phase);

	/* C horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 41 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, (0x6000 << 16) | horz_ini_phase);

	/* Y vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 39 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, (0x6000 << 16) | vert_ini_phase);

	/* C vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, 43 * 2);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, (0x6000 << 16) | vert_ini_phase);

	/* Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 36 * 2 + 1);
	/* [19:0] => Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, horz_step);

	/* C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 40 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, horz_step);

	/* Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 38 * 2 + 1);
	/* [19:0] => Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, vert_step);
	/* C vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, 42 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_DAT, vert_step);

	/* reset pscaler */
	WRITE_VREG(DOS_SW_RESET0, (1 << 10));
	WRITE_VREG(DOS_SW_RESET0, 0);

	if (is_cpu_t7c() ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S5)) {
		if (endian == 7)
			WRITE_VREG(PSCALE_CTRL2, (0x1ff << 16) | READ_VREG(PSCALE_CTRL2));
		else
			WRITE_VREG(PSCALE_CTRL2, (0 << 16) | READ_VREG(PSCALE_CTRL2));
	}

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SC2) {
		READ_RESET_REG(RESET2_REGISTER);
		READ_RESET_REG(RESET2_REGISTER);
		READ_RESET_REG(RESET2_REGISTER);
	}

	//WRITE_VREG(AV_SCRATCH_0, pico_start_x);
	//WRITE_VREG(AV_SCRATCH_1, pico_start_y);
	WRITE_VREG(AV_SCRATCH_L, data16);
	WRITE_VREG(AV_SCRATCH_6, res_pic_w);
	WRITE_VREG(AV_SCRATCH_7, res_pic_h);

	WRITE_VREG(PSCALE_RST, 0x7);
	WRITE_VREG(PSCALE_RST, 0x0);
}

static void vmjpeg_dump_state(struct vdec_s *vdec)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)(vdec->private);
	mmjpeg_debug_print(DECODE_ID(hw), 0, "====== %s\n", __func__);
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"width/height (%d/%d) buf_num %d\n",
		hw->frame_width,
		hw->frame_height,
		hw->buf_num);
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x"
		"dec_frm %d put_frm %d run %d not_run_ready %d input_empty %d run_flag %d\n",
		input_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		hw->frame_num,
		hw->put_num,
		hw->run_count,
		hw->not_run_ready,
		hw->input_empty,
		hw->run_flag);

	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"%s, newq(%d/%d), dispq(%d/%d) vf peek/get/put (%d/%d/%d)\n",
		__func__,
		kfifo_len(&hw->newframe_q),
		VF_POOL_SIZE,
		kfifo_len(&hw->display_q),
		VF_POOL_SIZE,
		hw->peek_num,
		hw->get_num,
		hw->put_num);
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"VIFF_BIT_CNT=0x%x\n",
		READ_VREG(VIFF_BIT_CNT));
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_LEVEL=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_WP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_WP));
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_RP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_RP));
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	mmjpeg_debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));
	if (input_frame_based(vdec) &&
		debug_enable & PRINT_FRAMEBASE_DATA) {
		int jj;
		if (hw->chunk && hw->chunk->block &&
			hw->chunk->size > 0) {
			u8 *data = NULL;

			if (!hw->chunk->block->is_mapped)
				data = codec_mm_vmap(hw->chunk->block->start +
					hw->chunk->offset, hw->chunk->size);
			else
				data = ((u8 *)hw->chunk->block->start_virt) +
					hw->chunk->offset;

			mmjpeg_debug_print(DECODE_ID(hw), 0,
				"frame data size 0x%x\n", hw->chunk->size);
			for (jj = 0; jj < hw->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					mmjpeg_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA, "%06x:", jj);
				mmjpeg_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA, "%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					mmjpeg_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA, "\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}
}
static void reset_process_time(struct vdec_mjpeg_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}

static void start_process_time(struct vdec_mjpeg_hw_s *hw)
{
	hw->decode_timeout_count = 2;
	hw->start_process_time = jiffies;
}

static void timeout_process(struct vdec_mjpeg_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"%s decoder timeout, pc=%x, level=%d, VIFF_BIT_CNT:0x%x\n",
		__func__,
		READ_VREG(MPC_E),
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VIFF_BIT_CNT));


	hw->vfbuf_use[hw->cur_idx]++;
	vdec_v4l_post_error_frame_event(ctx);
	amvdec_stop();

	mjpeg_reset_frame_buffer(hw);
	hw->dec_result = DEC_RESULT_DONE;
	vdec_v4l_post_error_event(ctx, DECODER_WARNING_DECODER_TIMEOUT);
	reset_process_time(hw);
	vdec_schedule_work(&hw->work);
}

static void check_timer_func(struct timer_list *timer)
{
	struct vdec_mjpeg_hw_s *hw = container_of(timer,
		struct vdec_mjpeg_hw_s, check_timer);
	struct vdec_s *vdec = hw_to_vdec(hw);
	int timeout_val = decode_timeout_val;

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"%s: status:nstatus=%d:%d\n",
		__func__, vdec->status, vdec->next_status);
	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"%s: %d,buftl=%x:%x:%x:%x\n",
		__func__, __LINE__,
		READ_VREG(VLD_MEM_VIFIFO_BUF_CNTL),
		STBUF_READ(&vdec->vbuf, get_wp),
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP));

	if (radr != 0) {
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			pr_info("WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			pr_info("READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}

	if (((debug_enable & PRINT_FLAG_TIMEOUT_STATUS) == 0) &&
		(timeout_val > 0) &&
		(hw->start_process_time > 0) &&
		((1000 * (jiffies - hw->start_process_time) / HZ) > timeout_val)) {
		if (hw->last_vld_level == READ_VREG(VLD_MEM_VIFIFO_LEVEL)) {
			if (hw->decode_timeout_count > 0)
				hw->decode_timeout_count--;
			if (hw->decode_timeout_count == 0)
				timeout_process(hw);
		}
		hw->last_vld_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	}

	if (READ_VREG(DEC_STATUS_REG) == DEC_DECODE_TIMEOUT) {
		pr_info("ucode DEC_DECODE_TIMEOUT\n");
		if (hw->decode_timeout_count > 0)
			hw->decode_timeout_count--;
		if (hw->decode_timeout_count == 0)
			timeout_process(hw);
		WRITE_VREG(DEC_STATUS_REG, 0);
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static void mjpeg_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vmjpeg_vf_put(vf, vdec_ctx);
}

static void mjpeg_get_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	memcpy(vf, vmjpeg_vf_get(vdec_ctx), sizeof(struct vframe_s));
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= mjpeg_get_video_frame,
	.put_vframe	= mjpeg_put_video_frame,
};

static void alloc_jpeg_buffer(struct vdec_mjpeg_hw_s *hw)
{
	int ret;
	ulong decbuf_start = 0;
	u32 canvas_width = 0;
	u32 canvas_height = 0;
	struct vdec_s *vdec = hw_to_vdec(hw);
	u32 tmp_size = 0;
	int num = 2;
	u32 canvas = 0;

	canvas_width	= ALIGN(hw->out_width, 64);
	canvas_height	= ALIGN(hw->out_height, 64);

	if (hw->jpeg_buffer.y_addr == 0) {
		if (is_need_fix_streambuf_rp())
			num -= 1;

		//y
		tmp_size = ALIGN(canvas_width * canvas_height, SZ_64K);
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, num,
				tmp_size, DRIVER_NAME, &decbuf_start);
		if (ret < 0) {
			mmjpeg_debug_print(DECODE_ID(hw), 0, "CMA alloc jpeg y buffer failed! size %d\n",
				tmp_size);
			return ;
		}
		if (!vdec_secure(vdec))
			codec_mm_memset(decbuf_start, 0, tmp_size);
		hw->jpeg_buffer.y_addr = decbuf_start;

		//u
		tmp_size = ALIGN(canvas_width / 2 * canvas_height / 2, SZ_64K);
		num++;
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, num,
			tmp_size, DRIVER_NAME, &decbuf_start);
		if (ret < 0) {
			mmjpeg_debug_print(DECODE_ID(hw), 0, "CMA alloc jpeg u buffer failed! size %d\n",
				tmp_size);
			decoder_bmmu_box_free_idx(hw->mm_blk_handle, num - 1);
			hw->jpeg_buffer.y_addr = 0;
			return ;
		}
		if (!vdec_secure(vdec))
			codec_mm_memset(decbuf_start, 0, tmp_size);

		hw->jpeg_buffer.u_addr = decbuf_start;

		//v
		tmp_size = ALIGN(canvas_width / 2 * canvas_height / 2, SZ_64K);
		num++;
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, num,
			tmp_size, DRIVER_NAME, &decbuf_start);
		if (ret < 0) {
			mmjpeg_debug_print(DECODE_ID(hw), 0, "CMA alloc jpeg v buffer failed! size %d\n",
				tmp_size);

			decoder_bmmu_box_free_idx(hw->mm_blk_handle, num - 1);
			hw->jpeg_buffer.u_addr = 0;
			decoder_bmmu_box_free_idx(hw->mm_blk_handle, num - 2);
			hw->jpeg_buffer.y_addr = 0;
			return ;
		}

		if (!vdec_secure(vdec))
			codec_mm_memset(decbuf_start, 0, tmp_size);

		hw->jpeg_buffer.v_addr = decbuf_start;
	}

	if (vdec->parallel_dec == 1) {
		if (hw->jpeg_buffer.y_canvas_index == -1)
			hw->jpeg_buffer.y_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->jpeg_buffer.u_canvas_index == -1)
			hw->jpeg_buffer.u_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->jpeg_buffer.v_canvas_index == -1)
			hw->jpeg_buffer.v_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
	} else {
		canvas = vdec->get_canvas(hw->buf_num + 1, 3);
		hw->jpeg_buffer.y_canvas_index = canvas_y(canvas);
		hw->jpeg_buffer.u_canvas_index = canvas_u(canvas);
		hw->jpeg_buffer.v_canvas_index = canvas_v(canvas);
	}

	hw->jpeg_buffer.canvas_config[0].phy_addr =
		hw->jpeg_buffer.y_addr;
	hw->jpeg_buffer.canvas_config[0].width =
		canvas_width;
	hw->jpeg_buffer.canvas_config[0].height =
		canvas_height;
	hw->jpeg_buffer.canvas_config[0].block_mode =
		hw->canvas_mode;
	hw->jpeg_buffer.canvas_config[0].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	hw->jpeg_buffer.canvas_config[1].phy_addr =
		hw->jpeg_buffer.u_addr;
	hw->jpeg_buffer.canvas_config[1].width =
		canvas_width / 2;
	hw->jpeg_buffer.canvas_config[1].height =
		canvas_height / 2;
	hw->jpeg_buffer.canvas_config[1].block_mode =
		hw->canvas_mode;
	hw->jpeg_buffer.canvas_config[1].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	hw->jpeg_buffer.canvas_config[2].phy_addr =
		hw->jpeg_buffer.v_addr;
	hw->jpeg_buffer.canvas_config[2].width =
		canvas_width / 2;
	hw->jpeg_buffer.canvas_config[2].height =
		canvas_height / 2;
	hw->jpeg_buffer.canvas_config[2].block_mode =
		hw->canvas_mode;
	hw->jpeg_buffer.canvas_config[2].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->jpeg_buffer.y_canvas_index,
		&hw->jpeg_buffer.canvas_config[0], VDEC_1);

	config_cav_lut(hw->jpeg_buffer.u_canvas_index,
		&hw->jpeg_buffer.canvas_config[1], VDEC_1);

	config_cav_lut(hw->jpeg_buffer.v_canvas_index,
		&hw->jpeg_buffer.canvas_config[2], VDEC_1);
}

static int alloc_tmp_canvas(struct vdec_mjpeg_hw_s *hw)
{
	int ret;
	u32 canvas;
	ulong decbuf_start = 0;
	u32 canvas_width = 0;
	struct vdec_s *vdec = hw_to_vdec(hw);
	u32 tmp_size = 0;
	int index = hw->buf_num;
	int num = 1;

	if (hw->decbuf_start != 0)
		return 0;

	if (is_need_fix_streambuf_rp())
		num -= 1;

	canvas_width	= ALIGN(hw->out_width, 64);
	tmp_size = ALIGN(canvas_width * 128, SZ_64K);

	ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, num,
		tmp_size * 3, DRIVER_NAME, &decbuf_start);

	if (ret < 0) {
		mmjpeg_debug_print(DECODE_ID(hw), 0, "CMA alloc failed! size %d  idx %d\n",
			tmp_size, index);
		return -1;
	}
	if (!vdec_secure(vdec))
		codec_mm_memset(decbuf_start, 0, tmp_size * 3);

	hw->decbuf_start = decbuf_start;
	if (vdec->parallel_dec == 1) {
		if (hw->buffer_spec[index].y_canvas_index == -1)
			hw->buffer_spec[index].y_canvas_index =
				vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->buffer_spec[index].u_canvas_index == -1)
			hw->buffer_spec[index].u_canvas_index =
				vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->buffer_spec[index].v_canvas_index == -1)
			hw->buffer_spec[index].v_canvas_index =
				vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
	} else {
		canvas = vdec->get_canvas(index, 3);
		hw->buffer_spec[index].y_canvas_index = canvas_y(canvas);
		hw->buffer_spec[index].u_canvas_index = canvas_u(canvas);
		hw->buffer_spec[index].v_canvas_index = canvas_v(canvas);
	}

	WRITE_VREG(PSCALE_CANADDR_TMP, spec2canvas(&hw->buffer_spec[index]));

	hw->buffer_spec[index].canvas_config[0].phy_addr =
		decbuf_start;
	hw->buffer_spec[index].canvas_config[0].width =
		canvas_width;
	hw->buffer_spec[index].canvas_config[0].height =
		128;
	hw->buffer_spec[index].canvas_config[0].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[index].canvas_config[0].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[index].y_canvas_index,
		&hw->buffer_spec[index].canvas_config[0], VDEC_1);

	hw->buffer_spec[index].canvas_config[1].phy_addr =
		decbuf_start + tmp_size;
	hw->buffer_spec[index].canvas_config[1].width =
		canvas_width;
	hw->buffer_spec[index].canvas_config[1].height =
		128;
	hw->buffer_spec[index].canvas_config[1].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[index].canvas_config[1].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[index].u_canvas_index,
		&hw->buffer_spec[index].canvas_config[1], VDEC_1);

	hw->buffer_spec[index].canvas_config[2].phy_addr =
		decbuf_start + tmp_size * 2;
	hw->buffer_spec[index].canvas_config[2].width =
		canvas_width;
	hw->buffer_spec[index].canvas_config[2].height =
		128;
	hw->buffer_spec[index].canvas_config[2].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[index].canvas_config[2].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[index].v_canvas_index,
		&hw->buffer_spec[index].canvas_config[2], VDEC_1);

	return 0;
}

static int vmjpeg_v4l_alloc_buff_config_canvas(struct vdec_mjpeg_hw_s *hw, int i)
{
	u32 canvas;
	dos_addr_t decbuf_start = 0, decbuf_u_start = 0, decbuf_v_start = 0;
	int decbuf_y_size = 0, decbuf_u_size = 0, decbuf_v_size = 0;
	u32 canvas_width = 0, canvas_height = 0;
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct aml_buf *aml_buf = hw->aml_buf;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	if (!aml_buf) {
		mmjpeg_debug_print(DECODE_ID(hw), 0, "[ERR]aml_buf is NULL!\n");
		return -1;
	}

	if (!hw->frame_width || !hw->frame_height) {
			struct vdec_pic_info pic = { 0 };
			vdec_v4l_get_pic_info(ctx, &pic);
			hw->frame_width = pic.visible_width;
			hw->frame_height = pic.visible_height;
			mmjpeg_debug_print(DECODE_ID(hw), 0,
				"[%d] set %d x %d from IF layer\n", ctx->id,
				hw->frame_width, hw->frame_height);
	}

	hw->buffer_spec[i].v4l_ref_buf_addr = (ulong)aml_buf;
	hw->buffer_spec[i].cma_alloc_addr = aml_buf->planes[0].addr;
	if (aml_buf->num_planes == 1) {
		decbuf_start	= aml_buf->planes[0].addr;
		decbuf_y_size	= aml_buf->planes[0].offset;
		decbuf_u_start	= decbuf_start + decbuf_y_size;
		decbuf_u_size	= decbuf_y_size / 4;
		decbuf_v_start	= decbuf_u_start + decbuf_u_size;
		decbuf_v_size	= decbuf_u_size;

		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
	} else if (aml_buf->num_planes == 2) {
		decbuf_start	= aml_buf->planes[0].addr;
		decbuf_y_size	= aml_buf->planes[0].length;
		decbuf_u_start	= aml_buf->planes[1].addr;
		decbuf_u_size	= aml_buf->planes[1].length >> 1;
		decbuf_v_start	= decbuf_u_start + decbuf_u_size;
		decbuf_v_size	= decbuf_u_size;

		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
		aml_buf->planes[1].bytes_used = aml_buf->planes[1].length;
	} else if (aml_buf->num_planes == 3) {
		decbuf_start	= aml_buf->planes[0].addr;
		decbuf_y_size	= aml_buf->planes[0].length;
		decbuf_u_start	= aml_buf->planes[1].addr;
		decbuf_u_size	= aml_buf->planes[1].length;
		decbuf_v_start	= aml_buf->planes[2].addr;
		decbuf_v_size	= aml_buf->planes[2].length;

		aml_buf->planes[0].bytes_used = aml_buf->planes[0].length;
		aml_buf->planes[1].bytes_used = aml_buf->planes[1].length;
		aml_buf->planes[2].bytes_used = aml_buf->planes[2].length;
	}

	if (hw->jpeg_flag) {
		canvas_width	= ALIGN(hw->out_width, 64);
		canvas_height	= ALIGN(hw->out_height, 64);
	} else {
		canvas_width	= ALIGN(hw->frame_width, 64);
		canvas_height	= ALIGN(hw->frame_height, 64);
	}

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] v4l ref buf addr: 0x%x\n", ctx->id, aml_buf);

	if (vdec->parallel_dec == 1) {
		if (hw->buffer_spec[i].y_canvas_index == -1)
			hw->buffer_spec[i].y_canvas_index =
			vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->buffer_spec[i].u_canvas_index == -1)
			hw->buffer_spec[i].u_canvas_index =
			vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
		if (hw->buffer_spec[i].v_canvas_index == -1)
			hw->buffer_spec[i].v_canvas_index =
			vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
	} else {
		canvas = vdec->get_canvas(i, 3);
		hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
		hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
		hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);
	}

	hw->buffer_spec[i].canvas_config[0].phy_addr =
		decbuf_start;
	hw->buffer_spec[i].canvas_config[0].width =
		canvas_width;
	hw->buffer_spec[i].canvas_config[0].height =
		canvas_height;
	hw->buffer_spec[i].canvas_config[0].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[i].canvas_config[0].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[i].y_canvas_index,
			&hw->buffer_spec[i].canvas_config[0], VDEC_1);

	hw->buffer_spec[i].canvas_config[1].phy_addr =
		decbuf_u_start;
	hw->buffer_spec[i].canvas_config[1].width =
		canvas_width / 2;
	hw->buffer_spec[i].canvas_config[1].height =
		canvas_height / 2;
	hw->buffer_spec[i].canvas_config[1].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[i].canvas_config[1].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[i].u_canvas_index,
			&hw->buffer_spec[i].canvas_config[1], VDEC_1);

	hw->buffer_spec[i].canvas_config[2].phy_addr =
		decbuf_v_start;
	hw->buffer_spec[i].canvas_config[2].width =
		canvas_width / 2;
	hw->buffer_spec[i].canvas_config[2].height =
		canvas_height / 2;
	hw->buffer_spec[i].canvas_config[2].block_mode =
		hw->canvas_mode;
	hw->buffer_spec[i].canvas_config[2].endian =
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;

	config_cav_lut(hw->buffer_spec[i].v_canvas_index,
			&hw->buffer_spec[i].canvas_config[2], VDEC_1);

	hw->aml_buf = NULL;

	return 0;
}

static int find_free_buffer(struct vdec_mjpeg_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->buf_num; i++) {
		if (hw->vfbuf_use[i] == 0 &&
			!hw->buffer_spec[i].cma_alloc_addr)
			break;
	}

	if (i >= hw->buf_num) {
		mmjpeg_debug_print(DECODE_ID(hw), 0,
			"[ERR]not find free buffer slot! buf_num %d\n",
			hw->buf_num);
		return -1;
	}

	if (vmjpeg_v4l_alloc_buff_config_canvas(hw, i))
		return -1;

	return i;
}

static void vmjpeg_workspace_init(struct vdec_mjpeg_hw_s *hw)
{
	int ret;
	u32 buf_size;

	buf_size = RP_WORKAROUND_SIZE;
	ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle,
			0, buf_size, DRIVER_NAME, &hw->buf_start);
	if (ret < 0) {
		pr_err("mjpeg workspace alloc size %d failed.\n", buf_size);
	}

	return;
}

static int vmjpeg_hw_ctx_restore(struct vdec_mjpeg_hw_s *hw)
{
	int index = -1;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	int i = 0;

	if (hw->v4l_params_parsed) {
		struct vdec_pic_info pic = { 0 };

		if (!hw->buf_num) {
			vdec_v4l_get_pic_info(v4l2_ctx, &pic);
			hw->buf_num = pic.dpb_frames +
				pic.dpb_margin;
			if (hw->buf_num > DECODE_BUFFER_NUM_MAX)
				hw->buf_num = DECODE_BUFFER_NUM_MAX;
		}

		index = find_free_buffer(hw);
		if ((index < 0) || (index >= hw->buf_num))
			return -1;
		hw->cur_idx = index;
		for (i = 0; i < hw->buf_num; i++) {
			if (hw->buffer_spec[i].cma_alloc_addr) {
				config_cav_lut(hw->buffer_spec[i].y_canvas_index,
						&hw->buffer_spec[i].canvas_config[0], VDEC_1);
				config_cav_lut(hw->buffer_spec[i].u_canvas_index,
					&hw->buffer_spec[i].canvas_config[1], VDEC_1);
				config_cav_lut(hw->buffer_spec[i].v_canvas_index,
					&hw->buffer_spec[i].canvas_config[2], VDEC_1);
			}
		}

		if (hw->jpeg_flag) {
			alloc_jpeg_buffer(hw);
			alloc_tmp_canvas(hw);
			WRITE_VREG(AV_SCRATCH_4, spec2canvas(&hw->jpeg_buffer));
			pr_err("AV_SCRATCH_4 0x%x\n", READ_VREG(AV_SCRATCH_4));
		} else {
			/* find next decode buffer index */
			WRITE_VREG(AV_SCRATCH_4, spec2canvas(&hw->buffer_spec[index]));
		}

		WRITE_VREG(AV_SCRATCH_5, index | 1 << 24);

	} else
		WRITE_VREG(AV_SCRATCH_5, 1 << 24);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6));
	WRITE_VREG(DOS_SW_RESET0, 0);

	if (hw->jpeg_flag)
		init_scaler_jpeg(hw);
	else
		init_scaler(hw);

	/* clear buffer IN/OUT registers */
	WRITE_VREG(MREG_TO_AMRISC, 0);
	WRITE_VREG(MREG_FROM_AMRISC, 0);

	WRITE_VREG(MCPU_INTR_MSK, 0xffff);

	WRITE_VREG(MREG_DECODE_PARAM, (hw->frame_height << 4) | 0x8000);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
	/* set interrupt mapping for vld */
	WRITE_VREG(ASSIST_AMR1_INT8, 8);
	WRITE_VREG(DEC_STATUS_REG, 0);

	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
	if (hw_to_vdec(hw)->is_scatter_input_mode) {
		u32 data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
		data32 = data32 |  // in scatter input mode, set to 0
			(1 << 22) |  // stream_fifo_empty_int_amrisc_enable
			//(1 << 21) |  // stream_map_empty_int_cpu_enable
			(1 << 7) |  // dec_done_int_amrisc_enable
			(0 << 4) |  // disable startcode interrupt when SCATTER_ENABLE
			(0 << 3) |  // startcode_found_int_amrisc_enable
			(1 << 2) |  // use parser_int_cpu_level instead of pulse to detect interrupt
			(1 << 0);    // parser_int_enable
		data32 &= ~(1 << 24);
		data32 &= ~(1 << 4);
		WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);
	}
	if (is_need_fix_streambuf_rp()) {
		if (!hw->init_flag)
			vmjpeg_workspace_init(hw);

		WRITE_VREG(AV_SCRATCH_L, hw->buf_start);
	}

	return 0;
}

static s32 vmjpeg_init(struct vdec_s *vdec)
{
	int i;
	int size = -1, fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	int num = 0;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	fw = fw_firmware_s_creat(fw_size);
	if (!fw)
		return -ENOMEM;

	if (hw->jpeg_flag) {
		size = get_firmware_data(VIDEO_DEC_JPEG, fw->data);
	} else {
		size = get_firmware_data(VIDEO_DEC_MJPEG_MULTI, fw->data);
	}

	if (size < 0) {
		pr_err("get firmware fail.");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	hw->frame_width = 0;
	hw->frame_height = 0;

	hw->frame_dur = hw->v4l_duration ? hw->v4l_duration :
		((hw->vmjpeg_amstream_dec_info.rate) ?
	hw->vmjpeg_amstream_dec_info.rate : 3840);
	hw->saved_resolution = 0;
	hw->eos = 0;
	hw->init_flag = 0;
	hw->frame_num = 0;
	hw->run_count = 0;
	hw->not_run_ready = 0;
	hw->input_empty = 0;
	atomic_set(&hw->peek_num, 0);
	atomic_set(&hw->get_num, 0);
	atomic_set(&hw->put_num, 0);

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		hw->vfbuf_use[i] = 0;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];

		hw->vfpool[i].index = -1;
		kfifo_put(&hw->newframe_q, vf);
	}

	if (is_need_fix_streambuf_rp())
		num += 1;

	if (hw->jpeg_flag)
		num += 4;

	if (num) {
		if (hw->mm_blk_handle) {
			decoder_bmmu_box_free(hw->mm_blk_handle);
			hw->mm_blk_handle = NULL;
		}

		hw->mm_blk_handle = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			ctx->id,
			num,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER,
			BMMU_ALLOC_FLAGS_WAIT);
	}

	timer_setup(&hw->check_timer, check_timer_func, 0);
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;

	WRITE_VREG(DECODE_STOP_POS, udebug_flag);
	INIT_WORK(&hw->work, vmjpeg_work);
	pr_info("w:h=%d:%d\n", hw->frame_width, hw->frame_height);
	return 0;
}

static int mjpeg_recycle_frame_buffer(struct vdec_mjpeg_hw_s *hw, int force_recycle)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct aml_buf *aml_buf;
	int i;

	for (i = 0; i < hw->buf_num; ++i) {
		if ((force_recycle || (hw->vfbuf_use[i])) &&
			hw->buffer_spec[i].cma_alloc_addr) {
			aml_buf = (struct aml_buf *)hw->buffer_spec[i].v4l_ref_buf_addr;

			mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
				"recycled buf idx: %d dma addr: 0x%lx fb idx: %d vf_ref %d \n",
				i, hw->buffer_spec[i].cma_alloc_addr,
				aml_buf->index,
				hw->vfbuf_use[i]);

			if (force_recycle)
				aml_buf_put_ref(&ctx->bm, aml_buf);

			hw->buffer_spec[i].v4l_ref_buf_addr = 0;
			hw->buffer_spec[i].cma_alloc_addr = 0;
			while (hw->vfbuf_use[i]) {
				hw->vfbuf_use[i]--;
			}
		}
	}

	if (force_recycle)
		hw->force_recycle = 0;

	return 0;
}

static int mjpeg_reset_frame_buffer(struct vdec_mjpeg_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->buf_num; ++i) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
				"%s buf idx %d dma addr: 0x%lx vf_ref %d\n",
				__func__, i,
				hw->buffer_spec[i].cma_alloc_addr,
				hw->vfbuf_use[i]);

		if (hw->buffer_spec[i].cma_alloc_addr) {
			hw->vfbuf_use[i] = 1;
		}
	}

	hw->force_recycle = 1;

	return 0;
}

static bool is_available_buffer(struct vdec_mjpeg_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int i, free_count = 0;
	int free_slot = 0;

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

	/* Wait for the buffer number negotiation to complete. */
	if (hw->buf_num == 0) {
		struct vdec_pic_info pic = { 0 };

		vdec_v4l_get_pic_info(ctx, &pic);
		hw->buf_num = pic.dpb_frames + pic.dpb_margin;

		if (hw->buf_num == 0)
			return false;

		if (hw->buf_num > DECODE_BUFFER_NUM_MAX)
			hw->buf_num = DECODE_BUFFER_NUM_MAX;
	}

	mjpeg_recycle_frame_buffer(hw, hw->force_recycle);

	for (i = 0; i < hw->buf_num; ++i) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"%s idx %d vf_ref %d cma_alloc_addr = 0x%lx\n",
		__func__, i,
		hw->vfbuf_use[i],
		hw->buffer_spec[i].cma_alloc_addr);
		if ((hw->vfbuf_use[i] == 0) &&
			!hw->buffer_spec[i].cma_alloc_addr) {
			free_slot++;

			break;
		}
	}

	if (ctx->ge2d &&
		atomic_read(&ctx->ge2d_cache_num) >= 4) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
			"%s ge2d cache: %d full!\n",
			__func__, atomic_read(&ctx->ge2d_cache_num));

		return false;
	}

	if (!free_slot) {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
			"%s not enough free_slot %d!\n",
		__func__, free_slot);
		for (i = 0; i < hw->buf_num; ++i) {
			mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
			"%s idx %d vf_ref %d cma_alloc_addr = 0x%lx\n",
			__func__, i,
			hw->vfbuf_use[i],
			hw->buffer_spec[i].cma_alloc_addr);
		}

		return false;
	}


	if (!hw->aml_buf && !aml_buf_empty(&ctx->bm)) {
		hw->aml_buf = aml_buf_get(&ctx->bm, BUF_USER_DEC, false);
		if (!hw->aml_buf) {
			return false;
		}
		hw->aml_buf->task->attach(hw->aml_buf->task, &task_dec_ops, hw_to_vdec(hw));
		hw->aml_buf->state = FB_ST_DECODER;
	}

	if (hw->aml_buf) {
		free_count++;
		free_count += aml_buf_ready_num(&ctx->bm);
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"%s get fb: 0x%lx fb idx: %d\n",
		__func__, hw->aml_buf, hw->aml_buf->index);
	} else {
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"no frame buffer!\n");
	}

	vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_1, free_count);

	return free_count >= run_ready_min_buf_num ? 1 : 0;
}

static unsigned long run_ready(struct vdec_s *vdec,
	unsigned long mask)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	int ret = 0;

	hw->not_run_ready++;

	if (hw->eos)
		return 0;

	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
		&& pre_decode_buf_level != 0) {
		u32 rp, wp, level;

		if (vdec->is_scatter_input_mode) {
			level = scatter_input_get_data_level(vdec->scatter_input_ctx,
						scatter_input_get_wp(vdec->scatter_input_ctx),
						scatter_input_get_shift_byte(vdec->scatter_input_ctx));
		} else {
			rp = STBUF_READ(&vdec->vbuf, get_rp);
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = vdec->input.size + wp - rp;
			else
				level = wp - rp;
		}
		if (level < pre_decode_buf_level)
			return PRE_LEVEL_NOT_ENOUGH;
	}

	ret = is_available_buffer(hw) ? CORE_MASK_VDEC_1 : 0;
	if (ret) {
		hw->not_run_ready = 0;
		hw->buffer_not_ready = 0;
	} else {
		hw->not_run_ready++;
		hw->buffer_not_ready = 1;
	}

	return ret;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *, int), void *arg)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int ret;

	hw->run_flag = 1;
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	hw->run_count++;
	vdec_reset_core(vdec);
	if (is_vdec_hevc_combine()) {
		if (!hw->vdec_pg_enable_flag) {
			hw->vdec_pg_enable_flag = 1;
			amvdec_enable();
		}
		hevc_reset_core(vdec);
		WRITE_VREG(HEVC_DBLK_CFGC, 0x80000000);
		WRITE_VREG(HEVC_CORE_ENABLE, 0);
	}

	if (vdec->mc_loaded) {
		/* firmware have load before,
		 * and not changes to another.
		 * ignore reload.
		 */
	} else {
		if (hw->jpeg_flag) {
			ret = amvdec_vdec_loadmc_ex(VFORMAT_JPEG, "jpeg", vdec, hw->fw->data);
			vdec->mc_type = VFORMAT_JPEG;
		} else {
			ret = amvdec_vdec_loadmc_ex(VFORMAT_MJPEG, "mmjpeg", vdec, hw->fw->data);
			vdec->mc_type = VFORMAT_MJPEG;
		}
		if (ret < 0) {
			pr_err("[%d] %s: the %s fw loading failed, err: %x\n",
				vdec->id, hw->jpeg_flag ? "jpeg" : "mmjpeg",
				fw_tee_enabled() ? "TEE" : "local", ret);
			vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			hw->run_flag = 0;
			return;
		}
		vdec->mc_loaded = 1;
	}

	ret = vdec_prepare_input(vdec, &hw->chunk);

	if (input_frame_based(vdec) && !(vdec_secure(vdec)) && hw->chunk && !vdec->is_scatter_input_mode) {
		u8 *data = NULL;

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, ret);
		else
			data = ((u8 *)hw->chunk->block->start_virt)
				+ hw->chunk->offset;

		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"%s: size 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
			__func__, ret,
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[ret - 4],
			data[ret - 3],	data[ret - 2],
			data[ret - 1]);
		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	}

	if (ret <= 0) {
		hw->input_empty++;
		mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"%s: %d,r=%d,buftl=%x:%x:%x\n",
			__func__, __LINE__, ret,
			READ_VREG(VLD_MEM_VIFIFO_BUF_CNTL),
			STBUF_READ(&vdec->vbuf, get_rp),
			READ_VREG(VLD_MEM_VIFIFO_WP));

		hw->dec_result = DEC_RESULT_AGAIN;
		vdec_schedule_work(&hw->work);
		hw->run_flag = 0;
		return;
	}
	ctx->current_timestamp = hw->chunk->timestamp;
	WRITE_VREG(VIFF_BIT_CNT, ret * 8);
	vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_0, ret);

	hw->input_empty = 0;
	hw->dec_result = DEC_RESULT_NONE;
	if (vmjpeg_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		mmjpeg_debug_print(DECODE_ID(hw), 0,
			"amvdec_mmjpeg: error HW context restore\n");
		vdec_schedule_work(&hw->work);
		hw->run_flag = 0;
		return;
	}
	vdec_prefix_config(PREFIX_ADDR(hw->buffer_spec[0].cma_alloc_addr));
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);
	hw->stat |= STAT_MC_LOAD;
	start_process_time(hw);
	hw->last_vld_level = 0;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"%s (0x%x 0x%x 0x%x) vldcrl 0x%x bitcnt 0x%x powerctl 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		__func__,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP),
		READ_VREG(VLD_DECODE_CONTROL),
		READ_VREG(VIFF_BIT_CNT),
		READ_VREG(POWER_CTL_VLD),
		READ_VREG(VLD_MEM_VIFIFO_START_PTR),
		READ_VREG(VLD_MEM_VIFIFO_CURR_PTR),
		READ_VREG(VLD_MEM_VIFIFO_CONTROL),
		READ_VREG(VLD_MEM_VIFIFO_BUF_CNTL),
		READ_VREG(VLD_MEM_VIFIFO_END_PTR));
	amvdec_start();
	vdec_profile(vdec, VDEC_PROFILE_DECODER_START, CORE_MASK_VDEC_1);
	vdec_enable_input(vdec);
	hw->stat |= STAT_VDEC_RUN;
	hw->init_flag = 1;
	hw->run_flag = 0;
}
static void wait_vmjpeg_search_done(struct vdec_mjpeg_hw_s *hw)
{
	u32 vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
	int count = 0;

	do {
		usleep_range(100, 500);
		if (vld_rp == READ_VREG(VLD_MEM_VIFIFO_RP))
			break;
		if (count > 1000) {
			mmjpeg_debug_print(DECODE_ID(hw), 0,
					"%s, count %d  vld_rp 0x%x VLD_MEM_VIFIFO_RP 0x%x\n",
					__func__, count, vld_rp, READ_VREG(VLD_MEM_VIFIFO_RP));
			break;
		} else
			vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
		count++;
	} while (1);
}

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct vdec_mjpeg_hw_s *hw = (struct vdec_mjpeg_hw_s *)vdec->private;
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
			pr_err("[%d] MJPEG isn't enough buff for notify eos.\n", ctx->id);
			return 0;
		}
		usleep_range(500, 1000);
	}

	index = find_free_buffer(hw);
	if (INVALID_IDX == index) {
		pr_err("[%d] MJPEG EOS get free buff fail.\n", ctx->id);
		return 0;
	}

	aml_buf = (struct aml_buf *)
		hw->buffer_spec[index].v4l_ref_buf_addr;

	vf->type		|= VIDTYPE_V4L_EOS;
	vf->timestamp		= ULLONG_MAX;
	vf->v4l_mem_handle	= (ulong)aml_buf;
	vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;

	vdec_vframe_ready(vdec, vf);
	aml_buf_set_vframe(aml_buf, vf);
	kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

	vdec_tracing(&ctx->vtr, VTRACE_DEC_PIC_0, aml_buf->index);
	aml_buf_done(&ctx->bm, aml_buf, BUF_USER_DEC);

	hw->eos = true;

	pr_info("[%d] mjpeg EOS notify.\n", ctx->id);

	return 0;
}

static void vmjpeg_work(struct work_struct *work)
{
	struct vdec_mjpeg_hw_s *hw = container_of(work,
	struct vdec_mjpeg_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	if (hw->dec_result == DEC_RESULT_AGAIN) {
		vdec_profile(vdec, VDEC_PROFILE_EVENT_AGAIN, CORE_MASK_VDEC_1);
	}

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"%s: result=%d,len=%d:%d\n",
		__func__, hw->dec_result,
		kfifo_len(&hw->newframe_q),
		kfifo_len(&hw->display_q));

	mmjpeg_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"%s (0x%x 0x%x 0x%x) vldcrl 0x%x bitcnt 0x%x powerctl 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		__func__,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP),
		READ_VREG(VLD_DECODE_CONTROL),
		READ_VREG(VIFF_BIT_CNT),
		READ_VREG(POWER_CTL_VLD),
		READ_VREG(VLD_MEM_VIFIFO_START_PTR),
		READ_VREG(VLD_MEM_VIFIFO_CURR_PTR),
		READ_VREG(VLD_MEM_VIFIFO_CONTROL),
		READ_VREG(VLD_MEM_VIFIFO_BUF_CNTL),
		READ_VREG(VLD_MEM_VIFIFO_END_PTR));

	vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_3, hw->dec_result);

	if (hw->dec_result == DEC_RESULT_DONE) {
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		hw->chunk = NULL;
	} else if (hw->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(hw_to_vdec(hw))) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		pr_info("%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		pr_info("%s: end of stream\n", __func__);
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_4, __LINE__);
		notify_v4l_eos(vdec);
		vdec_tracing(&ctx->vtr, VTRACE_DEC_ST_4, 0);

		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		hw->chunk = NULL;
		vdec_clean_input(hw_to_vdec(hw));
	}
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	/*disable mbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 0);
	if (vdec->is_scatter_input_mode) {
		scatter_input_wait_idle();
	} else {
		wait_vmjpeg_search_done(hw);
	}

	if (ctx->param_sets_from_ucode &&
		!hw->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else {
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1
			| CORE_MASK_HEVC);
	}
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg, CORE_MASK_VDEC_1);
}

static int vmjpeg_stop(struct vdec_mjpeg_hw_s *hw)
{
	pr_info("%s ...count = %d\n", __func__, hw->frame_num);

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		pr_info("%s amvdec_stop\n", __func__);
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
	hw->init_flag = 0;

	if (hw->mm_blk_handle) {
		void *bmmu_box_tmp = hw->mm_blk_handle;
		hw->mm_blk_handle = NULL;
		if (hw->run_flag)
			usleep_range(1000, 2000);
		decoder_bmmu_box_free(bmmu_box_tmp);
		bmmu_box_tmp= NULL;
	}

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}

	return 0;
}

static void reset(struct vdec_s *vdec)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)vdec->private;
	int i;

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	cancel_work_sync(&hw->work);
	reset_process_time(hw);

	for (i = 0; i < hw->buf_num; i++) {
		hw->buffer_spec[i].v4l_ref_buf_addr = 0;
		hw->buffer_spec[i].cma_alloc_addr = 0;
		hw->vfbuf_use[i] = 0;
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
		vdec->free_canvas_ex(hw->buffer_spec[i].y_canvas_index, vdec->id);
		vdec->free_canvas_ex(hw->buffer_spec[i].u_canvas_index, vdec->id);
		vdec->free_canvas_ex(hw->buffer_spec[i].v_canvas_index, vdec->id);
		hw->buffer_spec[i].y_canvas_index = -1;
		hw->buffer_spec[i].u_canvas_index = -1;
		hw->buffer_spec[i].v_canvas_index = -1;
	}

	vdec->free_canvas_ex(hw->jpeg_buffer.y_canvas_index, vdec->id);
	vdec->free_canvas_ex(hw->jpeg_buffer.u_canvas_index, vdec->id);
	vdec->free_canvas_ex(hw->jpeg_buffer.v_canvas_index, vdec->id);
	hw->jpeg_buffer.y_canvas_index = -1;
	hw->jpeg_buffer.u_canvas_index = -1;
	hw->jpeg_buffer.v_canvas_index = -1;

	hw->buf_num		= 0;
	hw->frame_width		= 0;
	hw->frame_height	= 0;
	hw->eos 		= false;
	hw->aml_buf		= NULL;

	atomic_set(&hw->peek_num, 0);
	atomic_set(&hw->get_num, 0);
	atomic_set(&hw->put_num, 0);

	pr_info("mjpeg: reset.\n");
}

static int ammvdec_mjpeg_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mjpeg_hw_s *hw = NULL;
	int config_val = 0;
	struct aml_vcodec_ctx *ctx = NULL;

	if (pdata == NULL) {
		pr_info("ammvdec_mjpeg memory resource undefined.\n");
		return -EFAULT;
	}

	hw =  vzalloc(sizeof(struct vdec_mjpeg_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_mjpeg device data allocation failed\n");
		return -ENOMEM;
	}

	/* the ctx from v4l2 driver. */
	hw->v4l2_ctx = pdata->private;
	ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	pdata->private = hw;
	pdata->dec_status = vmjpeg_dec_status;

	pdata->run = run;
	pdata->run_ready = run_ready;
	pdata->reset = reset;
	pdata->irq_handler = vmjpeg_isr;
	pdata->threaded_irq_handler = vmjpeg_isr_thread_fn;
	pdata->dump_state = vmjpeg_dump_state;

	snprintf(hw->vdec_name, sizeof(hw->vdec_name),
		"vmjpeg-%d", pdev->id);
	snprintf(hw->pts_name, sizeof(hw->pts_name),
		"%s-timestamp", hw->vdec_name);
	snprintf(hw->new_q_name, sizeof(hw->new_q_name),
		"%s-newframe_q", hw->vdec_name);
	snprintf(hw->disp_q_name, sizeof(hw->disp_q_name),
		"%s-dispframe_q", hw->vdec_name);

	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
			hw->buffer_spec[i].y_canvas_index = -1;
			hw->buffer_spec[i].u_canvas_index = -1;
			hw->buffer_spec[i].v_canvas_index = -1;
		}
		hw->jpeg_buffer.y_canvas_index = -1;
		hw->jpeg_buffer.u_canvas_index = -1;
		hw->jpeg_buffer.v_canvas_index = -1;
	}

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	platform_set_drvdata(pdev, pdata);
	hw->platform_dev = pdev;

	if (((debug_enable & IGNORE_PARAM_FROM_CONFIG) == 0) && pdata->config_len) {
		mmjpeg_debug_print(DECODE_ID(hw), 0, "pdata->config: %s\n", pdata->config);
		if (get_config_int(pdata->config, "parm_v4l_buffer_margin",
			&config_val) == 0)
			hw->dynamic_buf_num_margin = config_val;
		else
			hw->dynamic_buf_num_margin = dynamic_buf_num_margin;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hw->canvas_mode = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_endian",
			&config_val) == 0)
			hw->canvas_endian = config_val;

		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hw->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hw->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hw->is_used_v4l = config_val;

		if (hw->jpeg_flag) {
			if (get_config_int(pdata->config, "out_width",
					&config_val) == 0) {
				hw->out_width = config_val;
			}
			if (get_config_int(pdata->config, "out_height",
					&config_val) == 0) {
				hw->out_height = config_val;
			}
		}

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0)
			hw->metadata_config_flag = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_duration",
			&config_val) == 0)
			hw->v4l_duration = config_val;

		/*if (get_config_int(pdata->config,
			"parm_v4l_duration",
			&config_val) == 0)
			vdec_frame_rate_uevent(config_val);*/
	} else {
		hw->dynamic_buf_num_margin = dynamic_buf_num_margin;
	}

	if (out_width) {
		hw->out_width = out_width;
	}

	if (out_height) {
		hw->out_height = out_height;
	}

	if (enable_jpeg)
		hw->jpeg_flag = enable_jpeg;

	if (ctx->output_pix_fmt == V4L2_PIX_FMT_JPEG)
		hw->jpeg_flag = 1;

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;

	vdec_source_changed(VFORMAT_MJPEG, 3840, 2160, 30);
	if (vmjpeg_init(pdata) < 0) {
		pr_info("ammvdec_mjpeg init failed.\n");
		if (hw) {
			vfree(hw);
			hw = NULL;
		}
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	vdec_set_prepare_level(pdata, start_decode_buf_level);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);
	}

	return 0;
}

static KV_INT_TO_VOID ammvdec_mjpeg_remove(struct platform_device *pdev)
{
	struct vdec_mjpeg_hw_s *hw =
		(struct vdec_mjpeg_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec;
	int i;

	if (!hw)
		return KV_RET_x_TO_VOID(-1);
	vdec = hw_to_vdec(hw);

	if (hw->ge2d) {
		vdec_ge2d_destroy(hw->ge2d);
		hw->ge2d = NULL;
	}

	vmjpeg_stop(hw);

	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);
	if (vdec->parallel_dec == 1) {
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
			vdec->free_canvas_ex(hw->buffer_spec[i].y_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].u_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].v_canvas_index, vdec->id);
		}
	}

	vfree(hw);

	pr_info("%s\n", __func__);
	return KV_RET_x_TO_VOID(0);
}

/****************************************/

static struct platform_driver ammvdec_mjpeg_driver = {
	.probe = ammvdec_mjpeg_probe,
	.remove = ammvdec_mjpeg_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static void set_debug_flag(const char *module, int debug_flags)
{
	debug_enable = debug_flags;
}

static int __init ammvdec_mjpeg_driver_init_module(void)
{
	if (platform_driver_register(&ammvdec_mjpeg_driver)) {
		pr_err("failed to register ammvdec_mjpeg driver\n");
		return -ENODEV;
	}

	vcodec_profile_register_v2("MJPEG-V4L", VFORMAT_MJPEG, 1);
	vcodec_profile_register_v2("JPEG-V4L", VFORMAT_JPEG, 1);
	vcodec_feature_register(VFORMAT_MJPEG, 1);
	vcodec_feature_register(VFORMAT_JPEG, 1);
	register_set_debug_flag_func(DEBUG_AMVDEC_MJPEG_V4L, set_debug_flag);

	return 0;
}

static void __exit ammvdec_mjpeg_driver_remove_module(void)
{
	platform_driver_unregister(&ammvdec_mjpeg_driver);
}

/****************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static struct param_entry amvdec_mjpeg_v4l_params[] = {
	PARAM_UINT(debug_enable),
	PARAM_INT(pre_decode_buf_level),
	PARAM_UINT(udebug_flag),
	PARAM_UINT(dynamic_buf_num_margin),
	PARAM_UINT(decode_timeout_val),
	PARAM_UINT_ARRAY(max_process_time),
	PARAM_UINT(radr),
	PARAM_UINT(pscale_w),
	PARAM_UINT(pscale_h),
	PARAM_UINT(out_width),
	PARAM_UINT(out_height),
	PARAM_UINT(org_pic_type_test),
	PARAM_UINT(enable_jpeg),
	PARAM_UINT(buf_number),
	PARAM_UINT(start_decode_buf_level),
	PARAM_UINT(rval),
	PARAM_UINT(without_display_mode),
	{ /* sentinel */ }
};
module_param_cb(params, &key_value_param_ops, &amvdec_mjpeg_v4l_params, 0644);
#endif

MEDIA_PARAM(debug_enable, uint, 0664);
MODULE_PARM_DESC(debug_enable, "\n debug enable\n");
MEDIA_PARAM(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level,
		"\n ammvdec_h264 pre_decode_buf_level\n");
MEDIA_PARAM(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_mmpeg12 udebug_flag\n");

MEDIA_PARAM(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n dynamic_buf_num_margin\n");

MEDIA_PARAM(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n ammvdec_mjpeg decode_timeout_val\n");

MEDIA_PARAM_ARRAY(max_process_time, uint, &max_decode_instance_num, 0664);

MEDIA_PARAM(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

MEDIA_PARAM(pscale_w, uint, 0664);
MODULE_PARM_DESC(pscale_w, "\npscale_w\n");

MEDIA_PARAM(pscale_h, uint, 0664);
MODULE_PARM_DESC(pscale_h, "\npscale_h\n");

MEDIA_PARAM(out_width, uint, 0664);
MODULE_PARM_DESC(out_width, "\nout_width\n");

MEDIA_PARAM(out_height, uint, 0664);
MODULE_PARM_DESC(out_height, "\nout_height\n");

MEDIA_PARAM(org_pic_type_test, uint, 0664);
MODULE_PARM_DESC(org_pic_type_test, "\norg_pic_type_test\n");

MEDIA_PARAM(enable_jpeg, uint, 0664);
MODULE_PARM_DESC(enable_jpeg, "\nenable_jpeg\n");

MEDIA_PARAM(buf_number, uint, 0664);
MODULE_PARM_DESC(buf_number, "\nbuf_number\n");

MEDIA_PARAM(start_decode_buf_level, uint, 0664);
MODULE_PARM_DESC(start_decode_buf_level, "\nstart_decode_buf_level\n");

MEDIA_PARAM(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

MEDIA_PARAM(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n without_display_mode\n");

module_init(ammvdec_mjpeg_driver_init_module);
module_exit(ammvdec_mjpeg_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MJMPEG Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
