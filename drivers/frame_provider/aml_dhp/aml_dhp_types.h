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
#ifndef _AML_DHP_TYPES_H_
#define _AML_DHP_TYPES_H_

#include <linux/types.h>

#include "../../amvdec_ports/aml_image_info.h"

/*
 * Data Handler Proxy (DHP) supported data types and memory types.
 *
 * TAG(a, b, c, d):
 *   Macro to generate a 32-bit identifier using four character constants.
 *   This is used to define unique data type tags.
 */
#define TAG(a, b, c, d)    ((a << 24) | (b << 16) | (c << 8) | d)

/*
 * DHP Supported Data Types:
 * AML_DHP_TYPE_AVBCD: Represents amlogic video buffer compression decoding tasks.
 * AML_DHP_TYPE_AVBCE: Represents amlogic video buffer compression encoding tasks.
 * AML_DHP_TYPE_MEM:   Represents general memory processing tasks.
 */
#define AML_DHP_TYPE_AVBCD    TAG('V', 'B', 'C', 'D')
#define AML_DHP_TYPE_AVBCE    TAG('V', 'B', 'C', 'E')
#define AML_DHP_TYPE_MEM      TAG('P', 'M', 'E', 'M')
//ADD...

/*
 * Memory Type Definitions (AML_MEM_TYPE_*):
 *   These constants define the type of memory used in data processing tasks:
 * AML_MEM_TYPE_PFN:       Memory described by a Page Frame Number (PFN).
 * AML_MEM_TYPE_PHY_ADDR:  Memory described by a physical address.
 * AML_MEM_TYPE_KPTR_ADDR: Reserved for future use, represents a kernel pointer address (TODO).
 * AML_MEM_TYPE_UPTR_ADDR: Memory described by a user-space pointer.
 * AML_MEM_TYPE_SG_TBL:    Reserved for future use, represents a scatter-gather table.
 */
#define AML_MEM_TYPE_PFN       (1)
#define AML_MEM_TYPE_PHY_ADDR  (2)
#define AML_MEM_TYPE_KPTR_ADDR (3)
#define AML_MEM_TYPE_UPTR_ADDR (4)
#define AML_MEM_TYPE_SG_TBL    (5)

/*
 * struct aml_du_mem - Represents memory information for a data unit in the DHP driver.
 *
 * @type      : Type of memory (e.g., PFN, physical address, etc.) as defined by AML_MEM_TYPE_*.
 * @addr      : Generic address for the memory region (used for physical or virtual addresses).
 * @pfn       : Page Frame Number, used when @type is AML_MEM_TYPE_PFN.
 * @kptr      : Kernel pointer, reserved for future use when @type is AML_MEM_TYPE_KPTR_ADDR.
 * @uptr      : User pointer, used when @type is AML_MEM_TYPE_UPTR_ADDR.
 * @sgt       : Scatter-gather table, used when @type is AML_MEM_TYPE_SG_TBL (reserved for future use).
 * @size      : Size of the memory region in bytes.
 * @payload   : Auxiliary data related to the memory unit, purpose/application-specific.
 * @uncached  : Flag indicating whether the memory region is uncached (non-zero if true).
 * @syncflag  : Synchronization flags for memory operations, based on DHP_MEM_SYNC_*.
 * @img       : Image metadata, used when the memory content represents an image.
 */
struct aml_du_mem {
    __u32 type;
    union {
        __u64 addr;
        __u64 pfn;
        __u64 kptr; // TODO
        __u64 uptr;
        __u64 sgt;
    };
    __u32 size;
    __u32 payload;
    __u32 uncached;
    __u64 syncflag;
    union {
        struct image_info_s img;
    };
} __attribute__((packed));

/*
 * struct aml_du_avbcd - Describes AVBC decoder-related image data.
 *
 * @type     : Type of the AVBC data (e.g., H.264, HEVC).
 * @width    : Width of the image in pixels.
 * @height   : Height of the image in pixels.
 * @crop     : Cropping rectangle (top, left, bottom, right).
 * @pixel    : Pixel format (e.g., YUV420, RGB).
 * @bitdep   : Bit depth of the image (e.g., 8-bit, 10-bit).
 * @header   : Pointer to compression information header.
 * @hsize    : Size of the compression header in bytes.
 * @pts      : Presentation timestamp, used to identify frames in video..
 */
struct aml_du_avbcd {
    __u32 type;
    __u32 width;
    __u32 height;
    struct {
        __u32 top;
        __u32 left;
        __u32 bottom;
        __u32 right;
    } crop;
    __u32 pixel;
    __u32 bitdep;
    __u64 header;
    __u32 hsize;
    __u32 pts;
} __attribute__((packed));

#endif /* _AML_DHP_TYPES_H_ */

