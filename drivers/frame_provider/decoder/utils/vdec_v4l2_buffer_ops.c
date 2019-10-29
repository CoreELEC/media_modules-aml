#include "vdec_v4l2_buffer_ops.h"

int vdec_v4l_get_buffer(struct aml_vcodec_ctx *ctx,
	struct vdec_v4l2_buffer **out)
{
	int ret = -1;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->get_param(ctx->drv_handle,
		GET_PARAM_FREE_FRAME_BUFFER, out);

	return ret;
}
EXPORT_SYMBOL(vdec_v4l_get_buffer);

int vdec_v4l_set_pic_infos(struct aml_vcodec_ctx *ctx,
	struct aml_vdec_pic_infos *info)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->set_param(ctx->drv_handle,
		SET_PARAM_PIC_INFO, info);

	return ret;
}
EXPORT_SYMBOL(vdec_v4l_set_pic_infos);

int vdec_v4l_set_hdr_infos(struct aml_vcodec_ctx *ctx,
	struct aml_vdec_hdr_infos *hdr)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->set_param(ctx->drv_handle,
		SET_PARAM_HDR_INFO, hdr);

	return ret;
}
EXPORT_SYMBOL(vdec_v4l_set_hdr_infos);

int vdec_v4l_write_frame_sync(struct aml_vcodec_ctx *ctx)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->set_param(ctx->drv_handle,
		SET_PARAM_WRITE_FRAME_SYNC, NULL);

	return ret;
}
EXPORT_SYMBOL(vdec_v4l_write_frame_sync);

