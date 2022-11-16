// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices in your
 *    version of this file.
 *
 *****************************************************************************
 */
#ifdef __FREERTOS__
#include "base_type.h"
#include "dev_common_freertos.h"
#include "io_tools.h"
#elif defined(__linux__)
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#endif

#include <stddef.h>
#include "vc8000_axife.h"
/* mode description
 * 1: OYB normal(enable)
 * 2: bypass
 */
u32 AXIFEEnable(volatile u8 *hwregs, u32 mode)
{
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
    if (!hwregs)
        return -1;

    //AXI FE pass through
    if (mode == 1) {
        iowrite32(0x02, (void *)(hwregs + AXI_REG10_SW_FRONTEND_EN));
        iowrite32(0x00, (void *)(hwregs + AXI_REG11_SW_WORK_MODE));
    } else if (mode == 2) {
        iowrite32(0x02, (void *)(hwregs + AXI_REG10_SW_FRONTEND_EN));
        iowrite32(0x40, (void *)(hwregs + AXI_REG11_SW_WORK_MODE));
    }
    pr_info("%s: axife_reg10_addr=0x%p, *axife_reg10_addr=0x%08x\n", __func__, hwregs + 10 * 4,
            ioread32((void *)(hwregs + 10 * 4)));
    pr_info("%s: axife_reg11_addr=0x%p, *axife_reg11_addr=0x%08x\n", __func__, hwregs + 11 * 4,
            ioread32((void *)(hwregs + 11 * 4)));
#endif
    return 0;
}
