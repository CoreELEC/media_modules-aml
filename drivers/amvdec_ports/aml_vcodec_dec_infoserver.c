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

#include "aml_vcodec_dec_infoserver.h"
#include "aml_vcodec_dec.h"
#include "aml_vcodec_adapt.h"
#include "./decoder/utils.h"
#include "../frame_provider/decoder/utils/vdec.h"

void aml_vcodec_dec_info_init(struct aml_vcodec_ctx *ctx)
{
	int i;

	INIT_KFIFO(ctx->dec_intf.cc_free);
	INIT_KFIFO(ctx->dec_intf.cc_done);
	for (i = 0; i < USER_DATA_BUFF_NUM ; i++) {
		kfifo_put(&ctx->dec_intf.cc_free, &ctx->dec_intf.cc_pool[i]);
	}

	INIT_KFIFO(ctx->dec_intf.afd_free);
	INIT_KFIFO(ctx->dec_intf.afd_done);
	for (i = 0; i < USER_DATA_BUFF_NUM ; i++) {
		kfifo_put(&ctx->dec_intf.afd_free, &ctx->dec_intf.afd_pool[i]);
	}

	INIT_KFIFO(ctx->dec_intf.aux_free);
	INIT_KFIFO(ctx->dec_intf.aux_done);
	for (i = 0; i < USER_DATA_BUFF_NUM ; i++) {
		kfifo_put(&ctx->dec_intf.aux_free, &ctx->dec_intf.auxdata_pool[i]);
	}

	INIT_KFIFO(ctx->dec_intf.frm_free);
	INIT_KFIFO(ctx->dec_intf.frm_done);
	for (i = 0; i < FRM_INFO_BUFF_NUM ; i++) {
		kfifo_put(&ctx->dec_intf.frm_free, &ctx->dec_intf.frminfo_pool[i]);
	}

	return;
}

void aml_vcodec_dec_info_deinit(struct aml_vcodec_ctx *ctx)
{
	struct sei_usd_param_s *ud = NULL;

	while (kfifo_get(&ctx->dec_intf.afd_done, &ud)) {
		if (ud->v_addr) {
			vfree(ud->v_addr);
		}
		kfifo_put(&ctx->dec_intf.afd_free, ud);
	}

	while (kfifo_get(&ctx->dec_intf.cc_done, &ud)) {
		if (ud->v_addr) {
			vfree(ud->v_addr);
		}
		kfifo_put(&ctx->dec_intf.cc_free, ud);
	}

	while (kfifo_get(&ctx->dec_intf.aux_done, &ud)) {
		if (ud->v_addr) {
			vfree(ud->v_addr);
		}
		kfifo_put(&ctx->dec_intf.aux_free, ud);
	}

	return;
}

static void vcodec_afd_put(struct aml_vcodec_ctx *ctx,
	void *data)
{
	struct sei_usd_param_s *ud_in = NULL;
	struct sei_usd_param_s *ud = NULL;

	ud_in = (struct sei_usd_param_s *)data;

	if (kfifo_get(&ctx->dec_intf.afd_free, &ud)) {
		*ud = *ud_in;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "%s: data_len %d\n", __func__, ud->data_size);
		if (debug_mode & V4L_DEBUG_CODEC_EXINFO) {
			int i;
			for (i = 0; i < ud->data_size; i++) {
				pr_info("%02x ", ((char *)(ud->v_addr))[i]);
				if (((i + 1) & 0xf) == 0)
					pr_info("\n");
			}
		}
		kfifo_put(&ctx->dec_intf.afd_done, ud);
	}
}

static void vcodec_cc_data_put(struct aml_vcodec_ctx *ctx,
	void *data)
{
	struct sei_usd_param_s *ud_in = NULL;
	struct sei_usd_param_s *ud = NULL;

	ud_in = (struct sei_usd_param_s *)data;
	if (kfifo_get(&ctx->dec_intf.cc_free, &ud)) {
		*ud = *ud_in;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "%s: data_len %d\n", __func__, ud->data_size);
		if (debug_mode & V4L_DEBUG_CODEC_EXINFO) {
			int i;
			for (i = 0; i < ud->data_size; i++) {
				pr_info("%02x ", ((char *)(ud->v_addr))[i]);
				if (((i + 1) & 0xf) == 0)
					pr_info("\n");
			}
		}
		kfifo_put(&ctx->dec_intf.cc_done, ud);
	}
}

static void vcodec_aux_data_put(struct aml_vcodec_ctx *ctx,
	void *data)
{
	struct sei_usd_param_s *ud_in = NULL;
	struct sei_usd_param_s *ud = NULL;

	ud_in = (struct sei_usd_param_s *)data;
	if (kfifo_get(&ctx->dec_intf.aux_free, &ud)) {
		*ud = *ud_in;
		if (debug_mode & V4L_DEBUG_CODEC_EXINFO) {
			int i;
			for (i = 0; i < ud->data_size; i++) {
				pr_info("%02x ", ((char *)(ud->v_addr))[i]);
				if (((i + 1) & 0xf) == 0)
					pr_info("\n");
			}
		}
		kfifo_put(&ctx->dec_intf.aux_done, ud);
	}
}

static void vcodec_frame_data_put(struct aml_vcodec_ctx *ctx,
	void *data)
{
	struct dec_frame_info_s *frm_in = NULL;
	struct dec_frame_info_s *frm = NULL;

	frm_in = (struct dec_frame_info_s *)data;
	if (kfifo_get(&ctx->dec_intf.frm_free, &frm)) {
		*frm = *frm_in;
		if (debug_mode & V4L_DEBUG_CODEC_EXINFO) {
			pr_info("disp_w = %d disp_h = %d\n", frm->pic_width, frm->pic_height);
			pr_info("signal_info = 0x%x vf_type = %d\n", frm->signal_type, frm->vf_type);
		}
		kfifo_put(&ctx->dec_intf.frm_done, frm);
	}
}

static void vcodec_hdr10_data_put(struct aml_vcodec_ctx *ctx,
	void *data)
{
	struct aml_vdec_hdr_infos *in_data = NULL;

	in_data = &ctx->dec_intf.aux_static.hdr_info;
	*in_data = *((struct aml_vdec_hdr_infos *)data);
	if (debug_mode & V4L_DEBUG_CODEC_EXINFO) {
		int i, j;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++) {
				pr_info("primaries[%d][%d] %d \n", i, j, in_data->color_parms.primaries[i][j]);
			}
		}
		for (i = 0; i < 2; i++) {
			pr_info("white_point[%d] %d \n", i, in_data->color_parms.white_point[i]);
		}
		for (i = 0; i < 2; i++) {
			pr_info("luminance[%d] %d \n", i, in_data->color_parms.luminance[i]);
		}
		pr_info("max_content = %d max_pic_average = %d\n",
			in_data->color_parms.content_light_level.max_content,
			in_data->color_parms.content_light_level.max_pic_average);
	}
	return;
}

void aml_vcodec_decinfo_event_handler(struct aml_vcodec_ctx *ctx,
	int sub_cmd, void *data)
{
	ctx->dec_intf.dec_info_args.event_cnt = 1;
	switch (sub_cmd) {
	case AML_DECINFO_EVENT_AFD:
	{
		vcodec_afd_put(ctx, data);
		break;
	}
	case AML_DECINFO_EVENT_CC:
	{
		vcodec_cc_data_put(ctx, data);
		break;
	}
	case AML_DECINFO_EVENT_HDR10P:
	case AML_DECINFO_EVENT_CUVA:
	{
		vcodec_aux_data_put(ctx, data);
		break;
	}
	case AML_DECINFO_EVENT_FRAME:
	{
		vcodec_frame_data_put(ctx, data);
		break;
	}
	case AML_DECINFO_EVENT_STREAM:
	{
		break;
	}
	case AML_DECINFO_EVENT_STATISTIC:
	{
		break;
	}
	case AML_DECINFO_EVENT_HDR10:
	{
		vcodec_hdr10_data_put(ctx, data);
		break;
	}
	default:
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"unsupport dec info subcmd %x\n", sub_cmd);
		return;
	}

	ctx->dec_intf.dec_info_args.version_magic = 0x1234;
	ctx->dec_intf.dec_info_args.sub_cmd = sub_cmd;
	aml_vdec_dispatch_event(ctx, V4L2_EVENT_REPORT_DEC_INFO);
	return;
}

static int vcodec_get_data_afd(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	struct sei_usd_param_s *ud = NULL;
	struct sei_usd_param_s *ud_out = NULL;
	void __user *argp = NULL;
	int ret = 0;

	ud_out = &data->u.usd_param;
	argp = (void __user *)ud_out->data;

	if (kfifo_get(&ctx->dec_intf.afd_done, &ud)) {
		int copy_size = MIN(ud->data_size, ud_out->data_size);
		if ((!ud->v_addr) || copy_to_user(argp, ud->v_addr, copy_size)) {
			v4l_dbg(ctx, 0,
				"AFD data copy to user failed\n");
			ret = -EINVAL;
		}
		vfree(ud->v_addr);
		kfifo_put(&ctx->dec_intf.afd_free, ud);
	}

	ud_out->info_type = AML_AFD_TYPE;
	return ret;
}

static int vcodec_get_data_cc(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	struct sei_usd_param_s *ud = NULL;
	struct sei_usd_param_s *ud_out = NULL;
	void __user *argp = NULL;
	int ret = 0;

	ud_out = &data->u.usd_param;
	argp = (void __user *)ud_out->data;

	if (kfifo_get(&ctx->dec_intf.cc_done, &ud)) {
		int copy_size = MIN(ud->data_size, ud_out->data_size);
		if ((!ud->v_addr) || copy_to_user(argp, ud->v_addr, copy_size)) {
			v4l_dbg(ctx, 0,
				"AFD data copy to user failed\n");
			ret = -EINVAL;
		}
		vfree(ud->v_addr);
		kfifo_put(&ctx->dec_intf.cc_free, ud);
	}

	ud_out->info_type = AML_CC_TYPE;
	return ret;
}

static int vcodec_get_data_aux_data(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	struct sei_usd_param_s *ud = NULL;
	struct sei_usd_param_s *ud_out = NULL;
	void __user *argp = NULL;
	int ret = 0;

	ud_out = &data->u.usd_param;
	argp = (void __user *)ud_out->data;

	if ((!ud->v_addr) || kfifo_get(&ctx->dec_intf.aux_done, &ud)) {
		int copy_size = MIN(ud->data_size, ud_out->data_size);
		if (copy_to_user(argp, ud->v_addr, copy_size)) {
			v4l_dbg(ctx, 0,
				"AFD data copy to user failed\n");
			ret = -EINVAL;
		}
		vfree(ud->v_addr);
		kfifo_put(&ctx->dec_intf.aux_free, ud);
	}

	ud_out->info_type = AML_AUX_DATA_TYPE;
	return ret;
}

static int vcodec_get_data_frame(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	struct dec_frame_info_s *frm = NULL;
	struct dec_frame_info_s *frm_out = NULL;
	int ret = 0;

	frm_out = &data->u.frame_info;
	if (kfifo_get(&ctx->dec_intf.frm_done, &frm)) {
		memcpy(frm_out, frm, sizeof(struct dec_frame_info_s));
		kfifo_put(&ctx->dec_intf.frm_free, frm);
	}

	frm_out->info_type = AML_FRAME_TYPE;
	return ret;
}

static int vcodec_get_data_aux_data_hdr10(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	int ret = 0;
	struct aux_data_static_t *aux_out = NULL;

	aux_out = &data->u.aux_data;
	memcpy(&aux_out->hdr_info, &ctx->dec_intf.aux_static.hdr_info,
		sizeof(struct aml_vdec_hdr_infos));
	aux_out->info_type = AML_AUX_DATA_TYPE;
	return ret;
}

static int vcodec_get_data_statistic(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	int ret = 0;
	struct vdec_s *vdec = NULL;
	struct dec_statistics_info_s *statistic_out = NULL;
	struct vdec_info_statistic_s vstatistic_out = {0};

	vdec = ctx->ada_ctx->vdec;
	statistic_out = &data->u.decoder_statistics;
	statistic_out->info_ype = AML_STATISTIC_TYPE;
	if (vdec_status(vdec, &vstatistic_out) == -1) {
		ret = -1;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"%s : get statics info failed\n", __func__);
		return ret;
	}
	statistic_out->total_decoded_frames = vstatistic_out.vstatus.frame_count;
	statistic_out->error_frames = vstatistic_out.vstatus.error_count;
	statistic_out->drop_frames = vstatistic_out.vstatus.drop_frame_count;
	statistic_out->i_decoded_frames = vstatistic_out.vstatus.i_decoded_frames;
	statistic_out->i_error_frames = vstatistic_out.vstatus.i_concealed_frames;
	statistic_out->i_drop_frames = vstatistic_out.vstatus.i_lost_frames;
	statistic_out->p_decoded_frames = vstatistic_out.vstatus.p_decoded_frames;
	statistic_out->p_error_frames = vstatistic_out.vstatus.p_concealed_frames;
	statistic_out->p_drop_frames = vstatistic_out.vstatus.p_lost_frames;
	statistic_out->b_decoded_frames = vstatistic_out.vstatus.b_decoded_frames;
	statistic_out->b_error_frames = vstatistic_out.vstatus.b_concealed_frames;
	statistic_out->b_drop_frames = vstatistic_out.vstatus.b_lost_frames;
	statistic_out->total_decoded_datasize = vstatistic_out.vstatus.frame_data;
	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"total_decoded_frames %u error_frames %u drop_frames %u\n",
		statistic_out->total_decoded_frames,
		statistic_out->error_frames,
		statistic_out->drop_frames);
	return ret;
}

static int vcodec_get_data_stream(struct aml_vcodec_ctx *ctx,
	struct vdec_common_s *data)
{
	memcpy(&data->u.stream_info, &ctx->dec_intf.dec_stream,
		sizeof(struct dec_stream_info_s));
	data->u.stream_info.info_type = AML_STREAM_TYPE;

	return 0;
}

int aml_vcodec_decinfo_get(struct v4l2_ctrl *ctrl,
	struct aml_vcodec_ctx *ctx)
{
	struct vdec_common_s *info = NULL;
	int sub_cmd;
	int ret = 0;

	if (ctx == NULL || ctrl == NULL) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"pointer param invalid at %d\n", __LINE__);
		return -1;
	}

	info = &ctx->dec_intf.dec_comm;
	sub_cmd = info->type;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"%s\n get cmd : %x", __func__, sub_cmd);

	switch (sub_cmd) {
	case AML_DECINFO_GET_AFD_TYPE:
	{
		ret = vcodec_get_data_afd(ctx, info);
		break;
	}
	case AML_DECINFO_GET_HDR10P_TYPE:
	case AML_DECINFO_GET_CUVA_TYPE:
	case AML_DECINFO_GET_DV_TYPE:
	{
		ret = vcodec_get_data_aux_data(ctx, info);
		break;
	}
	case AML_DECINFO_GET_HDR10_TYPE:
	{
		ret = vcodec_get_data_aux_data_hdr10(ctx, info);
		break;
	}
	case AML_DECINFO_GET_CC_TYPE:
	{
		ret = vcodec_get_data_cc(ctx, info);
		break;
	}
	case AML_DECINFO_GET_STREAM_TYPE:
	{
		ret = vcodec_get_data_stream(ctx, info);
		break;
	}
	case AML_DECINFO_GET_STATISTIC_TYPE:
	{
		ret = vcodec_get_data_statistic(ctx, info);
		break;
	}
	case AML_DECINFO_GET_FRAME_TYPE:
	{
		ret = vcodec_get_data_frame(ctx, info);
		break;
	}
	default:
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"unsupport dec info subcmd %x\n", sub_cmd);
		return -1;
	}
	info->vdec_id = ctx->id;
	memcpy(ctrl->p_new.p, info, sizeof(struct vdec_common_s));

	return ret;
}

