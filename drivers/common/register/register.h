#ifndef _DOS_REGISTER_H_
#define _DOS_REGISTER_H_

#include <linux/platform_device.h>
#include "../../include/regs/dos_registers.h"
#include <linux/amlogic/media/utils/vdec_reg.h>


typedef enum {
	DOS_BUS,
	MAX_REG_BUS
} MM_BUS_ENUM;

struct bus_reg_desc {
	char const *reg_name;
	s32 reg_compat_offset;
};

#define WRITE_VREG(addr, val) write_dos_reg(addr, val)

#define READ_VREG(addr) read_dos_reg(addr)

int read_dos_reg(ulong addr);
void write_dos_reg(ulong addr, int val);
void dos_reg_write_bits(unsigned int reg, u32 val, int start, int len);


#define WRITE_VREG_BITS(r, val, start, len) dos_reg_write_bits(r, val, start, len)
#define CLEAR_VREG_MASK(r, mask)   write_dos_reg(r, read_dos_reg(r) & ~(mask))
#define SET_VREG_MASK(r, mask)     write_dos_reg(r, read_dos_reg(r) | (mask))


#define READ_HREG(r) read_dos_reg((r) | 0x1000)
#define WRITE_HREG(r, val) write_dos_reg((r) | 0x1000, val)
#define WRITE_HREG_BITS(r, val, start, len) \
	dos_reg_write_bits((r) | 0x1000, val, start, len)
//#define SET_HREG_MASK(r, mask) codec_set_dosbus_mask((r) | 0x1000, mask)
//#define CLEAR_HREG_MASK(r, mask) codec_clear_dosbus_mask((r) | 0x1000, mask)

//##############################################################
//#define DEBUG_SC2

typedef void (*reg_compat_func)(struct bus_reg_desc *, MM_BUS_ENUM bus);

#ifdef DEBUG_SC2
void sc2_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);
#endif

void t3_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);
void s5_mm_registers_compat(struct bus_reg_desc *desc, MM_BUS_ENUM bs);


ulong dos_reg_compat_convert(ulong addr);

void write_dos_reg(ulong addr, int val);

int read_dos_reg(ulong addr);


int dos_register_probe(struct platform_device *pdev, reg_compat_func reg_compat_fn);


#endif
