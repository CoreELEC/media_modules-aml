/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef DOS_REGISTERS_HEADER_
#define DOS_REGISTERS_HEADER_

#define NEW_REG_CHECK_MASK  (0xffff0000)
/*new reg mask, bit20~bit27 chipid, bit16~bit19 subid*/
#define MASK_S5_NEW_REGS   ((AM_MESON_CPU_MAJOR_ID_S5 << 20) & NEW_REG_CHECK_MASK)
#define MASK_S6_NEW_REGS   ((AM_MESON_CPU_MAJOR_ID_S6 << 20) & NEW_REG_CHECK_MASK)

#ifdef DDK_DEFINE
#include "media_extra.h"
#endif

#if defined(DOS_REGISTERS_V2)
/* T6D, T6W */
#include "dos_registers_v2.h"
#elif defined(DOS_REGISTERS_V3)
/* GXLX4, T6X */
#include "dos_registers_v3.h"
#else
/* S6 and before S6 */
#include "dos_registers_v1.h"
#endif
/* S5 */
#include "dos_registers_fb.h"

#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define ASSIST_AMR1_INT0 VDEC_ASSIST_AMR1_INT0
#define ASSIST_AMR1_INT1 VDEC_ASSIST_AMR1_INT1
#define ASSIST_AMR1_INT2 VDEC_ASSIST_AMR1_INT2
#define ASSIST_AMR1_INT3 VDEC_ASSIST_AMR1_INT3
#define ASSIST_AMR1_INT4 VDEC_ASSIST_AMR1_INT4
#define ASSIST_AMR1_INT5 VDEC_ASSIST_AMR1_INT5
#define ASSIST_AMR1_INT6 VDEC_ASSIST_AMR1_INT6
#define ASSIST_AMR1_INT7 VDEC_ASSIST_AMR1_INT7
#define ASSIST_AMR1_INT8 VDEC_ASSIST_AMR1_INT8
#define ASSIST_AMR1_INT9 VDEC_ASSIST_AMR1_INT9

/*
 * Pseudodefinition for build
 * example:
 * New registers defined in DOS_REGISTERS_V3, but no definition
 * in DOS_REGISTERS_V1. To fix DOS_REGISTERS_V1 code build error,
 * Pseudodefinite registers here, and it should be removed after
 * it's really defined.
 */
#define PSEUDODEF_REG (0)

#ifndef DOS_REGISTERS_V3

#define HEVC_AXI_BRESP_INT_STATU           PSEUDODEF_REG

#define PCLATCH                            PSEUDODEF_REG
#define HEVC_AXI_MON_CTRL                  PSEUDODEF_REG
#define HEVC_AXI_MON_A_START               PSEUDODEF_REG
#define HEVC_AXI_MON_A_END                 PSEUDODEF_REG
#define HEVC_AXI_MON_A_RSV                 PSEUDODEF_REG
#define HEVC_AXI_MON_B_START               PSEUDODEF_REG
#define HEVC_AXI_MON_B_END                 PSEUDODEF_REG
#define HEVC_AXI_MON_B_ERR_ID              PSEUDODEF_REG
#define HEVC_AXI_MON_B_ERR_ADDR            PSEUDODEF_REG
#define HEVC_AXI_MON_B_IGN_ID              PSEUDODEF_REG

#define HEVC_SCATTER_WRAP                  PSEUDODEF_REG
#define HEVC_SCATTER_DATA_START_ADDR       PSEUDODEF_REG
#define HEVC_SCATTER_DATA_ADDR_ADJUST      PSEUDODEF_REG
#define HEVC_SCATTER_MAP_FIFO_LEVEL        PSEUDODEF_REG

#define HEVC_SW_PARSER_CLK_ENABLE          PSEUDODEF_REG
#define HEVCD_IPP_GCLKGATE_CONFIG          PSEUDODEF_REG
#define HEVCD_MCR_DYNCLKGATE_CONFIG        PSEUDODEF_REG
#define HEVCD_MCR_DYNCLKGATE_STATUS        PSEUDODEF_REG
#define HEVCD_MPP_SUBMODULE_GCLK_EN_CONFIG PSEUDODEF_REG
#define HEVCD_MCR_DCM_FRM_REQ              PSEUDODEF_REG

#define HEVC_LPF_GCLK_EN0                  PSEUDODEF_REG
#define HEVC_LPF_GCLK_EN1                  PSEUDODEF_REG
#define HEVC_LPF_GCLK_EN2                  PSEUDODEF_REG
// LPF clock gate disable for sub-modules
#define HEVC_LPF_CG_OFF0                   PSEUDODEF_REG
#define HEVC_LPF_CG_OFF1                   PSEUDODEF_REG
#define HEVC_LPF_CG_OFF2                   PSEUDODEF_REG
#define HEVC_IQIT_CRC_1                    PSEUDODEF_REG
#define HEVC_IQIT_CRC_2                    PSEUDODEF_REG
#define HEVC_SAO_2D_OFFSET                 PSEUDODEF_REG
#define HEVC_SAO_2D_SIZE                   PSEUDODEF_REG
#define VDEC_SHIFT_COUNT                   PSEUDODEF_REG
#define HEVC_SCATTER_MANUAL_CONTROL        PSEUDODEF_REG
#define HEVC_SCATTER_MANUAL_DATA1          PSEUDODEF_REG
#define HEVC_SCATTER_MANUAL_DATA2          PSEUDODEF_REG
#endif

#if !defined(DOS_REGISTERS_V3) &&  !defined(DOS_REGISTERS_V2)
#define  AXI_ADDR_34                       PSEUDODEF_REG
#define HEVC_SCATTER_CONTROL               PSEUDODEF_REG
#endif

/* SYS_CTRL */
#define SYSCTRL_AXI_PIPE_CTRL0             0x0055

#endif // DOS_REGISTERS_HEADER_

