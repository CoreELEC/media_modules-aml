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

#endif // DOS_REGISTERS_HEADER_

