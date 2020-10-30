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
#include "aml_vcodec_vpp.h"
#include "aml_vcodec_vfm.h"
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

	if (!vpp || !vpp->ctx) {
		pr_err("fatal %s %d vpp:%p\n",
			__func__, __LINE__, vpp);
		return DI_ERR_UNDEFINED;
	}

	vpp_buf = container_of(buf, struct aml_v4l2_vpp_buf, di_buf);
	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
			"vpp vf %p flag:%x done\n",
			buf->vf, buf->flag);

	if (!(buf->flag & DI_FLAG_BUF_BY_PASS)) {
		/* recycle vf only in non-bypass mode */
		vpp_vf_put(vpp->ctx->ada_ctx->recv_name,
			vpp_buf->di_buf.vf, vpp->ctx->id);
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

	if (!vpp || !vpp->ctx || !vpp->ctx->vfm) {
	    pr_err("fatal %s %d vpp:%p\n",
		    __func__, __LINE__, vpp);
	    return DI_ERR_UNDEFINED;
	}
	vpp_buf = container_of(buf, struct aml_v4l2_vpp_buf, di_buf);

	kfifo_put(&vpp->out_done_q, vpp_buf);
	queue_work(vpp->wq, &vpp->vpp_work);

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
		"vpp di out done idx:%d flag:%x\n",
		VPP_BUF_GET_IDX(vpp_buf), buf->flag);
	vpp->out_num[OUTPUT_PORT]++;
	return DI_ERR_NONE;
}

static void aml_vpp_disp_worker(struct work_struct *work)
{
	struct aml_v4l2_vpp *vpp =
		container_of(work, struct aml_v4l2_vpp, vpp_work);
	struct aml_v4l2_vpp_buf *vpp_buf = NULL;
	struct di_buffer *buf;
	struct vframe_s *vf;

	if (!vpp || !vpp->ctx || !vpp->ctx->vfm) {
	    pr_err("fatal %s %d vpp:%p\n",
		    __func__, __LINE__, vpp);
	    return;
	}
	while (kfifo_get(&vpp->out_done_q, &vpp_buf)) {
		bool eos = false;

		buf = &vpp_buf->di_buf;
		vf = buf->vf;

		if (buf->flag & DI_FLAG_EOS) {
			v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
				"%s %d got eos\n",
				__func__, __LINE__);
			vf->type |= VIDTYPE_V4L_EOS;
			vf->flag = VFRAME_FLAG_EMPTY_FRAME_V4L;
			eos = true;
		}

		v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
			"vpp output %p idx:%d flag:%x\n",
			buf, VPP_BUF_GET_IDX(vpp_buf), buf->flag);

		if (buf->flag & DI_FLAG_BUF_BY_PASS && !eos) {
			/* retrieve input buffer info */
			vf = buf->vf->vf_ext;

			v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
				"vpp bypass %p idx:%d\n",
				buf, VPP_BUF_GET_IDX(vpp_buf));

			/* recycle output buffer directly */
			mutex_lock(&vpp->output_lock);
			kfifo_put(&vpp->frame, buf->vf);
			kfifo_put(&vpp->output, vpp_buf);
			mutex_unlock(&vpp->output_lock);
			up(&vpp->sem_out);
		} else
			vf->v4l_mem_handle =
				(ulong) &vpp_buf->aml_buf->frame_buffer;

		vfq_push(&vpp->ctx->vfm->vf_que, vf);
		vdec_device_vf_run(vpp->ctx);

		v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
			"vpp send out idx:%d\n",
			VPP_BUF_GET_IDX(vpp_buf));
	}
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
			return -EINTR;
retry:
		if (!vpp->running)
			break;

		if (kfifo_is_empty(&vpp->output)) {
			if (down_interruptible(&vpp->sem_out))
				return -EINTR;
			goto retry;
		}

		mutex_lock(&vpp->output_lock);
		if (!kfifo_get(&vpp->output, &out_buf)) {
			mutex_unlock(&vpp->output_lock);
			v4l_dbg(ctx, 0, "vpp can not get output\n");
			return -EAGAIN;
		}
		mutex_unlock(&vpp->output_lock);

		/* bind v4l2 buffers */
		if (!out_buf->aml_buf) {
			struct vdec_v4l2_buffer *out;

			if (get_fb_from_queue(vpp->ctx, &out, true)) {
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
				"vpp bind buf:%d to vpp_buf:%p\n",
				VPP_BUF_GET_IDX(out_buf), out_buf);
			out->status = FB_ST_DISPLAY;
			out->m.mem[0].bytes_used = out->m.mem[0].size;
			out->m.mem[1].bytes_used = out->m.mem[1].size;
		}

		/* safe to pop in_buf */
		if (!kfifo_get(&vpp->input, &in_buf)) {
			v4l_dbg(ctx, 0, "vpp can not get input\n");
			return -EAGAIN;
		}

		if (in_buf->di_buf.flag & DI_FLAG_EOS)
			eos = true;

		mutex_lock(&vpp->output_lock);
		if (!kfifo_get(&vpp->frame, &vf_out)) {
			mutex_unlock(&vpp->output_lock);
			v4l_dbg(ctx, 0, "vpp can not get frame\n");
			return -EAGAIN;
		}
		mutex_unlock(&vpp->output_lock);

		fb = &out_buf->aml_buf->frame_buffer;
		memcpy(vf_out->canvas0_config,
			in_buf->di_buf.vf->canvas0_config,
			2 * sizeof(struct canvas_config_s));
		vf_out->canvas0_config[0].phy_addr = fb->m.mem[0].addr;
		vf_out->canvas0_config[1].phy_addr = fb->m.mem[1].addr;

		if (eos)
			memset(vf_out, 0, sizeof(*vf_out));

		out_buf->di_buf.vf = vf_out;
		out_buf->di_buf.flag = 0;
		out_buf->di_buf.caller_data = vpp;
		di_fill_output_buffer(vpp->di_handle, &out_buf->di_buf);

		v4l_dbg(ctx, V4L_DEBUG_VPP_DETAIL,
				"v4l vpp handle in vf:%p outp:%p/%d iphy:%x/%x %dx%d ophy:%x/%x %dx%d\n",
				in_buf->di_buf.vf, out_buf->di_buf.vf,
				VPP_BUF_GET_IDX(out_buf),
				in_buf->di_buf.vf->canvas0_config[0].phy_addr,
				in_buf->di_buf.vf->canvas0_config[1].phy_addr,
				in_buf->di_buf.vf->canvas0_config[0].width,
				in_buf->di_buf.vf->canvas0_config[0].height,
				vf_out->canvas0_config[0].phy_addr,
				vf_out->canvas0_config[1].phy_addr,
				vf_out->canvas0_config[0].width,
				vf_out->canvas0_config[0].height
				);
		in_buf->di_buf.caller_data = vpp;
		di_empty_input_buffer(vpp->di_handle, &(in_buf->di_buf));
	}
	v4l_dbg(ctx, V4L_DEBUG_VPP_DETAIL, "exit vpp thread\n");
	return 0;
}

int aml_v4l2_vpp_get_buf_num(u32 mode)
{
	if (mode == VPP_MODE_DI)
		return 6;
	//TODO: support more modes
	return 2;
}

int aml_v4l2_vpp_init(
		struct aml_vcodec_ctx *ctx,
		u32 mode,
		u32 fmt,
		struct aml_v4l2_vpp** vpp_handle)
{
	struct di_init_parm init;
	u32 buf_size;
	int i, ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	struct aml_v4l2_vpp *vpp;

	if (mode > VPP_MODE_MAX || !ctx || !vpp_handle)
		return -EINVAL;
	if (fmt != V4L2_PIX_FMT_NV12M && fmt != V4L2_PIX_FMT_NV21M)
		return -EINVAL;

	vpp = kzalloc(sizeof(*vpp), GFP_KERNEL);
	if (!vpp)
		return -ENOMEM;

	init.work_mode = WORK_MODE_PRE_POST;
	init.buffer_mode = BUFFER_MODE_USE_BUF;
	init.ops.empty_input_done = v4l_vpp_empty_input_done;
	init.ops.fill_output_done = v4l_vpp_fill_output_done;
	init.caller_data = (void *)vpp;

	if (fmt == V4L2_PIX_FMT_NV12M)
		init.output_format = DI_OUTPUT_NV12;
	else
		init.output_format = DI_OUTPUT_NV21;

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

	buf_size = aml_v4l2_vpp_get_buf_num(mode);
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
	INIT_WORK(&vpp->vpp_work, aml_vpp_disp_worker);

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
	kfree(vpp);
	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_DETAIL,
		"vpp destroy done\n");
	return 0;
}
EXPORT_SYMBOL(aml_v4l2_vpp_destroy);

int aml_v4l2_vpp_push_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf)
{
	struct aml_v4l2_vpp_buf* in_buf;
	struct vdec_v4l2_buffer *fb = NULL;

	if (!vpp)
		return -EINVAL;

	/* TODO: delete it after VPP supports bypass mode well */
	if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE &&
		vpp->work_mode == VPP_MODE_DI) {
		vfq_push(&vpp->ctx->vfm->vf_que, vf);
		vdec_device_vf_run(vpp->ctx);
		return 0;
	}

	in_buf = kzalloc(sizeof(*in_buf), GFP_KERNEL);
	if (!in_buf)
		return -ENOMEM;

#if 0 //to debug di by frame
	if (vpp->in_num[INPUT_PORT] > 2)
		return 0;
	if (vpp->in_num[INPUT_PORT] == 2)
		vf->type |= VIDTYPE_V4L_EOS;
#endif

	in_buf->di_buf.vf = vf;
	in_buf->di_buf.flag = 0;
	if (vf->type & VIDTYPE_V4L_EOS)
		in_buf->di_buf.flag |= DI_FLAG_EOS;

	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
		"vpp got input vf:%p type:%d ts:%llx\n",
		vf, vf->type, vf->timestamp);

	fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
	in_buf->aml_buf = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	fb->vf_handle = (ulong) vf;

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

int aml_v4l2_vpp_rel_vframe(struct aml_v4l2_vpp* vpp, struct vframe_s *vf)
{
	struct vdec_v4l2_buffer *fb = NULL;
	struct aml_video_dec_buf *aml_buf = NULL;
	struct aml_v4l2_vpp_buf *vpp_buf = NULL;

	fb = (struct vdec_v4l2_buffer *) vf->v4l_mem_handle;
	aml_buf = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	vpp_buf = (struct aml_v4l2_vpp_buf *) aml_buf->vpp_buf_handle;
	v4l_dbg(vpp->ctx, V4L_DEBUG_VPP_BUFMGR,
			"vpp rel output vf:%p index:%d\n",
			vf, VPP_BUF_GET_IDX(vpp_buf));

	mutex_lock(&vpp->output_lock);
	kfifo_put(&vpp->frame, vf);
	kfifo_put(&vpp->output, vpp_buf);
	mutex_unlock(&vpp->output_lock);
	up(&vpp->sem_out);
	vpp->in_num[OUTPUT_PORT]++;
	return 0;
}
EXPORT_SYMBOL(aml_v4l2_vpp_rel_vframe);
