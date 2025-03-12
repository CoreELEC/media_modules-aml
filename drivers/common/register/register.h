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
#ifndef _DOS_REGISTER_H_
#define _DOS_REGISTER_H_

#include <linux/platform_device.h>
#include <linux/slab.h>
#include "../../include/regs/dos_registers.h"
#include <linux/amlogic/media/registers/register_ops.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/registers/regs/aiu_regs.h>
#include <linux/amlogic/media/registers/regs/ao_regs.h>
#include <linux/amlogic/media/registers/regs/demux_regs.h>
#include <linux/amlogic/media/registers/regs/dmc_regs.h>
#include <linux/amlogic/media/registers/regs/efuse_regs.h>
#include <linux/amlogic/media/registers/regs/hcodec_regs.h>
#include <linux/amlogic/media/registers/regs/hhi_regs.h>
#include <linux/amlogic/media/registers/regs/sys_regs.h>
#include <linux/amlogic/media/registers/regs/viu_regs.h>

#define WRITE_PARSER_REG(r, val) codec_parsbus_write(r, val)
#define READ_PARSER_REG(r) codec_parsbus_read(r)
#define WRITE_PARSER_REG_BITS(r, val, start, len) \
		codec_set_parsbus_bits(r, val, start, len)
#define CLEAR_PARSER_REG_MASK(r, mask) \
		codec_clear_parsbus_mask(r, mask)
#define SET_PARSER_REG_MASK(r, mask) \
		codec_set_parsbus_mask(r, mask)

#define WRITE_HHI_REG(r, val) codec_hhibus_write(r, val)
#define READ_HHI_REG(r) codec_hhibus_read(r)
#define WRITE_HHI_REG_BITS(r, val, start, len) \
		codec_set_hhibus_bits(r, val, start, len)

#define WRITE_AIU_REG(r, val) codec_aiubus_write(r, val)
#define READ_AIU_REG(r) codec_aiubus_read(r)
#define WRITE_AIU_REG_BITS(r, val, start, len) \
		codec_set_aiubus_bits(r, val, start, len)
#define CLEAR_AIU_REG_MASK(r, mask) \
		codec_clear_aiubus_mask(r, mask)
#define SET_AIU_REG_MASK(r, mask) \
		codec_set_aiubus_mask(r, mask)

#define WRITE_DEMUX_REG(r, val) codec_demuxbus_write(r, val)
#define READ_DEMUX_REG(r) codec_demuxbus_read(r)
#define WRITE_DEMUX_REG_BITS(r, val, start, len) \
		codec_set_demuxbus_bits(r, val, start, len)
#define CLEAR_DEMUX_REG_MASK(r, mask) \
		codec_clear_demuxbus_mask(r, mask)
#define SET_DEMUX_REG_MASK(r, mask) \
		codec_set_demuxbus_mask(r, mask)

#define WRITE_RESET_REG(r, val) codec_resetbus_write(r, val)
#define READ_RESET_REG(r) codec_resetbus_read(r)
#define WRITE_RESET_REG_BITS(r, val, start, len) \
		codec_set_resetbus_bits(r, val, start, len)
#define CLEAR_RESET_REG_MASK(r, mask) \
		codec_clear_resetbus_mask(r, mask)
#define SET_RESET_REG_MASK(r, mask) \
		codec_set_resetbus_mask(r, mask)

#define WRITE_EFUSE_REG(r, val) codec_efusebus_write(r, val)
#define READ_EFUSE_REG(r) codec_efusebus_read(r)
#define WRITE_EFUSE_REG_BITS(r, val, start, len) \
		codec_set_efusebus_bits(r, val, start, len)
#define CLEAR_EFUSE_REG_MASK(r, mask) \
		codec_clear_efusebus_mask(r, mask)
#define SET_EFUSE_REG_MASK(r, mask) \
		codec_set_efusebus_mask(r, mask)

#define WRITE_MPEG_REG(r, val) aml_write_cbus(r, val)
#define READ_MPEG_REG(r) aml_read_cbus(r)

#define READ_VCBUS_REG(r) aml_read_vcbus(r)
#define WRITE_VCBUS_REG(r, val) aml_write_vcbus(r, val)

#define READ_DMCREG(r) aml_read_dmcbus(r)
#define WRITE_DMCREG(r, val) aml_write_dmcbus(r, val)

#define READ_AOREG(r) aml_read_aobus(r)
#define WRITE_AOREG(r, val) aml_write_aobus(r, val)

#define READ_DOSREG(r) aml_read_dosbus(r)
#define WRITE_DOSREG(r, val) aml_write_dosbus(r, val)

typedef enum {
	DOS_BUS,
	DMC_BUS,
	SYSCTRL_BUS,
	MAX_REG_BUS
} MM_BUS_ENUM;

struct bus_reg_desc {
	char const *reg_name;
	s32 reg_compat_offset;
};


#define WRITE_VREG(addr, val) write_dos_reg_comp(addr, val)

#define READ_VREG(addr) read_dos_reg_comp(addr)

#ifdef CONFIG_ARM64
#define PREFIX_ADDR_MASK(addr)  (0x300000000UL & addr)
#define PREFIX_ADDR(addr) (PREFIX_ADDR_MASK(addr) >> 32)
#else
#define PREFIX_ADDR_MASK(addr)  (0x0UL)
#define PREFIX_ADDR(addr) (0UL)
#endif

/* read/write register */
int dos_wait_status(int reg, int mask, int idle);

u32 dos_reg_compat_convert(u32 addr);
void write_dos_reg(u32 addr, int val);
u32 read_dos_reg(u32 addr);
u32 read_dos_reg_comp(u32 addr);
void write_dos_reg_comp(u32 addr, int val);
void dos_reg_write_bits(u32 reg, u32 val, int start, int len);

#define WRITE_VREG_BITS(r, val, start, len) dos_reg_write_bits(r, val, start, len)
#define CLEAR_VREG_MASK(r, mask)   write_dos_reg_comp(r, read_dos_reg_comp(r) & ~(mask))
#define SET_VREG_MASK(r, mask)     write_dos_reg_comp(r, read_dos_reg_comp(r) | (mask))

#if defined(DOS_REGISTERS_V2) || defined(DOS_REGISTERS_V3)
#define READ_HREG(r) read_dos_reg_comp(r)
#define WRITE_HREG(r, val) write_dos_reg_comp(r, val)
#define WRITE_HREG_BITS(r, val, start, len) \
	dos_reg_write_bits(r, val, start, len)
#else
#define READ_HREG(r) read_dos_reg_comp((r) | 0x1000)
#define WRITE_HREG(r, val) write_dos_reg_comp((r) | 0x1000, val)
#define WRITE_HREG_BITS(r, val, start, len) \
	dos_reg_write_bits((r) | 0x1000, val, start, len)
//#define SET_HREG_MASK(r, mask) codec_set_dosbus_mask((r) | 0x1000, mask)
//#define CLEAR_HREG_MASK(r, mask) codec_clear_dosbus_mask((r) | 0x1000, mask)
#endif

//SYSCTRL REG
int read_sysctrl_reg(u32 reg);

void write_sysctrl_reg(u32 reg, int val);

//DMC REG
int read_dmc_reg(u32 reg);

void write_dmc_reg(u32 reg, int val);

//##############################################################

typedef void (*reg_compat_func)(struct bus_reg_desc *, MM_BUS_ENUM bus);

void t3_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);
void s5_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);
void s7_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);
void s6_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);

int dos_register_probe(struct platform_device *pdev, reg_compat_func reg_compat_fn);


#endif
