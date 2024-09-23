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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "../chips/decoder_cpu_ver_info.h"
#include "register.h"

static void __iomem *reg_base[MAX_REG_BUS];
struct bus_reg_desc *reg_desc[MAX_REG_BUS];

#define DBG_DOS_RD    0x01
#define DBG_DOS_WR    0x02
#define DBG_DOS_MAP   0x08

#define DBG_DMC_RD    0x10
#define DBG_DMC_WR    0x20

#define DBG_SYSCTRL_RD   0x100
#define DBG_SYSCTRL_WR   0x200

#define dbg_mm_reg(mask, fmt, args...) do {		\
		if (unlikely(register_debug & (mask)))	\
			pr_info(fmt, ##args);				\
		} while (0)


#define REG_OFFSET(new, old)  ((signed int)((new) - (old)))
#define REG_COMPAT_RANGE(start, end)  ((end) - (start) + 1)

static u32 register_debug;
module_param(register_debug, uint, 0664);

void registers_offset_config(struct bus_reg_desc *offset_from, s32 val, u32 size)
{
	u32 i;

	for (i = 0; i < size; i++) {
		offset_from[i].reg_compat_offset = val;
	}
}
EXPORT_SYMBOL(registers_offset_config);


void s5_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs)
{
	if (bs == DOS_BUS) {
		printk("s5 dos register compat\n");
		registers_offset_config(&desc[HEVC_ASSIST_AMR1_INT0],
			-(0x0025 - 0x0015),
			(HEVC_ASSIST_MBX_SSEL - HEVC_ASSIST_AMR1_INT0 + 1));

		registers_offset_config(&desc[HEVC_ASSIST_TIMER0_LO],
			-(0x0060 - 0x0036),
			(HEVC_ASSIST_DMA_INT_MSK2 - HEVC_ASSIST_TIMER0_LO + 1));

		registers_offset_config(&desc[HEVC_ASSIST_MBOX0_IRQ_REG],
			-(0x0070 - 0x0040),
			(HEVC_ASSIST_AXI_STATUS2_LO - HEVC_ASSIST_MBOX0_IRQ_REG + 1));

		registers_offset_config(&desc[HEVC_ASSIST_SCRATCH_0],
			-(0x00c0 - 0x00b0),
			(HEVC_ASSIST_SCRATCH_N - HEVC_ASSIST_SCRATCH_0) + 1);

		registers_offset_config(&desc[AV1D_IPP_DIR_CFG], -(0x0490 - 0x0419), 1);
	}
}

void t3_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs)
{
	if (bs == DOS_BUS)
		registers_offset_config(&desc[AV1D_IPP_DIR_CFG], -(0x0490 - 0x0419), 1);
}

void s6_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs)
{
	if (bs == DOS_BUS) {
		printk("s6 dos register compat\n");
		registers_offset_config(&desc[HEVC_STREAM_CRC],
			REG_OFFSET(0x3176, 0x3175),
			REG_COMPAT_RANGE(HEVC_STREAM_CRC, VP9_ACP_CTRL));

		registers_offset_config(&desc[HEVC_ASSIST_AMR1_INT0],
			REG_OFFSET(0x3015, 0x3025),
			REG_COMPAT_RANGE(HEVC_ASSIST_AMR1_INT0, HEVC_ASSIST_MBX_SSEL));

		registers_offset_config(&desc[HEVC_ASSIST_TIMER0_LO],
			REG_OFFSET(0x0036, 0x0060),
			REG_COMPAT_RANGE(HEVC_ASSIST_TIMER0_LO, HEVC_ASSIST_DMA_INT_MSK2));

		registers_offset_config(&desc[HEVC_ASSIST_MBOX0_IRQ_REG],
			REG_OFFSET(0x0040, 0x0070),
			REG_COMPAT_RANGE(HEVC_ASSIST_MBOX0_IRQ_REG, HEVC_ASSIST_AXI_STATUS2_LO));

		registers_offset_config(&desc[HEVC_ASSIST_SCRATCH_0],
			REG_OFFSET(0x00b0, 0x00c0),
			REG_COMPAT_RANGE(HEVC_ASSIST_SCRATCH_0, HEVC_ASSIST_SCRATCH_N));

		registers_offset_config(&desc[AV1D_IPP_DIR_CFG],
			REG_OFFSET(0x0419, 0x0490),
			REG_COMPAT_RANGE(AV1D_IPP_DIR_CFG, AV1D_IPP_DIR_CFG));
	}
}

void s7_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs)
{
	if (bs == DOS_BUS) {
		registers_offset_config(&desc[HEVC_SLICE_DATA_CTL], -(0x0172 - 0x0175), 1);

		registers_offset_config(&desc[HEVC_STREAM_CRC],
			-(0x0175 - 0x0173),
			(VP9_ACP_CTRL - HEVC_STREAM_CRC + 1));
	}
}

//###############################################################################

/*********************************************
 * reg : dos register
 * mask: valid bit mask
 * idle: wait 0 or 1 for idle
**********************************************/
int dos_wait_status(int reg, int mask, int idle)
{
	u32 nop_cnt = 10;
	ulong timeout = jiffies + (HZ/10);
	u32 val = READ_VREG(reg);

	while ((val & mask) != idle) {
		if (time_after(jiffies, timeout)) {
			pr_crit("%s timeout, reg %x, val %x, mask %x\n",
				__func__, reg, val, mask);
			return -EBUSY;
		}
		val = READ_VREG(reg);
	}
	while (nop_cnt--);

	return 0;
}
EXPORT_SYMBOL(dos_wait_status);

u32 dos_reg_compat_convert(u32 addr)
{
		s32 offset = 0;
		struct bus_reg_desc *dos_desc = reg_desc[DOS_BUS];

		if (dos_desc && !(addr & NEW_REG_CHECK_MASK))
			offset = dos_desc[addr].reg_compat_offset;
		addr &= (~NEW_REG_CHECK_MASK);

		return (addr + offset);
}
EXPORT_SYMBOL(dos_reg_compat_convert);

void write_dos_reg(u32 addr, int val)
{
	s32 offset = 0;
	struct bus_reg_desc *dos_desc = reg_desc[DOS_BUS];

	if (dos_desc && !(addr & NEW_REG_CHECK_MASK))
		offset = dos_desc[addr].reg_compat_offset;
	addr &= (~NEW_REG_CHECK_MASK);

	if (unlikely((offset + addr) < 0)) {
		pr_err("%s out of range, addr %x, offset %d\n",
			__func__, addr, offset);
		return;
	}

	dbg_mm_reg(DBG_DOS_WR, "%s((0x%x + %d), 0x%x)\n",
		__func__, addr, offset, val);

	writel(val, reg_base[DOS_BUS] + ((offset + addr) << 2));
}
EXPORT_SYMBOL(write_dos_reg);

int read_dos_reg(u32 addr)
{
	int value;
	struct bus_reg_desc *dos_desc = reg_desc[DOS_BUS];
	s32 offset = 0;

	if (dos_desc && !(addr & NEW_REG_CHECK_MASK))
		offset = dos_desc[addr].reg_compat_offset;
	addr &= (~NEW_REG_CHECK_MASK);

	if (unlikely((offset + addr) < 0)) {
		pr_err("%s out of range, addr %x, offset %d\n",
			__func__, addr, offset);
		return -ENXIO;
	}

	value = readl(reg_base[DOS_BUS] + ((offset + addr) << 2));

	dbg_mm_reg(DBG_DOS_RD, "%s(0x%x + %d) = 0x%x\n",
		__func__, addr, offset, value);

	return value;
}
EXPORT_SYMBOL(read_dos_reg);

int read_dos_reg_comp(u32 addr)
{
	if (is_support_new_dos_dev())
		return read_dos_reg(addr);
	else
		return aml_read_dosbus((uint)addr);
}
EXPORT_SYMBOL(read_dos_reg_comp);

void write_dos_reg_comp(u32 addr, int val)
{
	if (is_support_new_dos_dev())
		write_dos_reg(addr, val);
	else
		aml_write_dosbus((uint)addr, (uint)val);
}
EXPORT_SYMBOL(write_dos_reg_comp);

void dos_reg_write_bits(u32 reg, u32 val, int start, int len)
{
	u32 to_val = read_dos_reg_comp(reg);
	u32 mask = (((1L << (len)) - 1) << (start));

	to_val &= ~mask;
	to_val |= (val << start) & mask;
	write_dos_reg_comp(reg, to_val);
}
EXPORT_SYMBOL(dos_reg_write_bits);

/***************************************DMC REG*****************************************************/
void write_dmc_reg(u32 reg, int val)
{
	if (reg_base[DMC_BUS] == 0) {
		pr_err("%s(%x, %x), err iomap\n", __func__, reg, val);
		return;
	}

	dbg_mm_reg(DBG_DMC_WR, "%s(%x, %x)\n", __func__, reg, val);

	writel(val, reg_base[DMC_BUS] + (reg << 2));
}
EXPORT_SYMBOL(write_dmc_reg);

int read_dmc_reg(u32 reg)
{
	int val;

	if (reg_base[DMC_BUS] == 0) {
		pr_err("%s(%x), err iomap\n", __func__, reg);
		return -1;
	}

	val = readl(reg_base[DMC_BUS] + (reg << 2));

	dbg_mm_reg(DBG_DMC_RD, "%s(%x) = %x\n", __func__, reg, val);

	return val;
}
EXPORT_SYMBOL(read_dmc_reg);

/***************************************SYSCTRL REG*****************************************************/
void write_sysctrl_reg(u32 reg, int val)
{
	if (reg_base[SYSCTRL_BUS] == 0) {
		pr_err("%s(%x, %x), err iomap\n", __func__, reg, val);
		return;
	}

	dbg_mm_reg(DBG_SYSCTRL_WR, "%s(%x, %x)\n", __func__, reg, val);

	writel(val, reg_base[SYSCTRL_BUS] + (reg << 2));
}
EXPORT_SYMBOL(write_sysctrl_reg);

int read_sysctrl_reg(u32 reg)
{
	int val;

	if (reg_base[SYSCTRL_BUS] == 0) {
		pr_err("%s(%x), err iomap\n", __func__, reg);
		return -1;
	}

	val = readl(reg_base[SYSCTRL_BUS] + (reg << 2));

	dbg_mm_reg(DBG_SYSCTRL_RD, "%s(%x) = %x\n", __func__, reg, val);

	return val;
}
EXPORT_SYMBOL(read_sysctrl_reg);

int dos_register_probe(struct platform_device *pdev, reg_compat_func reg_compat_fn)
{
	u32 i;
	struct resource res;
	u32 res_size;

	if (pdev == NULL) {
		pr_info("no dev found, dos_register can not map\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_REG_BUS; i++) {
		if (of_address_to_resource(pdev->dev.of_node, i, &res)) {
			pr_err("of_address_to_resource failed\n");
			return -EINVAL;
		}

		res_size = resource_size(&res);
		reg_base[i] = ioremap(res.start, res_size);

		pr_info("%s, res start %llx, end %llx, iomap: %px\n",
			__func__, (unsigned long long)res.start,
			(unsigned long long)res.end, reg_base[i]);

		if ((i == DOS_BUS) && reg_compat_fn) {
			reg_desc[i] = (struct bus_reg_desc *)kzalloc(res_size *
				sizeof(struct bus_reg_desc), GFP_KERNEL);
			if (!reg_desc[i])
				pr_err("Warn: dos regs offset table alloc failed\n");
			else
				reg_compat_fn(reg_desc[i], i);
		}
	}

	return 0;
}
EXPORT_SYMBOL(dos_register_probe);


