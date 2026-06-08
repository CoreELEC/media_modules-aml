/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LOW_POWER_CLK__
#define __LOW_POWER_CLK__

#include <linux/amlogic/media/utils/vformat.h>

struct low_power_ctrl_t {
	void *vdec_mm_clk_gate_on;
	void *vdec_mm_clk_gate_off;

	void *clk_gate_on[VFORMAT_MAX];
	void *clk_gate_off[VFORMAT_MAX];
};


struct low_power_ctrl_t *dos_low_power_ctrl_init(void);
void *low_power_clk_on_get(int format, int mmu_flag);
void *low_power_clk_off_get(int format, int mmu_flag);
void scatter_input_low_power_clk_on(void);
#endif
