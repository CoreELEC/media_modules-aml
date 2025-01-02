// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    COPYRIGHT (C) 2014 VERISILICON ALL RIGHTS RESERVED
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
 *    COPYRIGHT (C) 2014 VERISILICON ALL RIGHTS RESERVED
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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/version.h>
/* Our header */
#include "memalloc.h"

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/compat.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../common/media_utils/media_kernel_version.h"

#ifndef HLINA_START_ADDRESS
#define HLINA_START_ADDRESS 0x02000000
#endif

#ifndef HLINA_SIZE
#define HLINA_SIZE 96
#endif

#ifndef HLINA_TRANSL_OFFSET
#define HLINA_TRANSL_OFFSET 0x0
#endif

/* the size of chunk in MEMALLOC_DYNAMIC */
#define CHUNK_SIZE (PAGE_SIZE * 4)

static struct attribute *vencmem_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(vencmem_class);
#define CLASS_NAME "vencmem"
#define DEVICE_NAME "memalloc"

static struct class vencmem_class = {
	.name = CLASS_NAME,
	.class_groups = vencmem_class_groups,
};

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

#define enc_pr(level, x...) \
	do { \
		if (level >= print_level) \
			printk(x); \
	} while (0)

static s32 print_level = LOG_ERROR;

static struct device*  device;
/* memory size in MBs for MEMALLOC_DYNAMIC */
static u32 alloc_size = 96;
static ulong alloc_base = 0;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 3, 13)
	static DEFINE_SEMAPHORE(s_vpu_sem);
#else
	static DEFINE_SEMAPHORE(s_vpu_sem, 1);
#endif
static struct memalloc_drv_context_t s_memalloc_drv_context;

/* user space SW will subtract HLINA_TRANSL_OFFSET from the bus address
 * and decoder HW will use the result as the address translated base
 * address. The SW needs the original host memory bus address for memory
 * mapping to virtual address.
 */
static ulong addr_transl = HLINA_TRANSL_OFFSET;

static s32 memalloc_major; /* dynamic */
static s32 s_register_flag;

/* module_param(name, type, perm) */
module_param(alloc_size, uint, 0);
module_param(alloc_base, ulong, 0);
module_param(addr_transl, ulong, 0);

static DEFINE_SPINLOCK(mem_lock);

typedef struct hlinc {
    ulong bus_address;
    u16 chunks_reserved;
    const struct file *filp; /* Client that allocated this chunk */
} hlina_chunk;

static hlina_chunk *hlina_chunks;
static size_t chunks;

static s32 AllocMemory(ulong *busaddr, u32 size, const struct file *filp);
static s32 FreeMemory(ulong busaddr, const struct file *filp);
static void ResetMems(void);

static s32 venc_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret = 0;
    unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot);

    return ret;
}

static long memalloc_ioctl(struct file *filp, u32 cmd, ulong arg)
{
    s32 ret = 0;
    MemallocParams memparams;
    ulong busaddr;
#ifdef CONFIG_COMPAT
    compat_ulong_t addr32;
    compat_MemallocParams memparams32;
#endif
    //spin_lock(&mem_lock);

    switch (cmd) {
    case MEMALLOC_IOCGMEMBASE:
        __put_user(alloc_base, (ulong __user *)arg);
        break;
#ifdef CONFIG_COMPAT
    case MEMALLOC_IOCGMEMBASE32:
        addr32 = (compat_ulong_t)alloc_base;
        __put_user(addr32, (compat_ulong_t __user *)arg);
        break;
#endif
    case MEMALLOC_IOCHARDRESET:
        ResetMems();
        break;
    case MEMALLOC_IOCXGETBUFFER:
        ret = copy_from_user(&memparams, (MemallocParams __user *)arg, sizeof(MemallocParams));
        if (ret)
            break;

        ret = AllocMemory(&memparams.bus_address, memparams.size, filp);

        memparams.translation_offset = addr_transl;

        ret |= copy_to_user((MemallocParams __user *)arg, &memparams, sizeof(MemallocParams));

        break;

#ifdef CONFIG_COMPAT
    case MEMALLOC_IOCXGETBUFFER32:
        {
            ret = copy_from_user(&memparams32, (compat_MemallocParams __user *)arg, sizeof(compat_MemallocParams));
            if (ret)
                break;
            memparams.bus_address = (ulong)memparams32.bus_address;
            memparams.size = memparams32.size;
            memparams.translation_offset = (ulong)memparams32.translation_offset;
            memparams.mem_type = memparams32.mem_type;

            ret = AllocMemory(&memparams.bus_address, memparams.size, filp);

            memparams32.bus_address = (compat_ulong_t)memparams.bus_address;
            memparams32.size = memparams.size;
            memparams32.translation_offset = addr_transl;
            memparams32.mem_type = memparams.mem_type;

            ret |= copy_to_user((void __user *)arg, &memparams32, sizeof(compat_MemallocParams));
        }
        break;
#endif
    case MEMALLOC_IOCSFREEBUFFER:
        __get_user(busaddr, (ulong __user *)arg);
        ret = FreeMemory(busaddr, filp);
        break;
#ifdef CONFIG_COMPAT
    case MEMALLOC_IOCSFREEBUFFER32:
        __get_user(addr32, (compat_ulong_t __user *)arg);
        busaddr = (ulong)addr32;
        ret = FreeMemory(busaddr, filp);
#endif
        break;
    default:
        break;

    }

    //spin_unlock(&mem_lock);

    return ret ? -EFAULT : 0;
}

static int memalloc_open(struct inode *inode, struct file *filp)
{
	bool first_open = false;
	s32 r = 0;

	//enc_pr(LOG_DEBUG, "[+] %s, filp=%lu, %lu, f_count=%lld\n", __func__,
			//(unsigned long)filp, ( ((unsigned long)filp)%8), filp->f_count.counter);
	spin_lock(&mem_lock);
	s_memalloc_drv_context.open_count++;
	if (s_memalloc_drv_context.open_count == 1) {
		first_open = true;
	}
	filp->private_data = (void *)(&s_memalloc_drv_context);
	spin_unlock(&mem_lock);
	if (first_open) {
#ifdef CONFIG_CMA
		alloc_base = codec_mm_alloc_for_dma(DEVICE_NAME, alloc_size*SZ_1M >> PAGE_SHIFT, 0, 0);
		if (alloc_base) {
			enc_pr(LOG_DEBUG, "memalloc: alloc_size = 0x%x,Linear memory base = %px\n", alloc_size,
					(void *)alloc_base);

			chunks = (alloc_size * 1024 * 1024) / CHUNK_SIZE;

			enc_pr(LOG_DEBUG, "memalloc: Total size %d MB; %d chunks of size %lu\n", alloc_size, (int)chunks,
					CHUNK_SIZE);

			hlina_chunks = vmalloc(chunks * sizeof(hlina_chunk));
			if (!hlina_chunks) {
				enc_pr(LOG_ERROR, "memalloc: cannot allocate hlina_chunks\n");
				r = -ENOMEM;
				codec_mm_free_for_dma(DEVICE_NAME, alloc_base);
				alloc_base = 0;
			} else {
				ResetMems();
			}
		} else {
			enc_pr(LOG_ERROR, "%s: failed to alloc alloc_base\n", __func__);
			r = -ENOMEM;
		}
#else
		enc_pr(LOG_ERROR, "No CMA and reserved memory for memalloc!!!\n");
		r = -ENOMEM;
#endif
	} else if (!alloc_base) {
		enc_pr(LOG_ERROR, "memalloc memory is not malloced yet wait & retry!\n");
		r = -EBUSY;
	}
	if (r != 0) {
		spin_lock(&mem_lock);
		s_memalloc_drv_context.open_count--;
		spin_unlock(&mem_lock);
	}
	enc_pr(LOG_DEBUG, "[-] %s, ret: %d\n", __func__, r);
	return r;
}

static s32 memalloc_release(struct inode *inode, struct file *filp)
{
	s32 ret = 0;
	u32 open_count;
	s32 i;

	//enc_pr(LOG_DEBUG, "memalloc_release filp=%lu, f_counter=%lld\n",
			//(unsigned long)filp, filp->f_count.counter);
	ret = down_interruptible(&s_vpu_sem);

	if (ret == 0) {
		spin_lock(&mem_lock);
		s_memalloc_drv_context.open_count--;
		open_count = s_memalloc_drv_context.open_count;
		spin_unlock(&mem_lock);

		enc_pr(LOG_DEBUG, "open_count=%u\n", open_count);
		for (i = 0; i < chunks; i++) {
			if (hlina_chunks[i].filp == filp) {
				enc_pr(LOG_DEBUG, "memalloc: Found unfreed memory at release time!\n");

				hlina_chunks[i].filp = NULL;
				hlina_chunks[i].chunks_reserved = 0;
			}
		}

		if (open_count == 0) {
			if (alloc_base) {
				enc_pr(LOG_DEBUG,
					"memalloc_release, alloc_base 0x%lx\n",
					alloc_base);
				codec_mm_free_for_dma(
					DEVICE_NAME,
					alloc_base);
			}
			alloc_base = 0;
			if (hlina_chunks)
				vfree(hlina_chunks);
			hlina_chunks = NULL;
		}
	}
	up(&s_vpu_sem);
	return 0;
}

#ifdef CONFIG_COMPAT
static long memalloc_compat_ioctl(struct file *filp,
    u32 cmd, ulong args)
{
    long ret;

    args = (ulong)compat_ptr(args);
    ret = memalloc_ioctl(filp, cmd, args);

    return ret;
}
#endif

/* VFS methods */
static struct file_operations memalloc_fops = {.owner = THIS_MODULE,
                                               .open = memalloc_open,
                                               .release = memalloc_release,
                                               .unlocked_ioctl = memalloc_ioctl,
                                               .mmap = venc_mem_mmap,
#ifdef CONFIG_COMPAT
											   .compat_ioctl = memalloc_compat_ioctl,
#endif
};

static s32 init_memalloc_device(void)
{
    s32  r = 0;

    r = register_chrdev(memalloc_major, DEVICE_NAME, &memalloc_fops);
    if (r <= 0) {
        enc_pr(LOG_ERROR, "memalloc: unable to get major <%d>\n", memalloc_major);
        return r;
    }

    memalloc_major = r;

    r = class_register(&vencmem_class);
    if (r < 0) {
        enc_pr(LOG_ERROR, "hantro: error create venc class!");
        return r;
    }
    s_register_flag = 1;

    device = device_create(&vencmem_class, NULL, MKDEV(memalloc_major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        class_unregister(&vencmem_class);
        return -1;
    }
    return r;
}

static s32 uninit_memalloc_device(void)
{
    if (device)
        device_destroy(&vencmem_class, MKDEV(memalloc_major, 0));

    if (s_register_flag)
        class_destroy(&vencmem_class);
    s_register_flag = 0;

    if (memalloc_major)
        unregister_chrdev(memalloc_major, DEVICE_NAME);
    memalloc_major = 0;
    return 0;
}

static void memalloc_cleanup(struct platform_device *pf_dev)
{
    uninit_memalloc_device();
    enc_pr(LOG_DEBUG, "module removed\n");
}

static s32 memalloc_init(struct platform_device *pf_dev)
{
    s32 result;

    memalloc_major = 0;
    s_register_flag = 0;
    result = init_memalloc_device();
    if (result < 0) {
        enc_pr(LOG_ERROR, "memalloc: could not allocate major number\n");
        result = -EBUSY;
        goto err;
    }
    /* 8g memory support */
    dma_coerce_mask_and_coherent(&pf_dev->dev, DMA_BIT_MASK(64));

    return 0;

err:
    uninit_memalloc_device();

    return result;
}

/* Cycle through the buffers we have, give the first free one */
static s32 AllocMemory(ulong *busaddr, u32 size, const struct file *filp)
{
    s32 i = 0;
    s32 j = 0;
    u32 skip_chunks = 0;

    /* calculate how many chunks we need; round up to chunk boundary */
    u32 alloc_chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    *busaddr = 0;

    /* run through the chunk table */
    for (i = 0; i < chunks;) {
        skip_chunks = 0;
        /* if this chunk is available */
        if (!hlina_chunks[i].chunks_reserved) {
            /* check that there is enough memory left */
            if (i + alloc_chunks > chunks)
                break;

            /* check that there is enough consecutive chunks available */
            for (j = i; j < i + alloc_chunks; j++) {
                if (hlina_chunks[j].chunks_reserved) {
                    skip_chunks = 1;
                    /* skip the used chunks */
                    i = j + hlina_chunks[j].chunks_reserved;
                    break;
                }
            }

            /* if enough free memory found */
            if (!skip_chunks) {
                *busaddr = hlina_chunks[i].bus_address;
                hlina_chunks[i].filp = filp;
                hlina_chunks[i].chunks_reserved = alloc_chunks;
                break;
            }
        } else {
            /* skip the used chunks */
            i += hlina_chunks[i].chunks_reserved;
        }
    }

    if (*busaddr == 0) {
        pr_info("memalloc: Allocation FAILED: size = %d\n", size);
        return -EFAULT;
    } else {
        PDEBUG("MEMALLOC OK: size: %d, reserved: %ld\n", size, alloc_chunks * CHUNK_SIZE);
    }

    return 0;
}

/* Free a buffer based on bus address */
static s32 FreeMemory(ulong busaddr, const struct file *filp)
{
    s32 i = 0;

    for (i = 0; i < chunks; i++) {
        /* user space SW has stored the translated bus address, add addr_transl to
		 * translate back to our address space
		 */
        if (hlina_chunks[i].bus_address == busaddr + addr_transl) {
            if (hlina_chunks[i].filp == filp) {
                hlina_chunks[i].filp = NULL;
                hlina_chunks[i].chunks_reserved = 0;
            } else {
                pr_warn("memalloc: Owner mismatch while freeing memory!\n");
            }
            break;
        }
    }
    return 0;
}

/* Reset "used" status */
static void ResetMems(void)
{
    s32 i = 0;
    ulong ba = alloc_base;

    for (i = 0; i < chunks; i++) {
        hlina_chunks[i].bus_address = ba;
        hlina_chunks[i].filp = NULL;
        hlina_chunks[i].chunks_reserved = 0;

        ba += CHUNK_SIZE;
    }
}

static s32 encmem_vce_probe(struct platform_device *pf_dev)
{
    enc_pr(LOG_DEBUG, "encmem_vce_probe\n");
    memalloc_init(pf_dev);
    return 0;
}

static KV_INT_TO_VOID encmem_vce_remove(struct platform_device *pf_dev)
{
    enc_pr(LOG_DEBUG, "encmem_vce_remove:\n");
    memalloc_cleanup(pf_dev);
    return KV_RET_x_TO_VOID(0);
}

static const struct of_device_id amlogic_venc_mem_match[] = {{
                                                                 .compatible = "encmem_rev",
                                                             },
                                                             {}};

static struct platform_driver venc_mem_driver = {.probe = encmem_vce_probe,
                                                 .remove = encmem_vce_remove,
                                                 .driver = {
                                                     .name = "encmem_rev",
                                                     .owner = THIS_MODULE,
                                                     .of_match_table = amlogic_venc_mem_match,
                                                 }};

int __init enc_memallc_init(void)
{
    if ((get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S5)
        && (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S6)) {
        //pr_info("The chip is not support vers memalloc!!\n");
        return -1;
    }
    return platform_driver_register(&venc_mem_driver);
}

void __exit enc_memallc_exit(void)
{
    platform_driver_unregister(&venc_mem_driver);
}

module_init(enc_memallc_init);
module_exit(enc_memallc_exit);

/* module description */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("Linear RAM allocation");
