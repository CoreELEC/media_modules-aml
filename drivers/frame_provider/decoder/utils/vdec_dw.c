/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/utils/vformat.h>
#include "../../../common/chips/chips.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "vdec_dw.h"

enum vdec_dec_mode {
	DM_INVALID		= 0,
	DM_AVBC_ONLY		= 0,
	DM_YUV_1_1_AVBC		= 1,
	DM_YUV_1_4_AVBC_A	= 2,
	DM_YUV_1_4_AVBC_B	= 3,
	DM_YUV_1_2_AVBC		= 4,
	DM_YUV_1_8_AVBC		= 8,
	DM_YUV_ONLY		= 0x10,
	DM_AVBC_1_1		= 0x21,
	DM_AVBC_1_4		= 0x22,
	DM_AVBC_1_2		= 0x24,
	DM_YUV_AUTO_1_2_AVBC	= 0x100,
	DM_YUV_AUTO_1_4_AVBC	= 0x200,
	DM_YUV_AUTO_1_2_AVBC_B	= 0x300,
	/* (0~540] 1/1, (540~1080] 1/4, (1080~4K] 1/16 */
	DM_YUV_AUTO_14_12_AVBC	= 0x400,
	DM_YUV_1_1_10BIT_AVBC	= 0x10001,
	DM_YUV_1_4_10BIT_AVBC	= 0x10003,
	DM_YUV_1_2_10BIT_AVBC	= 0x10004,
	DM_YUV_1_8_10BIT_AVBC	= 0x10008,
	DM_YUV_P010_ONLY	= 0x10010,
	/* (0~1080] 1/1, (1080~4K] 1/16 */
	DM_YUV_14_11_10BIT_AVBC	= 0x10200,
};

static const int avbc_dw_1_1[] = {
	DM_AVBC_ONLY,
	DM_YUV_1_1_AVBC,
	DM_YUV_1_4_AVBC_A,
	DM_YUV_1_4_AVBC_B,
	DM_YUV_1_2_AVBC,
	DM_YUV_1_8_AVBC,
	DM_YUV_AUTO_1_2_AVBC,
	DM_YUV_AUTO_1_4_AVBC,
	DM_YUV_AUTO_1_2_AVBC_B,
	DM_YUV_AUTO_14_12_AVBC,
	DM_YUV_1_1_10BIT_AVBC,
	DM_YUV_1_4_10BIT_AVBC,
	DM_YUV_1_2_10BIT_AVBC,
	DM_YUV_1_8_10BIT_AVBC,
	DM_YUV_14_11_10BIT_AVBC,
	DM_AVBC_1_1
};

enum vformat_e v4l2_fourcc_to_vtype(uint32_t fourcc) {
	switch (fourcc) {
		case V4L2_PIX_FMT_H264:
			return VFORMAT_H264;
		case V4L2_PIX_FMT_HEVC:
			return VFORMAT_HEVC;
		case V4L2_PIX_FMT_VP9:
			return VFORMAT_VP9;
		case V4L2_PIX_FMT_MPEG2:
			return VFORMAT_MPEG12;
		case V4L2_PIX_FMT_MPEG4:
			return VFORMAT_MPEG4;
		case V4L2_PIX_FMT_MJPEG:
			return VFORMAT_MJPEG;
		case V4L2_PIX_FMT_AV1:
		case V4L2_PIX_FMT_AV01:
			return VFORMAT_AV1;
		case V4L2_PIX_FMT_AVS:
			return VFORMAT_AVS;
		case V4L2_PIX_FMT_AVS2:
			return VFORMAT_AVS2;
		case V4L2_PIX_FMT_AVS3:
			return VFORMAT_AVS3;
		case V4L2_PIX_FMT_VC1_ANNEX_G:
			return VFORMAT_VC1;
		case V4L2_PIX_FMT_VC1_ANNEX_L:
			return VFORMAT_VC1;
		case V4L2_PIX_FMT_H266:
			return VFORMAT_H266;
		case V4L2_PIX_FMT_MPEG1:
			return VFORMAT_MPEG12;
		case V4L2_PIX_FMT_JPEG:
			return VFORMAT_JPEG;
	}
	return VFORMAT_MAX;
}
EXPORT_SYMBOL(v4l2_fourcc_to_vtype);


static const char * const format_name[] = {
	"MPEG12",
	"MPEG4",
	"H264",
	"MJPEG",
	"REAL",
	"JPEG",
	"VC1",
	"AVS",
	"YUV",
	"H264MVC",
	"H264_4K2K",
	"HEVC",
	"H264_ENC",
	"JPEG_ENC",
	"VP9",
	"AVS2",
	"AV1",
	"AVS3",
	"H266",
	"AVBCD"
};

static bool is_only_support_yuv_fmt(int format)
{
	switch (format) {
		case VFORMAT_MPEG12:
		case VFORMAT_MPEG4:
		case VFORMAT_MJPEG:
		case VFORMAT_JPEG:
		case VFORMAT_VC1:
		case VFORMAT_AVS:
			return true;
		default:
			return false;
	}
}

static bool is_unsupport_dw_format(int format)
{
	if (!is_support_format(format))
		return true;

	switch (format) {
		case VFORMAT_H264MVC:
		case VFORMAT_AVBCD:
			return true;
		default:
			return false;
	}
}

static struct dos_hw_dw_mode chip_dw_mode[VFORMAT_MAX];
static struct dos_hw_output_mode chip_output_mode[VFORMAT_MAX];

struct dos_hw_dw_mode* get_dos_dw_mode_all(void)
{
	return chip_dw_mode;
}
EXPORT_SYMBOL(get_dos_dw_mode_all);

int *get_dos_dw_mode(enum vformat_e fmt)
{
	if (!is_unsupport_dw_format(fmt))
		return chip_dw_mode[fmt].hw_dw_mode;
	else
		return NULL;
}
EXPORT_SYMBOL(get_dos_dw_mode);

struct dos_hw_output_mode* get_dos_output_mode_all(void)
{
	return chip_output_mode;
}
EXPORT_SYMBOL(get_dos_output_mode_all);

struct map_item {
	unsigned int bit;
	int dm;
	enum scalar_mode scalar;
	enum vformat_e fmt;
};

/* table entries: add/change here to extend behavior */
static const struct map_item map_table[] = {
	{ YUV_1_1, DM_YUV_1_1_AVBC,   SCALAR_1_1,  VFORMAT_MAX },
	{ YUV_1_2, DM_YUV_1_2_AVBC,   SCALAR_1_4,  VFORMAT_MAX },
	{ YUV_1_4, DM_YUV_1_4_AVBC_A, SCALAR_1_16, VFORMAT_MAX },
	{ YUV_1_4, DM_YUV_1_4_AVBC_B, SCALAR_1_16, VFORMAT_MAX },
	{ YUV_1_8, DM_YUV_1_8_AVBC,   SCALAR_1_64, VFORMAT_MAX },

	{ AVBC_1_1, DM_AVBC_1_1, SCALAR_1_1,  VFORMAT_MAX },
	{ AVBC_1_2, DM_AVBC_1_2, SCALAR_1_4,  VFORMAT_MAX },
	{ AVBC_1_4, DM_AVBC_1_4, SCALAR_1_16, VFORMAT_MAX },

	{ P010_1_1, DM_YUV_1_1_10BIT_AVBC, SCALAR_1_1,  VFORMAT_MAX },
	{ P010_1_2, DM_YUV_1_2_10BIT_AVBC, SCALAR_1_4,  VFORMAT_MAX },
	{ P010_1_4, DM_YUV_1_4_10BIT_AVBC, SCALAR_1_16, VFORMAT_MAX },
	{ P010_1_8, DM_YUV_1_8_10BIT_AVBC, SCALAR_1_64, VFORMAT_MAX },

	{ AV1_YUV_ONLY,  DM_YUV_ONLY, SCALAR_1_1, VFORMAT_AV1 },
	{ H266_YUV_ONLY, DM_YUV_ONLY, SCALAR_1_1, VFORMAT_H266 },
};

static void push_mode_if_not_present(struct dos_hw_dw_mode *dw_mode,
	int fmt_index, int dm)
{
	int i;

	for (i = 0; i < 32; i++) {
		/* already present */
		if (dw_mode[fmt_index].hw_dw_mode[i] == dm)
			return;

		/* found insertion slot */
		if (dw_mode[fmt_index].hw_dw_mode[i] == -1) {
			dw_mode[fmt_index].hw_dw_mode[i] = dm;
			break;
		}
	}
}

void fill_dw_mode_from_hw_flag(struct dos_hw_dw_mode *dw_mode,
	int fmt_index, unsigned int hw_flag, unsigned int mask)
{
	int map_n, i;

	/* walk mapping table */
	map_n = sizeof(map_table) / sizeof(map_table[0]);

	for (i = 0; i < map_n; i++) {
		if (!(hw_flag & map_table[i].bit) ||
			(mask && !(mask & map_table[i].bit)))
			continue; /* hw doesn't support this bit */

		if ((map_table[i].fmt == VFORMAT_MAX) ||
			(map_table[i].fmt == dw_mode[fmt_index].fmt))
			push_mode_if_not_present(dw_mode, fmt_index, map_table[i].dm);
	}
}

void fill_output_mode_from_hw_flag(struct dos_hw_output_mode *output_mode,
	int fmt_index, unsigned int hw_flag, unsigned int mask)
{
	int map_n, i;

	/* walk mapping table */
	map_n = sizeof(map_table) / sizeof(map_table[0]);

	output_mode[fmt_index].mode[0].format = FORMAT_PIX_NV12;
	output_mode[fmt_index].mode[1].format = FORMAT_AVBC;
	if (is_support_p010_mode() && !mask)
		output_mode[fmt_index].mode[2].format = FORMAT_P010;

	for (i = 0; i < map_n; i++) {
		if (!(hw_flag & map_table[i].bit) ||
			(mask && !(mask & map_table[i].bit)))
			continue; /* hw doesn't support this bit */

		if ((map_table[i].fmt == VFORMAT_MAX) ||
			(map_table[i].fmt == output_mode[fmt_index].fmt)) {
			if (map_table[i].bit & YUV_BIT_MASK) {
				output_mode[fmt_index].mode[0].scalar |= map_table[i].scalar;

				if (map_table[i].bit & YUV_1_1) {
					output_mode[fmt_index].mode[1].scalar |= map_table[i].scalar;
					output_mode[fmt_index].mode[1].flags |= FLAG_AVBCD;
				}
			} else if (map_table[i].bit & AVBC_BIT_MASK) {
				output_mode[fmt_index].mode[1].scalar |= map_table[i].scalar;
				output_mode[fmt_index].mode[1].flags |= FLAG_AVBCD;
			} else if (map_table[i].bit & P010_BIT_MASK) {
				output_mode[fmt_index].mode[2].scalar |= map_table[i].scalar;
				output_mode[fmt_index].mode[2].flags |= FLAG_AVBCD;
			}
		}
	}
}

static const char* format_to_str(u32 format)
{
	switch (format) {
		case FORMAT_PIX_NV12:
			return "FORMAT_PIX_NV12";
		case FORMAT_AVBC:
			return "FORMAT_AVBC";
		case FORMAT_P010:
			return "FORMAT_P010";
		default:
			return "";
	}
}

static void print_scalar(u32 scalar)
{
	if (scalar & SCALAR_1_1) {
		pr_cont("SCALAR_1_1");
	}
	if (scalar & SCALAR_1_4) {
		pr_cont(" | SCALAR_1_4");
	}
	if (scalar & SCALAR_1_16) {
		pr_cont(" | SCALAR_1_16");
	}
	if (scalar & SCALAR_1_64) {
		pr_cont(" | SCALAR_1_64");
	}
}

void pr_dw_infos(void)
{
	int fmt;
	int i;
	struct dos_hw_dw_mode *dw_mode = get_dos_dw_mode_all();
	struct dos_hw_output_mode *output_mode = get_dos_output_mode_all();

	for (fmt = 0; fmt < VFORMAT_MAX; fmt++) {
		if (is_unsupport_dw_format(fmt))
			continue;

		pr_cont("%s\n", format_name[fmt]);

		pr_cont("DoubleWrite  : [");
		for (i = 0; i < 32; i++) {
			if (dw_mode[fmt].hw_dw_mode[i] != -1)
				pr_cont(" 0x%x", dw_mode[fmt].hw_dw_mode[i]);
		}
		pr_cont(" ]\n");

		pr_cont("OutputMode   :\n");
		for (i = 0; i < 3; i++) {
			pr_cont("    format   : %s\n",
				format_to_str(output_mode[fmt].mode[i].format));

			pr_cont("    scalar   : ");
			print_scalar(output_mode[fmt].mode[i].scalar);

			pr_cont("\n    flag     : %s\n\n",
				(output_mode[fmt].mode[i].flags & FLAG_AVBCD) ? "FLAG_AVBCD" : "");
		}
	}
}

static void dos_dw_mode_param_init(void)
{
	int fmt;
	struct dos_hw_dw_mode *dw_mode = get_dos_dw_mode_all();
	struct dos_hw_output_mode *output_mode = get_dos_output_mode_all();

	for (fmt = 0; fmt < VFORMAT_MAX; fmt++) {
		dw_mode[fmt].fmt = fmt;
		output_mode[fmt].fmt = fmt;
		memset(dw_mode[fmt].hw_dw_mode, -1, sizeof(dw_mode[fmt].hw_dw_mode));

		switch (fmt) {
			case VFORMAT_MPEG12:
			case VFORMAT_MPEG4:
			case VFORMAT_MJPEG:
			case VFORMAT_JPEG:
			case VFORMAT_VC1:
			case VFORMAT_AVS:
				dw_mode[fmt].hw_dw_mode[0] = DM_YUV_ONLY;
				output_mode[fmt].mode[0].format = FORMAT_PIX_NV12;
				output_mode[fmt].mode[0].scalar = SCALAR_1_1;
				break;
			case VFORMAT_H264:
				dw_mode[fmt].hw_dw_mode[0] = DM_AVBC_ONLY;
				dw_mode[fmt].hw_dw_mode[1] = DM_YUV_ONLY;
				output_mode[fmt].mode[0].format = FORMAT_PIX_NV12;
				output_mode[fmt].mode[0].scalar = SCALAR_1_1;
				break;
			case VFORMAT_HEVC:
			case VFORMAT_VP9:
				dw_mode[fmt].hw_dw_mode[0] = DM_AVBC_ONLY;
				dw_mode[fmt].hw_dw_mode[1] = DM_YUV_ONLY;
				break;
			case VFORMAT_AVS2:
			case VFORMAT_AV1:
			case VFORMAT_AVS3:
			case VFORMAT_H266:
				dw_mode[fmt].hw_dw_mode[0] = DM_AVBC_ONLY;
				break;
			default:
				break;
		}
	}
}

void vdec_dos_dw_mode_init(void)
{
	int fmt;
	u32 dw_cap = get_dos_dw_capability();
	struct dos_hw_dw_mode *dw_mode = get_dos_dw_mode_all();
	struct dos_hw_output_mode *output_mode = get_dos_output_mode_all();

	dos_dw_mode_param_init();

	for (fmt = 0; fmt < VFORMAT_MAX; fmt++) {
		if (is_unsupport_dw_format(fmt) ||
			is_only_support_yuv_fmt(fmt))
			continue;

		if (fmt == VFORMAT_H264) {
			fill_dw_mode_from_hw_flag(dw_mode, fmt, dw_cap, YUV_1_2_4);
			fill_output_mode_from_hw_flag(output_mode, fmt, dw_cap, YUV_1_2_4);
		} else if (is_core_hevc_fmt(fmt)) {
			fill_dw_mode_from_hw_flag(dw_mode, fmt, dw_cap, 0);
			fill_output_mode_from_hw_flag(output_mode, fmt, dw_cap, 0);
		}
	}
}

struct output_mode *get_output_mode_by_fmt(enum vformat_e fmt)
{
	int i;
	struct dos_hw_output_mode *output_modes = get_dos_output_mode_all();

	for (i = 0; i < VFORMAT_MAX; i++) {
		if (output_modes[i].fmt == fmt) {
			return output_modes[i].mode;
		}
	}

	return 0;
}
EXPORT_SYMBOL(get_output_mode_by_fmt);

void parse_signal_output_mode(struct output_mode *output_mode, int *dw_list, int *list_size)
{
	u32 format = output_mode->format;
	u32 scalar = output_mode->scalar;
	int count = 0;

	if (format == 0) {
		*list_size = 0;
		return;
	}

	if (format & FORMAT_PIX_YUV_MASK) {
		if (scalar & SCALAR_1_1) {
			dw_list[count++] = DM_YUV_ONLY;
			dw_list[count++] = DM_YUV_1_1_AVBC;
		}
		if (scalar & SCALAR_1_4) {
			dw_list[count++] = DM_YUV_1_2_AVBC;
		}
		if (scalar & SCALAR_1_16) {
			dw_list[count++] = DM_YUV_1_4_AVBC_A;
			dw_list[count++] = DM_YUV_1_4_AVBC_B;
		}
		if (scalar & SCALAR_1_64)
			dw_list[count++] = DM_YUV_1_8_AVBC;
		if (scalar & SCALAR_AUTO_1080P_1_4)
			dw_list[count++] = DM_YUV_AUTO_1_2_AVBC;
		if (scalar & SCALAR_AUTO_1080P_1_16)
			dw_list[count++] = DM_YUV_AUTO_1_4_AVBC;
		if (scalar & SCALAR_AUTO_720P_1_4)
			dw_list[count++] = DM_YUV_AUTO_1_2_AVBC_B;
		if (scalar & SCALAR_AUTO_540P)
			dw_list[count++] = DM_YUV_AUTO_14_12_AVBC;
	}

    if (format & FORMAT_AVBC) {
		if (scalar & SCALAR_1_1) {
			memcpy(&dw_list[count], avbc_dw_1_1, sizeof(avbc_dw_1_1));
			count += sizeof(avbc_dw_1_1) / sizeof(avbc_dw_1_1[0]);
		}
		if (scalar & SCALAR_1_4)
			dw_list[count++] = DM_AVBC_1_2;
		if (scalar & SCALAR_1_16)
			dw_list[count++] = DM_AVBC_1_4;
	}

	if (format & FORMAT_P010) {
		if (scalar & SCALAR_1_1)
			dw_list[count++] = DM_YUV_1_1_10BIT_AVBC;
		if (scalar & SCALAR_1_4)
			dw_list[count++] = DM_YUV_1_2_10BIT_AVBC;
		if (scalar & SCALAR_1_16)
			dw_list[count++] = DM_YUV_1_4_10BIT_AVBC;
		if (scalar & SCALAR_1_64)
			dw_list[count++] = DM_YUV_1_8_10BIT_AVBC;
		if (scalar & SCALAR_AUTO_1080P_1_16)
			dw_list[count++] = DM_YUV_14_11_10BIT_AVBC;
	}

	*list_size = count;
}

static int dw_list_intersection(int *list1, int size1, int *list2, int size2, int *result_list)
{
	int i, j, k;
	int count = 0;
	int value;
	int in_list2;
	int found;

	if (size1 <= 0 || size2 <= 0) {
		return 0;
	}

	for (i = 0; i < size1; i++) {
		value = list1[i];
		in_list2 = 0;
		for (j = 0; j < size2; j++) {
			if (value == list2[j]) {
				in_list2 = 1;
				break;
			}
		}

		if (!in_list2) {
			continue;
		}

		found = 0;
		for (k = 0; k < count; k++) {
			if (result_list[k] == value) {
				found = 1;
				break;
			}
		}

		if (!found) {
			result_list[count++] = value;
		}
	}

	return count;
}

static int dw_list_intersection3_skip_empty(int *list1, int size1, int *list2, int size2,
	int *list3, int size3, int *result_list)
{
	int *valid_lists[MAX_OUTPUT_MODE_GROUPS];
	int valid_sizes[MAX_OUTPUT_MODE_GROUPS];
	int valid_count = 0;
	int temp_list[MAX_DW_LIST_SIZE];
	int temp_size;

	if (size1 > 0) {
		valid_lists[valid_count] = list1;
		valid_sizes[valid_count] = size1;
		valid_count++;
	}

	if (size2 > 0) {
		valid_lists[valid_count] = list2;
		valid_sizes[valid_count] = size2;
		valid_count++;
	}

	if (size3 > 0) {
		valid_lists[valid_count] = list3;
		valid_sizes[valid_count] = size3;
		valid_count++;
	}

	if (valid_count == 0) {
		return 0;
	} else if (valid_count == 1) {
		memcpy(result_list, valid_lists[0], valid_sizes[0] * sizeof(int));
		return valid_sizes[0];
	} else if (valid_count == 2) {
		return dw_list_intersection(valid_lists[0], valid_sizes[0],
			valid_lists[1], valid_sizes[1],
			result_list);
	} else {
		temp_size = dw_list_intersection(list1, size1, list2, size2, temp_list);

		if (temp_size > 0) {
			return dw_list_intersection(temp_list, temp_size, list3, size3, result_list);
		} else {
			return 0;
		}
	}
}

static int select_min_dw(int *list, int size)
{
	int i;
	int min_dw;

	if (size <= 0) {
		return -1;
	}

	/* 1. Prefer to select dw = 16 */
	for (i = 0; i < size; i++) {
		if (list[i] == DM_YUV_ONLY) {
			return DM_YUV_ONLY;
		}
	}

	/* 2. Otherwise, select the smallest dw */
	min_dw = list[0];
	for (i = 0; i < size; i++) {
		if (list[i] < min_dw) {
			min_dw = list[i];
		}
	}
	return min_dw;
}

static void expand_dos_dw_list(int *src_list, int src_size,
	int *dst_list, int *dst_size)
{
	int i;
	int has_0x1 = 0, has_0x2 = 0, has_0x3 = 0, has_0x4 = 0;

	*dst_size = 0;
	for (i = 0; i < src_size; i++) {
		if (src_list[i] != -1) {
			dst_list[(*dst_size)++] = src_list[i];
		}
	}

	for (i = 0; i < src_size; i++) {
		if (src_list[i] == DM_YUV_1_1_AVBC)
			has_0x1 = 1;
		if (src_list[i] == DM_YUV_1_4_AVBC_A)
			has_0x2 = 1;
		if (src_list[i] == DM_YUV_1_4_AVBC_B)
			has_0x3 = 1;
		if (src_list[i] == DM_YUV_1_2_AVBC)
			has_0x4 = 1;
	}

	if (has_0x1 && has_0x4) {
		dst_list[(*dst_size)++] = DM_YUV_AUTO_1_2_AVBC;
		dst_list[(*dst_size)++] = DM_YUV_AUTO_1_2_AVBC_B;
	}

	if (has_0x1 && has_0x2) {
		dst_list[(*dst_size)++] = DM_YUV_AUTO_1_4_AVBC;
		dst_list[(*dst_size)++] = DM_YUV_14_11_10BIT_AVBC;
	}

	if (has_0x1 && has_0x3 && has_0x4) {
		dst_list[(*dst_size)++] = DM_YUV_AUTO_14_12_AVBC;
	}
}

int parse_output_modes(struct output_mode_ext *output_mode_ext)
{
	struct parse_result result;
	int i;
	int *dos_dw_list;
	int temp_list[MAX_DW_LIST_SIZE];
	int temp_size;
	int dos_dw_list_size = MAX_DW_LIST_SIZE;
	int expanded_dos_dw_list[MAX_DW_LIST_SIZE];
	int expanded_dos_dw_list_size;

	memset(&result, 0, sizeof(result));
	result.min_dw = -1;

	for (i = 0; i < MAX_OUTPUT_MODE_GROUPS; i++) {
		parse_signal_output_mode(&output_mode_ext->modes[i],
			result.dw_list[i], &result.list_size[i]);
	}

	temp_size = dw_list_intersection3_skip_empty(
		result.dw_list[0], result.list_size[0],
		result.dw_list[1], result.list_size[1],
		result.dw_list[2], result.list_size[2],
		temp_list);

	if (temp_size <= 0) {
		pr_info("No temp_size found among the valid modes\n");
		return -1;
	}

	dos_dw_list = get_dos_dw_mode(output_mode_ext->fmt);
	if (!dos_dw_list) {
		pr_err("No dos_dw_mode found for fmt %d\n", output_mode_ext->fmt);
		return -1;
	}

	expand_dos_dw_list(dos_dw_list, dos_dw_list_size,
		expanded_dos_dw_list, &expanded_dos_dw_list_size);

	result.intersection_size = dw_list_intersection(
		expanded_dos_dw_list, expanded_dos_dw_list_size,
		temp_list, temp_size,
		result.intersection);

	if (result.intersection_size > 0) {
		result.min_dw = select_min_dw(result.intersection, result.intersection_size);
	} else {
		pr_info("No intersection found among the valid modes\n");
		return -1;
	}

	return result.min_dw;
}

EXPORT_SYMBOL(parse_output_modes);


