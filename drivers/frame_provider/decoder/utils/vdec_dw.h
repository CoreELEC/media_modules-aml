/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDEC_DW__
#define __VDEC_DW__

#include <linux/videodev2.h>
#include "../../../amvdec_ports/utils/aml_output_mode_define.h"

#define MAX_DW_LIST_SIZE	32
#define MAX_OUTPUT_MODE_GROUPS	3

#define V4L2_PIX_FMT_AV1      v4l2_fourcc('A', 'V', '1', '0') /* av1 */
#define V4L2_PIX_FMT_AVS      v4l2_fourcc('A', 'V', 'S', '0') /* avs */
#define V4L2_PIX_FMT_AVS2     v4l2_fourcc('A', 'V', 'S', '2') /* avs2 */
#define V4L2_PIX_FMT_AVS3     v4l2_fourcc('A', 'V', 'S', '3') /* avs3 */
#define V4L2_PIX_FMT_H266     v4l2_fourcc('H', '2', '6', '6') /* h266 */

struct dos_hw_dw_mode {
	enum vformat_e fmt;
	int hw_dw_mode[MAX_DW_LIST_SIZE];
};

struct dos_hw_output_mode {
	enum vformat_e fmt;
	struct output_mode mode[MAX_OUTPUT_MODE_GROUPS];
};

struct parse_result {
	int dw_list[MAX_OUTPUT_MODE_GROUPS][MAX_DW_LIST_SIZE];
	int list_size[MAX_OUTPUT_MODE_GROUPS];
	int intersection[MAX_DW_LIST_SIZE];
	int intersection_size;
	int min_dw;
};

struct dos_hw_dw_mode* get_dos_dw_mode_all(void);
struct dos_hw_output_mode* get_dos_output_mode_all(void);
int *get_dos_dw_mode(enum vformat_e fmt);

void vdec_dos_dw_mode_init(void);
void pr_dw_infos(void);

enum vformat_e v4l2_fourcc_to_vtype(uint32_t fourcc);
struct output_mode *get_output_mode_by_fmt(enum vformat_e fmt);
void print_output_modes(struct output_mode *modes);
int parse_output_modes(struct output_mode_ext *output_mode_ext);

#endif

