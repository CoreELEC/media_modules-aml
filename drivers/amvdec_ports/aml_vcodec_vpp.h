/*
* Copyright (C) 2020 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef _AML_VCODEC_VPP_H_
#define _AML_VCODEC_VPP_H_

#define SUPPORT_V4L_VPP

#include <linux/kfifo.h>
#ifdef SUPPORT_V4L_VPP
#include <linux/amlogic/media/di/di_interface.h>
#endif
#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"

enum vpp_work_mode {
	VPP_MODE_DI,
	VPP_MODE_COLOR_CONV,
	VPP_MODE_NOISE_REDUC,
	VPP_MODE_MAX
};

#define VPP_FRAME_SIZE 32

struct aml_v4l2_vpp_buf {
#ifdef SUPPORT_V4L_VPP
	struct di_buffer di_buf;
#endif
	struct aml_video_dec_buf *aml_buf;
	bool is_bypass_p;
};

struct aml_v4l2_vpp {
	int di_handle; /* handle of DI */
	struct aml_vcodec_ctx *ctx;
	u32 buf_size; /* buffer size for vpp */
	u32 work_mode; /* enum vpp_work_mode */
	DECLARE_KFIFO(input, typeof(struct aml_v4l2_vpp_buf*), VPP_FRAME_SIZE);
	DECLARE_KFIFO_PTR(output, typeof(struct aml_v4l2_vpp_buf*));
	DECLARE_KFIFO_PTR(frame, typeof(struct vframe_s *));

	/* handle fill_output_done() in irq context */
	struct workqueue_struct	*wq;
	DECLARE_KFIFO(out_done_q, struct aml_v4l2_vpp_buf *, VPP_FRAME_SIZE);

	struct vframe_s *vfpool;
	struct aml_v4l2_vpp_buf *vbpool;
	struct task_struct *task;
	bool running;
	struct semaphore sem_in, sem_out;

	/* In p to i transition, output/frame can be multi writer */
	struct mutex output_lock;

	/* for debugging */
	/*
	 * in[0] --> vpp <-- in[1]
	 * out[0]<-- vpp --> out[1]
	 */
	int in_num[2];
	int out_num[2];
	ulong fb_token;
};

#ifdef SUPPORT_V4L_VPP
/* get number of buffer needed for a working mode */
int aml_v4l2_vpp_get_buf_num(u32 mode);
int aml_v4l2_vpp_init(
	struct aml_vcodec_ctx *ctx,
	u32 mode,
	u32 fmt,
	bool is_drm,
	struct aml_v4l2_vpp** vpp_handle);
int aml_v4l2_vpp_destroy(struct aml_v4l2_vpp* vpp);
int aml_v4l2_vpp_push_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf);
int aml_v4l2_vpp_rel_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf);
void fill_vpp_buf_cb(void *v4l_ctx, struct vdec_v4l2_buffer *fb);
#else
static inline int aml_v4l2_vpp_get_buf_num(u32 mode) { return -1; }
static inline int aml_v4l2_vpp_init(
	struct aml_vcodec_ctx *ctx,
	u32 mode,
	u32 fmt,
	struct aml_v4l2_vpp** vpp_handle) { return -1; }
static inline int aml_v4l2_vpp_destroy(struct aml_v4l2_vpp* vpp) { return -1; }
static inline int aml_v4l2_vpp_push_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf) { return -1; }
static inline int aml_v4l2_vpp_rel_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf) { return -1; }
static inline void fill_vpp_buf_cb(void *v4l_ctx, struct vdec_v4l2_buffer *fb) { return; }
#endif

#endif
