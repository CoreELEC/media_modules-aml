#ifndef _AML_VCODEC_INFO_SERVER_H_
#define _AML_VCODEC_INFO_SERVER_H_

#include "aml_vcodec_drv.h"

void aml_vcodec_decinfo_event_handler(struct aml_vcodec_ctx *ctx,
	int sub_cmd, void *data);
void aml_vcodec_dec_info_init(struct aml_vcodec_ctx *ctx);
void aml_vcodec_dec_info_deinit(struct aml_vcodec_ctx *ctx);
int aml_vcodec_decinfo_get(struct v4l2_ctrl *ctrl, struct aml_vcodec_ctx *ctx);

#endif
