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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "decoder_cpu_ver_info.h"
#include "../register/register.h"

#define DECODE_CPU_VER_ID_NODE_NAME "cpu_ver_name"
#define CODEC_DOS_DEV_ID_NODE_NAME  "vcodec_dos_dev"
#define AM_SUCCESS 0
#define MAJOR_ID_START AM_MESON_CPU_MAJOR_ID_M6

static enum AM_MESON_CPU_MAJOR_ID cpu_ver_id = AM_MESON_CPU_MAJOR_ID_MAX;

static int cpu_sub_id = 0;

static bool codec_dos_dev = 0;		//to compat old dts

static struct dos_of_dev_s *platform_dos_dev = NULL;

inline bool is_support_new_dos_dev(void)
{
	return codec_dos_dev;
}
EXPORT_SYMBOL(is_support_new_dos_dev);

struct dos_of_dev_s *dos_dev_get(void)
{
	return platform_dos_dev;
}
EXPORT_SYMBOL(dos_dev_get);

static struct dos_of_dev_s dos_dev_data[AM_MESON_CPU_MAJOR_ID_MAX - MAJOR_ID_START] = {
	[AM_MESON_CPU_MAJOR_ID_M8B - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_M8B,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = false,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K,
	},

	[AM_MESON_CPU_MAJOR_ID_GXL - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_GXL,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K,
	},

	[AM_MESON_CPU_MAJOR_ID_G12A - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_G12A,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,	//hevcb clk must be same with hevcf in g12a
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K,
	},

	[AM_MESON_CPU_MAJOR_ID_G12B - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_G12B,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag	= false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu	= true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K,
	},

	[AM_MESON_CPU_MAJOR_ID_GXLX2 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_GXLX2,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag	= false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu	= true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K,
	},

	[AM_MESON_CPU_MAJOR_ID_SM1 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_SM1,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,  //support 8kp24
	},

	[AM_MESON_CPU_MAJOR_ID_TL1 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_TL1,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K, //support 8kp24
	},

	[AM_MESON_CPU_MAJOR_ID_TM2 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_TM2,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_SC2 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_SC2,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_T5 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_T5,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_4K, //unsupport vp9 & av1
	},

	[AM_MESON_CPU_MAJOR_ID_T5D - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_T5D,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 667,
		.max_hevcb_clock = 667,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_1080P,
		.hevc_max_resolution = RESOLUTION_1080P,	//unsupport 4k and avs2
	},

	[AM_MESON_CPU_MAJOR_ID_T7 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_T7,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = true,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_S4 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_S4,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_T3 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_T3,
		.reg_compat = t3_mm_registers_compat,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = true,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,	//8kp30, rdma, mmu copy
	},

	[AM_MESON_CPU_MAJOR_ID_S4D - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_S4D,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_T5W - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_T5W,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = true,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	[AM_MESON_CPU_MAJOR_ID_S5 - MAJOR_ID_START] = {
		.chip_id = AM_MESON_CPU_MAJOR_ID_S5,
		.reg_compat = s5_mm_registers_compat,	//register compact
		.max_vdec_clock        = 800,
		.max_hevcf_clock       = 800,
		.max_hevcb_clock       = 800,
		.hevc_clk_combine_flag = true,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = true,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = true,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},
};

/* sub id features */
static struct dos_of_dev_s dos_dev_sub_table[] = {
	{	/* g12b revb */
		.chip_id = AM_MESON_CPU_MINOR_ID_REVB_G12B,
		.reg_compat = NULL,
		.max_vdec_clock  = 667,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,	//g12b revb hevc clk support 800mhz
		.hevc_clk_combine_flag	= true,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu	= true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	{	/* tm2 revb */
		.chip_id = AM_MESON_CPU_MINOR_ID_REVB_TM2,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = true,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,
	},

	{
		.chip_id = AM_MESON_CPU_MINOR_ID_S4_S805X2,
		.reg_compat = NULL,
		.max_vdec_clock        = 500,
		.max_hevcf_clock       = 500,
		.max_hevcb_clock       = 500,
		.hevc_clk_combine_flag = true,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = false,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_1080P,
		.hevc_max_resolution = RESOLUTION_1080P,
	},

	{
		.chip_id = AM_MESON_CPU_MINOR_ID_T7C,
		.reg_compat = NULL,
		.max_vdec_clock  = 800,
		.max_hevcf_clock = 800,
		.max_hevcb_clock = 800,
		.hevc_clk_combine_flag  = false,
		.is_hw_parser_support   = false,
		.is_vdec_canvas_support = true,
		.is_support_h264_mmu    = true,
		.is_hevc_dual_core_mode_support = false,
		.vdec_max_resolution = RESOLUTION_4K,
		.hevc_max_resolution = RESOLUTION_8K,  //fixed endian issue
	}
};

/* dos device match table */
static const struct of_device_id cpu_ver_of_match[] = {
	{
		.compatible = "amlogic, cpu-major-id-axg",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_AXG - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-g12a",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_G12A - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-gxl",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_GXL - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-gxm",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_GXM - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-txl",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_TXL - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-txlx",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_TXLX - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-sm1",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_SM1 - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-tl1",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_TL1 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-tm2",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_TM2 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-sc2",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_SC2 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_T5 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5d",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_T5D - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t7",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_T7 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_S4 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t3",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_T3 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-p1",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_P1 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4d",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_S4D - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5w",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_T5W - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-s5",
		.data = &dos_dev_data[AM_MESON_CPU_MAJOR_ID_S5 - MAJOR_ID_START],
	},
	{},
};

static const struct of_device_id cpu_sub_id_of_match[] = {
	{
		.compatible = "amlogic, cpu-major-id-g12b-b",
		.data = &dos_dev_sub_table[0],
	},
	{
		.compatible = "amlogic, cpu-major-id-tm2-b",
		.data = &dos_dev_sub_table[1],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4-805x2",
		.data = &dos_dev_sub_table[2],
	},
	{
		.compatible = "amlogic, cpu-major-id-t7c",
		.data = &dos_dev_sub_table[3],
	},
};

static struct platform_device *get_dos_dev_from_dtb(void)
{
	struct device_node *pnode = NULL;

	pnode = of_find_node_by_name(NULL, CODEC_DOS_DEV_ID_NODE_NAME);
	if (pnode == NULL) {
		codec_dos_dev = false;
		pnode = of_find_node_by_name(NULL, DECODE_CPU_VER_ID_NODE_NAME);
		if (pnode == NULL) {
			pr_err("No find node.\n");
			return NULL;
		}
	} else {
		codec_dos_dev = true;
	}

	return of_find_device_by_node(pnode);
}

static struct dos_of_dev_s * get_dos_of_dev_data(struct platform_device *pdev)
{
	const struct of_device_id *pmatch = NULL;

	if (pdev == NULL)
		return NULL;

	pmatch = of_match_device(cpu_ver_of_match, &pdev->dev);
	if (NULL == pmatch) {
		pmatch = of_match_device(cpu_sub_id_of_match, &pdev->dev);
		if (NULL == pmatch) {
			pr_err("No find of_match_device\n");
			return NULL;
		}
	}

	return (struct dos_of_dev_s *)pmatch->data;
}

/* sos to get platform data */
static int dos_device_search_data(int id, int sub_id)
{
	int i, j;
	int sub_dev_id;

	for (i = 0; i < ARRAY_SIZE(dos_dev_data); i++) {
		if (id == dos_dev_data[i].chip_id) {
			platform_dos_dev = &dos_dev_data[i];
			pr_info("%s, get major %d dos dev data success\n", __func__, i);

			if (sub_id) {
				for (j = 0; j < ARRAY_SIZE(dos_dev_sub_table); j++) {
					sub_dev_id = (dos_dev_sub_table[i].chip_id & SUB_ID_MASK) >> 8;
					if (sub_id == sub_dev_id) {
						platform_dos_dev = &dos_dev_sub_table[j];
						pr_info("%s, get sub %d dos dev data success\n", __func__, j);
					}
				}
			}
		}
	}
	if (platform_dos_dev)
		return 0;
	else
		return -ENODEV;
}

struct platform_device *initial_dos_device(void)
{
	struct platform_device *pdev = NULL;
	struct dos_of_dev_s *of_dev_data;

	pdev = get_dos_dev_from_dtb();

	of_dev_data = get_dos_of_dev_data(pdev);

	if (of_dev_data) {
		cpu_ver_id = of_dev_data->chip_id & MAJOY_ID_MASK;
		cpu_sub_id = (of_dev_data->chip_id & SUB_ID_MASK) >> 8;
		platform_dos_dev = of_dev_data;
	} else {
		cpu_ver_id = (enum AM_MESON_CPU_MAJOR_ID)get_cpu_type();
		cpu_sub_id = (is_meson_rev_b()) ? CHIP_REVB : CHIP_REVA;

		pr_info("get dos device failed, id %d(%d), try to search dos device data\n", cpu_ver_id, cpu_sub_id);
		if (dos_device_search_data(cpu_ver_id, cpu_sub_id) < 0) {
			pr_err("get dos device failed, dos maybe out of work\n");
			//return NULL;
		}
	}
	if ((cpu_ver_id == AM_MESON_CPU_MAJOR_ID_G12B) && (cpu_sub_id == CHIP_REVB))
		cpu_ver_id = AM_MESON_CPU_MAJOR_ID_TL1;

	pr_info("dos init chip %d(%d), device info :\n", cpu_ver_id, cpu_sub_id);
	pr_info("\t max_vdec_clock  : %03d MHz \n", platform_dos_dev->max_vdec_clock);
	pr_info("\t max_hevcf_clock : %03d MHz \n", platform_dos_dev->max_hevcf_clock);
	pr_info("\t max_hevcb_clock : %03d MHz \n", platform_dos_dev->max_hevcb_clock);
	pr_info("\t hw_esparser_support : %s \n", platform_dos_dev->is_hw_parser_support?"yes":"no");
	pr_info("\t vdec_canvas_support : %s \n", platform_dos_dev->is_vdec_canvas_support?"yes":"no");
	/* to do print all ? */

	if (pdev && of_dev_data)
		dos_register_probe(pdev, of_dev_data->reg_compat);

	pr_info("initial_dos_device end\n");

	return pdev;
}
EXPORT_SYMBOL(initial_dos_device);

enum AM_MESON_CPU_MAJOR_ID get_cpu_major_id(void)
{
	if (AM_MESON_CPU_MAJOR_ID_MAX == cpu_ver_id)
		initial_dos_device();

	return cpu_ver_id;
}
EXPORT_SYMBOL(get_cpu_major_id);

int get_cpu_sub_id(void)
{
	return cpu_sub_id;
}
EXPORT_SYMBOL(get_cpu_sub_id);

bool is_cpu_meson_revb(void)
{
	if (AM_MESON_CPU_MAJOR_ID_MAX == cpu_ver_id)
		initial_dos_device();

	return (cpu_sub_id == CHIP_REVB);
}
EXPORT_SYMBOL(is_cpu_meson_revb);

bool is_cpu_tm2_revb(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TM2)
		&& (is_cpu_meson_revb()));
}
EXPORT_SYMBOL(is_cpu_tm2_revb);

bool is_cpu_s4_s805x2(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S4)
		&& (get_cpu_sub_id() == CHIP_REVX));
}
EXPORT_SYMBOL(is_cpu_s4_s805x2);

bool is_cpu_t7(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7)
		&& (get_cpu_sub_id() != CHIP_REVC));
}
EXPORT_SYMBOL(is_cpu_t7);

bool is_cpu_t7c(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7)
		&& (get_cpu_sub_id() == CHIP_REVC));
}
EXPORT_SYMBOL(is_cpu_t7c);

