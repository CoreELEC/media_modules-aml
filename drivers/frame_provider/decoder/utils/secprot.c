/*
 * drivers/amlogic/amports/arch/secprot.c
 *
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
*/

#include <linux/kernel.h>
#include <asm/compiler.h>
#include "secprot.h"
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#endif

#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>

bool vdec_tee_enabled(void)
{
	return tee_enabled();
}

int vdec_tee_load_video_fw_swap(u32 index, u32 vdec, bool is_swap)
{
	return (tee_load_video_fw_swap(index, vdec, is_swap));
}

int vdec_tee_load_video_fw(u32 index, u32 vdec)
{
	return (tee_load_video_fw(index, vdec));
}

int vdec_tee_vp9_prob_process(u32 cur_frame_type, u32 prev_frame_type,
		u32 prob_status, u32 prob_addr)
{
	return tee_vp9_prob_process(cur_frame_type, prev_frame_type, prob_status, prob_addr);
}

int vdec_tee_vp9_prob_malloc(u32 *prob_addr)
{
	return tee_vp9_prob_malloc(prob_addr);
}

int vdec_tee_vp9_prob_free(u32 prob_addr)
{
	return tee_vp9_prob_free(prob_addr);
}

#else
bool vdec_tee_enabled(void)
{
	//pr_info("no tee config\n");
	return 0;
}

int vdec_tee_load_video_fw_swap(u32 index, u32 vdec, bool is_swap)
{
	pr_info("no tee config\n");
	return (-1);
}

int vdec_tee_load_video_fw(u32 index, u32 vdec)
{
	pr_info("no tee config\n");
	return (-1);
}

int vdec_tee_vp9_prob_process(u32 cur_frame_type, u32 prev_frame_type,
		u32 prob_status, u32 prob_addr)
{
	return -1;
}

int vdec_tee_vp9_prob_malloc(u32 *prob_addr)
{
	*prob_addr = 0;
	return -1;
}

int vdec_tee_vp9_prob_free(u32 prob_addr)
{
	return -1;
}

#endif
EXPORT_SYMBOL(vdec_tee_enabled);
EXPORT_SYMBOL(vdec_tee_load_video_fw_swap);
EXPORT_SYMBOL(vdec_tee_load_video_fw);

EXPORT_SYMBOL(vdec_tee_vp9_prob_process);
EXPORT_SYMBOL(vdec_tee_vp9_prob_malloc);
EXPORT_SYMBOL(vdec_tee_vp9_prob_free);


#ifdef CONFIG_ARM64

int tee_config_device_secure(int dev_id, int secure)
{
	int ret = 0;
	register unsigned x0 asm("x0");
	register unsigned x1 asm("x1");
	register unsigned x2 asm("x2");

	x0 = OPTEE_SMC_CONFIG_DEVICE_SECURE;
	x1 = dev_id;
	x2 = secure;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x0")
		__asmeq("%2", "x1")
		__asmeq("%3", "x2")
		"smc    #0\n"
		: "=r"(x0)
		: "r"(x0), "r"(x1), "r"(x2));
	ret = x0;

	return ret;
}
#else
int tee_config_device_secure(int dev_id, int secure)
{
	int ret = 0;
	register unsigned int r0 asm("r0");
	register unsigned int r1 asm("r1");
	register unsigned int r2 asm("r2");

	r0 = OPTEE_SMC_CONFIG_DEVICE_SECURE;
	r1 = dev_id;
	r2 = secure;

	asm volatile(
		__asmeq("%0", "r0")
		__asmeq("%1", "r0")
		__asmeq("%2", "r1")
		__asmeq("%3", "r2")
		__SMC(0)
		: "=r"(r0)
		: "r"(r0), "r"(r1), "r"(r2));
	ret = r0;

	return ret;
}
#endif

