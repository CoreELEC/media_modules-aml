/* SPDX-License-Identifier: GPL-2.0
 ****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright 2020 Verisilicon(Beijing) Co.,Ltd. All Rights Reserved.
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
 *    Copyright 2020 Verisilicon(Beijing) Co.,Ltd. All Rights Reserved.
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

#ifndef __VC8000_NORMAL_CFG_H__
#define __VC8000_NORMAL_CFG_H__

/* Configure information without CMD, fill according to System Memory Map*/

#define RESOURCE_SHARED_INTER_SUBSYS                                                               \
    0 /*0:no resource sharing inter subsystems 1: existing resource sharing*/
/* Sub-System 0 */
#ifdef EMU
#define SUBSYS_0_IO_ADDR (0x6020000) /*customer specify according to own platform*/
#else
#define SUBSYS_0_IO_ADDR (0x90000) /*customer specify according to own platform*/
#endif
#define SUBSYS_0_IO_SIZE (10000 * 4) /* bytes */
#define INT_PIN_SUBSYS_0_VC8000E (-1)
#define INT_PIN_SUBSYS_0_DEC400 (-1)
#define REG_OFFSET_SUBSYS_0_VC8000E (0x1000)
#define REG_OFFSET_SUBSYS_0_DEC400 (0x2000)

/* Sub-System 1 */
#ifdef EMU
#define SUBSYS_0_IO_ADDR (0x6000000) /*customer specify according to own platform*/
#else
#define SUBSYS_1_IO_ADDR (0xA0000)
#endif
#define SUBSYS_1_IO_SIZE (10000 * 4)
#define INT_PIN_SUBSYS_1_CUTREE (-1)
#define REG_OFFSET_SUBSYS_1_CUTREE (0x1000)

/* Subsystem configure
 * base_addr, iosize, resource_shared
 */
static SUBSYS_CONFIG subsys_array[] = {
    {SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS},
    //{SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS},
};

/* Core configure
 * slice_idx, core_type, offset, reg_size, irq
 */
static CORE_CONFIG core_array[] = {
    {0, CORE_VC8000E, REG_OFFSET_SUBSYS_0_VC8000E, ENCODER_REGISTER_SIZE * 4,
     INT_PIN_SUBSYS_0_VC8000E}, // RESOURCE_SHARED_INTER_SUBSYS
    //{0, CORE_DEC400, REG_OFFSET_SUBSYS_0_DEC400, 1600*4, INT_PIN_SUBSYS_0_DEC400, RESOURCE_SHARED_INTER_SUBSYS},
    //{1, CORE_CUTREE, REG_OFFSET_SUBSYS_1_CUTREE, 500*4, INT_PIN_SUBSYS_1_CUTREE, RESOURCE_SHARED_INTER_SUBSYS},
};

#endif

// BELOW PORT FROM vc8000_vcmd_driver.c
// KEEP AS REFERENCE

#if 0
#define RESOURCE_SHARED_INTER_SUBSYS                                                               \
    0 /*0:no resource sharing inter subsystems 1: existing resource sharing*/
#define SUBSYS_0_IO_ADDR 0x90000     /*customer specify according to own platform*/
#define SUBSYS_0_IO_SIZE (10000 * 4) /* bytes */

#define SUBSYS_1_IO_ADDR 0xA0000     /*customer specify according to own platform*/
#define SUBSYS_1_IO_SIZE (10000 * 4) /* bytes */

#define INT_PIN_SUBSYS_0_VC8000E -1
#define INT_PIN_SUBSYS_0_CUTREE -1
#define INT_PIN_SUBSYS_0_DEC400 -1
#define INT_PIN_SUBSYS_0_L2CACHE -1
#define INT_PIN_SUBSYS_1_VC8000E -1
#define INT_PIN_SUBSYS_1_CUTREE -1
#define INT_PIN_SUBSYS_1_DEC400 -1

/*for all subsystem, the subsys info should be listed here for subsequent use*/
/*base_addr, iosize, resource_shared*/
SUBSYS_CONFIG subsys_array[] = {
  {SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_0
  //{SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_1
};

/*here config every core in all subsystem*/
/*NOTE: no matter what format(HEVC/H264/JPEG/AV1/...) is supported in VC8000E, just use [CORE_VC8000E] to indicate it's a VC8000E core*/
/*      CUTREE can work standalone, so it can be a subsytem or just one core of a subsytem.*/
/*subsys_idx, core_type, offset, reg_size, irq*/
CORE_CONFIG core_array[] = {
  {0, CORE_VC8000E, 0x1000, ENCODER_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_VC8000E
  //{0, CORE_MMU, 0x2000, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_VC8000E
  //{0, CORE_AXIFE, 0x3000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_AXIFE
  //{0, CORE_MMU_1, 0x4000, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_VC8000E
  //{0, CORE_AXIFE_1, 0x5000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_AXIFE_1
  //{0, CORE_DEC400, 0x6000, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VC8000E}, //subsys_0_DEC400
  //{0, CORE_L2CACHE, 0xc000, L2CACHE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_L2CACHE}, //subsys_0_l2cache
  //{0, CORE_CUTREE, 0xd000, IM_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_0_CUTREE

  //{1, CORE_CUTREE, 0x1000, IM_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_CUTREE}, //CUTREE
  //{1, CORE_MMU, 0x2000, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_1_MMU
  //{1, CORE_AXIFE, 0x3000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_1_AXIFE

};
#endif
// END KEEP AS REFERENCE