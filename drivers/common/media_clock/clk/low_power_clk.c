/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/vmalloc.h>
#include <linux/amlogic/media/utils/vformat.h>
#include "low_power_clk.h"
#include "../../register/register.h"
#include "../../../include/regs/dos_registers.h"
#include "../../chips/decoder_cpu_ver_info.h"


/* v2: advanced low power clock ctrl
 * v1: improved low power clock ctrl
 * v0: auto clock gate in modules with amrisc clock ctrl in software
*/

static void lp_clk_on_avbcd_v2(void)
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;

	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x1fe07);  //enable clk ,close vcpu parser iqit mpred ipp mpp clk

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0,((0x1	  << 0 ) | //lpf top
		(0x1	<< 1 ) | //cfg
		(0x0	<< 2 ) | //mem
		(0x1	<< 8 ) | //obuf
		(0x0	<< 12)	 //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1,((0x0	  << 0 ) | //oadp2
		(0x0	<< 8 )	 //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2,((0x0	  << 0 ) | //lrf
		(0x1	<< 16) | //H264 dcm frm_req
		(0x0	<< 17) | //lcevc
		(0x0	<< 24)	 //reserved
		));
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T6X) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_A9)) {
		SET_VREG_MASK(HEVC_LPF_CG_OFF2, 1 << 16);
	}
}

static void lp_clk_on_hevc_v2(void)
{
	int val;

	val = READ_VREG(DOS_GCLK_EN0);
	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);
	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x00008021);

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG, ((0x0 << 0) | //av1_avs3_vvc_enable
		(0x0 << 1)  //vvc_enable
		));

	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_compound
		(0x1 << 6)    // crc
		));

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1   << 1 ) | //cfg
		(0x1a  << 2 ) | //mem
		(0x1   << 8 ) | //obuf
		(0x1ff << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0) | //oadp2
		(0x99f	<< 8 )	 //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_vp9_v2(void)// Enable DOS Clocks
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	if (is_support_format(VFORMAT_H266)) {
		// VVC Enable -- VP9 will not share AV1 resource
		WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x0000e0e0);
	} else {
		// VVC Disabled -- VP9 share AV1 resource, need to clock on clk_av1_parser_core/av1_dma_ctl
		WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x0000c060);
	}

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG, ((0x0 << 0) | //av1_avs3_vvc_enable
		(0x0 << 1)   //vvc_enable
		));
	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_compound
		 (0x1 << 6) |  // crc
		 (0x1 << 12)   // scale
		 ));
	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1  << 1) | //cfg
		(0x1e << 2) | //mem
		(0x1  << 8) | //obuf
		(0x1ff << 12) //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0) | //oadp2
		(0x1f << 8)   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_av1_v2(void)
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);
	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0xc0000061);

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG, ((0x1 << 0 ) | //av1_avs3_vvc_enable
		(0x0 << 1 )   //vvc_enable
		));

	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_compound
		(0x1 << 1 ) |  // av1_interintra
		(0x1 << 6 ) |  // crc
		(0x1 << 7 ) |  // av1_ndef
		(0x1 << 12) |  // scale
		(0x1 << 13) |  // warp
		(0x1 << 14)    // warpparams
		));

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1   << 1 ) | //cfg
		(0xf   << 2 ) | //mem
		(0x1   << 8 ) | //obuf
		(0x1ff << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x1 << 0) | //oadp2
		(0x7f << 8)   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0xffff << 0) | //lrf
		(0x0    << 16) | //H264 dcm frm_req
		(0x0    << 17) | //lcevc
		(0x0    << 24)   //reserved
		));
}

static void lp_clk_on_avs2_v2(void)// Enable DOS Clocks
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x00008001);

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG, ((0x0 << 0) | //av1_avs3_vvc_enable
		(0x0 << 1)   //vvc_enable
		));

	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_compound
		(0x1 << 6)    // crc
		));

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1   << 1) | //cfg
		(0x1e  << 2) | //mem
		(0x1   << 8) | //obuf
		(0x1ff << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1,((0x0 << 0) | //oadp2
		(0xf9f << 8 )   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2,((0x0 << 0) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_avs3_v2(void)// Enable DOS Clocks
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x00008001);

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG, ((0x1 << 0) | //av1_avs3_vvc_enable
		(0x0    << 1 )   //vvc_enable
		));
	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_comound
		(0x1 << 6) |  // crc
		(0x1 << 8)    // avs3_ndef
		));

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1   << 1 ) | //cfg
		(0x1e  << 2 ) | //mem
		(0x1   << 8 ) | //obuf
		(0x1ff << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0) | //oadp2
		(0xf9f << 8)   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_vvc_v2(void)// Enable DOS Clocks
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val &= ~(0x3ff);
	val |= 0xc0000000;
	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);

	WRITE_VREG(HEVC_SW_PARSER_CLK_ENABLE, 0x00008001);

	WRITE_VREG(HEVCD_IPP_GCLKGATE_CONFIG,((0x1 << 0) | //av1_avs3_vvc_enable
		(0x1    << 1 )   //vvc_enable
		));

	WRITE_VREG(HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG, ((0x1 << 0) |  // av1_compound
		(0x1 << 1) |  // interintra(ciip)
		(0x1 << 2) |  // gpm_mvcal
		(0x1 << 3) |  // comv_data
		(0x1 << 4) |  // comv_wr
		(0x1 << 5) |  // prof
		(0x1 << 6) |  // crc
		(0x1 << 7) |  // av1_ndef
		(0x1 << 8) |  // avs3_ndef(affine)
		(0x1 << 9) |  // vvc_dmvgen
		(0x1 << 10) |  // vvc_sbtmvp
		(0x1 << 11) |  // dmvr
		(0x1 << 12) |  // scale
		(0x1 << 14) |  // warpparams
		(0x1 << 15)    // bdof
		));

	WRITE_VREG(HEVCD_MCR_DYNCLKGATE_CONFIG, 0x40000000); //enable HEVC

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x1   << 1) | //cfg
		(0x1e  << 2) | //mem
		(0x1   << 8) | //obuf
		(0x1ff << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0) | //oadp2
		(0xf9f << 8)   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0 ) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_vdec_v2(void)
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val |= 0xc00003ff;
	WRITE_VREG(DOS_GCLK_EN0, val);
#if 0
	if (is_axi_mon_enabled())
			WRITE_VREG(DOS_GCLK_EN3, 0x10603);//ddr, mcr, lpf, cbus
	else
#endif
	WRITE_VREG(DOS_GCLK_EN3, 0x10602);//ddr, mcr, lpf, cbus

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0) | //lpf top
		(0x0 << 1 ) | //cfg
		(0xd << 2 ) | //mem
		(0x0 << 8 ) | //obuf
		(0x0 << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0) | //oadp2
		(0x0 << 8 )   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_on_vdec_mmu_v2(void)// Enable DOS Clocks
{
	u32 val = READ_VREG(DOS_GCLK_EN0);

	val |= 0xc00003ff;
	WRITE_VREG(DOS_GCLK_EN0, val);

	WRITE_VREG(DOS_GCLK_EN3, 0x10e03);//ddr, mcr, lpf, cbus

	WRITE_VREG(HEVC_LPF_GCLK_EN0, ((0x1 << 0 ) | //lpf top
		(0x1  << 1 ) | //cfg
		(0x1f << 2 ) | //mem
		(0x1  << 8 ) | //obuf
		(0x0  << 12)   //dblk
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN1, ((0x0 << 0 ) | //oadp2
		(0x0 << 8 )   //evan
		));
	WRITE_VREG(HEVC_LPF_GCLK_EN2, ((0x0 << 0 ) | //lrf
		(0x0 << 16) | //H264 dcm frm_req
		(0x0 << 17) | //lcevc
		(0x0 << 24)   //reserved
		));
}

static void lp_clk_off_common_v2(void)
{
	WRITE_VREG(GCLK_EN, 0);
	CLEAR_VREG_MASK(DOS_GCLK_EN0, 0xc00003ff);
	WRITE_VREG(DOS_GCLK_EN3, 0x0);
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T6X) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_A9)) {
		CLEAR_VREG_MASK(HEVC_LPF_CG_OFF2, 1 << 16);
	}
}

/* improved low power ctrl for T6W T6D GXLX4 */
static void lp_clk_on_hevc_v1(void)
{
	CLEAR_VREG_MASK(DOS_GCLK_EN0, 0x3ff);
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
}
static void lp_clk_on_vdec_v1(void)
{
	WRITE_VREG_BITS(DOS_GCLK_EN0, 0x3ff, 0, 10);
	WRITE_VREG(DOS_GCLK_EN3, 0x1f7a7);
}
static void lp_clk_on_vdec_mmu_v1(void)
{
	WRITE_VREG_BITS(DOS_GCLK_EN0, 0x3ff, 0, 10);
	WRITE_VREG(DOS_GCLK_EN3, 0x1ffa7);
}
static void lp_clk_off_common_v1(void)
{
	/* clear vdec clk gate */
	CLEAR_VREG_MASK(DOS_GCLK_EN0, 0x3ff);
	/* turn off vcpu clock */
	CLEAR_VREG_MASK(DOS_GCLK_EN3, (1 << 5));
}

/* s7/s7d hevc clk */
static void hevc_amrisc_gate_off(void)
{
	CLEAR_VREG_MASK(DOS_GCLK_EN3, (1 << 2)); //turn off vcpu clock
}
static void hevc_amrisc_gate_on(void)
{
	SET_VREG_MASK(DOS_GCLK_EN3, (1 << 2)); //turn on vcpu clock
}

void scatter_input_low_power_clk_on(void)
{
	WRITE_VREG(DOS_GCLK_EN3, 0x1ffff);//ddr, mcr, lpf, cbus
}
EXPORT_SYMBOL(scatter_input_low_power_clk_on);

void *low_power_clk_off_get(int format, int mmu_flag)
{
	struct low_power_ctrl_t *lpc = dos_low_power_ctrl_get();

	if (!lpc)
		return NULL;

	if ((format == VFORMAT_H264) && (mmu_flag))
		return lpc->vdec_mm_clk_gate_off;

	return lpc->clk_gate_off[format];
}
EXPORT_SYMBOL(low_power_clk_off_get);

void *low_power_clk_on_get(int format, int mmu_flag)
{
	struct low_power_ctrl_t *lpc = dos_low_power_ctrl_get();

	if (!lpc)
		return NULL;

	if ((format == VFORMAT_H264) && (mmu_flag))
		return lpc->vdec_mm_clk_gate_on;

	return lpc->clk_gate_on[format];
}
EXPORT_SYMBOL(low_power_clk_on_get);

struct low_power_ctrl_t *dos_low_power_ctrl_init(void)
{
	int i;
	struct low_power_ctrl_t *lpc = NULL;

	if (is_vdec_hevc_combine() || is_vcpu_clk_set()) {
		lpc = vzalloc(sizeof(struct low_power_ctrl_t));
		if (!lpc) {
			pr_err("low power ctrl init failed\n");
			return NULL;
		}
	}
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_T6X) {
		/* t6d, t6w, gxlx4 */
		if (is_vdec_hevc_combine()) {
			for (i = 0; i < VFORMAT_MAX; i++) {
				if (is_core_hevc_fmt(i)) {
					lpc->clk_gate_on[i] = lp_clk_on_hevc_v1;
					lpc->clk_gate_off[i] = lp_clk_off_common_v1;
				} else if (is_core_vdec_fmt(i)) {
					lpc->clk_gate_on[i] = lp_clk_on_vdec_v1;
					lpc->clk_gate_off[i] = lp_clk_off_common_v1;
				}
			}
			lpc->vdec_mm_clk_gate_on = lp_clk_on_vdec_mmu_v1;
			lpc->vdec_mm_clk_gate_off = lp_clk_off_common_v1;
			return lpc;
		}
		/* s7, s7d, s6 */
		if (is_vcpu_clk_set()) {
			for (i = 0; i < VFORMAT_MAX; i++) {
				if (is_core_hevc_fmt(i)) {
					lpc->clk_gate_on[i] = hevc_amrisc_gate_on;
					lpc->clk_gate_off[i] = hevc_amrisc_gate_off;
				}
			}
			lpc->vdec_mm_clk_gate_on = hevc_amrisc_gate_on;
			lpc->vdec_mm_clk_gate_off = hevc_amrisc_gate_off;
		}
		return lpc;
	}

	for (i = 0; i < VFORMAT_MAX; i++) {

		lpc->clk_gate_off[i] = lp_clk_off_common_v2;

		switch (i) {
		case VFORMAT_HEVC:
			lpc->clk_gate_on[i] = lp_clk_on_hevc_v2;
			break;
		case VFORMAT_VP9:
			lpc->clk_gate_on[i] = lp_clk_on_vp9_v2;
			break;
		case VFORMAT_AV1:
			lpc->clk_gate_on[i] = lp_clk_on_av1_v2;
			break;
		case VFORMAT_AVS2:
			lpc->clk_gate_on[i] = lp_clk_on_avs2_v2;
			break;
		case VFORMAT_AVS3:
			lpc->clk_gate_on[i] = lp_clk_on_avs3_v2;
			break;
		case VFORMAT_H266:
			lpc->clk_gate_on[i] = lp_clk_on_vvc_v2;
			break;
		case VFORMAT_AVBCD:
			lpc->clk_gate_on[i] = lp_clk_on_avbcd_v2;
			break;

		case VFORMAT_MPEG12:
		case VFORMAT_MPEG4:
		case VFORMAT_H264:
		case VFORMAT_MJPEG:
		case VFORMAT_JPEG:
		case VFORMAT_AVS:
		case VFORMAT_VC1:
		case VFORMAT_H264MVC:
		case VFORMAT_H264_4K2K:
			lpc->clk_gate_on[i] = lp_clk_on_vdec_v2;
			break;
		default:
			break;
		}
	}
	lpc->vdec_mm_clk_gate_on = lp_clk_on_vdec_mmu_v2;
	lpc->vdec_mm_clk_gate_off = lp_clk_off_common_v2;

	return lpc;
}

