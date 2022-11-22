// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    COPYRIGHT (C) 2019 VERISILICON ALL RIGHTS RESERVED
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
 *    COPYRIGHT (C) 2019 VERISILICON ALL RIGHTS RESERVED
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

#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range
 *   SetPageReserved
 *   ClearPageReserved
 */
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>

#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/moduleparam.h>
/* request_irq(), free_irq() */
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/semaphore.h>
#include <linux/spinlock.h>
/* needed for virt_to_phys() */
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>

#include <asm/irq.h>

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/delay.h>

/* our own stuff */
#include <linux/platform_device.h>
#include "vcmdswhwregisters.h"
#include "bidirect_list.h"
#include "vc8000_driver.h"
#include "vc8000_vcmd_cfg.h"
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/moduleparam.h>
#include <linux/of_address.h>
#include <linux/kthread.h>
#include <linux/ioport.h>
#include <linux/dma-buf.h>
#include <linux/compat.h>


//#define VCMD_DEBUG_INTERNAL
/*these size need to be modified according to hw config.*/
#define VCMD_ENCODER_REGISTER_SIZE              (ENCODER_REGISTER_SIZE * 4)
#define VCMD_DECODER_REGISTER_SIZE              (DECODER_REGISTER_SIZE * 4)
#define VCMD_IM_REGISTER_SIZE                   (IM_REGISTER_SIZE * 4)
#define VCMD_JPEG_ENCODER_REGISTER_SIZE         (JPEG_ENCODER_REGISTER_SIZE * 4)
#define VCMD_JPEG_DECODER_REGISTER_SIZE         (JPEG_DECODER_REGISTER_SIZE * 4)

#define MAX_VCMD_NUMBER (MAX_VCMD_TYPE * MAX_SAME_MODULE_TYPE_CORE_NUMBER) //

#define HW_WORK_STATE_PEND                3

#define MAX_CMDBUF_INT_NUMBER             1
#define INT_MIN_SUM_OF_IMAGE_SIZE         (4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER * MAX_CMDBUF_INT_NUMBER)
#define MAX_PROCESS_CORE_NUMBER           (4 * 8)
#define PROCESS_MAX_SUM_OF_IMAGE_SIZE     (4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER * MAX_PROCESS_CORE_NUMBER)

#define MAX_SAME_MODULE_TYPE_CORE_NUMBER  4

#ifdef PCIE_EN
/*******************PCIE CONFIG*************************/
#ifdef EMU
#define START_MEM_OFFSET               0x4800000
#define PCI_VENDOR_ID_HANTRO           0x1d9b//0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO_PCI       0xface//0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              2
/* Base address DDR register */
#define PCI_DDR_BAR             4
#else
#define START_MEM_OFFSET               0//0x800000
#define PCI_VENDOR_ID_HANTRO           0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO_PCI       0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              4
/* Base address DDR register */
#define PCI_DDR_BAR             0
#endif

static unsigned long g_vcmd_base_hdwr;   /* PCI base register address (Hardware address) */
static unsigned long g_vcmd_base_ddr_hw; /* PCI base register address (memalloc) */
static u32 g_vcmd_base_len;              /* Base register address Length */
#endif

/*pcie address need to subtract this value then can be put to register*/
static size_t base_ddr_addr;

#ifdef HANTROAXIFE_SUPPORT
#define AXIFE_SIZE (64 * 4)
volatile u8 *axife_hwregs[MAX_VCMD_NUMBER][2];
#endif

#ifdef HANTROMMU_SUPPORT
#define MMU_SIZE (228 * 4)
extern unsigned int mmu_enable;
extern unsigned long gBaseDDRHw;
extern volatile u8 *mmu_hwregs[HXDEC_MAX_CORES][2];
#else
static unsigned int mmu_enable;
#endif

int ret;
/********variables declaration related with race condition**********/

#define CMDBUF_MAX_SIZE       (512 * 4 * 4)

#define CMDBUF_POOL_TOTAL_SIZE            (2 * 1024 * 1024) //approximately=128x(320x240)=128x2k=128x8kbyte=1Mbytes
#define TOTAL_DISCRETE_CMDBUF_NUM         (CMDBUF_POOL_TOTAL_SIZE / CMDBUF_MAX_SIZE)
#define CMDBUF_VCMD_REGISTER_TOTAL_SIZE   (9 * 1024 * 1024 - CMDBUF_POOL_TOTAL_SIZE * 2)
#define VCMD_REGISTER_SIZE                (128 * 4)

struct noncache_mem {
	u32 *virtualAddress;
	dma_addr_t busAddress;
	size_t mmu_bus_address; /* buffer physical address in MMU*/
	u32 size;
	u16 cmdbuf_id;
};

struct process_manager_obj {
	struct file *filp;
	u32 total_exe_time;
	spinlock_t spinlock;
	wait_queue_head_t wait_queue;
};

struct cmdbuf_obj {
	u32 module_type; //current CMDBUF type: input vc8000e=0,IM=1,vc8000d=2,jpege=3, jpegd=4
	u32 priority; //current CMDBUFpriority: normal=0, high=1
	u32 executing_time; //current CMDBUFexecuting_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	u32 cmdbuf_size; //current CMDBUF size
	u32 *cmdbuf_virtualAddress; //current CMDBUF start virtual address.
	size_t cmdbuf_busAddress; //current CMDBUF start physical address.
	size_t mmu_cmdbuf_busAddress; //current CMDBUF start mmu mapping address.
	u32 *status_virtualAddress; //current status CMDBUF start virtual address.
	size_t status_busAddress; //current status CMDBUF start physical address.
	size_t mmu_status_busAddress; //current status CMDBUF start mmu mapping address.
	u32 status_size; //current status CMDBUF size
	u32 executing_status; //current CMDBUF executing status.
	struct file *filp; //file pointer in the same process.
	u16 core_id; //which vcmd core is used.
	u16 cmdbuf_id; //used to manage CMDBUF in driver.It is a handle to identify cmdbuf.also is an interrupt vector.position in pool,same as status position.
	u8 cmdbuf_data_loaded; //0 means sw has not copied data into this CMDBUF; 1 means sw has copied data into this CMDBUF
	u8 cmdbuf_data_linked; //0 :not linked, 1:linked into a vcmd core list.
	u8 cmdbuf_run_done; //if 0,waiting for CMDBUF finish; if 1, op code in CMDBUF has finished one by one. HANTRO_VCMD_IOCH_WAIT_CMDBUF will check this variable.
	u8 cmdbuf_need_remove; // if 0, not need to remove CMDBUF; 1 CMDBUF can be removed if it is not the last CMDBUF;
	u8 has_end_cmdbuf; //if 1, the last opcode is end opCode.
	u8 no_normal_int_cmdbuf; //if 1, JMP will not send normal interrupt.
	struct process_manager_obj *process_manager_obj;
};

struct hantrovcmd_dev {
	struct vcmd_config vcmd_core_cfg; //config of each core,such as base addr, irq,etc
	u32 core_id; //vcmd core id for driver and sw internal use
	u32 sw_cmdbuf_rdy_num;
	spinlock_t *spinlock;
	wait_queue_head_t *wait_queue;
	wait_queue_head_t *wait_abort_queue;
	bi_list list_manager;

	volatile u8 *hwregs; /* IO mem base */
	u32 reg_mirror[ASIC_VCMD_SWREG_AMOUNT];
	u32 duration_without_int; //number of cmdbufs without interrupt.

	volatile u8 working_state;
	u32 total_exe_time;
	u16 status_cmdbuf_id; //used for analyse configuration in cwl.
	u32 hw_version_id; /*megvii 0x43421001, later 0x43421102*/
	u32 *vcmd_reg_mem_virtualAddress; //start virtual address of vcmd registers memory of  CMDBUF.
	size_t vcmd_reg_mem_busAddress; //start physical address of vcmd registers memory of  CMDBUF.

	unsigned int mmu_vcmd_reg_mem_busAddress; //start mmu mapping address of vcmd registers memory of CMDBUF.
	u32 vcmd_reg_mem_size; // size of vcmd registers memory of CMDBUF.
};

/*
 * Ioctl definitions
 */
#define VCMD_HW_ID                  0x4342


static struct noncache_mem vcmd_buf_mem_pool;
static struct noncache_mem vcmd_status_buf_mem_pool;
static struct noncache_mem vcmd_registers_mem_pool;

static u16 cmdbuf_used[TOTAL_DISCRETE_CMDBUF_NUM];
static u16 cmdbuf_used_pos;
static u16 cmdbuf_used_residual;

static struct hantrovcmd_dev *vcmd_manager[MAX_VCMD_TYPE][MAX_VCMD_NUMBER];
static bi_list_node *global_cmdbuf_node[TOTAL_DISCRETE_CMDBUF_NUM];

static bi_list global_process_manager;

static u16 vcmd_position[MAX_VCMD_TYPE];
static int vcmd_type_core_num[MAX_VCMD_TYPE];

#define EXECUTING_CMDBUF_ID_ADDR       26
#define VCMD_EXE_CMDBUF_COUNT           3

#define WORKING_STATE_IDLE          0
#define WORKING_STATE_WORKING       1
#define CMDBUF_EXE_STATUS_OK        0
#define CMDBUF_EXE_STATUS_CMDERR        1
#define CMDBUF_EXE_STATUS_BUSERR        2

static struct semaphore vcmd_reserve_cmdbuf_sem[MAX_VCMD_TYPE]; //for reserve

/***************************TYPE AND FUNCTION DECLARATION****************/

/* here's all the must remember stuff */

static int vcmd_reserve_IO(void);
static void vcmd_release_IO(void);
static void vcmd_reset_asic(struct hantrovcmd_dev *dev);
static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev);
static int allocate_cmdbuf(struct noncache_mem *new_cmdbuf_addr,
			   struct noncache_mem *new_status_cmdbuf_addr);
static void vcmd_link_cmdbuf(struct hantrovcmd_dev *dev,
			     bi_list_node *last_linked_cmdbuf_node);
static void vcmd_start(struct hantrovcmd_dev *dev,
		       bi_list_node *first_linked_cmdbuf_node);
static void create_kernel_process_manager(void);

#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
static irqreturn_t hantrovcmd_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hantrovcmd_isr(int irq, void *dev_id);
#endif

#ifdef VCMD_DEBUG_INTERNAL
static void printk_vcmd_register_debug(const void *hwregs, char *info);
#endif

/*********************local variable declaration*****************/
static unsigned long vcmd_sram_base;
static unsigned int vcmd_sram_size;
/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hantrovcmd_major;
static int total_vcmd_core_num;
/* dynamic allocation*/
static struct hantrovcmd_dev *hantrovcmd_data;

static int software_triger_abort;

//#define IRQ_SIMULATION

#ifdef IRQ_SIMULATION
struct timer_manager {
	u32 core_id; //vcmd core id for driver and sw internal use
	u32 timer_id;
	struct timer_list *timer;
};

static struct timer_list timer[10000];
struct timer_manager timer_reserve[10000];
#if 0
static struct timer_list timer0;
static struct timer_list timer1;
#endif
#endif

//hw_queue can be used for reserve cmdbuf memory
static DECLARE_WAIT_QUEUE_HEAD(vcmd_cmdbuf_memory_wait);

static DEFINE_SPINLOCK(vcmd_cmdbuf_alloc_lock);
static DEFINE_SPINLOCK(vcmd_process_manager_lock);

static spinlock_t owner_lock_vcmd[MAX_VCMD_NUMBER];

static wait_queue_head_t wait_queue_vcmd[MAX_VCMD_NUMBER];

static wait_queue_head_t abort_queue_vcmd[MAX_VCMD_NUMBER];

#if 0
/*allocate non-cacheable DMA memory*/
#define DRIVER_NAME_HANTRO_NON_CACH_MEM "non_cach_memory"

static struct platform_device *noncachable_mem_dev;

static const struct platform_device_info hantro_platform_info = {
	.name = DRIVER_NAME_HANTRO_NON_CACH_MEM,
	.id = -1,
	.dma_mask = DMA_BIT_MASK(32),
};

static int hantro_noncachable_mem_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	vcmd_buf_mem_pool.virtualAddress = dma_alloc_coherent(dev, CMDBUF_POOL_TOTAL_SIZE, &vcmd_buf_mem_pool.busAddress, GFP_KERNEL | GFP_DMA);
	vcmd_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
	vcmd_status_buf_mem_pool.virtualAddress = dma_alloc_coherent(dev, CMDBUF_POOL_TOTAL_SIZE, &vcmd_status_buf_mem_pool.busAddress, GFP_KERNEL | GFP_DMA);
	vcmd_status_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
	return 0;
}

static int hantro_noncachable_mem_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dma_free_coherent(dev, vcmd_buf_mem_pool.size, vcmd_buf_mem_pool.virtualAddress, vcmd_buf_mem_pool.busAddress);
	dma_free_coherent(dev, vcmd_status_buf_mem_pool.size, vcmd_status_buf_mem_pool.virtualAddress, vcmd_status_buf_mem_pool.busAddress);

	return 0;
}

static const struct platform_device_id hantro_noncachable_mem_platform_ids[] = {
	{
		.name = DRIVER_NAME_HANTRO_NON_CACH_MEM,
	},
	{/* sentinel */},
};

static const struct of_device_id  hantro_of_match[] = {
	{
		.compatible  =  "platform-vc8000e",
	},
	{/* sentinel */},
};

static struct platform_driver hantro_noncachable_mem_platform_driver = {
	.probe  = hantro_noncachable_mem_probe,
	.remove  = hantro_noncachable_mem_remove,
	.driver = {
		.name = DRIVER_NAME_HANTRO_NON_CACH_MEM,
		.owner  = THIS_MODULE,
		.of_match_table = hantro_of_match,
	},
	.id_table = hantro_noncachable_mem_platform_ids,
};

static void init_vcmd_non_cachable_memory_allocate(void)
{
	/*create device: This will create a {struct platform_device}, It has a member dev, which is a {struct device} */
	noncachable_mem_dev = platform_device_register_full(&hantro_platform_info);

	/*when this function is called, the .probe callback is invoked.*/
	platform_driver_register(&hantro_noncachable_mem_platform_driver);
}

static void release_vcmd_non_cachable_memory(void)
{
	/* when this function is called, .remove callback will be invoked. use it to clean up all resources allocated in .probe.*/
	platform_driver_unregister(&hantro_noncachable_mem_platform_driver);

	/*destroy the device*/
	platform_device_unregister(noncachable_mem_dev);
}
#endif

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

#define enc_pr(level, x...) \
        do { \
                    if (level >= print_level) \
                        printk(x); \
                } while (0)
static DEFINE_SEMAPHORE(s_vers_sem);
static spinlock_t s_dma_buf_lock = __SPIN_LOCK_UNLOCKED(s_dma_buf_lock);
static struct list_head s_dma_bufp_head = LIST_HEAD_INIT(s_dma_bufp_head);
static s32 print_level = LOG_DEBUG;
static struct platform_device *versenc_pdev;

struct vers_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	unsigned long paddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
};

/* To track the occupied dma_buf  */
struct versdrv_dma_buf_pool_t {
	struct list_head list;
	struct vers_dma_cfg dma_cfg;
	struct file *filp;
};

struct versdrv_dma_buf_info_t {
	u32 num_planes;
	s32 fd[3];
	u32 phys_addr[3]; /* phys address for DMA buffer */
};

static s32 vers_dma_buf_release(struct file *filp);
static s32 vers_dma_buffer_get_phys(struct vers_dma_cfg *cfg,
		unsigned long *addr);
static int vers_dma_buffer_map(struct vers_dma_cfg *cfg);
static void vers_dma_buffer_unmap(struct vers_dma_cfg *cfg);
static s32 vers_src_addr_config(struct versdrv_dma_buf_info_t *pinfo,
		struct file *filp);
static s32 vers_src_addr_unmap(struct versdrv_dma_buf_info_t *pinfo,
		struct file *filp);

static s32 vers_dma_buf_release(struct file *filp)
{
	struct versdrv_dma_buf_pool_t *pool, *n;
	struct vers_dma_cfg vb;

	list_for_each_entry_safe(pool, n, &s_dma_bufp_head, list) {
		if (pool->filp == filp) {
			vb = pool->dma_cfg;
			if (vb.attach) {
				vers_dma_buffer_unmap(&vb);
				spin_lock(&s_dma_buf_lock);
				list_del(&pool->list);
				spin_unlock(&s_dma_buf_lock);
				kfree(pool);
			}
		}
	}
	return 0;
}

static s32 vers_dma_buffer_get_phys(struct vers_dma_cfg *cfg,
		unsigned long *addr)
{
	int ret = 0;
	if (cfg->paddr == 0)
	{ /* only mapp once */
		ret = vers_dma_buffer_map(cfg);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "vers_dma_buffer_map failed\n");
			return ret;
		}
	}
	if (cfg->paddr) *addr = cfg->paddr;
	return ret;
}

static int vers_dma_buffer_map(struct vers_dma_cfg *cfg)
{
	int ret = -1;
	int fd = -1;
	struct page *page = NULL;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL) {
		enc_pr(LOG_ERROR, "error dma param\n");
		return -EINVAL;
	}
	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		enc_pr(LOG_ERROR, "failed to get dma buffer,fd %d\n",fd);
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		enc_pr(LOG_ERROR, "failed to set dma attach\n");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		enc_pr(LOG_ERROR, "failed to get dma sg\n");
		goto map_attach_err;
	}

	page = sg_page(sg->sgl);
	cfg->paddr = PFN_PHYS(page_to_pfn(page));
	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->vaddr = vaddr;
	cfg->sg = sg;

	return 0;

map_attach_err:
	dma_buf_detach(dbuf, d_att);
attach_err:
	dma_buf_put(dbuf);

	return ret;
}

static void vers_dma_buffer_unmap(struct vers_dma_cfg *cfg)
{
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	/*void *vaddr = NULL;*/
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL
			|| cfg->dbuf == NULL /*|| cfg->vaddr == NULL*/
			|| cfg->attach == NULL || cfg->sg == NULL) {
		enc_pr(LOG_ERROR, "unmap: Error dma param\n");
		return;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = cfg->dbuf;
	d_att = cfg->attach;
	sg = cfg->sg;

	dma_buf_unmap_attachment(d_att, sg, dir);
	dma_buf_detach(dbuf, d_att);
	dma_buf_put(dbuf);
}

static s32 vers_src_addr_config(struct versdrv_dma_buf_info_t *pinfo,
		struct file *filp)
{
	struct versdrv_dma_buf_pool_t *vbp;
	unsigned long phy_addr;
	struct vers_dma_cfg *cfg;
	s32 idx, ret = 0;
	if (pinfo->num_planes == 0 || pinfo->num_planes > 3)
		return -EFAULT;

	for (idx = 0; idx < pinfo->num_planes; idx++)
		pinfo->phys_addr[idx] = 0;
	for (idx = 0; idx < pinfo->num_planes; idx++) {
		vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
		if (!vbp) {
			ret = -ENOMEM;
			break;
		}
		memset(vbp, 0, sizeof(struct versdrv_dma_buf_pool_t));
		cfg = &vbp->dma_cfg;
		cfg->dir = DMA_TO_DEVICE;
		cfg->fd = pinfo->fd[idx];
		cfg->dev = &(versenc_pdev->dev);
		phy_addr = 0;
		ret = vers_dma_buffer_get_phys(cfg, &phy_addr);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "import fd %d failed\n", cfg->fd);
			kfree(vbp);
			ret = -1;
			break;
		}
		pinfo->phys_addr[idx] = (ulong) phy_addr;
		vbp->filp = filp;
		spin_lock(&s_dma_buf_lock);
		list_add(&vbp->list, &s_dma_bufp_head);
		spin_unlock(&s_dma_buf_lock);
	}

	return ret;
}

static s32 vers_src_addr_unmap(struct versdrv_dma_buf_info_t *pinfo,
		struct file *filp)
{
	struct versdrv_dma_buf_pool_t *pool, *n;
	struct vers_dma_cfg vb;
	ulong phys_addr;
	s32 plane_idx = 0;
	s32 ret = 0;
	s32 found;

	if (pinfo->num_planes == 0 || pinfo->num_planes > 3)
		return -EFAULT;

	enc_pr(LOG_INFO,
		"dma_unmap planes %d fd: %d-%d-%d, phy_add: 0x%lx-%lx-%lx\n",
		pinfo->num_planes, pinfo->fd[0],pinfo->fd[1], pinfo->fd[2],
		pinfo->phys_addr[0], pinfo->phys_addr[1], pinfo->phys_addr[2]);

	list_for_each_entry_safe(pool, n, &s_dma_bufp_head, list) {
		found = 0;
		if (pool->filp == filp) {
			vb = pool->dma_cfg;
			phys_addr = vb.paddr;
			if (vb.fd == pinfo->fd[0])
			{
				if (phys_addr != pinfo->phys_addr[0]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 0");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[0]);
				}
				found = 1;
				plane_idx++;
			}
			else if (vb.fd == pinfo->fd[1]
				&& pinfo->num_planes > 1) {
				if (phys_addr != pinfo->phys_addr[1]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 1");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[1]);
				}
				plane_idx++;
				found = 1;
			}
			else if (vb.fd == pinfo->fd[2]
				&& pinfo->num_planes > 2) {
				if (phys_addr != pinfo->phys_addr[2]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 2");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[2]);
				}
				plane_idx++;
				found = 1;
			}
			if (found && vb.attach) {
				vers_dma_buffer_unmap(&vb);
				spin_lock(&s_dma_buf_lock);
				list_del(&pool->list);
				spin_unlock(&s_dma_buf_lock);
				kfree(pool);
			}
		}
	}

	if (plane_idx != pinfo->num_planes) {
		enc_pr(LOG_DEBUG, "dma_unmap fd planes not match\n");
		enc_pr(LOG_DEBUG, " found %d need %d\n",
		       plane_idx, pinfo->num_planes);
	}
	return ret;
}

#ifdef VCMD_DEBUG_INTERNAL
static void PrintInstr(u32 i, u32 instr, u32 *size)
{
	if ((instr & 0xF8000000) == OPCODE_WREG) {
		int length = ((instr >> 16) & 0x3FF);

		pr_info("current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n",
			i, instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			length, (instr & 0xFFFF));
		*size = ((length + 2) >> 1) << 1;
	} else if ((instr & 0xF8000000) == OPCODE_END) {
		pr_info("current cmdbuf data %d = 0x%08x => [%s]\n", i, instr, "END");
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_NOP) {
		pr_info("current cmdbuf data %d = 0x%08x => [%s]\n", i, instr, "NOP");
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_RREG) {
		int length = ((instr >> 16) & 0x3FF);

		pr_info("current cmdbuf data %d = 0x%08x => [%s %s %d 0x%x]\n",
			i, instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			length, (instr & 0xFFFF));
		*size = 4;
	} else if ((instr & 0xF8000000) == OPCODE_JMP) {
		pr_info("current cmdbuf data %d = 0x%08x => [%s %s %s]\n", i,
			instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
			((instr >> 25) & 0x1) ? "IE" : "");
		*size = 4;
	} else if ((instr & 0xF8000000) == OPCODE_STALL) {
		pr_info("current cmdbuf data %d = 0x%08x => [%s %s 0x%x]\n", i,
			instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
			(instr & 0xFFFF));
		*size = 2;
	} else if ((instr & 0xF8000000) == OPCODE_CLRINT) {
		pr_info("current cmdbuf data %d = 0x%08x => [%s %d 0x%x]\n", i,
			instr, "CLRINT", (instr >> 25) & 0x3, (instr & 0xFFFF));
		*size = 2;
	} else
		*size = 1;
}
#endif
/******************************************************************************
 *cmdbuf object management
 ******************************************************************************
 */
static struct cmdbuf_obj *create_cmdbuf_obj(void)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	cmdbuf_obj = vmalloc(sizeof(struct cmdbuf_obj));
	if (!cmdbuf_obj) {
		PDEBUG("%s\n", "vmalloc for cmdbuf_obj fail!");
		return cmdbuf_obj;
	}
	memset(cmdbuf_obj, 0, sizeof(struct cmdbuf_obj));
	return cmdbuf_obj;
}

static void free_cmdbuf_obj(struct cmdbuf_obj *cmdbuf_obj)
{
	if (!cmdbuf_obj) {
		PDEBUG("%s\n", "remove_cmdbuf_obj NULL");
		return;
	}
	//free current cmdbuf_obj
	vfree(cmdbuf_obj);
}

static void free_cmdbuf_mem(u16 cmdbuf_id)
{
	unsigned long flags;

	spin_lock_irqsave(&vcmd_cmdbuf_alloc_lock, flags);
	cmdbuf_used[cmdbuf_id] = 0;
	cmdbuf_used_residual += 1;
	spin_unlock_irqrestore(&vcmd_cmdbuf_alloc_lock, flags);
	wake_up_interruptible_all(&vcmd_cmdbuf_memory_wait);
}

static bi_list_node *create_cmdbuf_node(void)
{
	bi_list_node *current_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	struct noncache_mem new_cmdbuf_addr;
	struct noncache_mem new_status_cmdbuf_addr;

	if (wait_event_interruptible(vcmd_cmdbuf_memory_wait,
				     allocate_cmdbuf(&new_cmdbuf_addr,
						     &new_status_cmdbuf_addr)))
		return NULL;
	cmdbuf_obj = create_cmdbuf_obj();
	if (!cmdbuf_obj) {
		PDEBUG("%s\n", "create_cmdbuf_obj fail!");
		free_cmdbuf_mem(new_cmdbuf_addr.cmdbuf_id);
		return NULL;
	}
	cmdbuf_obj->cmdbuf_busAddress = new_cmdbuf_addr.busAddress;
	cmdbuf_obj->mmu_cmdbuf_busAddress = new_cmdbuf_addr.mmu_bus_address;
	cmdbuf_obj->cmdbuf_virtualAddress = new_cmdbuf_addr.virtualAddress;
	cmdbuf_obj->cmdbuf_size = new_cmdbuf_addr.size;
	cmdbuf_obj->cmdbuf_id = new_cmdbuf_addr.cmdbuf_id;
	cmdbuf_obj->status_busAddress = new_status_cmdbuf_addr.busAddress;
	cmdbuf_obj->mmu_status_busAddress =
		new_status_cmdbuf_addr.mmu_bus_address;
	cmdbuf_obj->status_virtualAddress =
		new_status_cmdbuf_addr.virtualAddress;
	cmdbuf_obj->status_size = new_status_cmdbuf_addr.size;
	current_node = bi_list_create_node();
	if (!current_node) {
		PDEBUG("%s\n", "bi_list_create_node fail!");
		free_cmdbuf_mem(new_cmdbuf_addr.cmdbuf_id);
		free_cmdbuf_obj(cmdbuf_obj);
		return NULL;
	}
	current_node->data = (void *)cmdbuf_obj;
	current_node->next = NULL;
	current_node->previous = NULL;
	return current_node;
}

static void free_cmdbuf_node(bi_list_node *cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (!cmdbuf_node) {
		PDEBUG("%s\n", "remove_cmdbuf_node NULL");
		return;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)cmdbuf_node->data;
	//free cmdbuf mem in pool
	free_cmdbuf_mem(cmdbuf_obj->cmdbuf_id);

	//free struct cmdbuf_obj
	free_cmdbuf_obj(cmdbuf_obj);
	//free current cmdbuf_node entity.
	bi_list_free_node(cmdbuf_node);
}

//just remove, not free the node.
static bi_list_node *remove_cmdbuf_node_from_list(bi_list *list,
						  bi_list_node *cmdbuf_node)
{
	if (!cmdbuf_node) {
		PDEBUG("%s\n", "remove_cmdbuf_node_from_list  NULL");
		return NULL;
	}
	if (cmdbuf_node->next) {
		bi_list_remove_node(list, cmdbuf_node);
		return cmdbuf_node;
	} else {
		//the last one, should not be removed.
		return NULL;
	}
}

//calculate executing_time of each vcmd
static u32 calculate_executing_time_after_node(bi_list_node *exe_cmdbuf_node)
{
	u32 time_run_all = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;

	while (1) {
		if (!exe_cmdbuf_node)
			break;
		cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
		time_run_all += cmdbuf_obj_temp->executing_time;
		exe_cmdbuf_node = exe_cmdbuf_node->next;
	}
	return time_run_all;
}
static u32 calculate_executing_time_after_node_high_priority(bi_list_node *exe_cmdbuf_node)
{
	u32 time_run_all = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;

	if (!exe_cmdbuf_node)
		return time_run_all;
	cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
	time_run_all += cmdbuf_obj_temp->executing_time;
	exe_cmdbuf_node = exe_cmdbuf_node->next;
	while (1) {
		if (!exe_cmdbuf_node)
			break;
		cmdbuf_obj_temp = (struct cmdbuf_obj *)exe_cmdbuf_node->data;
		if (cmdbuf_obj_temp->priority == CMDBUF_PRIORITY_NORMAL)
			break;
		time_run_all += cmdbuf_obj_temp->executing_time;
		exe_cmdbuf_node = exe_cmdbuf_node->next;
	}
	return time_run_all;
}

/******************************************************************************
 *cmdbuf pool management
 ******************************************************************************
 */
static int allocate_cmdbuf(struct noncache_mem *new_cmdbuf_addr,
			   struct noncache_mem *new_status_cmdbuf_addr)
{
	unsigned long flags;

	spin_lock_irqsave(&vcmd_cmdbuf_alloc_lock, flags);
	if (cmdbuf_used_residual == 0) {
		spin_unlock_irqrestore(&vcmd_cmdbuf_alloc_lock, flags);
		//no empty cmdbuf
		return 0;
	}
	//there is one cmdbuf at least
	while (1) {
		if (cmdbuf_used[cmdbuf_used_pos] == 0 && !global_cmdbuf_node[cmdbuf_used_pos]) {
			cmdbuf_used[cmdbuf_used_pos] = 1;
			cmdbuf_used_residual -= 1;
			new_cmdbuf_addr->virtualAddress =
				vcmd_buf_mem_pool.virtualAddress + cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
			new_cmdbuf_addr->busAddress =
				vcmd_buf_mem_pool.busAddress + cmdbuf_used_pos * CMDBUF_MAX_SIZE;
			new_cmdbuf_addr->mmu_bus_address =
				vcmd_buf_mem_pool.mmu_bus_address + cmdbuf_used_pos * CMDBUF_MAX_SIZE;
			new_cmdbuf_addr->size = CMDBUF_MAX_SIZE;
			new_cmdbuf_addr->cmdbuf_id = cmdbuf_used_pos;
			new_status_cmdbuf_addr->virtualAddress =
				vcmd_status_buf_mem_pool.virtualAddress + cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
			new_status_cmdbuf_addr->busAddress =
				vcmd_status_buf_mem_pool.busAddress + cmdbuf_used_pos * CMDBUF_MAX_SIZE;
			new_status_cmdbuf_addr->mmu_bus_address =
				vcmd_status_buf_mem_pool.mmu_bus_address + cmdbuf_used_pos * CMDBUF_MAX_SIZE;
			new_status_cmdbuf_addr->size = CMDBUF_MAX_SIZE;
			new_status_cmdbuf_addr->cmdbuf_id = cmdbuf_used_pos;
			cmdbuf_used_pos++;
			if (cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
				cmdbuf_used_pos = 0;
			spin_unlock_irqrestore(&vcmd_cmdbuf_alloc_lock, flags);
			return 1;
		} else {
			cmdbuf_used_pos++;
			if (cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
				cmdbuf_used_pos = 0;
		}
	}
	return 0;
}

static bi_list_node *get_cmdbuf_node_in_list_by_addr(size_t cmdbuf_addr,
						     bi_list *list)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	new_cmdbuf_node = list->head;
	while (1) {
		if (!new_cmdbuf_node)
			return NULL;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) <= cmdbuf_addr) &&
		    (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr + cmdbuf_obj->cmdbuf_size) > cmdbuf_addr))) {
			return new_cmdbuf_node;
		}
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
	return NULL;
}

static int wait_abort_rdy(struct hantrovcmd_dev *dev)
{
	return dev->working_state == WORKING_STATE_IDLE;
}

static int select_vcmd(bi_list_node *new_cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *curr_cmdbuf_node = NULL;
	bi_list *list = NULL;
	struct hantrovcmd_dev *dev = NULL;
	struct hantrovcmd_dev *smallest_dev = NULL;
	u32 executing_time = 0xffff;
	int counter = 0;
	unsigned long flags = 0;
	u32 hw_rdy_cmdbuf_num = 0;
	size_t exe_cmdbuf_addr = 0;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	u32 cmdbuf_id = 0;

	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	//there is an empty vcmd to be used
	while (1) {
		dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
		list = &dev->list_manager;
		spin_lock_irqsave(dev->spinlock, flags);
		if (!list->tail) {
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			cmdbuf_obj->core_id = dev->core_id;
			return 0;
		} else {
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
		}
		if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}
	//there is a vcmd which tail node -> cmdbuf_run_done == 1. It means this vcmd has nothing to do, so we select it.
	counter = 0;
	while (1) {
		dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
		list = &dev->list_manager;
		spin_lock_irqsave(dev->spinlock, flags);
		curr_cmdbuf_node = list->tail;
		if (!curr_cmdbuf_node) {
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			cmdbuf_obj->core_id = dev->core_id;
			return 0;
		}
		cmdbuf_obj_temp = (struct cmdbuf_obj *)curr_cmdbuf_node->data;
		if (cmdbuf_obj_temp->cmdbuf_run_done == 1) {
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			cmdbuf_obj->core_id = dev->core_id;
			return 0;
		} else {
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
		}
		if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	//another case, tail = executing node, and vcmd=pend state (finish but not generate interrupt)
	counter = 0;
	while (1) {
		dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
		list = &dev->list_manager;
		//read executing cmdbuf address
		if (dev->hw_version_id <= HW_ID_1_0_C)
			hw_rdy_cmdbuf_num = vcmd_get_register_value(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXE_CMDBUF_COUNT);
		else {
			hw_rdy_cmdbuf_num = *(dev->vcmd_reg_mem_virtualAddress +
					      VCMD_EXE_CMDBUF_COUNT);
			if (hw_rdy_cmdbuf_num != dev->sw_cmdbuf_rdy_num)
				hw_rdy_cmdbuf_num += 1;
		}
		spin_lock_irqsave(dev->spinlock, flags);
		curr_cmdbuf_node = list->tail;
		if (!curr_cmdbuf_node) {
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			cmdbuf_obj->core_id = dev->core_id;
			return 0;
		}

		if (dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num) {
			bi_list_insert_node_tail(list, new_cmdbuf_node);
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			cmdbuf_obj->core_id = dev->core_id;
			return 0;
		} else {
			spin_unlock_irqrestore(dev->spinlock, flags);
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
		}
		if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
			break;
	}

	//there is no idle vcmd,if low priority,calculate exe time, select the least one.
	// or if high priority, calculate the exe time, select the least one and abort it.
	if (cmdbuf_obj->priority == CMDBUF_PRIORITY_NORMAL) {
		counter = 0;
		//calculate total execute time of all devices
		while (1) {
			dev = vcmd_manager[cmdbuf_obj->module_type]
				[vcmd_position[cmdbuf_obj->module_type]];
			//read executing cmdbuf address
			if (dev->hw_version_id <= HW_ID_1_0_C) {
				exe_cmdbuf_addr = VCMDGetAddrRegisterValue((const void *)dev->hwregs,
									   dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR);
				list = &dev->list_manager;
				spin_lock_irqsave(dev->spinlock, flags);
				//get the executing cmdbuf node.
				curr_cmdbuf_node =
					get_cmdbuf_node_in_list_by_addr(exe_cmdbuf_addr, list);

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			} else {
				/*cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,dev->reg_mirror,
				 *                           HWIF_VCMD_CMDBUF_EXECUTING_ID);
				 */
				cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress + EXECUTING_CMDBUF_ID_ADDR + 1);
				spin_lock_irqsave(dev->spinlock, flags);
				if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM ||
				    cmdbuf_id == 0) {
					pr_info("cmdbuf_id greater than the ceiling !!\n");
					spin_unlock_irqrestore(dev->spinlock, flags);
					return -1;
				}
				//get the executing cmdbuf node.
				curr_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
				if (!curr_cmdbuf_node) {
					list = &dev->list_manager;
					curr_cmdbuf_node = list->head;
					while (1) {
						if (!curr_cmdbuf_node)
							break;
						cmdbuf_obj_temp =
							(struct cmdbuf_obj *)curr_cmdbuf_node->data;
						if (cmdbuf_obj_temp->cmdbuf_data_linked &&
						    cmdbuf_obj_temp->cmdbuf_run_done == 0)
							break;
						curr_cmdbuf_node = curr_cmdbuf_node->next;
					}
				}

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			}
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//find the device with the least total_exe_time.
		counter = 0;
		executing_time = 0xffffffff;
		while (1) {
			dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
			if (dev->total_exe_time <= executing_time) {
				executing_time = dev->total_exe_time;
				smallest_dev = dev;
			}
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//insert list
		list = &smallest_dev->list_manager;
		spin_lock_irqsave(smallest_dev->spinlock, flags);
		bi_list_insert_node_tail(list, new_cmdbuf_node);
		spin_unlock_irqrestore(smallest_dev->spinlock, flags);
		cmdbuf_obj->core_id = smallest_dev->core_id;
		return 0;
	} else {
		//CMDBUF_PRIORITY_HIGH
		counter = 0;
		//calculate total execute time of all devices
		while (1) {
			dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
			if (dev->hw_version_id <= HW_ID_1_0_C) {
				//read executing cmdbuf address
				exe_cmdbuf_addr = VCMDGetAddrRegisterValue(
					(const void *)dev->hwregs,
					dev->reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR);
				list = &dev->list_manager;
				spin_lock_irqsave(dev->spinlock, flags);
				//get the executing cmdbuf node.
				curr_cmdbuf_node =
					get_cmdbuf_node_in_list_by_addr(exe_cmdbuf_addr, list);

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node_high_priority(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			} else {
				/*cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,dev->reg_mirror,
				 *                      HWIF_VCMD_CMDBUF_EXECUTING_ID);
				 */
				cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress + EXECUTING_CMDBUF_ID_ADDR);
				spin_lock_irqsave(dev->spinlock, flags);
				if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM || cmdbuf_id == 0) {
					pr_info("cmdbuf_id greater than the ceiling !!\n");
					spin_unlock_irqrestore(dev->spinlock, flags);
					return -1;
				}
				//get the executing cmdbuf node.
				curr_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
				if (!curr_cmdbuf_node) {
					list = &dev->list_manager;
					curr_cmdbuf_node = list->head;
					while (1) {
						if (!curr_cmdbuf_node)
							break;
						cmdbuf_obj_temp =
							(struct cmdbuf_obj *)curr_cmdbuf_node->data;
						if (cmdbuf_obj_temp->cmdbuf_data_linked &&
						    cmdbuf_obj_temp->cmdbuf_run_done == 0)
							break;
						curr_cmdbuf_node = curr_cmdbuf_node->next;
					}
				}

				//calculate total execute time of this device
				dev->total_exe_time =
					calculate_executing_time_after_node(curr_cmdbuf_node);
				spin_unlock_irqrestore(dev->spinlock, flags);
			}
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//find the smallest device.
		counter = 0;
		executing_time = 0xffffffff;
		while (1) {
			dev = vcmd_manager[cmdbuf_obj->module_type][vcmd_position[cmdbuf_obj->module_type]];
			if (dev->total_exe_time <= executing_time) {
				executing_time = dev->total_exe_time;
				smallest_dev = dev;
			}
			vcmd_position[cmdbuf_obj->module_type]++;
			if (vcmd_position[cmdbuf_obj->module_type] >= vcmd_type_core_num[cmdbuf_obj->module_type])
				vcmd_position[cmdbuf_obj->module_type] = 0;
			counter++;
			if (counter >= vcmd_type_core_num[cmdbuf_obj->module_type])
				break;
		}
		//abort the vcmd and wait
		vcmd_write_register_value((const void *)smallest_dev->hwregs,
					  smallest_dev->reg_mirror,
					  HWIF_VCMD_START_TRIGGER, 0);
		software_triger_abort = 1;
		if (wait_event_interruptible(*smallest_dev->wait_abort_queue, wait_abort_rdy(smallest_dev))) {
			software_triger_abort = 0;
			return -ERESTARTSYS;
		}
		software_triger_abort = 0;
		//need to select inserting position again because hw maybe have run to the next node.
		//CMDBUF_PRIORITY_HIGH
		spin_lock_irqsave(smallest_dev->spinlock, flags);
		curr_cmdbuf_node = smallest_dev->list_manager.head;
		while (1) {
			//if list is empty or tail,insert to tail
			if (!curr_cmdbuf_node)
				break;
			cmdbuf_obj_temp = (struct cmdbuf_obj *)curr_cmdbuf_node->data;
			//if find the first node which priority is normal, insert node prior to  the node
			if (cmdbuf_obj_temp->priority ==
				    CMDBUF_PRIORITY_NORMAL && cmdbuf_obj_temp->cmdbuf_run_done == 0)
				break;
			curr_cmdbuf_node = curr_cmdbuf_node->next;
		}
		bi_list_insert_node_before(list, curr_cmdbuf_node, new_cmdbuf_node);
		cmdbuf_obj->core_id = smallest_dev->core_id;
		spin_unlock_irqrestore(smallest_dev->spinlock, flags);

		return 0;
	}
	return 0;
}

static int wait_process_resource_rdy(struct process_manager_obj *process_manager_obj)
{
	return process_manager_obj->total_exe_time <= PROCESS_MAX_SUM_OF_IMAGE_SIZE;
}

static long reserve_cmdbuf(struct file *filp,
			   struct exchange_parameter *input_para)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *process_manager_node = NULL;
	struct process_manager_obj *process_manager_obj = NULL;
	unsigned long flags;

	input_para->cmdbuf_id = 0;
	if (input_para->cmdbuf_size > CMDBUF_MAX_SIZE)
		return -1;
	spin_lock_irqsave(&vcmd_process_manager_lock, flags);
	process_manager_node = global_process_manager.head;
	while (1) {
		if (!process_manager_node) {
			//should not happen
			pr_err("hantrovcmd: ERROR process_manager_node !!\n");
			spin_unlock_irqrestore(&vcmd_process_manager_lock, flags);
			return -1;
		}
		process_manager_obj = (struct process_manager_obj *)process_manager_node->data;
		if (filp == process_manager_obj->filp)
			break;
		process_manager_node = process_manager_node->next;
	}
	spin_unlock_irqrestore(&vcmd_process_manager_lock, flags);
	spin_lock_irqsave(&process_manager_obj->spinlock, flags);
	process_manager_obj->total_exe_time += input_para->executing_time;
	spin_unlock_irqrestore(&process_manager_obj->spinlock, flags);
	if (wait_event_interruptible(
		    process_manager_obj->wait_queue,
		    wait_process_resource_rdy(process_manager_obj)))
		return -1;

	new_cmdbuf_node = create_cmdbuf_node();
	if (!new_cmdbuf_node)
		return -1;

	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	cmdbuf_obj->module_type = input_para->module_type;
	cmdbuf_obj->priority = input_para->priority;
	cmdbuf_obj->executing_time = input_para->executing_time;
	cmdbuf_obj->cmdbuf_size = CMDBUF_MAX_SIZE;
	input_para->cmdbuf_size = CMDBUF_MAX_SIZE;
	cmdbuf_obj->filp = filp;
	cmdbuf_obj->process_manager_obj = process_manager_obj;

	input_para->cmdbuf_id = cmdbuf_obj->cmdbuf_id;
	global_cmdbuf_node[input_para->cmdbuf_id] = new_cmdbuf_node;

	return 0;
}

static long release_cmdbuf(struct file *filp, u16 cmdbuf_id)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *last_cmdbuf_node = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list *list = NULL;
	u32 module_type;

	unsigned long flags;
	struct hantrovcmd_dev *dev = NULL;
	/*get cmdbuf object according to cmdbuf_id*/
	new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->filp != filp) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	module_type = cmdbuf_obj->module_type;
	//TODO
	if (down_interruptible(&vcmd_reserve_cmdbuf_sem[module_type]))
		return -ERESTARTSYS;
	dev = &hantrovcmd_data[cmdbuf_obj->core_id];

	//spin_lock_irqsave(dev->spinlock, flags);
	list = &dev->list_manager;
	cmdbuf_obj->cmdbuf_need_remove = 1;
	last_cmdbuf_node = new_cmdbuf_node->previous;
	while (1) {
		//remove current node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_need_remove == 1) {
			new_cmdbuf_node = remove_cmdbuf_node_from_list(
				list, new_cmdbuf_node);
			if (new_cmdbuf_node) {
				//free node
				global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
				if (cmdbuf_obj->process_manager_obj) {
					spin_lock_irqsave(&cmdbuf_obj->process_manager_obj->spinlock, flags);
					cmdbuf_obj->process_manager_obj->total_exe_time -= cmdbuf_obj->executing_time;
					spin_unlock_irqrestore(&cmdbuf_obj->process_manager_obj->spinlock,
							       flags);
					wake_up_interruptible_all(&cmdbuf_obj->process_manager_obj->wait_queue);
				}
				free_cmdbuf_node(new_cmdbuf_node);
			}
		}
		if (!last_cmdbuf_node)
			break;
		new_cmdbuf_node = last_cmdbuf_node;
		last_cmdbuf_node = new_cmdbuf_node->previous;
	}
	//spin_unlock_irqrestore(dev->spinlock, flags);
	up(&vcmd_reserve_cmdbuf_sem[module_type]);
	return 0;
}

static long release_cmdbuf_node(bi_list *list, bi_list_node *cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	/*get cmdbuf object according to cmdbuf_id*/
	new_cmdbuf_node = cmdbuf_node;
	if (!new_cmdbuf_node)
		return -1;
	//remove node from list
	new_cmdbuf_node = remove_cmdbuf_node_from_list(list, new_cmdbuf_node);
	if (new_cmdbuf_node) {
		//free node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
		free_cmdbuf_node(new_cmdbuf_node);
		return 0;
	}
	return 1;
}

static long release_cmdbuf_node_cleanup(bi_list *list)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	while (1) {
		new_cmdbuf_node = list->head;
		if (!new_cmdbuf_node)
			return 0;
		//remove node from list
		bi_list_remove_node(list, new_cmdbuf_node);
		//free node
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
		free_cmdbuf_node(new_cmdbuf_node);
	}
	return 0;
}

static bi_list_node *find_last_linked_cmdbuf(bi_list_node *current_node)
{
	bi_list_node *new_cmdbuf_node = current_node;
	bi_list_node *last_cmdbuf_node;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (!current_node)
		return NULL;
	last_cmdbuf_node = new_cmdbuf_node;
	new_cmdbuf_node = new_cmdbuf_node->previous;
	while (1) {
		if (!new_cmdbuf_node)
			return last_cmdbuf_node;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_data_linked)
			return new_cmdbuf_node;
		last_cmdbuf_node = new_cmdbuf_node;
		new_cmdbuf_node = new_cmdbuf_node->previous;
	}
	return NULL;
}

static long link_and_run_cmdbuf(struct file *filp,
				struct exchange_parameter *input_para)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *last_cmdbuf_node;
	u32 *jmp_addr = NULL;
	u32 opCode;
	u32 tempOpcode;
	u32 record_last_cmdbuf_rdy_num;
	struct hantrovcmd_dev *dev = NULL;
	unsigned long flags;
	int return_value;
	u16 cmdbuf_id = input_para->cmdbuf_id;

	new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->filp != filp) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj->cmdbuf_data_loaded = 1;
	cmdbuf_obj->cmdbuf_size = input_para->cmdbuf_size;
#ifdef VCMD_DEBUG_INTERNAL
	{
		u32 i, inst = 0, size = 0;

		pr_info("vcmd link, current cmdbuf content, cmdbuf_obj->cmdbuf_virtualAddress %px, cmdbuff_size %d\n",
                cmdbuf_obj->cmdbuf_virtualAddress, cmdbuf_obj->cmdbuf_size);
		for (i = 0; i < cmdbuf_obj->cmdbuf_size / 4; i++) {
			if (i == inst) {
				PrintInstr(i, *(cmdbuf_obj->cmdbuf_virtualAddress + i), &size);
				inst += size;
			} else {
				pr_info("current cmdbuf data %d = 0x%x\n", i, *(cmdbuf_obj->cmdbuf_virtualAddress + i));
			}
		}
		pr_info("end vcmd link");
	}
#endif
	//test nop and end opcode, then assign value.
	cmdbuf_obj->has_end_cmdbuf = 0; //0: has jmp opcode,1 has end code
	cmdbuf_obj->no_normal_int_cmdbuf = 0; //0: interrupt when JMP,1 not interrupt when JMP
	jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
	opCode = tempOpcode = *(jmp_addr - 4);
	opCode >>= 27;
	opCode <<= 27;
	//we can't identify END opcode or JMP opcode, so we don't support END opcode in control sw and driver.
	if (opCode == OPCODE_JMP) {
		//jmp
		opCode = tempOpcode;
		opCode &= 0x02000000;
		if (opCode == JMP_IE_1)
			cmdbuf_obj->no_normal_int_cmdbuf = 0;
		else
			cmdbuf_obj->no_normal_int_cmdbuf = 1;
	} else {
		//not support other opcode
		return -1;
	}

	if (down_interruptible(&vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]))
		return -ERESTARTSYS;

	return_value = select_vcmd(new_cmdbuf_node);
	if (return_value)
		return return_value;

	dev = &hantrovcmd_data[cmdbuf_obj->core_id];
	input_para->core_id = cmdbuf_obj->core_id;
	PDEBUG("Allocate cmd buffer [%d] to core [%d]\n", cmdbuf_id, input_para->core_id);
	//set ddr address for vcmd registers copy.
	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd executing register into ddr memory.
		//now core id is got and output ddr address of vcmd register can be filled in.
		//each core has its own fixed output ddr address of vcmd registers.
		jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress;
		if (mmu_enable) {
			*(jmp_addr + 2) = 0;
			*(jmp_addr + 1) =
				(u32)((dev->mmu_vcmd_reg_mem_busAddress + (EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
		} else {
			if (sizeof(size_t) == 8) {
				*(jmp_addr + 2) = (u32)(
					(u64)(dev->vcmd_reg_mem_busAddress + (EXECUTING_CMDBUF_ID_ADDR + 1) * 4) >> 32);
			} else {
				*(jmp_addr + 2) = 0;
			}
			*(jmp_addr + 1) =
				(u32)((dev->vcmd_reg_mem_busAddress + (EXECUTING_CMDBUF_ID_ADDR + 1) * 4));
		}

		jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
		//read vcmd all registers into ddr memory.
		//now core id is got and output ddr address of vcmd registers can be filled in.
		//each core has its own fixed output ddr address of vcmd registers.
		if (mmu_enable) {
			if (sizeof(size_t) == 8)
				*(jmp_addr - 6) = 0;
			*(jmp_addr - 7) = (u32)(dev->mmu_vcmd_reg_mem_busAddress);
		} else {
			if (sizeof(size_t) == 8) {
				*(jmp_addr - 6) = (u32)((u64)dev->vcmd_reg_mem_busAddress >> 32);
			} else {
				*(jmp_addr - 6) = 0;
			}
			*(jmp_addr - 7) = (u32)(dev->vcmd_reg_mem_busAddress);
		}
	}
	//start to link and/or run
	spin_lock_irqsave(dev->spinlock, flags);
	last_cmdbuf_node = find_last_linked_cmdbuf(new_cmdbuf_node);
	record_last_cmdbuf_rdy_num = dev->sw_cmdbuf_rdy_num;
	PDEBUG("dev->sw_cmdbuf_rdy_num = %d before vcmd_link_cmdbuf\n", dev->sw_cmdbuf_rdy_num);
	vcmd_link_cmdbuf(dev, last_cmdbuf_node);
	PDEBUG("dev->sw_cmdbuf_rdy_num = %d after vcmd_link_cmdbuf\n", dev->sw_cmdbuf_rdy_num);
	if (dev->working_state == WORKING_STATE_IDLE) {
		//run
		while (last_cmdbuf_node &&
		       ((struct cmdbuf_obj *)last_cmdbuf_node->data)->cmdbuf_run_done)
			last_cmdbuf_node = last_cmdbuf_node->next;

		if (last_cmdbuf_node && last_cmdbuf_node->data) {
			PDEBUG("vcmd start for cmdbuf id %d, cmdbuf_run_done = %d\n",
			       ((struct cmdbuf_obj *)last_cmdbuf_node->data)->cmdbuf_id,
			       ((struct cmdbuf_obj *)last_cmdbuf_node->data)->cmdbuf_run_done);
		}
		vcmd_start(dev, last_cmdbuf_node);
	} else {
		//just update cmdbuf ready number
		PDEBUG("dev state = %d, cmdbuf rdy num updated %d -> %d\n",
		       dev->working_state, record_last_cmdbuf_rdy_num,
		       dev->sw_cmdbuf_rdy_num);
		if (record_last_cmdbuf_rdy_num != dev->sw_cmdbuf_rdy_num)
			vcmd_write_register_value((const void *)dev->hwregs,
						  dev->reg_mirror,
						  HWIF_VCMD_RDY_CMDBUF_COUNT,
						  dev->sw_cmdbuf_rdy_num);
	}
	spin_unlock_irqrestore(dev->spinlock, flags);

	up(&vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);

	return 0;
}

/******************************************************************************/
static int check_cmdbuf_irq(struct hantrovcmd_dev *dev,
			    struct cmdbuf_obj *cmdbuf_obj, u32 *irq_status_ret)
{
	int rdy = 0;
	unsigned long flags;

	spin_lock_irqsave(dev->spinlock, flags);
	if (cmdbuf_obj->cmdbuf_run_done) {
		rdy = 1;
		*irq_status_ret =
			cmdbuf_obj->executing_status; //need to decide how to assign this variable
	}
	spin_unlock_irqrestore(dev->spinlock, flags);
	return rdy;
}

#ifdef IRQ_SIMULATION
static void get_random_bytes(void *buf, int nbytes);
#if 0
void hantrovcmd_trigger_irq_0(struct timer_list *timer)
{
	PDEBUG("trigger core 0 irq\n");
	del_timer(timer);
	hantrovcmd_isr(0, (void *)&hantrovcmd_data[0]);
}

void hantrovcmd_trigger_irq_1(struct timer_list *timer)
{
	PDEBUG("trigger core 1 irq\n");
	del_timer(timer);
	hantrovcmd_isr(0, (void *)&hantrovcmd_data[1]);
}
#else
static void hantrovcmd_trigger_irq(struct timer_list *timer)
{
	u32 timer_id = 0;
	u32 core_id = 0;
	u32 i;

	for (i = 0; i < 10000; i++) {
		if (timer_reserve[i].timer == timer) {
			timer_id = timer_reserve[i].timer_id;
			core_id = timer_reserve[i].core_id;
			break;
		}
	}
	PDEBUG("trigger core 0 irq\n");
	hantrovcmd_isr(core_id, (void *)&hantrovcmd_data[core_id]);
	del_timer(timer);
	timer_reserve[timer_id].timer = NULL;
}

#endif
#endif

static unsigned int wait_cmdbuf_ready(struct file *filp, u16 cmdbuf_id,
				      u32 *irq_status_ret)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	bi_list_node *new_cmdbuf_node = NULL;
	struct hantrovcmd_dev *dev = NULL;

	PDEBUG("%s\n", __func__);
	new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
	if (!new_cmdbuf_node) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
	if (cmdbuf_obj->filp != filp) {
		//should not happen
		pr_err("hantrovcmd: ERROR cmdbuf_id !!\n");
		return -1;
	}
	dev = &hantrovcmd_data[cmdbuf_obj->core_id];
#ifdef IRQ_SIMULATION
	{
		u32 random_num;
		//get_random_bytes(&random_num, sizeof(u32));
		random_num = (u32)((u64)100 * cmdbuf_obj->executing_time / (4096 * 2160) + 50);
		PDEBUG("random_num=%d\n", random_num);
#if 0
	/*init a timer to trigger irq*/
	if (cmdbuf_obj->core_id == 0) {
		//init_timer(&timer0);
		//timer0.function = hantrovcmd_trigger_irq_0;
		timer_setup(&timer0, hantrovcmd_trigger_irq_0, 0);
		timer0.expires =  jiffies + random_num * HZ / 10; //the expires time is 1s
		add_timer(&timer0);
	}

	if (cmdbuf_obj->core_id == 1) {
		//init_timer(&timer1);
		//timer1.function = hantrovcmd_trigger_irq_1;
		timer_setup(&timer1, hantrovcmd_trigger_irq_1, 0);
		timer1.expires =  jiffies + random_num * HZ / 10; //the expires time is 1s
		add_timer(&timer1);
	}
#else
		{
			u32 i;
			struct timer_list *temp_timer = NULL;

			for (i = 0; i < 10000; i++) {
				if (!timer_reserve[i].timer) {
					timer_reserve[i].timer_id = i;
					timer_reserve[i].core_id = cmdbuf_obj->core_id;
					temp_timer = timer_reserve[i].timer = &timer[i];
					break;
				}
			}
			//if (cmdbuf_obj->core_id==0)
			{
				//init_timer(&timer0);
				//timer0.function = hantrovcmd_trigger_irq_0;
				timer_setup(temp_timer, hantrovcmd_trigger_irq, 0);
				temp_timer->expires =
					jiffies + random_num * HZ / 10; //the expires time is 1s
				add_timer(temp_timer);
			}
		}
#endif
	}
#endif

	if (wait_event_interruptible(*dev->wait_queue, check_cmdbuf_irq(dev, cmdbuf_obj, irq_status_ret))) {
		PDEBUG("vcmd_wait_queue_0 interrupted\n");
		//abort the vcmd
		//vcmd_write_register_value((const void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_START_TRIGGER,0);
		return -ERESTARTSYS;
	}
	return 0;
}

static long hantrovcmd_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	int err = 0;

	/*
     * extract the type and number bitfields, and don't encode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
	if (_IOC_TYPE(cmd) != HANTRO_IOC_MAGIC
#ifdef HANTROMMU_SUPPORT
	    && _IOC_TYPE(cmd) != HANTRO_IOC_MMU
#endif
	)
		return -ENOTTY;
	if ((_IOC_TYPE(cmd) == HANTRO_IOC_MAGIC &&
	     _IOC_NR(cmd) > HANTRO_IOC_MAXNR)
#ifdef HANTROMMU_SUPPORT
	    || (_IOC_TYPE(cmd) == HANTRO_IOC_MMU &&
		_IOC_NR(cmd) > HANTRO_IOC_MMU_MAXNR)
#endif
	)
		return -ENOTTY;

	/*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
	if (_IOC_DIR(cmd) & _IOC_READ)
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	if (err)
		return -EFAULT;

	switch (cmd) {
	case HANTRO_IOCTL_CONFIG_DMA: {
		struct versdrv_dma_buf_info_t dma_info;
		enc_pr(LOG_INFO,
			"[+]HANTRO_IOCTL_CONFIG_DMA_BUF\n");

		if (copy_from_user(&dma_info,
			(struct versdrv_dma_buf_info_t *)arg,
			sizeof(struct versdrv_dma_buf_info_t)))
			return -EFAULT;
		ret = down_interruptible(&s_vers_sem);
		if (ret != 0)
			break;
		if (vers_src_addr_config(&dma_info, filp)) {
			up(&s_vers_sem);
			enc_pr(LOG_ERROR,
					"src addr config error\n");
			ret = -EFAULT;
			break;
		}
		up(&s_vers_sem);
		ret = copy_to_user((void __user *)arg,
			&dma_info,
			sizeof(struct versdrv_dma_buf_info_t));
		if (ret) {
			ret = -EFAULT;
			break;
		}
		enc_pr(LOG_INFO,
			"[-]HANTRO_IOCTL_CONFIG_DMA_BUF %d, %d, %d\n",
			dma_info.fd[0],
			dma_info.fd[1],
			dma_info.fd[2]);
	}
		break;

	case HANTRO_IOCTL_UNMAP_DMA: {
		struct versdrv_dma_buf_info_t dma_info;

		enc_pr(LOG_ALL,
			"[+]HANTRO_IOCTL_UNMAP_DMA\n");

		if (copy_from_user(&dma_info,
			(struct versdrv_dma_buf_info_t *)arg,
			sizeof(struct versdrv_dma_buf_info_t)))
			return -EFAULT;
		ret = down_interruptible(&s_vers_sem);
		if (ret != 0)
			break;
		if (vers_src_addr_unmap(&dma_info, filp)) {
			up(&s_vers_sem);
			enc_pr(LOG_ERROR,
				"dma addr unmap config error\n");
			ret = -EFAULT;
			break;
		}
		up(&s_vers_sem);
		enc_pr(LOG_ALL,
			"[-]HANTRO_IOCTL_UNMAP_DMA\n");
	}
		break;

	case HANTRO_IOCH_GET_VCMD_ENABLE: {
		__put_user(1, (unsigned long __user *)arg);
		break;
	}

	case HANTRO_IOCH_GET_MMU_ENABLE: {
		__put_user(mmu_enable, (unsigned int __user  *)arg);
		break;
	}

	case HANTRO_IOCH_GET_CMDBUF_PARAMETER: {
		struct cmdbuf_mem_parameter local_cmdbuf_mem_data;

		PDEBUG(" VCMD GET_CMDBUF_PARAMETER\n");
		local_cmdbuf_mem_data.cmd_unit_size = CMDBUF_MAX_SIZE;
		local_cmdbuf_mem_data.status_unit_size = CMDBUF_MAX_SIZE;
		local_cmdbuf_mem_data.cmd_total_size = CMDBUF_POOL_TOTAL_SIZE;
		local_cmdbuf_mem_data.status_total_size = CMDBUF_POOL_TOTAL_SIZE;
		local_cmdbuf_mem_data.status_phy_addr = vcmd_status_buf_mem_pool.busAddress;
		local_cmdbuf_mem_data.cmd_phy_addr = vcmd_buf_mem_pool.busAddress;
		if (mmu_enable) {
			local_cmdbuf_mem_data.status_hw_addr = vcmd_status_buf_mem_pool.mmu_bus_address;
			local_cmdbuf_mem_data.cmd_hw_addr = vcmd_buf_mem_pool.mmu_bus_address;
		} else {
			local_cmdbuf_mem_data.status_hw_addr = vcmd_status_buf_mem_pool.busAddress - base_ddr_addr;
			local_cmdbuf_mem_data.cmd_hw_addr = vcmd_buf_mem_pool.busAddress - base_ddr_addr;
		}
		local_cmdbuf_mem_data.base_ddr_addr = base_ddr_addr;
		ret = copy_to_user((struct cmdbuf_mem_parameter __user *)arg, &local_cmdbuf_mem_data,
			     sizeof(struct cmdbuf_mem_parameter));
		break;
	}
	case HANTRO_IOCH_GET_VCMD_PARAMETER: {
		struct config_parameter input_para;

		PDEBUG(" VCMD get vcmd config parameter\n");
		ret = copy_from_user(&input_para, (struct config_parameter __user *)arg,
			       sizeof(struct config_parameter));
		if (vcmd_type_core_num[input_para.module_type]) {
			input_para.submodule_main_addr =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_main_addr;
			input_para.submodule_dec400_addr =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_dec400_addr;
			input_para.submodule_L2Cache_addr =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_L2Cache_addr;
			input_para.submodule_MMU_addr[0] =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_MMU_addr[0];
			input_para.submodule_MMU_addr[1] =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_MMU_addr[1];
			input_para.submodule_axife_addr[0] =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_axife_addr[0];
			input_para.submodule_axife_addr[1] =
				vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_axife_addr[1];
			input_para.config_status_cmdbuf_id =
				vcmd_manager[input_para.module_type][0]->status_cmdbuf_id;
			input_para.vcmd_hw_version_id =
				vcmd_manager[input_para.module_type][0]->hw_version_id;
			input_para.vcmd_core_num = vcmd_type_core_num[input_para.module_type];
		} else {
			input_para.submodule_main_addr = 0xffff;
			input_para.submodule_dec400_addr = 0xffff;
			input_para.submodule_L2Cache_addr = 0xffff;
			input_para.submodule_MMU_addr[0] = 0xffff;
			input_para.submodule_MMU_addr[1] = 0xffff;
			input_para.submodule_axife_addr[0] = 0xffff;
			input_para.submodule_axife_addr[1] = 0xffff;
			input_para.config_status_cmdbuf_id = 0;
			input_para.vcmd_core_num = 0;
			input_para.vcmd_hw_version_id = HW_ID_1_0_C;
		}
		ret = copy_to_user((struct config_parameter __user *)arg, &input_para,
			     sizeof(struct config_parameter));
		break;
	}
	case HANTRO_IOCH_RESERVE_CMDBUF: {
		int ret;
		struct exchange_parameter input_para;

		ret = copy_from_user(&input_para, (struct exchange_parameter __user *)arg,
			       sizeof(struct exchange_parameter));
		ret = reserve_cmdbuf(filp, &input_para);
		if (ret == 0)
			ret = copy_to_user((struct exchange_parameter __user *)arg,
				     &input_para,
				     sizeof(struct exchange_parameter));
		PDEBUG(" VCMD Reserve CMDBUF %d\n", input_para.cmdbuf_id);
		return ret;
	}

	case HANTRO_IOCH_LINK_RUN_CMDBUF: {
		struct exchange_parameter input_para;
		long retVal;

		ret = copy_from_user(&input_para, (struct exchange_parameter __user *)arg,
			       sizeof(struct exchange_parameter));

		PDEBUG("VCMD link and run cmdbuf\n");
		retVal = link_and_run_cmdbuf(filp, &input_para);
		ret = copy_to_user((struct exchange_parameter __user *)arg, &input_para,
			     sizeof(struct exchange_parameter));
		return retVal;
		break;
	}

	case HANTRO_IOCH_WAIT_CMDBUF: {
		u16 cmdbuf_id;
		unsigned int tmp;
		u32 irq_status_ret = 0;

		__get_user(cmdbuf_id, (u16 __user *)arg);
		/*high 16 bits are core id, low 16 bits are cmdbuf_id*/

		PDEBUG("VCMD wait for CMDBUF finishing.\n");

		//TODO
		tmp = wait_cmdbuf_ready(filp, cmdbuf_id, &irq_status_ret);
		cmdbuf_id = (u16)irq_status_ret;
		if (tmp == 0) {
			__put_user(cmdbuf_id, (u16 __user *)arg);
			return tmp; //return core_id
		} else {
			return -1;
		}

		break;
	}
	case HANTRO_IOCH_RELEASE_CMDBUF: {
		u16 cmdbuf_id;

		__get_user(cmdbuf_id, (u16 __user *)arg);
		/*16 bits are cmdbuf_id*/

		PDEBUG("VCMD release CMDBUF\n");

		release_cmdbuf(filp, cmdbuf_id);
		return 0;
		break;
	}
	case HANTRO_IOCH_POLLING_CMDBUF: {
		u16 core_id;
		u32 i;

		__get_user(core_id, (u16 __user *)arg);

		/*16 bits are cmdbuf_id*/
		if ((core_id >= total_vcmd_core_num) && (core_id != 0xffff))
			return -1;
		if (core_id != 0xffff)
			hantrovcmd_isr(core_id, &hantrovcmd_data[core_id]);
		else
		{
			for (i=0; i < total_vcmd_core_num; i++)
			{
				hantrovcmd_isr(i, &hantrovcmd_data[i]);
			}
		}
		return 0;
		break;
	}
	default: {
#ifdef HANTROMMU_SUPPORT
		if (_IOC_TYPE(cmd) == HANTRO_IOC_MMU)
			return MMUIoctl(cmd, filp, arg, mmu_hwregs);
#endif
	}
	}
	return 0;
}

/******************************************************************************
 *process manager object management
 ******************************************************************************
 */
static struct process_manager_obj *create_process_manager_obj(void)
{
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_obj = vmalloc(sizeof(struct process_manager_obj));
	if (!process_manager_obj) {
		PDEBUG("%s\n", "vmalloc for process_manager_obj fail!");
		return process_manager_obj;
	}
	memset(process_manager_obj, 0, sizeof(struct process_manager_obj));
	return process_manager_obj;
}

static void free_process_manager_obj( struct process_manager_obj *process_manager_obj)
{
	if (!process_manager_obj) {
		PDEBUG("%s\n", "free_process_manager_obj NULL");
		return;
	}
	//free current cmdbuf_obj
	vfree(process_manager_obj);
}

static bi_list_node *create_process_manager_node(void)
{
	bi_list_node *current_node = NULL;
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_obj = create_process_manager_obj();
	if (!process_manager_obj) {
		PDEBUG("%s\n", "create_process_manager_obj fail!");
		return NULL;
	}
	process_manager_obj->total_exe_time = 0;
	spin_lock_init(&process_manager_obj->spinlock);
	init_waitqueue_head(&process_manager_obj->wait_queue);
	current_node = bi_list_create_node();
	if (!current_node) {
		PDEBUG("%s\n", "bi_list_create_node fail!");
		free_process_manager_obj(process_manager_obj);
		return NULL;
	}
	current_node->data = (void *)process_manager_obj;
	return current_node;
}

static void free_process_manager_node(bi_list_node *process_node)
{
	struct process_manager_obj *process_manager_obj = NULL;

	if (!process_node) {
		PDEBUG("%s\n", "free_process_manager_node NULL");
		return;
	}
	process_manager_obj = (struct process_manager_obj *)process_node->data;
	//free struct process_manager_obj
	free_process_manager_obj(process_manager_obj);
	//free current process_manager_obj entity.
	bi_list_free_node(process_node);
}

static long release_process_node_cleanup(bi_list *list)
{
	bi_list_node *new_process_node = NULL;

	while (1) {
		new_process_node = list->head;
		if (!new_process_node)
			break;
		//remove node from list
		bi_list_remove_node(list, new_process_node);
		//remove node from list
		free_process_manager_node(new_process_node);
	}
	return 0;
}

static void create_kernel_process_manager(void)
{
	bi_list_node *process_manager_node;
	struct process_manager_obj *process_manager_obj = NULL;

	process_manager_node = create_process_manager_node();
	process_manager_obj =
		(struct process_manager_obj *)process_manager_node->data;
	process_manager_obj->filp = NULL;
	bi_list_insert_node_tail(&global_process_manager, process_manager_node);
}

/* Update the last JMP cmd in cmdbuf_ojb in order to jump to next_cmdbuf_obj. */
static void cmdbuf_update_jmp_cmd(int hw_version_id,
				  struct cmdbuf_obj *cmdbuf_obj,
				  struct cmdbuf_obj *next_cmdbuf_obj,
				  int jmp_IE_1)
{
	u32 *jmp_addr;
	u32 operation_code;

	if (!cmdbuf_obj)
		return;

	if (cmdbuf_obj->has_end_cmdbuf == 0) {
		//need to link, current cmdbuf link to next cmdbuf
		jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress +
			   (cmdbuf_obj->cmdbuf_size / 4);
		if (!next_cmdbuf_obj) {
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) = (u32)(operation_code & ~JMP_RDY_1);
		} else {
			if (hw_version_id > HW_ID_1_0_C) {
				//set next cmdbuf id
				*(jmp_addr - 1) = next_cmdbuf_obj->cmdbuf_id;
			}
			if (mmu_enable) {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj->mmu_cmdbuf_busAddress) >> 32);
				} else {
					*(jmp_addr - 2) = 0;
				}
				*(jmp_addr - 3) = (u32)(next_cmdbuf_obj->mmu_cmdbuf_busAddress);
			} else {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) >> 32);
				} else {
					*(jmp_addr - 2) = 0;
				}
				*(jmp_addr - 3) = (u32)(
					next_cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr);
			}
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) =
				(u32)(operation_code | JMP_RDY_1 | jmp_IE_1 |
				      ((next_cmdbuf_obj->cmdbuf_size + 7) / 8));
		}

#ifdef VCMD_DEBUG_INTERNAL
		{
			u32 i;

			pr_info("vcmd link, last cmdbuf content\n");
			for (i = cmdbuf_obj->cmdbuf_size / 4 - 8;
			     i < cmdbuf_obj->cmdbuf_size / 4; i++) {
				pr_info("current linked cmdbuf data %d =0x%x\n",
					i, *(cmdbuf_obj->cmdbuf_virtualAddress + i));
			}
		}
#endif
	}
}

/* delink given cmd buffer (cmdbuf_node) and remove it from list.
 * Also modify the last JMP of buf P to point to cmdbuf N.
 * Used when a process is terminated but there are pending cmd bufs in vmcd list.
 * E.g.,
 * before:
 *
 *    L->L->...->P->X->N->        ...        ->L
 *    ^             ^                          ^
 *    head          cmdbuf_node                 tail
 *
 *  end:
 *
 *   L->L->...->P->N->        ...        ->L
 *    ^                                     ^
 *    head                                  tail
 *
 *  Return: pointer to N or NULL if N doesn't exist.
 */
static void vcmd_delink_rm_cmdbuf(struct hantrovcmd_dev *dev,
				  bi_list_node *cmdbuf_node)
{
	bi_list *list = &dev->list_manager;
	struct cmdbuf_obj *cmdbuf_obj = (struct cmdbuf_obj *)cmdbuf_node->data;
	bi_list_node *prev = cmdbuf_node->previous;
	bi_list_node *next = cmdbuf_node->next;

	PDEBUG("Delink and remove cmdbuf [%d] from vcmd list.\n",
	       cmdbuf_obj->cmdbuf_id);
	if (prev) {
		PDEBUG("prev cmdbuf [%d].\n",
		       ((struct cmdbuf_obj *)prev->data)->cmdbuf_id);
	} else {
		PDEBUG("NO prev cmdbuf.\n");
	}
	if (next) {
		PDEBUG("next cmdbuf [%d].\n",
		       ((struct cmdbuf_obj *)next->data)->cmdbuf_id);
	} else {
		PDEBUG("NO next cmdbuf.\n");
	}

	bi_list_remove_node(list, cmdbuf_node);
	global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
	free_cmdbuf_node(cmdbuf_node);

	cmdbuf_update_jmp_cmd(dev->hw_version_id, prev ? prev->data : NULL,
			      next ? next->data : NULL,
			      dev->duration_without_int >
				      INT_MIN_SUM_OF_IMAGE_SIZE);
}

static int hantrovcmd_open(struct inode *inode, struct file *filp)
{
	int result = 0;
	struct hantrovcmd_dev *dev = hantrovcmd_data;
	bi_list_node *process_manager_node;
	unsigned long flags;
	struct process_manager_obj *process_manager_obj = NULL;

	filp->private_data = (void *)dev;
	process_manager_node = create_process_manager_node();
	if (!process_manager_node)
		return -1;
	process_manager_obj =
		(struct process_manager_obj *)process_manager_node->data;
	process_manager_obj->filp = filp;
	spin_lock_irqsave(&vcmd_process_manager_lock, flags);
	bi_list_insert_node_tail(&global_process_manager, process_manager_node);
	spin_unlock_irqrestore(&vcmd_process_manager_lock, flags);

	PDEBUG("dev opened\n");
	return result;
}

static int hantrovcmd_release(struct inode *inode, struct file *filp)
{
	struct hantrovcmd_dev *dev =
		(struct hantrovcmd_dev *)filp->private_data;
	u32 core_id = 0;
	u32 release_cmdbuf_num = 0;
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj_temp = NULL;
	bi_list_node *process_manager_node;
	struct process_manager_obj *process_manager_obj = NULL;
	int vcmd_aborted = 0; // vcmd is aborted in this function
	struct cmdbuf_obj *restart_cmdbuf = NULL;

	unsigned long flags;
	long retVal = 0;

	PDEBUG("dev closed for process %p\n", (void *)filp);
	if (down_interruptible(
		    &vcmd_reserve_cmdbuf_sem[dev->vcmd_core_cfg.sub_module_type]))
		return -ERESTARTSYS;

	vers_dma_buf_release(filp);

	if (dev->hw_version_id >= HW_ID_1_2_1) {
		for (core_id = 0; core_id < total_vcmd_core_num; core_id++) {
			if (!(&dev[core_id]))
				continue;
			spin_lock_irqsave(dev[core_id].spinlock, flags);
			new_cmdbuf_node = dev[core_id].list_manager.head;
			while (1) {
				if (!new_cmdbuf_node)
					break;
				cmdbuf_obj_temp = (struct cmdbuf_obj *)new_cmdbuf_node->data;
				PDEBUG("Process %p is releasing: checking cmdbuf %d of process %p.\n",
				       filp, cmdbuf_obj_temp->cmdbuf_id, cmdbuf_obj_temp->filp);
				if (dev[core_id].hwregs &&
				    cmdbuf_obj_temp->filp == filp) {
					if (cmdbuf_obj_temp->cmdbuf_run_done) {
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(
							&dev[core_id].list_manager, new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 0) {
						cmdbuf_obj_temp->cmdbuf_data_linked = 1;
						cmdbuf_obj_temp->cmdbuf_run_done = 1;
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(
							&dev[core_id].list_manager, new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 &&
						   dev[core_id].working_state == WORKING_STATE_IDLE) {
						vcmd_delink_rm_cmdbuf(&dev[core_id], new_cmdbuf_node);
						if (restart_cmdbuf == cmdbuf_obj_temp)
							restart_cmdbuf =
								new_cmdbuf_node->next ? new_cmdbuf_node->next->data : NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 &&
						   dev[core_id].working_state == WORKING_STATE_WORKING) {
						bi_list_node *last_cmdbuf_node = NULL;
						bi_list_node *done_cmdbuf_node = NULL;
						int abort_cmdbuf_id;
						int loop_count = 0;

						//abort the vcmd and wait
						PDEBUG("Abort due to linked cmdbuf %d of current process.\n",
						       cmdbuf_obj_temp->cmdbuf_id);
#ifdef VCMD_DEBUG_INTERNAL
						printk_vcmd_register_debug(
							(const void *)dev[core_id].hwregs, "Before trigger to 0");
#endif
						// disable abort interrupt
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
						vcmd_aborted = 1;
						software_triger_abort = 1;
#ifdef VCMD_DEBUG_INTERNAL
						printk_vcmd_register_debug(
							(const void *)dev[core_id].hwregs,
							"After trigger to 0");
#endif
						// wait vcmd core aborted and vcmd enters IDLE mode.
						while (vcmd_get_register_value((const void *)dev[core_id].hwregs,
									       dev[core_id].reg_mirror, HWIF_VCMD_WORK_STATE)) {
							loop_count++;
							if (!(loop_count % 10)) {
								u32 irq_status = vcmd_read_reg(
									(const void *)dev[core_id].hwregs,
									VCMD_REGISTER_INT_STATUS_OFFSET);
								pr_err("hantrovcmd: expected idle state, but irq status = 0x%0x\n",
								       irq_status);
								pr_err("hantrovcmd: vcmd current status is %d\n",
								       vcmd_get_register_value((const void *)dev[core_id].hwregs,
											       dev[core_id].reg_mirror, HWIF_VCMD_WORK_STATE));
							}
							mdelay(10); // wait 10ms
							if (loop_count > 100) { // too long
								pr_err("hantrovcmd: too long before vcmd core to IDLE state\n");
								spin_unlock_irqrestore(
									dev[core_id].spinlock, flags);
								up(&vcmd_reserve_cmdbuf_sem[dev->vcmd_core_cfg.sub_module_type]);
								return -ERESTARTSYS;
							}
						}
						dev[core_id].working_state = WORKING_STATE_IDLE;
						// clear interrupt & restore abort_e
						if (vcmd_get_register_value((const void *)dev[core_id].hwregs,
									    dev[core_id].reg_mirror, HWIF_VCMD_IRQ_ABORT)) {
							PDEBUG("Abort interrupt triggered, now clear it to avoid abort int...\n");
							vcmd_write_reg((const void *)dev[core_id].hwregs,
								       VCMD_REGISTER_INT_STATUS_OFFSET, 0x1 << 4);
							PDEBUG("Now irq status = 0x%0x.\n",
							       vcmd_read_reg((const void *)dev[core_id].hwregs,
									     VCMD_REGISTER_INT_STATUS_OFFSET));
						}

						abort_cmdbuf_id = vcmd_get_register_value((const void *)dev[core_id].hwregs,
											  dev[core_id].reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID);
						PDEBUG("Abort when executing cmd buf %d.\n", abort_cmdbuf_id);
						dev[core_id].sw_cmdbuf_rdy_num = 0;
						dev[core_id].duration_without_int = 0;
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT, 0);
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT, 0);

						/* Mark cmdbuf_run_done to 1 for all the cmd buf executed. */
						done_cmdbuf_node =
							dev[core_id].list_manager.head;
						while (done_cmdbuf_node) {
							if (!((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_run_done) {
								((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_run_done = 1;
								((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_data_linked = 0;
								PDEBUG("Set cmdbuf [%d] cmdbuf_run_done to 1.\n",
								       ((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id);
							}
							if (((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id == abort_cmdbuf_id)
								break;
							done_cmdbuf_node = done_cmdbuf_node->next;
						}
						if (cmdbuf_obj_temp->cmdbuf_run_done) {
							/* current cmdbuf is in fact has been executed, but due to interrupt is not triggered,
							 * the status is not updated.
							 * Just delink and remove it from the list. */
							if (done_cmdbuf_node &&
							    done_cmdbuf_node->data) {
								PDEBUG("done_cmdbuf_node is cmdbuf [%d].\n",
								       ((struct cmdbuf_obj *)done_cmdbuf_node->data)->cmdbuf_id);
							}
							done_cmdbuf_node = done_cmdbuf_node->next;
							if (done_cmdbuf_node)
								restart_cmdbuf =
									(struct cmdbuf_obj *)done_cmdbuf_node->data;
							if (restart_cmdbuf)
								PDEBUG("Set restart cmdbuf [%d] via if.\n", restart_cmdbuf->cmdbuf_id);
						} else {
							last_cmdbuf_node = new_cmdbuf_node;
							/* cmd buf num from aborted cmd buf to current cmdbuf_obj_temp */
							if (cmdbuf_obj_temp->cmdbuf_id != abort_cmdbuf_id) {
								last_cmdbuf_node = new_cmdbuf_node->previous;

								while (last_cmdbuf_node &&
								       ((struct cmdbuf_obj *)last_cmdbuf_node->data)->cmdbuf_id != abort_cmdbuf_id) {
									restart_cmdbuf = (struct cmdbuf_obj *)last_cmdbuf_node->data;
									last_cmdbuf_node = last_cmdbuf_node->previous;
									dev[core_id].sw_cmdbuf_rdy_num++;
									dev[core_id].duration_without_int += restart_cmdbuf->executing_time;
									PDEBUG("Keep valid cmdbuf [%d] in the list.\n", restart_cmdbuf->cmdbuf_id);
								}
							}
							if (restart_cmdbuf) {
								PDEBUG("Set restart cmdbuf [%d] via else.\n", restart_cmdbuf->cmdbuf_id);
							}
						}

						// remove first linked cmdbuf from list
						vcmd_delink_rm_cmdbuf(&dev[core_id], new_cmdbuf_node);
					}
					software_triger_abort = 0;
					release_cmdbuf_num++;
					PDEBUG("release reserved cmdbuf\n");
				} else if (vcmd_aborted && !cmdbuf_obj_temp->cmdbuf_run_done) {
					/* VCMD is aborted, need to re-calculate the duration_without_int */
					if (!restart_cmdbuf)
						restart_cmdbuf = cmdbuf_obj_temp; /* first cmdbuf to be restarted */
					dev[core_id].duration_without_int += cmdbuf_obj_temp->executing_time;
					dev[core_id].sw_cmdbuf_rdy_num++;
				}

				new_cmdbuf_node = new_cmdbuf_node->next;
			}

			if (restart_cmdbuf && restart_cmdbuf->core_id == core_id) {
				u32 irq_status1, irq_status2;

				PDEBUG("Restart from cmdbuf [%d] after aborting.\n",
				       restart_cmdbuf->cmdbuf_id);

				irq_status1 = vcmd_read_reg((const void *)dev[core_id].hwregs,
							    VCMD_REGISTER_INT_STATUS_OFFSET);
				vcmd_write_reg((const void *)dev[core_id].hwregs,
					       VCMD_REGISTER_INT_STATUS_OFFSET, irq_status1);
				irq_status2 = vcmd_read_reg((const void *)dev[core_id].hwregs,
							    VCMD_REGISTER_INT_STATUS_OFFSET);
				PDEBUG("Clear irq status from 0x%0x -> 0x%0x\n",
				       irq_status1, irq_status2);
				if (mmu_enable) {
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR,
						(u32)(restart_cmdbuf->mmu_cmdbuf_busAddress));
					if (sizeof(size_t) == 8) {
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
							(u32)((u64)(restart_cmdbuf->mmu_cmdbuf_busAddress) >> 32));
					} else {
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
					}
				} else {
					vcmd_write_register_value((const void *)dev[core_id].hwregs,
								  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR,
						(u32)(restart_cmdbuf->cmdbuf_busAddress - base_ddr_addr));
					if (sizeof(size_t) == 8) {
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
							(u32)((u64)(restart_cmdbuf->cmdbuf_busAddress - base_ddr_addr) >> 32));
					} else {
						vcmd_write_register_value((const void *)dev[core_id].hwregs,
									  dev[core_id].reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
					}
				}
				vcmd_write_register_value(
					(const void *)dev[core_id].hwregs,
					dev[core_id].reg_mirror,
					HWIF_VCMD_EXE_CMDBUF_COUNT, 0);
				vcmd_write_register_value((const void *)dev[core_id].hwregs,
							  dev[core_id].reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH,
					(u32)((restart_cmdbuf->cmdbuf_size + 7) / 8));
				vcmd_write_register_value((const void *)dev[core_id].hwregs,
							  dev[core_id].reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID,
					restart_cmdbuf->cmdbuf_id);
				vcmd_write_register_value((const void *)dev[core_id].hwregs,
							  dev[core_id].reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT,
					dev->sw_cmdbuf_rdy_num);
#ifdef VCMD_DEBUG_INTERNAL
				printk_vcmd_register_debug(
					(const void *)dev[core_id].hwregs, "before restart");
#endif
				vcmd_write_register_value((const void *)dev[core_id].hwregs, dev[core_id].reg_mirror,
							  HWIF_VCMD_START_TRIGGER, 1);

				PDEBUG("Restart from cmdbuf [%d] dev register sw_cmdbuf_rdy_num is %d\n",
				       restart_cmdbuf->cmdbuf_id,
				       vcmd_get_register_value((const void *)dev[core_id].hwregs,
							       dev[core_id].reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT));

				PDEBUG("Restart from cmdbuf [%d] after aborting: start trigger = %d.\n",
				       restart_cmdbuf->cmdbuf_id,
				       vcmd_get_register_value((const void *)dev[core_id].hwregs,
							       dev[core_id].reg_mirror, HWIF_VCMD_START_TRIGGER));
				PDEBUG("dev state from %d -> WORKING.\n", dev[core_id].working_state);
				dev[core_id].working_state = WORKING_STATE_WORKING;
#ifdef VCMD_DEBUG_INTERNAL
				printk_vcmd_register_debug((const void *)dev[core_id].hwregs, "after restart");
#endif
			} else {
				PDEBUG("No more command buffer to be restarted!\n");
			}
			spin_unlock_irqrestore(dev[core_id].spinlock, flags);
			// VCMD aborted but not restarted, nedd to wake up
			if (vcmd_aborted && !restart_cmdbuf)
				wake_up_interruptible_all(dev[core_id].wait_queue);
		}
	} else {
		for (core_id = 0; core_id < total_vcmd_core_num; core_id++) {
			if ((&dev[core_id]) == NULL)
				continue;
			spin_lock_irqsave(dev[core_id].spinlock, flags);
			new_cmdbuf_node = dev[core_id].list_manager.head;
			while (1) {
				if (!new_cmdbuf_node)
					break;
				cmdbuf_obj_temp = (struct cmdbuf_obj *)new_cmdbuf_node->data;
				if (dev[core_id].hwregs && cmdbuf_obj_temp->filp == filp) {
					if (cmdbuf_obj_temp->cmdbuf_run_done) {
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(&dev[core_id].list_manager,
									     new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 0) {
						cmdbuf_obj_temp->cmdbuf_data_linked = 1;
						cmdbuf_obj_temp->cmdbuf_run_done = 1;
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(
							&dev[core_id].list_manager, new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 &&
						   dev[core_id].working_state == WORKING_STATE_IDLE) {
						cmdbuf_obj_temp->cmdbuf_run_done = 1;
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(
							&dev[core_id].list_manager, new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
					} else if (cmdbuf_obj_temp->cmdbuf_data_linked ==
							   1 && dev[core_id].working_state == WORKING_STATE_WORKING) {
						bi_list_node *last_cmdbuf_node;
						u32 record_last_cmdbuf_rdy_num;
						//abort the vcmd and wait
						//vcmd_write_register_value((const void *)dev[core_id].hwregs,dev[core_id].reg_mirror,HWIF_VCMD_START_TRIGGER,0);
						software_triger_abort = 1;
						if (wait_event_interruptible(*dev[core_id].wait_abort_queue, wait_abort_rdy(&dev[core_id]))) {
							spin_unlock_irqrestore(dev[core_id].spinlock, flags);
							up(&vcmd_reserve_cmdbuf_sem[dev->vcmd_core_cfg.sub_module_type]);
							software_triger_abort = 0;
							return -ERESTARTSYS;
						}
						software_triger_abort = 0;
						cmdbuf_obj_temp->cmdbuf_run_done = 1;
						cmdbuf_obj_temp->cmdbuf_need_remove = 1;
						retVal = release_cmdbuf_node(
							&dev[core_id].list_manager, new_cmdbuf_node);
						if (retVal == 1)
							cmdbuf_obj_temp->process_manager_obj = NULL;
						//link
						last_cmdbuf_node =
							find_last_linked_cmdbuf(dev[core_id].list_manager.tail);
						record_last_cmdbuf_rdy_num = dev->sw_cmdbuf_rdy_num;
						vcmd_link_cmdbuf(dev, last_cmdbuf_node);
						//re-run
						if (dev[core_id].sw_cmdbuf_rdy_num)
							vcmd_start(dev, last_cmdbuf_node);
					}
					release_cmdbuf_num++;
					PDEBUG("release reserved cmdbuf\n");
				}
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
			spin_unlock_irqrestore(dev[core_id].spinlock, flags);
		}
	}

	if (release_cmdbuf_num)
		wake_up_interruptible_all(&vcmd_cmdbuf_memory_wait);
	spin_lock_irqsave(&vcmd_process_manager_lock, flags);
	process_manager_node = global_process_manager.head;
	while (1) {
		if (!process_manager_node)
			break;
		process_manager_obj = (struct process_manager_obj *)
					      process_manager_node->data;
		if (process_manager_obj->filp == filp)
			break;
		process_manager_node = process_manager_node->next;
	}
	//remove node from list
	bi_list_remove_node(&global_process_manager, process_manager_node);
	spin_unlock_irqrestore(&vcmd_process_manager_lock, flags);
	free_process_manager_node(process_manager_node);
	up(&vcmd_reserve_cmdbuf_sem[dev->vcmd_core_cfg.sub_module_type]);
	return 0;
}

#ifdef CONFIG_COMPAT
static long venc_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = hantrovcmd_ioctl(filp, cmd, args);

	return ret;
}
#endif

/* VFS methods */
static const struct file_operations hantrovcmd_fops = {
	.owner = THIS_MODULE,
	.open = hantrovcmd_open,
	.release = hantrovcmd_release,
	.unlocked_ioctl = hantrovcmd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = venc_compat_ioctl,
#endif
	.fasync = NULL,
};

static u32 vcmd_release_AXIFE_IO(void)
{
#ifdef HANTROAXIFE_SUPPORT
	int i = 0, j = 0;

	for (i = 0; i < total_vcmd_core_num; i++) {
		for (j = 0; j < 2; j++) {
			if (axife_hwregs[i][j]) {
				iounmap(axife_hwregs[i][j]);
				release_mem_region(
					vcmd_core_array[i].vcmd_base_addr +
						vcmd_core_array[i].submodule_axife_addr[j], AXIFE_SIZE);
				axife_hwregs[i][j] = NULL;
			}
		}
	}
#endif
	return 0;
}

static u32 vcmd_release_MMU_IO(void)
{
#ifdef HANTROMMU_SUPPORT
	int i = 0, j = 0;

	for (i = 0; i < total_vcmd_core_num; i++) {
		for (j = 0; j < 2; j++) {
			if (mmu_hwregs[i][j]) {
				iounmap(mmu_hwregs[i][j]);
				release_mem_region(
					vcmd_core_array[i].vcmd_base_addr +
						vcmd_core_array[i].submodule_MMU_addr[j], MMU_SIZE);
				mmu_hwregs[i][j] = NULL;
			}
		}
	}
#endif
	return 0;
}

static u32 ConfigAXIFE(u32 mode)
{
#ifdef HANTROAXIFE_SUPPORT
	u32 i = 0;
	u8 sub_module_type = 0;

	for (i = 0; i < total_vcmd_core_num; i++) {
		sub_module_type = vcmd_core_array[i].sub_module_type;
		pr_info("%s: sub_module_type is %d\n", __func__,
			sub_module_type);
		if (vcmd_core_array[i].submodule_axife_addr[0] != 0xffff) {
			if (!request_mem_region(
				    vcmd_core_array[i].vcmd_base_addr +
					    vcmd_core_array[i].submodule_axife_addr[0], AXIFE_SIZE, "vc8000")) {
				pr_info("failed to request_mem_region for axife_hwregs[%d][0]\n", i);
				return -EBUSY;
			}
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
			axife_hwregs[i][0] = (volatile u8 *)ioremap_nocache(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_axife_addr[0], AXIFE_SIZE);
#else
			axife_hwregs[i][0] = (volatile u8 *)ioremap(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_axife_addr[0], AXIFE_SIZE);
#endif
			if (axife_hwregs[i][0]) {
				PDEBUG("%s: axife_hwregs[%d][0]=%p\n", __func__, i, axife_hwregs[i][0]);
				AXIFEEnable(axife_hwregs[i][0], mode);
			}
		}
		if (vcmd_core_array[i].submodule_axife_addr[1] != 0xffff) {
			if (!request_mem_region(
				    vcmd_core_array[i].vcmd_base_addr +
					    vcmd_core_array[i].submodule_axife_addr[1], AXIFE_SIZE, "vc8000")) {
				pr_info("failed to request_mem_region for axife_hwregs[%d][0]\n", i);
				return -EBUSY;
			}

#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
			axife_hwregs[i][1] = (volatile u8 *)ioremap_nocache(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_axife_addr[1], AXIFE_SIZE);
#else
			axife_hwregs[i][1] = (volatile u8 *)ioremap(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_axife_addr[1], AXIFE_SIZE);
#endif
			if (axife_hwregs[i][1]) {
				PDEBUG("%s: axife_hwregs[%d][1]=%p\n", __func__,
				       i axife_hwregs[i][1]);
				AXIFEEnable(axife_hwregs[i][1], mode);
			}
		}
	}
#endif
	return 0;
}

static u32 ConfigMMU(void)
{
#ifdef HANTROMMU_SUPPORT
	u32 i = 0;
	enum MMUStatus mmu_status = MMU_STATUS_FALSE;
	u8 sub_module_type = 0;

	/* MMU only initial once for the vcmd core no matter how many MMU we have */
	for (i = 0; i < total_vcmd_core_num; i++) {
		sub_module_type = vcmd_core_array[i].sub_module_type;
		pr_info("%s, MMUInit: sub_module_type is %d\n", __func__,
			sub_module_type);
		if (vcmd_core_array[i].submodule_MMU_addr[0] != 0xffff) {
			if (!request_mem_region(vcmd_core_array[i].vcmd_base_addr +
					    vcmd_core_array[i].submodule_MMU_addr[0], MMU_SIZE, "vc8000")) {
				pr_info("failed to request_mem_region for mmu_hwregs[%d][0]\n", i);
				return -EBUSY;
			}
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
			mmu_hwregs[i][0] = (volatile u8 *)ioremap_nocache(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_MMU_addr[0], MMU_SIZE);
#else
			mmu_hwregs[i][0] = (volatile u8 *)ioremap(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_MMU_addr[0], MMU_SIZE);
#endif
			if (mmu_hwregs[i][0]) {
				mmu_status = MMUInit(mmu_hwregs[i][0]);
				if (mmu_status == MMU_STATUS_NOT_FOUND) {
					pr_info("MMU does not exist!\n");
					return -1;
				} else if (mmu_status != MMU_STATUS_OK) {
					return -2;
				} else
					pr_info("MMU detected!\n");
				PDEBUG("mmu_hwregs[%d][0]=%p\n", i, mmu_hwregs[i][0]);
			}
		}

		if (vcmd_core_array[i].submodule_MMU_addr[1] != 0xffff) {
			if (!request_mem_region(
				    vcmd_core_array[i].vcmd_base_addr +
					    vcmd_core_array[i].submodule_MMU_addr[1],
				    MMU_SIZE, "vc8000")) {
				pr_info("failed to request_mem_region for mmu_hwregs[%d][0]\n", i);
				return -EBUSY;
			}
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
			mmu_hwregs[i][1] = (volatile u8 *)ioremap_nocache(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_MMU_addr[1], MMU_SIZE);
#else
			mmu_hwregs[i][1] = (volatile u8 *)ioremap(
				vcmd_core_array[i].vcmd_base_addr +
					vcmd_core_array[i].submodule_MMU_addr[1], MMU_SIZE);
#endif
			PDEBUG("mmu_hwregs[%d][1]=%p\n", i, mmu_hwregs[i][1]);
		}
	}
	mmu_status = MMUEnable(mmu_hwregs);
	PDEBUG("mmu_enable is %d\n", mmu_enable);
	if (mmu_status != MMU_STATUS_OK) {
		pr_info("MMUEnable: MMU enable failed\n");
		return -3;
	}
#endif
	return 0;
}

static u32 MMU_Kernel_map(void)
{
#ifdef HANTROMMU_SUPPORT
	struct kernel_addr_desc addr;

	if (mmu_enable) {
		addr.bus_address = vcmd_buf_mem_pool.busAddress - gBaseDDRHw;
		addr.size = vcmd_buf_mem_pool.size;
		if (MMUKernelMemNodeMap(&addr) != MMU_STATUS_OK)
			return -1;
		vcmd_buf_mem_pool.mmu_bus_address = addr.mmu_bus_address;
		pr_info("%s: vcmd_buf_mem_pool.mmu_bus_address=0x%llx.\n",
			__func__, (unsigned long long)vcmd_buf_mem_pool.mmu_bus_address);
	}
	if (mmu_enable) {
		addr.bus_address =
			vcmd_status_buf_mem_pool.busAddress - gBaseDDRHw;
		addr.size = vcmd_status_buf_mem_pool.size;
		if (MMUKernelMemNodeMap(&addr) != MMU_STATUS_OK)
			return -1;
		vcmd_status_buf_mem_pool.mmu_bus_address = addr.mmu_bus_address;
		pr_info("%s: vcmd_status_buf_mem_pool.mmu_bus_address=0x%llx.\n",
			__func__, (unsigned long long)vcmd_status_buf_mem_pool.mmu_bus_address);
	}
	if (mmu_enable) {
		addr.bus_address =
			vcmd_registers_mem_pool.busAddress - gBaseDDRHw;
		addr.size = vcmd_registers_mem_pool.size;
		if (MMUKernelMemNodeMap(&addr) != MMU_STATUS_OK)
			return -1;
		vcmd_registers_mem_pool.mmu_bus_address = addr.mmu_bus_address;
		pr_info("%s: vcmd_registers_mem_pool.mmu_bus_address=0x%llx.\n",
			__func__, (unsigned long long)vcmd_registers_mem_pool.mmu_bus_address);
	}
#endif
	return 0;
}

static u32 vcmd_pool_release(void)
{
	return 0;
}

static u32 MMU_Kernel_unmap(void)
{
#ifdef HANTROMMU_SUPPORT
	struct kernel_addr_desc addr;

	if (vcmd_buf_mem_pool.virtualAddress) {
		if (mmu_enable) {
			addr.bus_address =
				vcmd_buf_mem_pool.busAddress - gBaseDDRHw;
			addr.size = vcmd_buf_mem_pool.size;
			MMUKernelMemNodeUnmap(&addr);
		}
	}
	if (vcmd_status_buf_mem_pool.virtualAddress) {
		if (mmu_enable) {
			addr.bus_address = vcmd_status_buf_mem_pool.busAddress -
					   gBaseDDRHw;
			addr.size = vcmd_status_buf_mem_pool.size;
			MMUKernelMemNodeUnmap(&addr);
		}
	}
	if (vcmd_registers_mem_pool.virtualAddress) {
		if (mmu_enable) {
			addr.bus_address =
				vcmd_registers_mem_pool.busAddress - gBaseDDRHw;
			addr.size = vcmd_registers_mem_pool.size;
			MMUKernelMemNodeUnmap(&addr);
		}
	}
#endif

	return 0;
}


static struct attribute *venc_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(venc_class);
#define CLASS_NAME "venc"
#define DEVICE_NAME "vc8000"

static struct class venc_class = {
	.name = CLASS_NAME,
	.class_groups = venc_class_groups,
};
#ifdef PCIE_EN
/*------------------------------------------------------------------------------
 * Function name   : vcmd_pcie_init
 * Description     : Initialize PCI Hw access
 *
 * Return type     : int
 *------------------------------------------------------------------------------
 */
static void *vaddr = NULL;
static dma_addr_t paddr = 0;
static int alloc_size_byte = 0;
static int vcmd_pcie_init(struct platform_device *pf_dev)
{
	int i = 0;
	int ret = 0;

	versenc_pdev = pf_dev;
	alloc_size_byte = 1024 * 1024 * 10;
	pr_info("============== this is probe func !!!\n");
	ret = of_reserved_mem_device_init(&pf_dev->dev);
	if (ret)
	{
        pr_info("reserve memory init fail:%d\n", ret);
        return ret;
	}

	vaddr = dma_alloc_coherent(&pf_dev->dev, alloc_size_byte, &paddr, GFP_KERNEL);
	pr_info("------- vaddr: %lpx, paddr: %lx\n", vaddr, paddr);
	g_vcmd_base_hdwr = 0xfe380000;
	pr_info("Base hw val 0x%llx\n", (unsigned long long)g_vcmd_base_hdwr);

	g_vcmd_base_len = 0x2000;
	pr_info("Base hw len 0x%x\n", (unsigned int)g_vcmd_base_len);

	for (i = 0; i < total_vcmd_core_num; i++) {
		vcmd_core_array[i].vcmd_base_addr =
			g_vcmd_base_hdwr +
			vcmd_core_array[i].vcmd_base_addr; //the offset is based on which bus interface is chosen
	}

	vcmd_sram_base = g_vcmd_base_hdwr + 0x000000; //axi0 interface
	vcmd_sram_size = 0x2000;


	g_vcmd_base_ddr_hw = paddr;

	pr_info("sam debug >> pyh Base memory val 0x%llx\n",
		(unsigned long long)g_vcmd_base_ddr_hw);

    base_ddr_addr = 0;

	vcmd_buf_mem_pool.busAddress = g_vcmd_base_ddr_hw + START_MEM_OFFSET;

	vcmd_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
    pr_info("Init: vcmd_buf_mem_pool.busAddress=0x%llx.\n",
		(unsigned long long)vcmd_buf_mem_pool.busAddress);

	vcmd_buf_mem_pool.virtualAddress = vaddr;
	pr_info("Init: vcmd_buf_mem_pool.virtualAddress=0x%lx.\n",
		(unsigned long)vcmd_buf_mem_pool.virtualAddress);

	if (!vcmd_buf_mem_pool.virtualAddress) {
		pr_info("Init: failed to ioremap.\n");
		return -1;
	}

	//sam test
	*(vcmd_buf_mem_pool.virtualAddress+1) = 0x10;

	pr_info("sam debug >> virtualAddress1 = 0x%lx.\n",
			*(vcmd_buf_mem_pool.virtualAddress+1));
	//end

    vcmd_status_buf_mem_pool.busAddress = g_vcmd_base_ddr_hw + START_MEM_OFFSET + CMDBUF_POOL_TOTAL_SIZE;
	vcmd_status_buf_mem_pool.size = CMDBUF_POOL_TOTAL_SIZE;
    pr_info("Init: vcmd_status_buf_mem_pool.busAddress=0x%llx.\n",
		(unsigned long long)vcmd_status_buf_mem_pool.busAddress);

    vcmd_status_buf_mem_pool.virtualAddress = vaddr + START_MEM_OFFSET + CMDBUF_POOL_TOTAL_SIZE;
    pr_info("Init: vcmd_status_buf_mem_pool.virtualAddress=0x%lx.\n",
		(unsigned long)vcmd_status_buf_mem_pool.virtualAddress);

    if (!vcmd_status_buf_mem_pool.virtualAddress) {
        pr_info("Init: failed to ioremap.\n");
        return -1;
    }

	//sam test
	*(vcmd_status_buf_mem_pool.virtualAddress+1) = 0x11;

	pr_info("sam debug >> virtualAddress1 = 0x%lx.\n",
			*(vcmd_status_buf_mem_pool.virtualAddress+1));
	//end

    vcmd_registers_mem_pool.busAddress = g_vcmd_base_ddr_hw +
					     START_MEM_OFFSET +
					     CMDBUF_POOL_TOTAL_SIZE * 2;
	vcmd_registers_mem_pool.size = CMDBUF_VCMD_REGISTER_TOTAL_SIZE;
    pr_info("Init: vcmd_registers_mem_pool.busAddress=0x%llx.\n",
		(unsigned long long)vcmd_registers_mem_pool.busAddress);

	vcmd_registers_mem_pool.virtualAddress = vaddr + START_MEM_OFFSET + CMDBUF_POOL_TOTAL_SIZE * 2;
	pr_info("Init: vcmd_registers_mem_pool.virtualAddress=0x%lx.\n",
		(unsigned long)vcmd_registers_mem_pool.virtualAddress);

	if (!vcmd_registers_mem_pool.virtualAddress) {
		pr_info("Init: failed to ioremap.\n");
		return -1;
	}

	//sam test
    *(vcmd_registers_mem_pool.virtualAddress+1) = 0x22;
    pr_info("sam debug >> virtualAddress1 = 0x%llx.\n",
		*(vcmd_registers_mem_pool.virtualAddress+1));
	//end
	return 0;
}
#endif

static void vcmd_link_cmdbuf(struct hantrovcmd_dev *dev,
			     bi_list_node *last_linked_cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *next_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	struct cmdbuf_obj *next_cmdbuf_obj = NULL;
	u32 *jmp_addr = NULL;
	u32 operation_code;

	new_cmdbuf_node = last_linked_cmdbuf_node;
	//for the first cmdbuf.
	if (new_cmdbuf_node) {
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		PDEBUG("%s %d ", __func__, cmdbuf_obj->cmdbuf_id);
		if (cmdbuf_obj->cmdbuf_data_linked == 0) {
			dev->sw_cmdbuf_rdy_num++;
			cmdbuf_obj->cmdbuf_data_linked = 1;
			dev->duration_without_int = 0;
			if (cmdbuf_obj->has_end_cmdbuf == 0) {
				if (cmdbuf_obj->no_normal_int_cmdbuf == 1) {
					dev->duration_without_int = cmdbuf_obj->executing_time;
					//maybe nop is modified, so write back.
					if (dev->duration_without_int >= INT_MIN_SUM_OF_IMAGE_SIZE) {
						jmp_addr =
							cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
						operation_code = *(jmp_addr - 4);
						operation_code = JMP_IE_1 | operation_code;
						*(jmp_addr - 4) = operation_code;
						dev->duration_without_int = 0;
					}
				}
			}
		}
	}
	while (1) {
		if (!new_cmdbuf_node)
			break;
		if (!new_cmdbuf_node->next)
			break;
		next_cmdbuf_node = new_cmdbuf_node->next;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		next_cmdbuf_obj = (struct cmdbuf_obj *)next_cmdbuf_node->data;
		if (cmdbuf_obj->has_end_cmdbuf == 0 &&
		    !next_cmdbuf_obj->cmdbuf_run_done) {
			//need to link, current cmdbuf link to next cmdbuf
			PDEBUG("Link cmdbuf %d to cmdbuf %d",
			       cmdbuf_obj->cmdbuf_id, next_cmdbuf_obj->cmdbuf_id);
			jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
			if (dev->hw_version_id > HW_ID_1_0_C) {
				//set next cmdbuf id
				*(jmp_addr - 1) = next_cmdbuf_obj->cmdbuf_id;
			}
			if (mmu_enable) {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj->mmu_cmdbuf_busAddress) >> 32);
				} else {
					*(jmp_addr - 2) = 0;
				}
				*(jmp_addr - 3) = (u32)(
					next_cmdbuf_obj->mmu_cmdbuf_busAddress);
				PDEBUG(KERN_INFO
				       "%s: next_cmdbuf_obj->mmu_cmdbuf_busAddress=0x%08x\n",
				       __func__, next_cmdbuf_obj->mmu_cmdbuf_busAddress);
			} else {
				if (sizeof(size_t) == 8) {
					*(jmp_addr - 2) = (u32)(
						(u64)(next_cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) >> 32);
				} else {
					*(jmp_addr - 2) = 0;
				}
				*(jmp_addr - 3) = (u32)(
					next_cmdbuf_obj->cmdbuf_busAddress -
					base_ddr_addr);
			}
			operation_code = *(jmp_addr - 4);
			operation_code >>= 16;
			operation_code <<= 16;
			*(jmp_addr - 4) =
				(u32)(operation_code | JMP_RDY_1 | ((next_cmdbuf_obj->cmdbuf_size + 7) / 8));
			next_cmdbuf_obj->cmdbuf_data_linked = 1;
			dev->sw_cmdbuf_rdy_num++;
			//modify nop code of next cmdbuf
			if (next_cmdbuf_obj->has_end_cmdbuf == 0) {
				if (next_cmdbuf_obj->no_normal_int_cmdbuf == 1) {
					dev->duration_without_int += next_cmdbuf_obj->executing_time;

					//maybe we see the modified nop before abort, so need to write back.
					if (dev->duration_without_int >=
					    INT_MIN_SUM_OF_IMAGE_SIZE) {
						jmp_addr =
							next_cmdbuf_obj->cmdbuf_virtualAddress + (next_cmdbuf_obj->cmdbuf_size / 4);
						operation_code = *(jmp_addr - 4);
						operation_code = JMP_IE_1 |
								 operation_code;
						*(jmp_addr - 4) = operation_code;
						dev->duration_without_int = 0;
					}
				}
			} else {
				dev->duration_without_int = 0;
			}
#ifdef VCMD_DEBUG_INTERNAL
			{
				u32 i;

				pr_info("vcmd link, last cmdbuf content\n");
				for (i = cmdbuf_obj->cmdbuf_size / 4 - 8;
				     i < cmdbuf_obj->cmdbuf_size / 4; i++) {
					pr_info("current linked cmdbuf data %d =0x%x\n",
						i, *(cmdbuf_obj->cmdbuf_virtualAddress + i));
				}
			}
#endif
		}
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
}

/* delink all the cmd buffers from the cmdbuf in front of last_linked_cmdbuf_node
 *  to head of the list. All the cmd bufs marked as X will be delinked.
 *  E.g.,
 *  X->X->...->X->L->L->        ...        ->L
 *  ^             ^                          ^
 *  head          last_linked_cmdbuf_node    tail
 */

static void vcmd_delink_cmdbuf(struct hantrovcmd_dev *dev,
			       bi_list_node *last_linked_cmdbuf_node)
{
	bi_list_node *new_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	new_cmdbuf_node = last_linked_cmdbuf_node;
	while (1) {
		if (!new_cmdbuf_node)
			break;
		cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
		if (cmdbuf_obj->cmdbuf_data_linked)
			cmdbuf_obj->cmdbuf_data_linked = 0;
		else
			break;
		new_cmdbuf_node = new_cmdbuf_node->next;
	}
	dev->sw_cmdbuf_rdy_num = 0;
}

static void ConfigAIIXFE_MMU_BYVCMD(struct hantrovcmd_dev **device)
{
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 i = 0;
	u64 address = 0;
	u32 mirror_index, register_index, register_value;
	u32 write_command = 0;

	if (!device)
		return;
	struct hantrovcmd_dev *dev = *device;

	mirror_index = VCMD_REGISTER_INDEX_SW_INIT_CMD0;
	write_command = OPCODE_WREG | (1 << 26) | (1 << 16);
#ifdef HANTROAXIFE_SUPPORT
	//enable AXIFE by VCMD
	for (i = 0; i < 2; i++) {
		if (dev->vcmd_core_cfg.submodule_axife_addr[i] != 0xffff) {
			register_index = AXI_REG10_SW_FRONTEND_EN;
			register_value = 0x02;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_axife_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = AXI_REG11_SW_WORK_MODE;
			register_value = 0x00;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_axife_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;
		}
	}
#endif
#ifdef HANTROMMU_SUPPORT
	//enable MMU by VCMD
	address = GetMMUAddress();
	pr_info("%s: address = 0x%llx", __func__, address);
	for (i = 0; i < 2; i++) {
		if (dev->vcmd_core_cfg.submodule_MMU_addr[i] != 0xffff) {
			register_index = MMU_REG_ADDRESS;
			register_value = address;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_MMU_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_PAGE_TABLE_ID;
			register_value = 0x10000;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_MMU_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_PAGE_TABLE_ID;
			register_value = 0x00000;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_MMU_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;

			register_index = MMU_REG_CONTROL;
			register_value = 1;
			dev->reg_mirror[mirror_index++] =
				write_command | (dev->vcmd_core_cfg.submodule_MMU_addr[i] + register_index);
			dev->reg_mirror[mirror_index++] = register_value;
		}
	}
#endif
	//END command
	dev->reg_mirror[mirror_index++] = OPCODE_END;
	dev->reg_mirror[mirror_index] = 0x00;

	for (i = 0; i < mirror_index - VCMD_REGISTER_INDEX_SW_INIT_CMD0; i++) {
		register_index = (i + VCMD_REGISTER_INDEX_SW_INIT_CMD0) * 4;
		vcmd_write_reg((const void *)dev->hwregs, register_index,
			       dev->reg_mirror[i + VCMD_REGISTER_INDEX_SW_INIT_CMD0]);
	}
#endif
}

static void vcmd_start(struct hantrovcmd_dev *dev,
		       bi_list_node *first_linked_cmdbuf_node)
{
	struct cmdbuf_obj *cmdbuf_obj = NULL;

	if (dev->working_state == WORKING_STATE_IDLE) {
		if ((first_linked_cmdbuf_node) && dev->sw_cmdbuf_rdy_num) {
			cmdbuf_obj = (struct cmdbuf_obj *)
					     first_linked_cmdbuf_node->data;
#ifdef VCMD_DEBUG_INTERNAL
			printk_vcmd_register_debug((const void *)dev->hwregs,
						   "vcmd_start enters");
#endif
			//0x40
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
			if (dev->hw_version_id >= HW_ID_1_2_1)
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_INIT_MODE,
					1); //when start vcmd, first vcmd is init mode
#endif
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE, 0);
			//this bit should be set 1 only when need to reset dec400
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_CORE_CLK_GATE_DISABLE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_CORE, 0);
			vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_ALL, 0);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
			//0x48
			if (dev->hw_version_id <= HW_ID_1_0_C)
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_INTCMD_EN, 0xffff);
			else {
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_JMPP_EN, 1);
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_JMPD_EN, 1);
			}

			if (dev->hw_version_id < HW_ID_1_1_1)
				vcmd_set_register_mirror_value(
					dev->reg_mirror, HWIF_VCMD_IRQ_RESET_EN, 1);

			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
			//0x4c
			vcmd_set_register_mirror_value(dev->reg_mirror,
						       HWIF_VCMD_TIMEOUT_EN, 1);
			vcmd_set_register_mirror_value(dev->reg_mirror,
						       HWIF_VCMD_TIMEOUT_CYCLES, 0x1dcd6500);
			if (mmu_enable) {
				vcmd_set_register_mirror_value(
					dev->reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR,
					(u32)(cmdbuf_obj->mmu_cmdbuf_busAddress));
				pr_info("cmdbuf_obj->mmu_cmdbuf_busAddress=0x%08x\n",
					(u32)cmdbuf_obj->mmu_cmdbuf_busAddress);
				if (sizeof(size_t) == 8) {
					vcmd_set_register_mirror_value(
						dev->reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						(u32)((u64)(cmdbuf_obj->mmu_cmdbuf_busAddress) >> 32));
				} else {
					vcmd_set_register_mirror_value(dev->reg_mirror,
								       HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
				}
			} else {
				vcmd_set_register_mirror_value(
					dev->reg_mirror,
					HWIF_VCMD_EXECUTING_CMD_ADDR,
					(u32)(cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr));
				if (sizeof(size_t) == 8) {
					vcmd_set_register_mirror_value(
						dev->reg_mirror,
						HWIF_VCMD_EXECUTING_CMD_ADDR_MSB,
						(u32)((u64)(cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) >> 32));
				} else {
					vcmd_set_register_mirror_value(dev->reg_mirror,
								       HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
				}
			}
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH,
				(u32)((cmdbuf_obj->cmdbuf_size + 7) / 8));
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT,
				dev->sw_cmdbuf_rdy_num);
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);
			if (dev->hw_version_id > HW_ID_1_0_C) {
				vcmd_write_register_value((const void *)dev->hwregs,
							  dev->reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID,
					(u32)cmdbuf_obj->cmdbuf_id);
			}

			vcmd_write_reg((const void *)dev->hwregs, 0x44,
				       vcmd_read_reg((const void *)dev->hwregs, 0x44));
			vcmd_write_reg((const void *)dev->hwregs, 0x40,
				       dev->reg_mirror[0x40 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x48,
				       dev->reg_mirror[0x48 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x4c,
				       dev->reg_mirror[0x4c / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x50,
				       dev->reg_mirror[0x50 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x54,
				       dev->reg_mirror[0x54 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x58,
				       dev->reg_mirror[0x58 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x5c,
				       dev->reg_mirror[0x5c / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x60,
				       dev->reg_mirror[0x60 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x64, 0xffffffff); //not interrupt cpu

			dev->working_state = WORKING_STATE_WORKING;

			if (dev->hw_version_id >= HW_ID_1_2_1)
				ConfigAIIXFE_MMU_BYVCMD(&dev);

			//start
			vcmd_set_register_mirror_value(dev->reg_mirror,
						       HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1); //this bit should be set 1 only when need to reset dec400
			vcmd_set_register_mirror_value(
				dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
			PDEBUG("To write vcmd register 16:0x%x\n",
			       dev->reg_mirror[0x40 / 4]);
			vcmd_write_reg((const void *)dev->hwregs, 0x40,
				       dev->reg_mirror[0x40 / 4]);
#ifdef VCMD_DEBUG_INTERNAL
      printk_vcmd_register_debug((const void *)dev->hwregs, "vcmd_start exits ");
#endif
    }
  }

}

static void create_read_all_registers_cmdbuf(struct exchange_parameter *input_para)
{
	u32 register_range[] = { VCMD_ENCODER_REGISTER_SIZE,
				 VCMD_IM_REGISTER_SIZE,
				 VCMD_DECODER_REGISTER_SIZE,
				 VCMD_JPEG_ENCODER_REGISTER_SIZE,
				 VCMD_JPEG_DECODER_REGISTER_SIZE };
	u32 counter_cmdbuf_size = 0;
	u32 *set_base_addr = vcmd_buf_mem_pool.virtualAddress +
			     input_para->cmdbuf_id * CMDBUF_MAX_SIZE / 4;
	//u32 *status_base_virt_addr=vcmd_status_buf_mem_pool.virtualAddress + input_para->cmdbuf_id*CMDBUF_MAX_SIZE/4+(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr/2/4+0);
	ptr_t status_base_phy_addr =
		vcmd_status_buf_mem_pool.busAddress +
		input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
		(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 + 0);
	u32 map_status_base_phy_addr =
		vcmd_status_buf_mem_pool.mmu_bus_address + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
		(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 + 0);
	u32 offset_inc = 0;
	u32 offset_inc_dec400 = 0;

	if (vcmd_manager[input_para->module_type][0]->hw_version_id >
	    HW_ID_1_0_C) {
		pr_info("vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",
			vcmd_manager[input_para->module_type][0]->hw_version_id);

		//read vcmd executing cmdbuf id registers to ddr for balancing core load.
		*(set_base_addr + 0) = (OPCODE_RREG) | (1 << 16) | (EXECUTING_CMDBUF_ID_ADDR * 4);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 1) = (u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		*(set_base_addr + 2) = (u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 3) = 0;
		counter_cmdbuf_size += 4;

		//read main IP all registers
		*(set_base_addr + 4) =
			(OPCODE_RREG) | ((register_range[input_para->module_type] / 4) << 16) |
			(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr + 0);
		counter_cmdbuf_size += 4;
		if (mmu_enable) {
			*(set_base_addr + 5) = map_status_base_phy_addr;
		} else {
			*(set_base_addr + 5) = (u32)(status_base_phy_addr - base_ddr_addr);
		}
		counter_cmdbuf_size += 4;
		if (mmu_enable) {
			*(set_base_addr + 6) = 0;
		} else {
			if (sizeof(size_t) == 8)
				*(set_base_addr + 6) = (u32)((u64)(status_base_phy_addr - base_ddr_addr) >> 32);
			else
				*(set_base_addr + 6) = 0;
		}
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 7) = 0;
		counter_cmdbuf_size += 4;

		if (vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_L2Cache_addr != 0xffff) {
			//read L2 cache register
			offset_inc = 4;
			status_base_phy_addr =
				vcmd_status_buf_mem_pool.busAddress +
				input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
				(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_L2Cache_addr / 2 + 0);
			map_status_base_phy_addr =
				vcmd_status_buf_mem_pool.mmu_bus_address + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
				(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_L2Cache_addr / 2 + 0);
			//read L2cache IP first register
			*(set_base_addr +
			  8) = (OPCODE_RREG) | (1 << 16) |
			       (vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_L2Cache_addr + 0);
			counter_cmdbuf_size += 4;
			if (mmu_enable)
				*(set_base_addr + 9) = map_status_base_phy_addr;
			else
				*(set_base_addr + 9) = (u32)(status_base_phy_addr - base_ddr_addr);
			counter_cmdbuf_size += 4;
			if (mmu_enable) {
				*(set_base_addr + 10) = 0;
			} else {
				if (sizeof(size_t) == 8) {
					*(set_base_addr + 10) = (u32)(
						(u64)(status_base_phy_addr - base_ddr_addr) >> 32);
				} else {
					*(set_base_addr + 10) = 0;
				}
			}
			counter_cmdbuf_size += 4;
			//alignment
			*(set_base_addr + 11) = 0;
			counter_cmdbuf_size += 4;
		}
		if (vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_dec400_addr != 0xffff) {
			//read dec400 register
			offset_inc_dec400 = 4;
			status_base_phy_addr =
				vcmd_status_buf_mem_pool.busAddress +
				input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
				(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_dec400_addr / 2 + 0);
			map_status_base_phy_addr =
				vcmd_status_buf_mem_pool.mmu_bus_address + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
				(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_dec400_addr / 2 + 0);
			//read DEC400 IP first register
			*(set_base_addr + 8 + offset_inc) =
				(OPCODE_RREG) | (0x2b << 16) |
				(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_dec400_addr + 0);
			counter_cmdbuf_size += 4;
			if (mmu_enable)
				*(set_base_addr + 9 + offset_inc) = map_status_base_phy_addr;
			else
				*(set_base_addr + 9 + offset_inc) = (u32)(status_base_phy_addr - base_ddr_addr);
			counter_cmdbuf_size += 4;
			if (mmu_enable) {
				*(set_base_addr + 10 + offset_inc) = 0;
			} else {
				if (sizeof(size_t) == 8) {
					*(set_base_addr + 10 + offset_inc) =
						(u32)((u64)(status_base_phy_addr - base_ddr_addr) >> 32);
				} else {
					*(set_base_addr + 10 + offset_inc) = 0;
				}
			}
			counter_cmdbuf_size += 4;
			//alignment
			*(set_base_addr + 11 + offset_inc) = 0;
			counter_cmdbuf_size += 4;
		}

#if 0
	//INT code, interrupt immediately
	*(set_base_addr + 4) = (OPCODE_INT) | 0 | input_para->cmdbuf_id;
	counter_cmdbuf_size += 4;
	//alignment
	*(set_base_addr + 5) = 0;
	counter_cmdbuf_size += 4;
#endif
		//read vcmd registers to ddr
		*(set_base_addr + 8 + offset_inc + offset_inc_dec400) =
			(OPCODE_RREG) | (27 << 16) | (0);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 9 + offset_inc + offset_inc_dec400) =
			(u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		*(set_base_addr + 10 + offset_inc + offset_inc_dec400) =
			(u32)0; //will be changed in link stage
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 11 + offset_inc + offset_inc_dec400) = 0;
		counter_cmdbuf_size += 4;
		//JMP RDY = 0
		*(set_base_addr + 12 + offset_inc + offset_inc_dec400) =
            (u32)((OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 13 + offset_inc + offset_inc_dec400) = 0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 14 + offset_inc + offset_inc_dec400) = 0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 15 + offset_inc + offset_inc_dec400) =
			input_para->cmdbuf_id;
		//don't add the last alignment DWORD in order to  identify END command or JMP command.
		//counter_cmdbuf_size += 4;
		input_para->cmdbuf_size =
			(16 + offset_inc + offset_inc_dec400) * 4;
	} else {
		pr_info("vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",
			vcmd_manager[input_para->module_type][0]->hw_version_id);
		//read all registers
		*(set_base_addr + 0) =
			(OPCODE_RREG) |
			((register_range[input_para->module_type] / 4) << 16) |
			(vcmd_manager[input_para->module_type][0]->vcmd_core_cfg.submodule_main_addr + 0);
		counter_cmdbuf_size += 4;
		if (mmu_enable)
			*(set_base_addr + 1) = map_status_base_phy_addr;
		else
			*(set_base_addr + 1) = (u32)(status_base_phy_addr - base_ddr_addr);
		counter_cmdbuf_size += 4;
		if (mmu_enable) {
			*(set_base_addr + 2) = 0;
		} else {
			if (sizeof(size_t) == 8) {
				*(set_base_addr + 2) =
					(u32)((u64)(status_base_phy_addr - base_ddr_addr) >> 32);
			} else {
				*(set_base_addr + 2) = 0;
			}
		}
		counter_cmdbuf_size += 4;
		//alignment
		*(set_base_addr + 3) = 0;
		counter_cmdbuf_size += 4;
#if 0
	//INT code, interrupt immediately
	*(set_base_addr + 4) = (OPCODE_INT) | 0 | input_para->cmdbuf_id;
	counter_cmdbuf_size += 4;
	//alignment
	*(set_base_addr + 5) = 0;
	counter_cmdbuf_size += 4;
#endif
		//JMP RDY = 0
        *(set_base_addr + 4) = (u32)((OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0);
		counter_cmdbuf_size += 4;
		*(set_base_addr + 5) = 0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 6) = 0;
		counter_cmdbuf_size += 4;
		*(set_base_addr + 7) = input_para->cmdbuf_id;
		//don't add the last alignment DWORD in order to  identify END command or JMP command.
		//counter_cmdbuf_size += 4;
		input_para->cmdbuf_size = 8 * 4;
	}
}

static void read_main_module_all_registers(u32 main_module_type)
{
	int ret;
	struct exchange_parameter input_para;
	u32 irq_status_ret = 0;
	u32 *status_base_virt_addr;

	input_para.executing_time = 0;
	input_para.priority = CMDBUF_PRIORITY_NORMAL;
	input_para.module_type = main_module_type;
	input_para.cmdbuf_size = 0;
	ret = reserve_cmdbuf(NULL, &input_para);
	vcmd_manager[main_module_type][0]->status_cmdbuf_id =
		input_para.cmdbuf_id;
	create_read_all_registers_cmdbuf(&input_para);
	link_and_run_cmdbuf(NULL, &input_para);
	msleep(5);
	hantrovcmd_isr(input_para.core_id,
		       &hantrovcmd_data[input_para.core_id]);
	wait_cmdbuf_ready(NULL, input_para.cmdbuf_id, &irq_status_ret);
	status_base_virt_addr =
		vcmd_status_buf_mem_pool.virtualAddress + input_para.cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
		(vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
	pr_info("vc8000_vcmd_driver: main module register 0:0x%x\n", *status_base_virt_addr);
	pr_info("vc8000_vcmd_driver: main module register 80:0x%x\n", *(status_base_virt_addr + 80));
	pr_info("vc8000_vcmd_driver: main module register 214:0x%x\n", *(status_base_virt_addr + 214));
	pr_info("vc8000_vcmd_driver: main module register 226:0x%x\n", *(status_base_virt_addr + 226));
	pr_info("vc8000_vcmd_driver: main module register 287:0x%x\n", *(status_base_virt_addr + 287));
	//don't release cmdbuf because it can be used repeatedly
	//release_cmdbuf(input_para.cmdbuf_id);
}

void vers_resume_hw(u32 on)
{
	int ret;
	struct exchange_parameter input_para;
	u32 irq_status_ret = 0;
	u32 *status_base_virt_addr;
	u32 k = 0;
	u32 main_module_type = 0;
	struct hantrovcmd_dev *dev = NULL;

	if (!on) {
	    release_cmdbuf_node_cleanup(&hantrovcmd_data[0].list_manager);
	    release_process_node_cleanup(&global_process_manager);
	    dev = &hantrovcmd_data[0];
	    dev->working_state = WORKING_STATE_IDLE;
	    dev->sw_cmdbuf_rdy_num = 0;
	    return;
	}

	init_bi_list(&global_process_manager);
	create_kernel_process_manager();
	//for cmdbuf management
	cmdbuf_used_pos = 0;
	for (k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
	    cmdbuf_used[k] = 0;
	    global_cmdbuf_node[k] = NULL;
	}
	//cmdbuf_used[0] not be used, because int vector must non-zero
	cmdbuf_used_residual = TOTAL_DISCRETE_CMDBUF_NUM;
	cmdbuf_used_pos = 1;
	cmdbuf_used[0] = 1;
	cmdbuf_used_residual -= 1;
	input_para.executing_time = 0;
	input_para.priority = CMDBUF_PRIORITY_NORMAL;
	input_para.module_type = main_module_type;
	input_para.cmdbuf_size = 0;
	ret = reserve_cmdbuf(NULL, &input_para);
	vcmd_manager[main_module_type][0]->status_cmdbuf_id =
	    input_para.cmdbuf_id;
	create_read_all_registers_cmdbuf(&input_para);
	link_and_run_cmdbuf(NULL, &input_para);
	msleep(5);
	hantrovcmd_isr(input_para.core_id, &hantrovcmd_data[input_para.core_id]);
	wait_cmdbuf_ready(NULL, input_para.cmdbuf_id, &irq_status_ret);
	status_base_virt_addr =
	    vcmd_status_buf_mem_pool.virtualAddress + input_para.cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
	    (vcmd_manager[input_para.module_type][0]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
}

static struct device*  device;
int __init hantroenc_vcmd_init(struct platform_device *pf_dev)
{
	int i, k;
	int result;

	total_vcmd_core_num =
		sizeof(vcmd_core_array) / sizeof(struct vcmd_config);

#ifdef PCIE_EN
	result = vcmd_pcie_init(pf_dev);
	if (result)
		goto err1;
#endif

	for (i = 0; i < total_vcmd_core_num; i++) {
		pr_info("vcmd: module init - vcmdcore[%d] addr =0x%llx\n", i,
			(unsigned long long)vcmd_core_array[i].vcmd_base_addr);
	}
	pr_info("vc8000_vcmd_driver:vmalloc hantrovcmd_data start\n");
	hantrovcmd_data =
		vmalloc(sizeof(struct hantrovcmd_dev) * total_vcmd_core_num);
	pr_info("vc8000_vcmd_driver:vmalloc hantrovcmd_data end\n");
	if (!hantrovcmd_data)
		goto err1;
	memset(hantrovcmd_data, 0, sizeof(struct hantrovcmd_dev) * total_vcmd_core_num);
	for (k = 0; k < MAX_VCMD_TYPE; k++) {
		vcmd_type_core_num[k] = 0;
		vcmd_position[k] = 0;
		for (i = 0; i < MAX_VCMD_NUMBER; i++)
			vcmd_manager[k][i] = NULL;
	}

	init_bi_list(&global_process_manager);
	result = ConfigAXIFE(1); //1: normal, 2: bypass
	if (result < 0) {
		vcmd_release_AXIFE_IO();
		goto err1;
	}
	result = ConfigMMU();
	if (result < 0) {
		vcmd_release_MMU_IO();
		goto err1;
	}
	result = MMU_Kernel_map();
	if (result < 0)
		goto err;
	for (i = 0; i < total_vcmd_core_num; i++) {
		hantrovcmd_data[i].vcmd_core_cfg = vcmd_core_array[i];
		hantrovcmd_data[i].hwregs = NULL;
		hantrovcmd_data[i].core_id = i;
		hantrovcmd_data[i].working_state = WORKING_STATE_IDLE;
		hantrovcmd_data[i].sw_cmdbuf_rdy_num = 0;
		hantrovcmd_data[i].spinlock = &owner_lock_vcmd[i];
		spin_lock_init(&owner_lock_vcmd[i]);
		hantrovcmd_data[i].wait_queue = &wait_queue_vcmd[i];
		init_waitqueue_head(&wait_queue_vcmd[i]);
		hantrovcmd_data[i].wait_abort_queue = &abort_queue_vcmd[i];
		init_waitqueue_head(&abort_queue_vcmd[i]);
		init_bi_list(&hantrovcmd_data[i].list_manager);
		hantrovcmd_data[i].duration_without_int = 0;
		vcmd_manager[vcmd_core_array[i].sub_module_type]
			    [vcmd_type_core_num[vcmd_core_array[i].sub_module_type]] = &hantrovcmd_data[i];
		vcmd_type_core_num[vcmd_core_array[i].sub_module_type]++;
		hantrovcmd_data[i].vcmd_reg_mem_busAddress =
			vcmd_registers_mem_pool.busAddress + i * VCMD_REGISTER_SIZE - base_ddr_addr;
		//next todo: split out
		hantrovcmd_data[i].mmu_vcmd_reg_mem_busAddress =
			vcmd_registers_mem_pool.mmu_bus_address + i * VCMD_REGISTER_SIZE;
		hantrovcmd_data[i].vcmd_reg_mem_virtualAddress =
			vcmd_registers_mem_pool.virtualAddress + i * VCMD_REGISTER_SIZE / 4;
		hantrovcmd_data[i].vcmd_reg_mem_size = VCMD_REGISTER_SIZE;
		memset(hantrovcmd_data[i].vcmd_reg_mem_virtualAddress, 0, VCMD_REGISTER_SIZE);
	}

	result = register_chrdev(hantrovcmd_major, "vc8000", &hantrovcmd_fops);
	if (result < 0) {
		pr_info("vc8000_vcmd_driver: unable to get major <%d>\n",
			hantrovcmd_major);
		goto err1;
	} else if (result != 0) {
		/* this is for dynamic major */
		hantrovcmd_major = result;
	}

    ret = class_register(&venc_class);
	if (ret < 0) {
        pr_info("hantro: error create venc class!");
		return ret;
	}

    device = device_create(&venc_class, NULL, MKDEV(hantrovcmd_major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        class_destroy(&venc_class);
        unregister_chrdev(hantrovcmd_major, DEVICE_NAME);
        ret = PTR_ERR(device);
		goto err;
    }
	result = vcmd_reserve_IO();
	if (result < 0)
		goto err;
	vcmd_reset_asic(hantrovcmd_data);

	/* get the IRQ line */
	for (i = 0; i < total_vcmd_core_num; i++) {
		if (!hantrovcmd_data[i].hwregs)
			continue;
		/* get interrupt resource */
		hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq =
			platform_get_irq_byname(pf_dev, "vc9000e_irq0");
		if (hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq != -1) {
			result = request_irq(
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq,
				hantrovcmd_isr,
#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
				SA_INTERRUPT | SA_SHIRQ,
#else
				IRQF_SHARED,
#endif
				"versenc-irq", (void *)&hantrovcmd_data[i]);
			if (result == -EINVAL) {
				pr_err("vc8000_vcmd_driver: Bad vcmd_irq number or handler. core_id=%d\n", i);
				vcmd_release_IO();
				goto err;
			} else if (result == -EBUSY) {
				pr_err("vc8000_vcmd_driver: IRQ <%d> busy, change your config. core_id=%d\n",
				       hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq, i);
				vcmd_release_IO();
				goto err;
			}
		} else {
			pr_info("vc8000_vcmd_driver: IRQ not in use!\n");
		}
	}
	//cmdbuf pool allocation
	//init_vcmd_non_cachable_memory_allocate();
	//for cmdbuf management
	cmdbuf_used_pos = 0;
	for (k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
		cmdbuf_used[k] = 0;
		global_cmdbuf_node[k] = NULL;
	}
	//cmdbuf_used[0] not be used, because int vector must non-zero
	cmdbuf_used_residual = TOTAL_DISCRETE_CMDBUF_NUM;
	cmdbuf_used_pos = 1;
	cmdbuf_used[0] = 1;
	cmdbuf_used_residual -= 1;

	pr_info("vc8000_vcmd_driver: module inserted. Major <%d>\n",
		hantrovcmd_major);

	create_kernel_process_manager();
	for (i = 0; i < MAX_VCMD_TYPE; i++) {
		if (vcmd_type_core_num[i] == 0)
			continue;
		sema_init(&vcmd_reserve_cmdbuf_sem[i], 1);
	}
#ifdef IRQ_SIMULATION
	for (i = 0; i < 10000; i++)
		timer_reserve[i].timer = NULL;
#endif
	/*read all registers for each type of module for analyzing configuration in cwl*/
	for (i = 0; i < MAX_VCMD_TYPE; i++) {
		if (vcmd_type_core_num[i] == 0)
			continue;
		PDEBUG("hantrovcmd_init: vcmd_core_type is %d\n", i);
		read_main_module_all_registers(i);
	}

	return 0;
err:
#ifdef HANTROMMU_SUPPORT
	MMU_Kernel_unmap();
	vcmd_pool_release();
#endif
	unregister_chrdev(hantrovcmd_major, "vc8000");
err1:
	if (hantrovcmd_data)
		vfree(hantrovcmd_data);
	pr_info("vc8000_vcmd_driver: module not inserted\n");
	return result;
}

void __exit hantroenc_vcmd_cleanup(struct platform_device *pf_dev)
{
	int i = 0;
	u32 result;

	for (i = 0; i < total_vcmd_core_num; i++) {
		if (!hantrovcmd_data[i].hwregs)
			continue;
		//disable interrupt at first
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
			       VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//disable HW
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
			       VCMD_REGISTER_CONTROL_OFFSET, 0x0000);
		//read status register
		result = vcmd_read_reg((const void *)hantrovcmd_data[i].hwregs,
				       VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)hantrovcmd_data[i].hwregs,
			       VCMD_REGISTER_INT_STATUS_OFFSET, result);

		/* free the vcmd IRQ */
		if (hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq != -1) {
			free_irq(hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq,
				 (void *)&hantrovcmd_data[i]);
		}
		release_cmdbuf_node_cleanup(&hantrovcmd_data[i].list_manager);
	}

	release_process_node_cleanup(&global_process_manager);

#ifdef HANTROMMU_SUPPORT
	MMUCleanup(mmu_hwregs);
#endif

	vcmd_release_IO();
	vfree(hantrovcmd_data);

	//release_vcmd_non_cachable_memory();
	MMU_Kernel_unmap();
	vcmd_pool_release();
	pr_info("vc8000_vcmd_driver: free resource\n");
    dma_free_coherent(&pf_dev->dev, alloc_size_byte, vaddr, paddr);
    if (device)
		device_destroy(&venc_class, MKDEV(hantrovcmd_major, 0));

	class_destroy(&venc_class);

	unregister_chrdev(hantrovcmd_major, "vc8000");
	pr_info("vc8000_vcmd_driver: module removed\n");
}

static int vcmd_reserve_IO(void)
{
	u32 hwid;
	int i;
	u32 found_hw = 0;

	pr_info("%s: total_vcmd_core_num is %d\n", __func__,
		total_vcmd_core_num);
	for (i = 0; i < total_vcmd_core_num; i++) {
		hantrovcmd_data[i].hwregs = NULL;

		if (!request_mem_region(
			    hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
			    hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize,
			    "vc8000_vcmd_driver")) {
			pr_info("hantrovcmd: failed to reserve HW regs for vcmd %d\n", i);
			pr_info("hantrovcmd: vcmd_base_addr = 0x%08lx, iosize = %d\n",
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
			continue;
		}

#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
		hantrovcmd_data[i].hwregs = (volatile u8 __force *)ioremap_nocache(
			hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
			hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
#else
		hantrovcmd_data[i].hwregs = (volatile u8 *)ioremap(
			hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
			hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
#endif
		if (!hantrovcmd_data[i].hwregs) {
			pr_info("hantrovcmd: failed to ioremap HW regs\n");
			release_mem_region(
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
			continue;
		}

		/*read hwid and check validness and store it*/
		hwid = (u32)ioread32((void __iomem *)hantrovcmd_data[i].hwregs);
		pr_info("%s: hantrovcmd_data[%d].hwregs=0x%p\n", __func__,
			i, hantrovcmd_data[i].hwregs);
		pr_info("hwid=0x%08x\n", hwid);
		hantrovcmd_data[i].hw_version_id = hwid;

		/* check for vcmd HW ID */
		if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
			pr_info("hantrovcmd: HW not found at 0x%llx\n",
				(unsigned long long)hantrovcmd_data[i]
					.vcmd_core_cfg.vcmd_base_addr);
			iounmap((void __iomem *)hantrovcmd_data[i].hwregs);
			release_mem_region(
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
			hantrovcmd_data[i].hwregs = NULL;
			continue;
		}

		found_hw = 1;

		pr_info("hantrovcmd: HW at base <0x%llx> with ID <0x%08x>\n",
			(unsigned long long)hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr, hwid);
	}

	if (found_hw == 0) {
		pr_err("hantrovcmd: NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

static void vcmd_release_IO(void)
{
	u32 i;

	vcmd_release_AXIFE_IO();
	vcmd_release_MMU_IO();
	for (i = 0; i < total_vcmd_core_num; i++) {
		if (hantrovcmd_data[i].hwregs) {
			iounmap((void __iomem *)hantrovcmd_data[i].hwregs);
			release_mem_region(
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr,
				hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
			hantrovcmd_data[i].hwregs = NULL;
		}
	}
}

#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
static irqreturn_t hantrovcmd_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t hantrovcmd_isr(int irq, void *dev_id)
#endif
{
	unsigned int handled = 0;
	struct hantrovcmd_dev *dev = (struct hantrovcmd_dev *)dev_id;
	u32 irq_status = 0;
	unsigned long flags;
	bi_list_node *new_cmdbuf_node = NULL;
	bi_list_node *base_cmdbuf_node = NULL;
	struct cmdbuf_obj *cmdbuf_obj = NULL;
	size_t exe_cmdbuf_busAddress;
	u32 cmdbuf_processed_num = 0;
	u32 cmdbuf_id = 0;

	/*If core is not reserved by any user, but irq is received, just clean it*/
	spin_lock_irqsave(dev->spinlock, flags);
	if (!dev->list_manager.head) {
		PDEBUG("hantrovcmd_isr:received IRQ but core has nothing to do.\n");
		irq_status = vcmd_read_reg((const void *)dev->hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET);
		vcmd_write_reg((const void *)dev->hwregs,
			       VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	PDEBUG("hantrovcmd_isr: received IRQ!\n");
	irq_status = vcmd_read_reg((const void *)dev->hwregs,
				   VCMD_REGISTER_INT_STATUS_OFFSET);
#ifdef VCMD_DEBUG_INTERNAL
	{
		u32 i, fordebug;

		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			fordebug = vcmd_read_reg((const void *)dev->hwregs, i * 4);
			pr_info("vcmd register %d:0x%x\n", i, fordebug);
		}
	}
#endif

	if (!irq_status) {
		//printk(KERN_INFO"hantrovcmd_isr error,irq_status :0x%x",irq_status);
		spin_unlock_irqrestore(dev->spinlock, flags);
		return IRQ_HANDLED;
	}

	PDEBUG("irq_status of %d is: 0x%x\n", dev->core_id, irq_status);
	vcmd_write_reg((const void *)dev->hwregs,
		       VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);
	dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

	if (dev->hw_version_id > HW_ID_1_0_C && (irq_status & 0x3f)) {
		//if error,read from register directly.
		cmdbuf_id =
			vcmd_get_register_value((const void *)dev->hwregs,
						dev->reg_mirror,
						HWIF_VCMD_CMDBUF_EXECUTING_ID);
		if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
			pr_err("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	} else if (dev->hw_version_id > HW_ID_1_0_C) {
		//read cmdbuf id from ddr
#ifdef VCMD_DEBUG_INTERNAL
		{
			u32 i, fordebug;

			pr_info("ddr vcmd register phy_addr=0x%x\n",
				(u32)dev->vcmd_reg_mem_busAddress);
			pr_info("ddr vcmd register virt_addr=0x%x\n",
				(u32)dev->vcmd_reg_mem_virtualAddress);
			for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
				fordebug =
					*(dev->vcmd_reg_mem_virtualAddress + i);
				pr_info("ddr vcmd register %d:0x%x\n", i,
					fordebug);
			}
		}
#endif

		cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress +
			      EXECUTING_CMDBUF_ID_ADDR);
		PDEBUG("hantrovcmd_isr: cmdbuf_id %d from virtual!!\n", cmdbuf_id);
		if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
			pr_err("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	}

	if (dev->hw_version_id < HW_ID_1_1_1) {
		if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_RESET)) {
			//reset error,all cmdbuf that is not  done will be run again.
			new_cmdbuf_node = dev->list_manager.head;
			dev->working_state = WORKING_STATE_IDLE;
			//find the first run_done=0
			while (1) {
				if (!new_cmdbuf_node)
					break;
				cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
				if (cmdbuf_obj->cmdbuf_run_done == 0)
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
			base_cmdbuf_node = new_cmdbuf_node;
			vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
			vcmd_link_cmdbuf(dev, base_cmdbuf_node);
			if (dev->sw_cmdbuf_rdy_num != 0) {
				//restart new command
				vcmd_start(dev, base_cmdbuf_node);
			}
			handled++;
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
	}

	if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ABORT)) {
		//abort error,don't need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				pr_err("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_busAddress = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the cmdbuf that tigers ABORT
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock, flags);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) <=
				      exe_cmdbuf_busAddress) &&
				     (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr + cmdbuf_obj->cmdbuf_size) >
				       exe_cmdbuf_busAddress))) &&
				    cmdbuf_obj->cmdbuf_run_done == 0)
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if (cmdbuf_obj->cmdbuf_run_done == 0) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		if (software_triger_abort == 0) {
			//for QCFE
			vcmd_link_cmdbuf(dev, base_cmdbuf_node);
			if (dev->sw_cmdbuf_rdy_num != 0) {
				//restart new command
				vcmd_start(dev, base_cmdbuf_node);
			}
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		//to let high priority cmdbuf be inserted
		wake_up_interruptible_all(dev->wait_abort_queue);
		handled++;
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR)) {
		//bus error, don't need to reset where to record status?
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				pr_err("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_busAddress = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the buserr cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
							       flags);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
						     new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) <=
				      exe_cmdbuf_busAddress) &&
				     (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr + cmdbuf_obj->cmdbuf_size) >
				       exe_cmdbuf_busAddress))) &&
				    (cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		new_cmdbuf_node = base_cmdbuf_node;
		if (new_cmdbuf_node) {
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_BUSERR;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT)) {
		//time out,need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				pr_err("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_busAddress = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the timeout cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
							       flags);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) <=
				      exe_cmdbuf_busAddress) &&
				     (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr + cmdbuf_obj->cmdbuf_size) >
				       exe_cmdbuf_busAddress))) &&
				    (cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		new_cmdbuf_node = new_cmdbuf_node->previous;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//reset
			vcmd_reset_current_asic(dev);
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		return IRQ_HANDLED;
	}
	if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR)) {
		//command error,don't need to reset
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				pr_err("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			exe_cmdbuf_busAddress = VCMDGetAddrRegisterValue(
				(const void *)dev->hwregs, dev->reg_mirror,
				HWIF_VCMD_EXECUTING_CMD_ADDR);
			//find the cmderror cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
							       flags);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
						     new_cmdbuf_node->data;
				if ((((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr) <=
				      exe_cmdbuf_busAddress) &&
				     (((cmdbuf_obj->cmdbuf_busAddress - base_ddr_addr + cmdbuf_obj->cmdbuf_size) >
				       exe_cmdbuf_busAddress))) &&
				    (cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status =
					CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		new_cmdbuf_node = base_cmdbuf_node;
		if (new_cmdbuf_node) {
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			cmdbuf_obj->executing_status =
				CMDBUF_EXE_STATUS_CMDERR; //cmderr
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		return IRQ_HANDLED;
	}

	if (vcmd_get_register_mirror_value(dev->reg_mirror,
					   HWIF_VCMD_IRQ_ENDCMD)) {
		//end command interrupt
		new_cmdbuf_node = dev->list_manager.head;
		dev->working_state = WORKING_STATE_IDLE;
		if (dev->hw_version_id > HW_ID_1_0_C) {
			new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
			if (!new_cmdbuf_node) {
				pr_err("hantrovcmd_isr error cmdbuf_id !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		} else {
			//find the end cmdbuf
			while (1) {
				if (!new_cmdbuf_node) {
					spin_unlock_irqrestore(dev->spinlock,
							       flags);
					return IRQ_HANDLED;
				}
				cmdbuf_obj = (struct cmdbuf_obj *)
						     new_cmdbuf_node->data;
				if ((cmdbuf_obj->has_end_cmdbuf == 1) &&
				    (cmdbuf_obj->cmdbuf_run_done == 0))
					break;
				new_cmdbuf_node = new_cmdbuf_node->next;
			}
		}
		base_cmdbuf_node = new_cmdbuf_node;
		// this cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		base_cmdbuf_node = base_cmdbuf_node->next;
		vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
		vcmd_link_cmdbuf(dev, base_cmdbuf_node);
		if (dev->sw_cmdbuf_rdy_num != 0) {
			//restart new command
			vcmd_start(dev, base_cmdbuf_node);
		}
		spin_unlock_irqrestore(dev->spinlock, flags);
		if (cmdbuf_processed_num)
			wake_up_interruptible_all(dev->wait_queue);
		handled++;
		return IRQ_HANDLED;
	}
	if (dev->hw_version_id <= HW_ID_1_0_C)
		cmdbuf_id = vcmd_get_register_mirror_value(
			dev->reg_mirror, HWIF_VCMD_IRQ_INTCMD);
	if (cmdbuf_id) {
		if (dev->hw_version_id <= HW_ID_1_0_C) {
			if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
				pr_err("hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
				spin_unlock_irqrestore(dev->spinlock, flags);
				return IRQ_HANDLED;
			}
		}
		new_cmdbuf_node = global_cmdbuf_node[cmdbuf_id];
		if (!new_cmdbuf_node) {
			pr_err("hantrovcmd_isr error cmdbuf_id %d!!\n", cmdbuf_id);
			spin_unlock_irqrestore(dev->spinlock, flags);
			return IRQ_HANDLED;
		}
		// interrupt cmdbuf and cmdbufs prior to itself, run_done = 1
		while (1) {
			if (!new_cmdbuf_node)
				break;
			cmdbuf_obj = (struct cmdbuf_obj *)new_cmdbuf_node->data;
			if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
				cmdbuf_obj->cmdbuf_run_done = 1;
				cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
				cmdbuf_processed_num++;
			} else
				break;
			new_cmdbuf_node = new_cmdbuf_node->previous;
		}
		handled++;
	}

	spin_unlock_irqrestore(dev->spinlock, flags);
	if (cmdbuf_processed_num)
		wake_up_interruptible_all(dev->wait_queue);
	if (!handled)
		PDEBUG("IRQ received, but not hantro's!\n");
	return IRQ_HANDLED;
}

static void vcmd_reset_asic(struct hantrovcmd_dev *dev)
{
	int i, n;
	u32 result;

	for (n = 0; n < total_vcmd_core_num; n++) {
		if (dev[n].hwregs) {
			//disable interrupt at first
			vcmd_write_reg((const void *)dev[n].hwregs,
				       VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
			//reset core
			vcmd_write_reg((const void *)dev[n].hwregs,
				       VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
			//read status register
			result = vcmd_read_reg((const void *)dev[n].hwregs,
					       VCMD_REGISTER_INT_STATUS_OFFSET);
			//clean status register
			vcmd_write_reg((const void *)dev[n].hwregs,
				       VCMD_REGISTER_INT_STATUS_OFFSET, result);
			for (i = VCMD_REGISTER_CONTROL_OFFSET;
			     i < dev[n].vcmd_core_cfg.vcmd_iosize; i += 4) {
				//set all register 0
				vcmd_write_reg((const void *)dev[n].hwregs, i,
					       0x0000);
			}
			//enable all interrupt
			vcmd_write_reg((const void *)dev[n].hwregs,
				       VCMD_REGISTER_INT_CTL_OFFSET,
				       0xffffffff);
		}
	}
}

static void vcmd_reset_current_asic(struct hantrovcmd_dev *dev)
{
	u32 result;

	if (dev->hwregs) {
		//disable interrupt at first
		vcmd_write_reg((const void *)dev->hwregs,
			       VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
		//reset core
		vcmd_write_reg((const void *)dev->hwregs,
			       VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
		//read status register
		result = vcmd_read_reg((const void *)dev->hwregs,
				       VCMD_REGISTER_INT_STATUS_OFFSET);
		//clean status register
		vcmd_write_reg((const void *)dev->hwregs,
			       VCMD_REGISTER_INT_STATUS_OFFSET, result);
	}
}

#ifdef VCMD_DEBUG_INTERNAL
static void printk_vcmd_register_debug(const void *hwregs, char *info)
{
	u32 i, fordebug;

	for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
		fordebug = vcmd_read_reg((const void *)hwregs, i * 4);
		pr_info("%s vcmd register %d:0x%x\n", info, i, fordebug);
	}
}
#endif
