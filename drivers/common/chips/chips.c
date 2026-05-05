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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>

#include "chips.h"
#include "decoder_cpu_ver_info.h"
#include "../register/register.h"
#include <linux/amlogic/media/utils/vformat.h>


bool check_efuse_chip(int vformat)
{
	unsigned int status, i = 0;
	int type[] = {15, 14, 11, 2}; /* avs2, vp9, h265, h264 */

	status =  (READ_EFUSE_REG(EFUSE_LIC2) >> 8 & 0xf);
	if (!status)
		return false;

	do {
		if ((status & 1) && (type[i] == vformat))
			return true;
		i++;
	} while (status >>= 1);

	return false;
}
EXPORT_SYMBOL(check_efuse_chip);



static int codec_profile_desc_init(char *desc, int format)
{
	// NOTE: vdec_feature.c:vcodec_feature_profile_and_level  is translated from follow array, please modify it together if need.
	const char *vformat_profile[] = {
		"MPEG-1, MPEG-2 MP/HL",
		"MPEG-4 ASP",
		"H.264 AVC HP",
		"MJPEG unlimited pixel resolution",
		"Real Softdec",
		"JPEG unlimited pixel resolution",
		"WMV/VC-1 SP/MP/AP",
		"AVS-P16(AVS+)/AVS-P2 JiZhun Profile",
		"YUV Softdec",
		"H.264(MVC) AVC HP",
		"H.264(4K/2K) AVC HP",
		"H.265 HEVC MP-10",
		"",
		"",
		"VP9 Profile-2",
		"AVS2 P2 Profile",
		"AV1 MP-10",
		"AVS3 Phase1",
		"H.266 VVC main10",
		"Unknown Format",
	};

	strncpy(desc, vformat_profile[format], PRO_LEVEL_LEN);

	return strlen(desc);
}

static int codec_level_idc_init(int format)
{
	u32 i, cpu, sub;
	static const u32 chip_level[][1 + VFORMAT_MAX] = {
		/* chip,                     mp2, mp4, 264, mjpg, real, jpg, vc1, avs, yuv, mvc, 2k4k, 265, 264enc, jpenc, vp9, avs2, av1, avs3, vvc, avbcd*/
		{AM_MESON_CPU_MAJOR_ID_G12A,  0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_G12B,  0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_GXLX2, 0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_SM1,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_TL1,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_TM2,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_SC2,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T5,    0,   5,   50,  0,    0,    0,   0,   0,   0,   50,  50,  51,  0,      0,     0,   0,    0,   0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T5D,   0,   5,   50,  0,    0,    0,   0,   0,   0,   50,  50,  41,  0,      0,     0,   0,    41,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T7,    0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    60,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S4,    0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T3,    0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    60,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S4D,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T5W,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S5,    0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  61,  0,      0,     61,  0,    61,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_GXLX3, 0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    0,   0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T5M,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T3X,   0,   5,   52,  0,    0,    0,   0,   0,   0,   52,  52,  60,  0,      0,     0,   0,    60,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_TXHD2, 0,   5,   42,  0,    0,    0,   0,   0,   0,   42,  42,  50,  0,      0,     0,   0,    0,   0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S1A,   0,   5,   42,  0,    0,    0,   0,   0,   0,   42,  42,  41,  0,      0,     0,   0,    0,   0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S7,    0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S7D,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_S6,    0,   5,   52,  0,    0,    0,   0,   0,   0,   52,  52,  52,  0,      0,     0,   0,    52,  0,    52,   0},
		{AM_MESON_CPU_MAJOR_ID_T6D,   0,   5,   42,  0,    0,    0,   0,   0,   0,   42,  42,  41,  0,      0,     0,   0,    41,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_GXLX4, 0,   5,   52,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MAJOR_ID_T6W,   0,   5,   52,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    51,   0},
		{AM_MESON_CPU_MAJOR_ID_T6X,   0,   5,   52,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    51,   0},
		{AM_MESON_CPU_MAJOR_ID_A9,    0,   5,   52,  0,    0,    0,   0,   0,   0,   51,  51,  52,  0,      0,     0,   0,    52,  0,    0,    0},
	};
	static const u32 sub_chip_level[][1 + VFORMAT_MAX] = {
		/* chip,                          mp2, mp4, 264, mjpg, real, jpg, vc1, avs, yuv, mvc, 2k4k, 265, 264enc, jpenc, vp9, avs2, av1, avs3, vvc, avbcd*/
		{AM_MESON_CPU_MINOR_ID_REVB_G12B,  0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,     0,  0,    0,    0},
		{AM_MESON_CPU_MINOR_ID_REVB_TM2,   0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    51,  0,    0,    0},
		{AM_MESON_CPU_MINOR_ID_S4_S805X2,  0,   5,   42,  0,    0,    0,   0,   0,   0,   42,  42,  41,  0,      0,     0,   0,    41,  0,    0,    0},
		{AM_MESON_CPU_MINOR_ID_T7C,        0,   5,   51,  0,    0,    0,   0,   0,   0,   51,  51,  51,  0,      0,     0,   0,    60,  0,    0,    0},
		{AM_MESON_CPU_MINOR_ID_S7_S805X3,  0,   5,   42,  0,    0,    0,   0,   0,   0,   42,  42,  41,  0,      0,     0,   0,    41,  0,    0,    0},
	};

	cpu = get_cpu_major_id();
	sub = get_cpu_sub_id();
	if (sub) {
		cpu |= (sub << 8);
		for (i = 0; i < sizeof(sub_chip_level)/((VFORMAT_MAX + 1) * sizeof(u32)); i++) {
			if (cpu == chip_level[i][0]) {
				return chip_level[i][1 + format];
			}
		}
	}
	cpu = get_cpu_major_id();
	for (i = 0; i < sizeof(chip_level)/((VFORMAT_MAX + 1) * sizeof(u32)); i++) {
		if (cpu == chip_level[i][0]) {
			return chip_level[i][1 + format];
		}
	}

	return 0;
}

void vcodec_profile_level_init(struct profile_level_t *plt)
{
	u32 i, len;
	char *desc;

	for (i = 0; i < VFORMAT_MAX; i++) {
		desc = &plt->profile_level_desc[i][0];

		if (!is_support_format(i))
			continue;

		len = codec_profile_desc_init(desc, i);

		plt->fmt_level[i] = codec_level_idc_init(i);
		if (plt->fmt_level[i]) {
			if (plt->fmt_level[i] > 9) {
				snprintf(desc + len, PRO_LEVEL_LEN - len,
					"@L%d.%d ", plt->fmt_level[i]/10, plt->fmt_level[i]%10);
			} else {
				snprintf(desc + len, PRO_LEVEL_LEN - len,
					"@L%d ", plt->fmt_level[i]);
			}
		}
	}
}

void show_profile_level_idc(struct profile_level_t *plt)
{
	u32 i;

	pr_info("vdec profile & level:");
	for (i = 0; i < VFORMAT_MAX; i++) {
		if (is_support_format(i))
			pr_info("\t%s\n", plt->profile_level_desc[i]);
	}
}


