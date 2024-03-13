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
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "vdec.h"

static const struct codec_profile_t *vcodec_profile[SUPPORT_VDEC_NUM] = { 0 };
static struct codec_profile_t driver_profile[VFORMAT_MAX * 2] = { 0 };

static int vcodec_profile_idx;

struct codec_profile {
	int (*codec_profile_reg)(struct codec_profile_t *);
};

ssize_t vcodec_profile_read(char *buf)
{
	char *pbuf = buf;
	int i = 0;

	for (i = 0; i < vcodec_profile_idx; i++) {
		pbuf += snprintf(pbuf, PAGE_SIZE - (pbuf - buf), "%s:%s;\n", vcodec_profile[i]->name,
						vcodec_profile[i]->profile);
	}

	return pbuf - buf;
}
EXPORT_SYMBOL(vcodec_profile_read);

int vcodec_profile_register(const struct codec_profile_t *vdec_profile)
{
	if (vcodec_profile_idx < SUPPORT_VDEC_NUM) {
		vcodec_profile[vcodec_profile_idx] = vdec_profile;
		vcodec_profile_idx++;
		pr_debug("regist %s codec profile\n", vdec_profile->name);
	}

	return 0;
}
EXPORT_SYMBOL(vcodec_profile_register);

bool is_support_profile(char *name)
{
	int ret = 0;
	int i, size = ARRAY_SIZE(vcodec_profile);

	for (i = 0; i < size; i++) {
		if (!vcodec_profile[i])
			break;
		if (!strcmp(name, vcodec_profile[i]->name))
			return true;
	}
	return ret;
}
EXPORT_SYMBOL(is_support_profile);

static int mpeg12_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (!is_support_format(VFORMAT_MPEG12)) {
		vdec_profile->name = "mpeg12_unsupport";
	}

	return 0;
}

static int mpeg4_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (!is_support_format(VFORMAT_MPEG4)) {
		vdec_profile->name = "mpeg4_unsupport";
	}

	return 0;
}

static int h264_codec_profile(struct codec_profile_t *vdec_profile)
{
	enum AM_MESON_CPU_MAJOR_ID cpu_major_id = get_cpu_major_id();

	if (is_support_format(VFORMAT_H264)) {
		if (vdec_is_support_4k()) {
			if (cpu_major_id >= AM_MESON_CPU_MAJOR_ID_TXLX) {
				vdec_profile->profile =
					"4k, dwrite, compressed, frame_dv, fence, multi_frame_dv";
			} else if (cpu_major_id >= AM_MESON_CPU_MAJOR_ID_GXTVBB) {
				vdec_profile->profile = "4k, frame_dv, fence, multi_frame_dv";
			}
		} else {
			if ((cpu_major_id == AM_MESON_CPU_MAJOR_ID_T5D) ||
				is_cpu_s4_s805x2() ||
				(cpu_major_id == AM_MESON_CPU_MAJOR_ID_TXHD2) ||
				is_cpu_s7_s805x3()) {
				vdec_profile->profile =
					"dwrite, compressed, frame_dv, multi_frame_dv";
			} else if (cpu_major_id != AM_MESON_CPU_MAJOR_ID_S1A) {
				vdec_profile->profile = "dwrite, compressed";
			}
		}
	} else {
		vdec_profile->name = "h264_unsupport";
	}

	return 0;
}

static int mjpeg_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (!is_support_format(VFORMAT_MJPEG)) {
		vdec_profile->name = "mjpeg_unsupport";
	}

	return 0;
}

static int vc1_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_VC1)) {
#if defined(CONFIG_ARCH_MESON) /*meson1 only support progressive */
		vdec_profile->profile = "progressive, wmv3";
#else
		vdec_profile->profile = "progressive, interlace, wmv3";
#endif
	} else {
		vdec_profile->name = "vc1_unsupport";
	}

	return 0;
}

static int avs_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_AVS)) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB)
			vdec_profile->profile = "mavs+";
	} else {
		vdec_profile->name = "avs_unsupport";
	}

	return 0;
}

static int h264mvc_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (!is_support_format(VFORMAT_H264MVC)) {
		vdec_profile->name = "h264mvc_unsupport";
	}

	return 0;
}

static int h265_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_HEVC)) {
		if (hevc_is_support_8k()) {
			vdec_profile->profile =
				"8k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, multi_frame_dv";
		} else if (hevc_is_support_4k()) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB) {
				vdec_profile->profile =
					"4k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, multi_frame_dv";
			} else {
				vdec_profile->profile = "4k";
			}
		} else {
			if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S1A) {
				vdec_profile->profile = "8bit, 10bit";
			} else {
				vdec_profile->profile =
					"8bit, 10bit, dwrite, compressed, frame_dv, multi_frame_dv";
			}
		}
	} else {
		vdec_profile->name = "hevc_unsupport";
	}

	return 0;
}

static bool is_support_4k_vp9(void)
{
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TXHD2)
		return false;
	if (hevc_is_support_4k())
		return true;
	return false;
}

static int vp9_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_VP9)) {
		if (hevc_is_support_8k()) {
			vdec_profile->profile =
				"8k, 10bit, dwrite, compressed, fence";
		} else if (is_support_4k_vp9()) {
			vdec_profile->profile =
				"4k, 10bit, dwrite, compressed, fence";
		} else {
			vdec_profile->profile =
				"10bit, dwrite, compressed, fence";
		}
	} else {
		vdec_profile->name = "vp9_unsupport";
	}

	return 0;
}

static int avs2_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_AVS2)) {
		if (hevc_is_support_8k()) {
			vdec_profile->profile = "8k, 10bit, dwrite, compressed";
		} else if (hevc_is_support_4k()) {
			vdec_profile->profile = "4k, 10bit, dwrite, compressed";
		} else {
			vdec_profile->profile = "10bit, dwrite, compressed";
		}
	} else {
		vdec_profile->name = "avs2_unsupport";
	}

	return 0;
}

static int av1_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_AV1)) {
		if (hevc_is_support_8k()) {
			vdec_profile->profile =
				"8k, 10bit, dwrite, compressed, no_head, frame_dv, multi_frame_dv, fence";
		} else if (hevc_is_support_4k()) {
			vdec_profile->profile =
				"4k, 10bit, dwrite, compressed, no_head, frame_dv, multi_frame_dv, fence";
		} else {
			vdec_profile->profile =
				"10bit, dwrite, compressed, no_head, frame_dv, multi_frame_dv, fence";
		}
	} else {
		vdec_profile->name = "av1_unsupport";
	}

	return 0;
}

static int avs3_codec_profile(struct codec_profile_t *vdec_profile)
{
	if (is_support_format(VFORMAT_AVS3)) {
		if (hevc_is_support_8k()) {
			vdec_profile->profile = "8k, 10bit, dwrite, compressed";
		} else if (hevc_is_support_4k()) {
			vdec_profile->profile = "4k, 10bit, dwrite, compressed";
		} else {
			vdec_profile->profile = "10bit, dwrite, compressed";
		}
	} else {
		vdec_profile->name = "avs3_unsupport";
	}

	return 0;
}

static struct codec_profile decoder_profile[VFORMAT_MAX] =
{
	[VFORMAT_MPEG12] = {
		.codec_profile_reg = mpeg12_codec_profile,
	},

	[VFORMAT_MPEG4] = {
		.codec_profile_reg = mpeg4_codec_profile,
	},

	[VFORMAT_H264] = {
		.codec_profile_reg = h264_codec_profile,
	},

	[VFORMAT_MJPEG] = {
		.codec_profile_reg = mjpeg_codec_profile,
	},

	[VFORMAT_VC1] = {
		.codec_profile_reg = vc1_codec_profile,
	},

	[VFORMAT_AVS] = {
		.codec_profile_reg = avs_codec_profile,
	},

	[VFORMAT_H264MVC] = {
		.codec_profile_reg = h264mvc_codec_profile,
	},

	[VFORMAT_HEVC] = {
		.codec_profile_reg = h265_codec_profile,
	},

	[VFORMAT_VP9] = {
		.codec_profile_reg = vp9_codec_profile,
	},

	[VFORMAT_AVS2] = {
		.codec_profile_reg = avs2_codec_profile,
	},

	[VFORMAT_AV1] = {
		.codec_profile_reg = av1_codec_profile,
	},

	[VFORMAT_AVS3] = {
		.codec_profile_reg = avs3_codec_profile,
	},
};

int vcodec_profile_register_v2(char *name,
		enum vformat_e vformat, int is_v4l)
{
	struct codec_profile_t *vdec_profile = NULL;

	if ((vformat < VFORMAT_MAX) &&
		(decoder_profile[vformat].codec_profile_reg != NULL)) {
		vdec_profile = &driver_profile[(vformat * 2) + is_v4l];
		vdec_profile->name = name;
		vdec_profile->profile = "";
		decoder_profile[vformat].codec_profile_reg(vdec_profile);
		vcodec_profile_register(vdec_profile);
	}

	return 0;
}
EXPORT_SYMBOL(vcodec_profile_register_v2);
