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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef DECODER_CPU_VER_INFO_H
#define DECODER_CPU_VER_INFO_H
#include <linux/amlogic/cpu_version.h>

enum AM_MESON_CPU_MAJOR_ID
{
	AM_MESON_CPU_MAJOR_ID_M6	= 0x16,
	AM_MESON_CPU_MAJOR_ID_M6TV	= 0x17,
	AM_MESON_CPU_MAJOR_ID_M6TVL	= 0x18,
	AM_MESON_CPU_MAJOR_ID_M8	= 0x19,
	AM_MESON_CPU_MAJOR_ID_MTVD	= 0x1A,
	AM_MESON_CPU_MAJOR_ID_M8B	= 0x1B,
	AM_MESON_CPU_MAJOR_ID_MG9TV	= 0x1C,
	AM_MESON_CPU_MAJOR_ID_M8M2	= 0x1D,
	AM_MESON_CPU_MAJOR_ID_UNUSE	= 0x1E,
	AM_MESON_CPU_MAJOR_ID_GXBB	= 0x1F,
	AM_MESON_CPU_MAJOR_ID_GXTVBB	= 0x20,
	AM_MESON_CPU_MAJOR_ID_GXL	= 0x21,
	AM_MESON_CPU_MAJOR_ID_GXM	= 0x22,
	AM_MESON_CPU_MAJOR_ID_TXL	= 0x23,
	AM_MESON_CPU_MAJOR_ID_TXLX	= 0x24,
	AM_MESON_CPU_MAJOR_ID_AXG	= 0x25,
	AM_MESON_CPU_MAJOR_ID_GXLX	= 0x26,
	AM_MESON_CPU_MAJOR_ID_TXHD	= 0x27,
	AM_MESON_CPU_MAJOR_ID_G12A	= 0x28,
	AM_MESON_CPU_MAJOR_ID_G12B	= 0x29,
	AM_MESON_CPU_MAJOR_ID_GXLX2	= 0x2a,
	AM_MESON_CPU_MAJOR_ID_SM1	= 0x2b,
	AM_MESON_CPU_MAJOR_ID_A1	= 0x2c,
	AM_MESON_CPU_MAJOR_ID_TXLX2	= 0x2d,
	AM_MESON_CPU_MAJOR_ID_TL1	= 0x2e,
	AM_MESON_CPU_MAJOR_ID_TM2	= 0x2f,
	AM_MESON_CPU_MAJOR_ID_C1	= 0x30,
	AM_MESON_CPU_MAJOR_ID_MAX,
};

enum AM_MESON_CPU_MAJOR_ID get_cpu_major_id(void);

/* porting cpu version from kernel */
#define MESON_CPU_VERSION_LVL_MAJOR 0
#define MESON_CPU_VERSION_LVL_MINOR 1
#define MESON_CPU_VERSION_LVL_PACK  2
#define MESON_CPU_VERSION_LVL_MISC  3
#define MESON_CPU_VERSION_LVL_MAX   MESON_CPU_VERSION_LVL_MISC

#ifdef CONFIG_AMLOGIC_CPU_VERSION
unsigned char get_meson_cpu_version(int level);
int arch_big_cpu(int cpu);
#else
unsigned char get_meson_cpu_version(int level);
#if 0
{
	return -1;
}
#endif

static inline int arch_big_cpu(int cpu)
{
	return 0;
}
#endif
static inline bool is_meson_rev_a(void);
static inline bool is_meson_rev_b(void);
static inline bool is_meson_rev_c(void);
static inline bool is_meson_gxl_package_805X(void);

#if 0
static inline u32 get_cpu_package(void)
{
	unsigned int pk;

	pk = get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0;
	return pk;
}

static inline bool package_id_is(unsigned int id)
{
	return get_cpu_package() == id;
}
#endif

#if 0
static inline bool is_meson_rev_a(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xA);
}

static inline bool is_meson_rev_b(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xB);
}

static inline bool is_meson_rev_c(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xC);
}

static inline bool is_meson_gxl_package_805X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x30);
}
#endif

#endif
