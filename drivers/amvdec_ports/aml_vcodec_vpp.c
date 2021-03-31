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

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <uapi/linux/sched/types.h>
#include "aml_vcodec_vpp.h"
#include "aml_vcodec_adapt.h"
#include "vdec_drv_if.h"

#define VPP_BUF_GET_IDX(vpp_buf) (vpp_buf->aml_buf->vb.vb2_buf.index)
#define INPUT_PORT 0
#define OUTPUT_PORT 1

extern int dump_vpp_input;

static enum DI_ERRORTYPE
	v4l_vpp_empty_input_done(struct di_buffer *buf)
{
	struct aml_v4l2_vpp *vpp = buf->caller_data;
	struct aml_v4l2_vpp_buf *vpp_buf;
	struct vdec_v4l2_buffer *fb = NULL;
	bool bypass = false;

	if (!vpp || !vpp->ctx) {
		pr_err("fatal %s %d vpp:%px\n",
			__func__, __LINE__, vpp);
		return DI_ERR_UNDEFINED;
	}

	vpp_buf	= container_of(buf, struct aml_v4l2_vpp_buf, di_buf);
	fb 	= &vpp_buf->aml_buf->frame_buffer;
	bypass	= (buf->flag & DI_FLAG_BUF_BY_PASS);

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
		"vpp_input done: vf:%px, idx: %d, flag:%x %s, in:%d, out:%d, vf:%d, vpp done:%d\n",
		buf->vf, buf->vf->index,
		buf->flag,
		bypass ? "bypass" : "",
		kfifo_len(&vpp->input),
		kfifo_len(&vpp->output),
		kfifo_len(&vpp->frame),
		kfifo_len(&vpp->out_done_q));

	if (!bypass) {
		/* recycle vf only in non-bypass mode */
		fb->put_vframe(fb->caller, vpp_buf->di_buf.vf);
	}

	vpp->out_num[INPUT_PORT]++;
	kfree(vpp_buf);
	return DI_ERR_NONE;
}

static enum DI_ERRORTYPE
	v4l_vpp_fill_output_done(struct di_buffer *buf)
{
	struct aml_v4l2_vpp *vpp = buf->caller_data;
	struct aml_v4l2_vpp_buf *vpp_buf = NULL;
	struct vdec_v4l2_buffer *fb = NULL;
	struct vdec_v4l2_buffer *fb_dec = NULL;
	struct vframe_s *vf_dec = NULL;
	bool bypass = false;

	if (!vpp || !vpp->ctx) {
	    pr_err("fatal %s %d vpp:%px\n",
		    __func__, __LINE__, vpp);
	    return DI_ERR_UNDEFINED;
	}

	vpp_buf	= container_of(buf, struct aml_v4l2_vpp_buf, di_buf);
	fb	= &vpp_buf->aml_buf->frame_buffer;
	bypass	= (buf->flag & DI_FLAG_BUF_BY_PASS);

	/* recovery fb handle. */
	buf->vf->v4l_mem_handle = (ulong)fb;

	if (bypass) {
		vf_dec = buf->vf->vf_ext;
		fb_dec = (struct vdec_v4l2_buffer *) vf_dec->v4l_mem_handle;

		fb_dec->is_vpp_bypass = true;
		fb->fill_buf_done(vpp->ctx, fb_dec);

		/* recycle output buffer directly */
		mutex_lock(&vpp->output_lock);
		kfifo_put(&vpp->frame, buf->vf);
		kfifo_put(&vpp->output, vpp_buf);
		mutex_unlock(&vpp->output_lock);
		up(&vpp->sem_out);
	} else {
		kfifo_put(&vpp->out_done_q, vpp_buf);

		fb->fill_buf_done(vpp->ctx, fb);
	}

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
		"vpp_output done: vf:%px, idx:%d, flag:%x %s, in:%d, out:%d, vf:%d, vpp done:%d\n",
		bypass ? vf_dec : buf->vf,
		bypass ? vf_dec->index : buf->vf->index,
		buf->flag, bypass ? "bypass" : "",
		kfifo_len(&vpp->input),
		kfifo_len(&vpp->output),
		kfifo_len(&vpp->frame),
		kfifo_len(&vpp->out_done_q));

	vpp->out_num[OUTPUT_PORT]++;
	return DI_ERR_NONE;
}

static void vpp_vf_get(void *caller, struct vframe_s **vf_out)
{
	struct aml_v4l2_vpp *vpp = (struct aml_v4l2_vpp *)caller;
	struct aml_v4l2_vpp_buf *vpp_buf = NULL;
	struct di_buffer *buf = NULL;
	struct vframe_s *vf = NULL;

	if (!vpp || !vpp->ctx) {
	    pr_err("fatal %s %d vpp:%px\n",
		    __func__, __LINE__, vpp);
	    return;
	}

	if (kfifo_get(&vpp->out_done_q, &vpp_buf)) {
		bool eos = false;

		buf	= &vpp_buf->di_buf;
		vf	= buf->vf;

		if (buf->flag & DI_FLAG_EOS) {
			v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
				"%s %d got eos\n",
				__func__, __LINE__);
			vf->type |= VIDTYPE_V4L_EOS;
			vf->flag = VFRAME_FLAG_EMPTY_FRAME_V4L;
			eos = true;
		}

		*vf_out = vf;

		v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
			"%s: vf:%px, index:%d, flag:%x, ts:%lld\n",
			__func__, vf, vf->index, buf->flag,
			vf->timestamp);
	}
}

static void vpp_vf_put(void *caller, struct vframe_s *vf)
{
	struct aml_v4l2_vpp *vpp = (struct aml_v4l2_vpp *)caller;
	struct vdec_v4l2_buffer *fb = NULL;
	struct aml_video_dec_buf *aml_buf = NULL;
	struct aml_v4l2_vpp_buf *vpp_buf = NULL;
	struct di_buffer *buf = NULL;

	fb	= (struct vdec_v4l2_buffer *) vf->v4l_mem_handle;
	aml_buf	= container_of(fb, struct aml_video_dec_buf, frame_buffer);
	vpp_buf	= (struct aml_v4l2_vpp_buf *) aml_buf->vpp_buf_handle;
	buf	= &vpp_buf->di_buf;

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
		"%s: vf:%px, index:%d, flag:%x, ts:%lld\n",
		__func__, vf, vf->index, buf->flag,
		vf->timestamp);

	mutex_lock(&vpp->output_lock);
	kfifo_put(&vpp->frame, vf);
	kfifo_put(&vpp->output, vpp_buf);
	mutex_unlock(&vpp->output_lock);
	up(&vpp->sem_out);
}

static int aml_v4l2_vpp_thread(void* param)
{
	struct aml_v4l2_vpp* vpp = param;
	struct aml_vcodec_ctx *ctx = vpp->ctx;

	v4l_dbg(ctx, V4L_DEBUG_VPP_DETAIL, "enter vpp thread\n");
	while (vpp->running) {
		bool eos = false;
		struct aml_v4l2_vpp_buf *in_buf;
		struct aml_v4l2_vpp_buf *out_buf = NULL;
		struct vframe_s *vf_out = NULL;
		struct vdec_v4l2_buffer *fb;

		if (down_interruptible(&vpp->sem_in))
			goto exit;
retry:
		if (!vpp->running)
			break;

		if (kfifo_is_empty(&vpp->output)) {
			if (down_interruptible(&vpp->sem_out))
				goto exit;
			goto retry;
		}

		mutex_lock(&vpp->output_lock);
		if (!kfifo_get(&vpp->output, &out_buf)) {
			mutex_unlock(&vpp->output_lock);
			v4l_dbg(ctx, 0, "vpp can not get output\n");
			goto exit;
		}
		mutex_unlock(&vpp->output_lock);

		/* bind v4l2 buffers */
		if (!out_buf->aml_buf) {
			struct vdec_v4l2_buffer *out;

			if (!ctx->fb_ops.query(&ctx->fb_ops, &vpp->fb_token)) {
				usleep_range(500, 550);
				mutex_lock(&vpp->output_lock);
				kfifo_put(&vpp->output, out_buf);
				mutex_unlock(&vpp->output_lock);
				goto retry;
			}

			if (ctx->fb_ops.alloc(&ctx->fb_ops, vpp->fb_token, &out, true)) {
				usleep_range(5000, 5500);
				mutex_lock(&vpp->output_lock);
				kfifo_put(&vpp->output, out_buf);
				mutex_unlock(&vpp->output_lock);
				goto retry;
			}

			out_buf->aml_buf = container_of(out,
				struct aml_video_dec_buf, frame_buffer);
			out_buf->aml_buf->vpp_buf_handle = (ulong) out_buf;
			v4l_dbg(ctx, V4L_DEBUG_VPP_BUFMGR,
				"vpp bind buf:%d to vpp_buf:%px\n",
				VPP_BUF_GET_IDX(out_buf), out_buf);

			out->caller	= vpp;
			out->put_vframe	= vpp_vf_put;
			out->get_vframe	= vpp_vf_get;

			out->m.mem[0].bytes_used = out->m.mem[0].size;
			out->m.mem[1].bytes_used = out->m.mem[1].size;
		}

		/* safe to pop in_buf */
		if (!kfifo_get(&vpp->input, &in_buf)) {
			v4l_dbg(ctx, 0, "vpp can not get input\n");
			goto exit;
		}

		if (in_buf->di_buf.flag & DI_FLAG_EOS)
			eos = true;

		mutex_lock(&vpp->output_lock);
		if (!kfifo_get(&vpp->frame, &vf_out)) {
			mutex_unlock(&vpp->output_lock);
			v4l_dbg(ctx, 0, "vpp can not get frame\n");
			goto exit;
		}
		mutex_unlock(&vpp->output_lock);

		fb = &out_buf->aml_buf->frame_buffer;
		fb->status = FB_ST_VPP;

		memcpy(vf_out->canvas0_config,
			in_buf->di_buf.vf->canvas0_config,
			2 * sizeof(struct canvas_config_s));
		vf_out->canvas0_config[0].phy_addr = fb->m.mem[0].addr;
		if (fb->num_planes == 1)
			vf_out->canvas0_config[1].phy_addr =
				fb->m.mem[0].addr + fb->m.mem[0].offset;
		else
			vf_out->canvas0_config[1].phy_addr =
				fb->m.mem[1].addr;

		if (eos)
			memset(vf_out, 0, sizeof(*vf_out));

		/* fill outbuf parms. */
		out_buf->di_buf.vf = vf_out;
		out_buf->di_buf.flag = 0;
		out_buf->di_buf.caller_data = vpp;

		/* fill inbuf parms. */
		in_buf->di_buf.caller_data = vpp;

		v4l_dbg(ctx, V4L_DEBUG_VPP_DETAIL,
			"vpp_handle start: dec vf:%px/%d, vpp vf:%px/%d, iphy:%lx/%lx %dx%d ophy:%lx/%lx %dx%d, %s "
			"in:%d, out:%d, vf:%d, vpp done:%d",
			in_buf->di_buf.vf, in_buf->di_buf.vf->index,
			out_buf->di_buf.vf, VPP_BUF_GET_IDX(out_buf),
			in_buf->di_buf.vf->canvas0_config[0].phy_addr,
			in_buf->di_buf.vf->canvas0_config[1].phy_addr,
			in_buf->di_buf.vf->canvas0_config[0].width,
			in_buf->di_buf.vf->canvas0_config[0].height,
			vf_out->canvas0_config[0].phy_addr,
			vf_out->canvas0_config[1].phy_addr,
			vf_out->canvas0_config[0].width,
			vf_out->canvas0_config[0].height,
			vpp->is_bypass_p ? "bypass-prog" : "",
			kfifo_len(&vpp->input),
			kfifo_len(&vpp->output),
			kfifo_len(&vpp->frame),
			kfifo_len(&vpp->out_done_q));

		if (vpp->is_bypass_p) {
			in_buf->di_buf.flag |= DI_FLAG_BUF_BY_PASS;

			out_buf->di_buf.flag = in_buf->di_buf.flag;
			out_buf->di_buf.vf->vf_ext = in_buf->di_buf.vf;
			v4l_vpp_fill_output_done(&out_buf->di_buf);

			v4l_vpp_empty_input_done(&in_buf->di_buf);
		} else {
			di_fill_output_buffer(vpp->di_handle, &out_buf->di_buf);
			di_empty_input_buffer(vpp->di_handle, &in_buf->di_buf);
		}
	}
exit:
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	v4l_dbg(ctx, V4L_DEBUG_VPP_DETAIL, "exit vpp thread\n");
	return 0;
}

int aml_v4l2_vpp_get_buf_num(u32 mode)
{
	if (mode == VPP_MODE_DI)
		return 4;
	//TODO: support more modes
	return 2;
}

int aml_v4l2_vpp_init(
		struct aml_vcodec_ctx *ctx,
		struct aml_vpp_cfg_infos *cfg,
		struct aml_v4l2_vpp** vpp_handle)
{
	struct di_init_parm init;
	u32 buf_size;
	int i, ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	struct aml_v4l2_vpp *vpp;

	if (!cfg || cfg->mode > VPP_MODE_MAX || !ctx || !vpp_handle)
		return -EINVAL;
	if (cfg->fmt != V4L2_PIX_FMT_NV12 && cfg->fmt != V4L2_PIX_FMT_NV12M &&
		cfg->fmt != V4L2_PIX_FMT_NV21 && cfg->fmt != V4L2_PIX_FMT_NV21M)
		return -EINVAL;

	vpp = kzalloc(sizeof(*vpp), GFP_KERNEL);
	if (!vpp)
		return -ENOMEM;

	init.work_mode = WORK_MODE_PRE_POST;
	init.buffer_mode = BUFFER_MODE_USE_BUF;
	init.ops.empty_input_done = v4l_vpp_empty_input_done;
	init.ops.fill_output_done = v4l_vpp_fill_output_done;
	init.caller_data = (void *)vpp;

	if ((cfg->fmt == V4L2_PIX_FMT_NV12) || (cfg->fmt == V4L2_PIX_FMT_NV12M))
		init.output_format = DI_OUTPUT_NV12 | DI_OUTPUT_LINEAR;
	else
		init.output_format = DI_OUTPUT_NV21 | DI_OUTPUT_LINEAR;

	if (cfg->is_drm)
		init.output_format |= DI_OUTPUT_TVP;

	vpp->di_handle = di_create_instance(init);
	if (vpp->di_handle < 0) {
		v4l_dbg(ctx, 0, "di_create_instance fail\n");
		ret = -EINVAL;
		goto error;
	}
	INIT_KFIFO(vpp->input);
	INIT_KFIFO(vpp->output);
	INIT_KFIFO(vpp->frame);
	INIT_KFIFO(vpp->out_done_q);
	vpp->ctx = ctx;
	vpp->is_bypass_p = cfg->is_bypass_p;

	buf_size = cfg->buf_size;
	vpp->buf_size = buf_size;

	/* setup output fifo */
	ret = kfifo_alloc(&vpp->output, buf_size, GFP_KERNEL);
	if (ret) {
		v4l_dbg(ctx, 0, "%s %d fail\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error2;
	}

	vpp->vbpool = kzalloc(buf_size * sizeof(*vpp->vbpool), GFP_KERNEL);
	if (!vpp->vbpool) {
		v4l_dbg(ctx, 0, "%s %d fail\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error3;
	}

	/* setup vframe fifo */
	ret = kfifo_alloc(&vpp->frame, buf_size, GFP_KERNEL);
	if (ret) {
		v4l_dbg(ctx, 0, "%s %d fail\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error4;
	}

	vpp->vfpool = kzalloc(buf_size * sizeof(*vpp->vfpool), GFP_KERNEL);
	if (!vpp->vfpool) {
		v4l_dbg(ctx, 0, "%s %d fail\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error5;
	}

	for (i = 0 ; i < buf_size ; i++) {
		kfifo_put(&vpp->output, &vpp->vbpool[i]);
		kfifo_put(&vpp->frame, &vpp->vfpool[i]);
	}

	mutex_init(&vpp->output_lock);
	sema_init(&vpp->sem_in, 0);
	sema_init(&vpp->sem_out, 0);

	vpp->wq = alloc_ordered_workqueue("aml-v4l-vpp",
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!vpp->wq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to create vpp workqueue\n");
		ret = -EINVAL;
		goto error6;
	}

	vpp->running = true;
	vpp->task = kthread_run(aml_v4l2_vpp_thread, vpp,
		"aml-%s", "aml-v4l2-vpp");
	if (IS_ERR(vpp->task)) {
		ret = PTR_ERR(vpp->task);
		goto error7;
	}
	sched_setscheduler_nocheck(vpp->task, SCHED_FIFO, &param);
	atomic_inc(&ctx->dev->vpp_count);

	*vpp_handle = vpp;
	return 0;
error7:
	destroy_workqueue(vpp->wq);
error6:
	kfree(vpp->vfpool);
error5:
	kfifo_free(&vpp->frame);
error4:
	kfree(vpp->vbpool);
error3:
	kfifo_free(&vpp->output);
error2:
	di_destroy_instance(vpp->di_handle);
error:
	kfree(vpp);
	return ret;
}
EXPORT_SYMBOL(aml_v4l2_vpp_init);

int aml_v4l2_vpp_destroy(struct aml_v4l2_vpp* vpp)
{
	struct aml_v4l2_vpp_buf* in_buf;

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
		"vpp destroy begin\n");
	vpp->running = false;
	up(&vpp->sem_in);
	up(&vpp->sem_out);
	kthread_stop(vpp->task);

	di_destroy_instance(vpp->di_handle);
	/* no more vpp callback below this line */

	flush_workqueue(vpp->wq);
	destroy_workqueue(vpp->wq);

	kfifo_free(&vpp->frame);
	kfree(vpp->vfpool);
	kfifo_free(&vpp->output);
	kfree(vpp->vbpool);
	while (kfifo_get(&vpp->input, &in_buf))
		kfree(in_buf);
	kfifo_free(&vpp->input);
	mutex_destroy(&vpp->output_lock);
	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
		"vpp destroy done\n");
	atomic_dec(&vpp->ctx->dev->vpp_count);
	kfree(vpp);

	return 0;
}
EXPORT_SYMBOL(aml_v4l2_vpp_destroy);

int aml_v4l2_vpp_push_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf)
{
	struct aml_v4l2_vpp_buf* in_buf;
	struct vdec_v4l2_buffer *fb = NULL;

	if (!vpp)
		return -EINVAL;

	in_buf = kzalloc(sizeof(*in_buf), GFP_ATOMIC);
	if (!in_buf)
		return -ENOMEM;

#if 0 //to debug di by frame
	if (vpp->in_num[INPUT_PORT] > 2)
		return 0;
	if (vpp->in_num[INPUT_PORT] == 2)
		vf->type |= VIDTYPE_V4L_EOS;
#endif

	if (vf->canvas0_config[0].block_mode == CANVAS_BLKMODE_LINEAR)
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;

	in_buf->di_buf.vf = vf;
	in_buf->di_buf.flag = 0;
	if (vf->type & VIDTYPE_V4L_EOS)
		in_buf->di_buf.flag |= DI_FLAG_EOS;

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
		"vpp_push_vframe: vf:%px, idx:%d, type:%x, ts:%lld\n",
		vf, vf->index, vf->type, vf->timestamp);

	fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
	in_buf->aml_buf = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	fb->vframe = (void *)vf;

	do {
		unsigned int dw_mode = VDEC_DW_NO_AFBC;
		struct file *fp;

		if (!dump_vpp_input || vpp->ctx->is_drm_mode)
			break;
		if (vdec_if_get_param(vpp->ctx, GET_PARAM_DW_MODE, &dw_mode))
			break;
		if (dw_mode == VDEC_DW_AFBC_ONLY)
			break;

		fp = filp_open("/data/dec_dump_before.raw",
				O_CREAT | O_RDWR | O_LARGEFILE | O_APPEND, 0600);
		if (!IS_ERR(fp)) {
			struct vb2_buffer *vb = &in_buf->aml_buf->vb.vb2_buf;

			kernel_write(fp,vb2_plane_vaddr(vb, 0),vb->planes[0].length, 0);
			if (in_buf->aml_buf->frame_buffer.num_planes == 2)
				kernel_write(fp,vb2_plane_vaddr(vb, 1),
						vb->planes[1].length, 0);
			dump_vpp_input--;
			filp_close(fp, NULL);
		}
	} while(0);

	kfifo_put(&vpp->input, in_buf);
	up(&vpp->sem_in);
	vpp->in_num[INPUT_PORT]++;
	return 0;
}
EXPORT_SYMBOL(aml_v4l2_vpp_push_vframe);

void fill_vpp_buf_cb(void *v4l_ctx, struct vdec_v4l2_buffer *fb)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)v4l_ctx;
	struct vframe_s *vf = NULL;
	int ret = -1;

	for (;;) {
		fb->get_vframe(fb->caller, &vf);

		if (vf == NULL)
			break;

		ret = aml_v4l2_vpp_push_vframe(ctx->vpp, vf);
		if (ret < 0) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"vpp push vframe err, ret: %d\n", ret);
		}
	}
}
