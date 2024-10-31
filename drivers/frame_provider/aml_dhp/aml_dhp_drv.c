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

//#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/highmem.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/vmalloc.h>
#include <linux/kfifo.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/major.h>
#include <linux/mman.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/err.h>
#include <linux/hashtable.h>
#include <linux/radix-tree.h>

#include "aml_dhp_drv.h"
#include "aml_dhp_if.h"
#include "../../common/media_utils/media_utils.h"
#include "../../common/media_utils/media_kernel_version.h"

#define DEVICE_NAME	"aml_dhp_dev"
#define CLASS_NAME	"aml_dhp"

#define DHP_DRV_VER DHP_VER(1, 0, 0)

#define LOG(level, fmt, args...) \
	do { \
		if (debug >= level) \
			pr_info(fmt, ##args); \
	} while (0)

#define LOG_ERR(fmt, args...)	LOG(0, "[ERROR]: " fmt, ##args)
#define LOG_INFO(fmt, args...)	LOG(0, "[INFO]: " fmt, ##args)
#define LOG_WARN(fmt, args...)	LOG(1, "[WARN]: " fmt, ##args)
#define LOG_DEBUG(fmt, args...)	LOG(2, "[DEBUG]: " fmt, ##args)
#define LOG_TRACE(fmt, args...)	LOG(3, "[TRACE]: " fmt, ##args)

/*
 * struct aml_dhp_dev - Represents a DHP (Data Handler Proxy) device.
 *
 * @cdev	: Character device structure for this device.
 * @dev		: Pointer to the associated device structure.
 * @dev_no	: Device number assigned to this device.
 *
 * @mutex	: Mutex to protect access to the instance list and count.
 * @inst_head	: Head of the list that manages device instances.
 * @inst_cnt	: Count of active instances associated with this device.
 */
struct aml_dhp_dev {
	struct cdev		cdev;
	struct device		*dev;
	dev_t			dev_no;

	/* Manage instance info.*/
	struct mutex		mutex;
	struct list_head	inst_head;
	u32			inst_cnt;
};

/*
 * struct pfn_hashtable - Represents a hash table that stores PFNs.
 *
 * @tbl		: Used to store PFN entries.
 * @pfn_count	: The total count of unique PFNs in the hashtable.
 */
struct pfn_hashtable {
	DECLARE_HASHTABLE(tbl, 10);
	u32			pfn_count;
};

/*
 * struct pfn_node - Represents a single entry in the PFN hashtable.
 *
 * @pfn		: The PFN (Page Frame Number) being stored in the hashtable.
 * @uptr	: A user-space pointer (or other associated data) related to the PFN.
 * @hash	: A hash list node, part of the hash table entry.
 */
struct pfn_node {
	u64			pfn;
	ulong			uptr;
	struct hlist_node	hash;
};

static struct aml_dhp_dev *g_dev;
static u32 debug;

static void aml_dhp_du_free(struct data_unit *task);
void __aml_dhp_release(struct kref *kref);

/*
 * aml_dhp_dbuf_put - Decrease reference count and free resources when done.
 *
 * @buf_priv : Pointer to buffer-specific private data (data_unit).
 *
 * This function decreases the reference count for the associated buffer. When
 * the reference count drops to zero, it frees the scatter-gather table, releases
 * the device, and deallocates the data unit (du). Finally, it decreases the
 * driver's reference count and calls its release function if needed.
 */
static void aml_dhp_dbuf_put(void *buf_priv)
{
	struct data_unit *du = buf_priv;
	struct aml_dhp_drv *drv = du->priv;
	struct sg_table *sgt = &du->sg_tbl;

	if (!refcount_dec_and_test(&du->refcnt))
		return;

	if (!du->uncached)
		dma_unmap_sg(drv->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);

	sg_free_table(&du->sg_tbl);

	put_device(du->dev);

	aml_dhp_du_free(du);

	kref_put(&drv->ref, __aml_dhp_release);
}

/*
 * aml_dhp_dbuf_release - DMA buffer release callback.
 *
 * @dbuf : Pointer to the DMA buffer structure.
 *
 * This function is called when the DMA buffer is released. It forwards
 * the release process by calling the `aml_dhp_dbuf_put` function with
 * the private data of the buffer.
 */
static void aml_dhp_dbuf_release(struct dma_buf *dbuf)
{
	aml_dhp_dbuf_put(dbuf->priv);
}

/*
 * aml_dhp_vm_open - Handle VMA open for the device.
 *
 * @vma : Pointer to the virtual memory area structure.
 *
 * This function increments the reference count for the VMA header
 * (`du_vma_hdr`) when the memory area is mapped into a process's
 * address space.
 */
static void aml_dhp_vm_open(struct vm_area_struct *vma)
{
	struct du_vma_hdr *vmah = vma->vm_private_data;

	refcount_inc(vmah->refcnt);
}

/*
 * aml_dhp_vm_close - Handle VMA close for the device.
 *
 * @vma : Pointer to the virtual memory area structure.
 *
 * This function is called when the virtual memory area is unmapped.
 * It decreases the reference count and releases resources using the
 * `put` callback provided by the VMA header.
 */
static void aml_dhp_vm_close(struct vm_area_struct *vma)
{
	struct du_vma_hdr *vmah = vma->vm_private_data;

	vmah->put(vmah->arg);
}

const struct vm_operations_struct aml_dhp_vm_ops = {
	.open = aml_dhp_vm_open,
	.close = aml_dhp_vm_close,
};

/*
 * struct aml_dhp_attachment - Represents an attachment for the DHP (Data Handler Proxy)
 *                              device to a device and scatter-gather list (SG).
 *
 * @list       : A list entry to link this attachment to other attachments (typically
 *               in a list of active attachments for a device).
 * @dev        : Pointer to the associated device structure that this attachment is linked to.
 * @sgt        : Scatter-gather table representing the memory mapping for this
 *               attachment. It describes the physical memory buffers to be used.
 * @dma_dir    : Direction of DMA data transfer (e.g., DMA_TO_DEVICE, DMA_FROM_DEVICE).
 * @mapped     : Boolean flag indicating whether the SG table is currently mapped for DMA operations.
 * @uncached   : Boolean flag indicating whether the SG table's memory is uncached.
 */
struct aml_dhp_attachment {
	struct list_head list;
	struct device *dev;
	struct sg_table *sgt;
	enum dma_data_direction dma_dir;
	bool mapped;
	bool uncached;
};

/*
 * dup_sg_table - Duplicates a scatter-gather table (SG table).
 *
 * @table      : The original scatter-gather table to be duplicated.
 *
 * This function creates a new scatter-gather table (`new_table`), allocates memory for
 * it, and copies the entries from the original scatter-gather table (`table`) to the
 * new table.
 * The function iterates over each scatter-gather entry in the original table and
 * copies the page, length, and offset information to the new table.
 *
 * Return: A pointer to the newly created scatter-gather table, or an error pointer
 *         in case of failure.
 */
static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kmalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

/*
 * aml_dhp_dbuf_attach - Attaches a DMA buffer to the given buffer attachment.
 *
 * @dbuf        : Pointer to the DMA buffer structure.
 * @dbuf_attach : Pointer to the DMA buffer attachment structure.
 *
 * This function allocates an attachment for the given DMA buffer and copies the
 * scatter-gather list from the data_unit (du). It sets up the DMA mapping for the
 * attachment but does not map the scatter-gather list yet. Returns 0 on success,
 * or a negative error code on failure.
 */
static int aml_dhp_dbuf_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *dbuf_attach)
{
	struct data_unit *du = dbuf->priv;
	struct aml_dhp_attachment *attach;
	struct sg_table *table;

	attach = kmalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	table = dup_sg_table(&du->sg_tbl);
	if (IS_ERR(table)) {
		kfree(attach);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&attach->list);
	attach->sgt		= table;
	attach->dev		= dbuf_attach->dev;
	attach->uncached	= du->uncached;
	attach->dma_dir		= DMA_NONE;
	attach->mapped		= false;
	dbuf_attach->priv	= attach;

	mutex_lock(&du->lock);
	list_add(&attach->list, &du->attachments);
	mutex_unlock(&du->lock);

	return 0;
}

/*
 * aml_dhp_dbuf_detach - Detaches a DMA buffer from the given attachment.
 *
 * @dbuf      : Pointer to the DMA buffer structure.
 * @db_attach : Pointer to the DMA buffer attachment structure.
 *
 * This function frees the resources associated with the attachment, including
 * unmapping any previously mapped scatter-gather tables and freeing the memory
 * associated with the scatter-gather list and the attachment itself.
 */
static void aml_dhp_dbuf_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *db_attach)
{
	struct data_unit *du = dbuf->priv;
	struct aml_dhp_attachment *attach = db_attach->priv;

	if (!attach)
		return;

	mutex_lock(&du->lock);
	list_del(&attach->list);
	mutex_unlock(&du->lock);

	sg_free_table(attach->sgt);
	kfree(attach->sgt);
	kfree(attach);

	db_attach->priv = NULL;

}

/*
 * aml_dhp_dbuf_map - Maps the DMA buffer into the client's address space.
 *
 * @db_attach : Pointer to the DMA buffer attachment structure.
 * @dma_dir   : The direction of the DMA transfer.
 *
 * This function maps the scatter-gather table into the client's address space
 * with the specified direction. If the table has already been mapped in the
 * same direction, it returns the previously mapped scatter-gather table.
 * Returns a pointer to the scatter-gather table or an error pointer on failure.
 */
static struct sg_table *aml_dhp_dbuf_map(struct dma_buf_attachment *db_attach,
	enum dma_data_direction dma_dir)
{
	struct aml_dhp_attachment *attach = db_attach->priv;
	ulong attr = 0;
	/* stealing dmabuf mutex to serialize map/unmap operations */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0)
	struct mutex *lock = &db_attach->dmabuf->lock;
#endif
	struct sg_table *sgt = attach->sgt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0)
	mutex_lock(lock);
#endif
	if (attach->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0)
		mutex_unlock(lock);
#endif
		return sgt;
	}

	/* mapping to the client with new direction. */
	if (dma_map_sgtable(db_attach->dev, sgt, dma_dir, attr)) {
		LOG_ERR("failed to map scatterlist\n");
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0)
		mutex_unlock(lock);
#endif
		return ERR_PTR(-EIO);
	}

	attach->dma_dir = dma_dir;
	attach->mapped = true;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0)
	mutex_unlock(lock);
#endif
	return sgt;
}

/*
 * aml_dhp_dbuf_unmap - Unmaps the DMA buffer from the client's address space.
 *
 * @db_attach : Pointer to the DMA buffer attachment structure.
 * @sgt       : Pointer to the scatter-gather table.
 * @dma_dir   : The direction of the DMA transfer.
 *
 * This function currently does not perform any unmapping operations, but it
 * is included for completeness and future handling of unmapping if needed.
 */
static void aml_dhp_dbuf_unmap(struct dma_buf_attachment *db_attach,
	struct sg_table *sgt, enum dma_data_direction dma_dir)
{
	struct aml_dhp_attachment *attach = db_attach->priv;
	ulong attr = 0;

	if (attach->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sgtable(db_attach->dev, sgt, dma_dir, attr);
	}

	attach->mapped = false;
}

/*
 * __aml_dhp_dbuf_mmap - Maps the DMA buffer to the user-space virtual memory area.
 *
 * @buf_priv : Pointer to the private data of the buffer (data_unit).
 * @vma      : Pointer to the virtual memory area structure.
 *
 * This function maps the buffer's pages into the user's virtual memory area.
 * It iterates through the scatter-gather list and maps each page using remap_pfn_range.
 * Additionally, it sets up virtual memory area flags and operations, and initializes
 * the VMA's private data.
 */
static int __aml_dhp_dbuf_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct data_unit *du = buf_priv;
	struct aml_dhp_drv *drv = du->priv;
	struct sg_table *sgt = &du->sg_tbl;
	ulong addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if (!du) {
		LOG_ERR("[%u]: No buffer to map\n", drv->uid);
		return -EINVAL;
	}

	if (!IS_ALIGNED(vma->vm_start, PAGE_SIZE)) {
		LOG_ERR("[%u]: VMA start address not aligned: %lx\n",
			drv->uid, vma->vm_start);
		return -EINVAL;
	}

	if (du->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sgtable_page(sgt, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma,
				addr,
				page_to_pfn(page),
				PAGE_SIZE,
				vma->vm_page_prot);
		if (ret) {
			LOG_ERR("[%u]: Failed to map page at VMA addr: %lx, PFN: %lx, error: %d\n",
				drv->uid, addr, page_to_pfn(page), ret);
			return ret;
		}

		addr += PAGE_SIZE;

		if (addr >= vma->vm_end)
			break;
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 3, 0)
	vma->vm_flags		|= VM_DONTEXPAND | VM_DONTDUMP;
#else
	vm_flags_set(vma, vma->vm_flags | VM_DONTEXPAND | VM_DONTDUMP);
#endif

	vma->vm_private_data	= &du->vmah;
	vma->vm_ops		= &aml_dhp_vm_ops;

	vma->vm_ops->open(vma);

	du->mapped = true;

	LOG_DEBUG("[%u]: DBUF Mapped addr:%lx at VMA start %lx, size %lu\n",
		drv->uid, (ulong)sg_phys(sgt->sgl),
		vma->vm_start,
		addr - vma->vm_start);

	return 0;
}

/*
 * aml_dhp_dbuf_mmap - Public interface for mapping a DMA buffer to a user-space VMA.
 *
 * @dbuf : Pointer to the DMA buffer structure.
 * @vma  : Pointer to the virtual memory area structure.
 *
 * This function is a wrapper around __aml_dhp_dbuf_mmap, used to map a DMA buffer
 * into the user-space virtual memory area.
 */
static int aml_dhp_dbuf_mmap(struct dma_buf *dbuf,
	struct vm_area_struct *vma)
{
	return __aml_dhp_dbuf_mmap(dbuf->priv, vma);
}

/*
 * aml_dhp_dbuf_begin_cpu_access - Synchronize DMA buffer for CPU access.
 *
 * @dbuf     : Pointer to the DMA buffer structure.
 * @direction: The direction of data transfer (DMA_FROM_DEVICE, DMA_TO_DEVICE, etc.).
 *
 * This function ensures the DMA buffer's contents are synchronized and safe for CPU access.
 * If the DMA buffer is mapped to user-space or attached to devices, appropriate
 * synchronization is performed for the specified access direction.
 *
 * Return:
 * 0 on success, negative error code on failure.
 */
static int aml_dhp_dbuf_begin_cpu_access(struct dma_buf *dbuf,
	enum dma_data_direction direction)
{
	struct data_unit *du = dbuf->priv;
	struct aml_dhp_drv *drv = du->priv;
	struct aml_dhp_attachment *attach;

	/* Validate direction parameter */
	if (!valid_dma_direction(direction)) {
		LOG_ERR("[%u]: Invalid direction: %d\n", drv->uid, direction);
		return -EINVAL;
	}

	LOG_DEBUG("[%u]: %s, uncached:%d, mapped:%d, dirt %d\n",
		drv->uid, __func__, du->uncached, du->mapped, direction);

	mutex_lock(&du->lock);

	/* Sync for process mmap access */
	if (!du->uncached && du->mapped) {
		LOG_DEBUG("[%u]: Syncing dmabuf for CPU access\n", drv->uid);
		dma_sync_sgtable_for_cpu(du->dev, &du->sg_tbl, direction);
	}

	/* Sync for attached devices */
	list_for_each_entry(attach, &du->attachments, list) {
		if (!attach->mapped)
			continue;

		LOG_DEBUG("[%u]: Syncing attachment for CPU access\n", drv->uid);
		dma_sync_sgtable_for_cpu(attach->dev, attach->sgt, direction);
	}

	mutex_unlock(&du->lock);

	return 0;
}


/*
 * aml_dhp_dbuf_end_cpu_access - Synchronize DMA buffer after CPU access.
 *
 * @dbuf     : Pointer to the DMA buffer structure.
 * @direction: The direction of data transfer (DMA_FROM_DEVICE, DMA_TO_DEVICE, etc.).
 *
 * This function ensures the DMA buffer's contents are synchronized after CPU access,
 * making it safe for use by devices. Appropriate synchronization is performed for
 * the specified access direction, whether for user-space mappings or attached devices.
 *
 * Return:
 * 0 on success, negative error code on failure.
 */
static int aml_dhp_dbuf_end_cpu_access(struct dma_buf *dbuf,
	enum dma_data_direction direction)
{
	struct data_unit *du = dbuf->priv;
	struct aml_dhp_drv *drv = du->priv;
	struct aml_dhp_attachment *attach;

	/* Validate direction parameter */
	if (!valid_dma_direction(direction)) {
		LOG_ERR("[%u]: Invalid direction: %d\n", drv->uid, direction);
		return -EINVAL;
	}

	LOG_DEBUG("[%u]: %s, uncached:%d, mapped:%d, dirt %d\n",
		drv->uid, __func__, du->uncached, du->mapped, direction);

	mutex_lock(&du->lock);

	/* Sync for process mmap access */
	if (!du->uncached && du->mapped) {
		LOG_DEBUG("[%u]: Syncing dmabuf for device access\n", drv->uid);
		dma_sync_sgtable_for_device(du->dev, &du->sg_tbl, direction);
	}

	/* Sync for attached devices */
	list_for_each_entry(attach, &du->attachments, list) {
		if (!attach->mapped)
			continue;

		LOG_DEBUG("[%u]: Syncing attachment for device access\n", drv->uid);
		dma_sync_sgtable_for_device(attach->dev, attach->sgt, direction);
	}

	mutex_unlock(&du->lock);

	return 0;
}

static const struct dma_buf_ops aml_dhp_dbuf_ops = {
	.attach		= aml_dhp_dbuf_attach,
	.detach		= aml_dhp_dbuf_detach,
	.map_dma_buf	= aml_dhp_dbuf_map,
	.unmap_dma_buf	= aml_dhp_dbuf_unmap,
	.begin_cpu_access = aml_dhp_dbuf_begin_cpu_access,
	.end_cpu_access	= aml_dhp_dbuf_end_cpu_access,
	.mmap		= aml_dhp_dbuf_mmap,
	.release	= aml_dhp_dbuf_release,
};

/*
 * aml_get_dmabuf - Allocates and exports a DMA buffer.
 *
 * @du   : Pointer to the data_unit structure.
 * @addr : Physical address of the memory.
 * @size : Size of the buffer.
 * @flags: Flags for DMA buffer allocation.
 *
 * This function allocates a scatter-gather table and initializes it with the specified
 * physical address and size. It then exports the DMA buffer, which can be shared with
 * other drivers or user-space. Returns a pointer to the DMA buffer or NULL on failure.
 */
static struct dma_buf *aml_get_dmabuf(struct data_unit *du, ulong addr, u32 size, ulong flags)
{
	struct aml_dhp_drv *drv = du->priv;
	struct sg_table *sgt = &du->sg_tbl;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dbuf;
	u32 sg_size = size;
	int pages = 0;
	int ents = 0;
	int i;

	if (!addr || size == 0 || !IS_ALIGNED(addr, PAGE_SIZE)) {
		LOG_ERR("[%u]: Invalid address or size: addr=%lx, size=%u\n",
			drv->uid, addr, size);
		return NULL;
	}

	pages = DIV_ROUND_UP(size, PAGE_SIZE);
	if (sg_alloc_table(sgt, pages, GFP_KERNEL)) {
		LOG_ERR("[%u]: Failed to allocate sg_table for %d pages\n",
			drv->uid, pages);
		return NULL;
	}

	for (i = 0; i < pages; i++) {
		struct page *pg = pfn_to_page(PFN_DOWN(addr) + i);
		u32 sz = (sg_size < PAGE_SIZE) ? sg_size : PAGE_SIZE;

		sg_set_page(&sgt->sgl[i], pg, sz, 0);

		sg_size -= sz;
	}

	if (!du->uncached) {
		ents = dma_map_sg(drv->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
		if (ents < 0) {
			LOG_ERR("[%u]: dma map sg failed, ret=%d\n", drv->uid, ents);
			sg_free_table(sgt);
			return NULL;
		}
	}

	exp_info.ops	= &aml_dhp_dbuf_ops;
	exp_info.size	= size;
	exp_info.flags	= flags;
	exp_info.priv	= du;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf)) {
		LOG_ERR("[%u]: dma_buf export failed\n", drv->uid);
		sg_free_table(sgt);
		return NULL;
	}

	LOG_DEBUG("[%u]: Get dbuf:%px, addr:%lx, size %u\n",
		drv->uid, dbuf, addr, size);

	return dbuf;
}

/*
 * aml_dhp_vma_put - Releases resources when the VMA is unmapped.
 *
 * @buf_priv : Pointer to the buffer's private data.
 * @vma      : Pointer to the virtual memory area structure.
 *
 * This function is invoked when a VMA is unmapped. It calls the close operation
 * defined in the VMA's operations and ensures that any related cleanup is performed.
 */
static void aml_dhp_vma_put(void *buf_priv)
{
	struct data_unit *du = buf_priv;
	struct aml_dhp_drv *drv = du->priv;

	if (!refcount_dec_and_test(&du->refcnt))
		return;

	aml_dhp_du_free(du);

	sg_free_table(&du->sg_tbl);

	put_device(du->dev);

	kref_put(&drv->ref, __aml_dhp_release);
}

/**
 * aml_dhp_vma_hdr_init - Initializes the VMA header for a data unit.
 * @du: Pointer to the data unit structure.
 *
 * This function initializes the VMA header for the data unit, setting
 * the reference count and associating it with the data unit's device.
 * It also increments the reference count for the device associated with the data unit.
 */
static void aml_dhp_vma_hdr_init(struct data_unit *du)
{
	//struct aml_dhp_drv *drv = du->priv;

	if (WARN_ON(!du->dev))
		return;

	du->vmah.refcnt	= &du->refcnt;
	du->vmah.put	= aml_dhp_vma_put;
	du->vmah.arg	= du;
	du->dev		= get_device(du->dev);

	refcount_set(&du->refcnt, 1);
}

/**
 * aml_dhp_du_free - Frees the data unit and puts it back to the free queue.
 * @du: Pointer to the data unit structure.
 *
 * This function is called to complete the data unit's operation and
 * put it back into the free FIFO queue for reuse.
 */
static void aml_dhp_du_free(struct data_unit *du)
{
	struct aml_dhp_drv *drv = du->priv;

	complete(&du->comp);

	LOG_DEBUG("[%u]: put_free, DU:%px, len:%d, \n",
		drv->uid, du, kfifo_len(&drv->du_free));

	kfifo_put(&drv->du_free, du);
}

/**
 * aml_dhp_alloc_fd - Allocates a file descriptor for a DMA buffer.
 * @du:       Pointer to the data unit structure.
 * @addr:     Address of the DMA buffer.
 * @size:     Size of the DMA buffer.
 * @uncached: Indicating whether the buffer should be allocated as uncached memory.
 *
 * This function allocates a file descriptor for a DMA buffer associated with
 * the data unit, initializing the necessary structures and reference counts.
 *
 * Return: The allocated file descriptor on success or a negative error code on failure.
 */
static int aml_dhp_alloc_fd(struct data_unit *du, ulong addr, u32 size, bool uncached)
{
	struct aml_dhp_drv *drv = du->priv;
	struct dma_buf *dbuf;
	u32 flags = O_RDWR;
	int fd;

	dbuf = aml_get_dmabuf(du, addr, size, flags);
	if (IS_ERR(dbuf))
		return PTR_ERR(dbuf);

	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		dma_buf_put(dbuf);
		return -1;
	}

	du->dbuf = dbuf;
	du->uncached = uncached;

	aml_dhp_vma_hdr_init(du);

	kref_get(&drv->ref);

	LOG_DEBUG("[%u]: %s alloc FD:%d\n",
		drv->uid, current->comm, fd);

	return fd;
}

int aml_dhp_request(struct aml_du_mem *src, struct aml_du_mem *dst, void *meta, int msize)
{
	struct aml_dhp_drv *drv = NULL;
	struct aml_du_base base = { 0 };
	struct data_unit *du = NULL;
	struct list_head *pos;
	ulong timeout = 0;
	int ret = 0;
#define TIMEOUT_MAX (6000)

	if (msize > AML_DU_META_MAX * sizeof(u32)) {
		LOG_ERR("DU meta data is oversize.\n");
		return -EINVAL;
	}

	mutex_lock(&g_dev->mutex);

	if (list_empty(&g_dev->inst_head)) {
		LOG_ERR("No dhp service.\n");
		goto err;
	}

	list_for_each(pos, &g_dev->inst_head) {
		drv = list_entry(pos, struct aml_dhp_drv, node);
		if (drv) {
			if (!kfifo_get(&drv->du_free, &du)) {
				ret = -EAGAIN;
				goto err;
			}

			kref_get(&drv->ref);
			base.src = *src;
			base.dst = *dst;
			memcpy(base.meta, meta, msize);

			du->pb	= &base;

			LOG_DEBUG("[%u]: get_free, DU:%px, len:%d, \n",
				drv->uid, du, kfifo_len(&drv->du_free));

			mutex_lock(&drv->du_mutex);
			LOG_DEBUG("[%u]: put_done, DU:%px, len:%d, \n",
				drv->uid, du, kfifo_len(&drv->du_done));
			kfifo_put(&drv->du_done, du);
			drv->du_pending = 1;
			wake_up_interruptible(&drv->du_wq);
			mutex_unlock(&drv->du_mutex);

			break;
		}
	}

	mutex_unlock(&g_dev->mutex);

	if (du) {
		timeout = wait_for_completion_timeout(&du->comp,
			msecs_to_jiffies(TIMEOUT_MAX));
		if (timeout) {
			LOG_DEBUG("[%u]: DU:%px task done, elapse:%d ms\n",
				drv->uid, du, TIMEOUT_MAX - jiffies_to_msecs(timeout));
		} else {
			LOG_WARN("[%u]: DU:%px task timeout.\n", drv->uid, du);
		}
	}

	kref_put(&drv->ref, __aml_dhp_release);

	return 0;
err:
	mutex_unlock(&g_dev->mutex);

	return ret;
}

int __aml_dhp_task(void *src, void *dst, void *meta, int size)
{
	return aml_dhp_request(src, dst, meta, size);
}

/**
 * get_du_mem_pfn - Get the Page Frame Number (PFN) from aml_du_mem structure.
 * @m: Pointer to the aml_du_mem structure.
 *
 * This function retrieves the PFN from a memory descriptor based on its type.
 * Returns the PFN for memory types: PFN, physical address, or kernel pointer.
 *
 * Return: Page Frame Number (PFN) or 0 if invalid memory type.
 */
static ulong get_du_mem_pfn(struct aml_du_mem *m)
{
	ulong pfn = 0;

	switch (m->type) {
	case AML_MEM_TYPE_PFN: {
		pfn = m->pfn;
		break;
	}
	case AML_MEM_TYPE_PHY_ADDR: {
		pfn = __phys_to_pfn(m->addr);
		break;
	}
	case AML_MEM_TYPE_KPTR_ADDR: {
		pfn = virt_to_pfn((void *)m->kptr);
		break;
	}
	default:
		return 0;
	}

	return pfn;
}

/**
 * get_du_mem_addr - Get the physical or virtual address from aml_du_mem.
 * @m: Pointer to the aml_du_mem structure.
 *
 * This function retrieves the actual memory address from a memory descriptor
 * depending on whether it's represented by a PFN, physical address, or kernel pointer.
 *
 * Return: Address or 0 if invalid memory type.
 */
static ulong get_du_mem_addr(struct aml_du_mem *m)
{
	ulong addr = 0;

	switch (m->type) {
	case AML_MEM_TYPE_PFN: {
		addr = (ulong)__pfn_to_phys(m->pfn);
		break;
	}
	case AML_MEM_TYPE_PHY_ADDR: {
		addr = m->addr;
		break;
	}
	case AML_MEM_TYPE_KPTR_ADDR: {
		addr = __virt_to_phys(m->kptr);
		break;
	}
	default:
		return 0;
	}

	return addr;
}

static u32 get_du_mem_size(struct aml_du_mem *m)
{
	return m->size;
}

static u32 get_du_mem_cache_mode(struct aml_du_mem *m)
{
	return m->uncached;
}

/**
 * du_alloc_fd - Allocate a file descriptor for a data unit (DU) memory.
 * @drv: Pointer to the aml_dhp_drv structure (driver).
 * @arg: Argument passed from user space.
 *
 * This function allocates a file descriptor for a data unit (DU) based
 * on its memory source address and size, returning the FD to user space.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int du_alloc_fd(struct aml_dhp_drv *drv, ulong arg)
{
	struct aml_dhp_ioctl_data __user *uarg = (void *)arg;
	struct aml_dhp_ioctl_data io;
	struct aml_du_base *base;
	struct data_unit *du;
	bool uncached = false;
	ulong addr = 0;
	u32 size = 0;
	int fd = -1;
	int ret = 0;

	if (!kfifo_get(&drv->du_done, &du)) {
		return -EFAULT;
	}

	LOG_DEBUG("[%u]: get_done, DU:%px, len:%d, \n",
		drv->uid, du, kfifo_len(&drv->du_done));

	base = du->pb;

	addr = get_du_mem_addr(&base->src);
	if (!addr) {
		LOG_ERR("source addr is invalid.\n");
		return -EFAULT;
	}

	size = get_du_mem_size(&base->src);
	if (!size) {
		LOG_ERR("source size is invalid.\n");
		return -EFAULT;
	}

	uncached = !!get_du_mem_cache_mode(&base->src);

	fd = aml_dhp_alloc_fd(du, addr, size, uncached);
	if (fd < 0) {
		ret = fd;
		goto err;
	}

	io.version	= DHP_DRV_VER;
	io.type		= get_du_type(base);
	io.fd		= fd;
	memcpy(io.data, base, sizeof(*base));

	if (copy_to_user(uarg, &io, sizeof(io))) {
		dma_buf_put(du->dbuf);
		put_unused_fd(fd);
		ret = -EFAULT;
		goto err;
	}

	LOG_DEBUG("[%u]: IOCTL_DHP, Get FD:%d.\n",
		drv->uid, fd);

	return 0;
err:
	LOG_DEBUG("[%u]: put_done, DU:%px, len:%d, \n",
		drv->uid, du, kfifo_len(&drv->du_done));
	kfifo_put(&drv->du_done, du);

	return ret;
}

/**
 * __remap_uptr - Remap PFNs to user-space memory.
 * @pfn: Page Frame Number to remap.
 * @len: Length of the memory region.
 * @uncached: Enable uncached mode.
 *
 * This function allocates anonymous user-space memory and remaps the physical page frames (PFNs)
 * into the user-space virtual address space.
 *
 * Return: User-space pointer to the remapped memory, or 0 on failure.
 */
static ulong __remap_uptr(ulong pfn, int len, bool uncached)
{
	struct vm_area_struct *vma = NULL;
	ulong prot = PROT_READ | PROT_WRITE;
	ulong flags = MAP_ANONYMOUS | MAP_SHARED;
	int ulen = PAGE_ALIGN(len);
	ulong uptr = 0;

	/* Attempt to allocate user space memory using vm_mmap */
	uptr = vm_mmap(NULL, 0, ulen, prot, flags, 0);
	if (IS_ERR_VALUE(uptr)) {
		LOG_ERR("Failed to allocate user space memory\n");
		return 0;
	}

	/* Find the virtual memory area (VMA) corresponding to the newly allocated memory */
	vma = find_vma(current->mm, uptr);
	if (!vma || uptr < vma->vm_start || uptr + ulen > vma->vm_end) {
		LOG_ERR("Invalid VMA or address range!\n");
		goto free_uptr;
	}

	if (uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Attempt to map the physical page frames (PFN) to user space memory */
	if (remap_pfn_range(vma, uptr, pfn, ulen, vma->vm_page_prot)) {
		LOG_ERR("Failed to remap pfn range\n");
		goto free_uptr;
	}

	return uptr;

free_uptr:
	vm_munmap(uptr, ulen);

	return 0;
}

/**
 * du_mmap - Map DU memory to user-space.
 * @drv: Pointer to the aml_dhp_drv structure (driver).
 * @arg: Argument passed from user space.
 *
 * This function maps the PFNs of a data unit (DU) memory into the user-space
 * virtual address space and returns the mapped address to user space.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int du_mmap(struct aml_dhp_drv *drv, ulong arg)
{
	struct aml_dhp_ioctl_data __user *uarg = (void *)arg;
	struct aml_dhp_ioctl_data io;
	bool uncached = false;
	ulong uptr = 0;
	ulong pfn = 0;
	u32 size = 0;
	int ret = 0;

	if (copy_from_user(&io, uarg, sizeof(io))) {
		return -EFAULT;
	}

	pfn = get_du_mem_pfn(&io.mem);
	if (!pfn) {
		LOG_ERR("DU PFN is invalid.\n");
		return -EFAULT;
	}

	size = get_du_mem_size(&io.mem);
	if (!size) {
		LOG_ERR("DU size is invalid.\n");
		return -EFAULT;
	}

	uncached = !!get_du_mem_cache_mode(&io.mem);

	uptr = __remap_uptr(pfn, size, uncached);
	if (!uptr && IS_ERR_VALUE(uptr)) {
		LOG_ERR("Failed to remap userspace addr.\n");
		return -EFAULT;
	}

	io.version	= DHP_DRV_VER;
	io.type		= AML_DHP_TYPE_MEM;
	io.mem.uptr	= uptr;

	if (copy_to_user(uarg, &io, sizeof(io))) {
		return -EFAULT;
	}

	LOG_DEBUG("[%u]: DU Mapped addr:%lx at %lx, size %u\n",
		drv->uid, pfn, uptr, size);

	return ret;
}

/**
 * __remap_sgt - Remap scatter-gather table (SGT) to user-space.
 * @uptr_table: Output table to store the user-space pointers (uptr).
 * @pfn_table: Input table of physical page frame numbers (PFNs).
 * @pfn_size: The size of the PFN table (number of PFNs).
 * @uncached: Flag to indicate whether caching is enabled for the mapping.
 *
 * This function maps the PFNs in the scatter-gather table (SGT) to user-space addresses.
 * It validates and remaps unique PFNs, ensuring that the mapping is correctly done
 * while handling memory protection based on the caching flag.
 *
 * Return: The length of the user-space memory region mapped (in bytes) on success,
 *         or a negative error code on failure.
 */
static int __remap_sgt(u64 *uptr_table, u64 *pfn_table, u32 pfn_size, bool uncached)
{
	struct vm_area_struct *vma = NULL;
	struct pfn_hashtable *pfn_htbl = NULL;
	struct pfn_node *nodes = NULL, *node;
	ulong uptr = 0;
	ulong addr;
	int unique_pfns = 0;
	int ulen = 0;
	int ret = 0;
	int i;

	pfn_htbl = vmalloc(sizeof(*pfn_htbl));
	if (!pfn_htbl) {
		LOG_ERR("Failed to allocate memory for PFNs hashtable.\n");
		return -ENOMEM;
	}

	nodes = vmalloc(sizeof(*node) * pfn_size);
	if (!nodes) {
		vfree(pfn_htbl);
		pr_err("Failed to allocate memory for PFN node array.\n");
		return -ENOMEM;
	}

	hash_init(pfn_htbl->tbl);

	/* Calculate unique PFNs using hash table */
	for (i = 0; i < pfn_size; i++) {
		u64 pfn = pfn_table[i];
		unsigned int hash_index = hash_64(pfn, 10);
		bool found = false;

		/* Iterate over the bucket corresponding to the PFN's hash index */
		hash_for_each_possible(pfn_htbl->tbl, node, hash, hash_index) {
			if (node->pfn == pfn) {
				found = true;
				break;
			}
		}

		if (found)
			continue;

		/* Insert the unique PFN into the hash table */
		node = &nodes[i];
		node->pfn = pfn;
		node->uptr = 0;
		hash_add(pfn_htbl->tbl, &node->hash, hash_index);

		unique_pfns++;
	}

	/* Allocate contiguous memory in user space based on unique PFNs */
	ulen = unique_pfns * PAGE_SIZE;
	uptr = vm_mmap(NULL, 0, ulen, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0);
	if (IS_ERR_VALUE(uptr)) {
		ulen = -ENOMEM;
		LOG_ERR("Failed to allocate user-space memory\n");
		goto err;
	}

	/* Validate the VMA (virtual memory area) range */
	vma = find_vma(current->mm, uptr);
	if (!vma || uptr < vma->vm_start || (uptr + ulen) > vma->vm_end) {
		vm_munmap(uptr, ulen);
		ulen = -EINVAL;
		LOG_ERR("Invalid VMA or address range!\n");
		goto err;
	}

	if (uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Map PFNs to user-space memory, using cached mappings where available */
	addr = uptr;
	for (i = 0; i < pfn_size; i++) {
		u64 pfn = pfn_table[i];
		u32 hash_index = hash_64(pfn, 10);
		bool found = false;

		hash_for_each_possible(pfn_htbl->tbl, node, hash, hash_index) {
			if (node->pfn == pfn) {
				found = true;
				break;
			}
		}

		if (!found) {
			vm_munmap(uptr, ulen);
			ulen = -EINVAL;
			LOG_ERR("Failed to find valid pfn:%llx\n", pfn);
			goto err;
		}

		if (node->uptr) {
			uptr_table[i] = node->uptr;  // Use cached mapping
		} else {
			ret = remap_pfn_range(vma, addr, pfn, PAGE_SIZE, vma->vm_page_prot);
			if (ret) {
				vm_munmap(uptr, ulen);
				ulen = -EFAULT;
				LOG_ERR("Failed to map PFN %llx to user space at addr %lx, error %d\n", pfn, addr, ret);
				goto err;
			}

			node->uptr = addr;
			uptr_table[i] = addr;
			addr += PAGE_SIZE;
		}
	}
err:
	if (nodes)
		vfree(nodes);
	if (pfn_htbl)
		vfree(pfn_htbl);

	return ulen;
}

/**
 * du_sgt_mmap - Map DU scatter-gather table to user-space.
 * @drv: Pointer to the aml_dhp_drv structure (driver).
 * @arg: Argument passed from user space, containing memory and PFN details.
 *
 * This function maps a scatter-gather table (SGT) of PFNs into the user-space virtual
 * address space. It allocates memory for both the PFN and user-space pointer tables,
 * performs the remapping of the PFNs to user space, and then returns the results back to user space.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int du_sgt_mmap(struct aml_dhp_drv *drv, ulong arg)
{
	struct aml_dhp_ioctl_data __user *uarg = (void *)arg;
	struct aml_dhp_ioctl_data io;
	bool uncached = false;
	u64 *pfn_table = NULL;
	u32 pfn_num;
	u64 *uptr_table = NULL;
	int uptr_num;
	int uptr_len;
	ktime_t kt_earlier;
	int ret = 0;

	/* Copy data from user space */
	if (copy_from_user(&io, uarg, sizeof(io)))
		return -EFAULT;

	/* Get memory size for data unit (DU) */
	pfn_num = get_du_mem_size(&io.base.src);
	if (!pfn_num) {
		LOG_ERR("DU size is invalid.\n");
		return -EINVAL;
	}

	uptr_num = get_du_mem_size(&io.base.dst);
	if (pfn_num > uptr_num) {
		LOG_ERR("dst-tab:%d is smaller than src-tab:%d.\n",
			uptr_num, pfn_num);
		return -EINVAL;
	}

	/* Allocate memory for sg_table and uptr_table */
	pfn_table = vmalloc(pfn_num * sizeof(u64));
	if (!pfn_table) {
		return -ENOMEM;
	}

	uptr_table = vmalloc(pfn_num * sizeof(u64));
	if (!uptr_table) {
		ret = -ENOMEM;
		goto err;
	}

	/* Copy pfn table from user */
	if (copy_from_user(pfn_table, (void __user *)io.base.src.sgt, pfn_num * sizeof(u64))) {
		ret = -EFAULT;
		goto err;
	}

	uncached = !!get_du_mem_cache_mode(&io.base.dst);

	/* Remap PFNs table */
	kt_earlier = ktime_get();
	uptr_len = __remap_sgt(uptr_table, pfn_table, pfn_num, uncached);
	if (uptr_len < 0) {
		ret = uptr_len;
		goto err;
	}

	LOG_TRACE("[%u]: Remap SGT elapse:%lld ms.\n",
		drv->uid, ktime_ms_delta(ktime_get(), kt_earlier));

	/* Copy the mapped results back to user space */
	io.version = DHP_DRV_VER;
	io.type = AML_DHP_TYPE_MEM;
	io.base.dst.type = AML_MEM_TYPE_SG_TBL;
	io.base.dst.payload = uptr_len;

	if (copy_to_user((void __user *)io.base.dst.sgt, uptr_table, pfn_num * sizeof(u64)) ||
		copy_to_user(uarg, &io, sizeof(io))) {
		vm_munmap(uptr_table[0], uptr_len);
		ret = -EFAULT;
	}

	LOG_DEBUG("[%u]: DU Mapped SGT to uptr: %lx, size: %u\n",
		drv->uid, (ulong)uptr_table[0], uptr_len);
err:
	if (pfn_table)
		vfree(pfn_table);
	if (uptr_table)
		vfree(uptr_table);

	return ret;
}

/**
 * __aml_dhp_sgt_sync - Synchronizes a scatter-gather table for CPU or device access.
 * @drv: Pointer to the DHP driver structure.
 * @mem: Pointer to the memory descriptor structure.
 * @dir: Direction of data transfer (e.g., DMA_FROM_DEVICE or DMA_TO_DEVICE).
 * @for_cpu: Boolean indicating whether the synchronization is for CPU access (true)
 *           or device access (false).
 *
 * This function ensures proper synchronization of the scatter-gather table to allow
 * CPU or device access. Unique PFNs are identified using a hash table, and synchronization
 * operations are performed for each unique PFN. The function supports both directions
 * of data transfer and handles resource cleanup in case of errors.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int __aml_dhp_sgt_sync(struct aml_dhp_drv *drv,
	struct aml_du_mem *mem,
	enum dma_data_direction dir,
	bool for_cpu)
{
	struct pfn_hashtable *pfn_htbl = NULL;
	struct pfn_node *nodes = NULL, *node;
	u64 *pfn_table = NULL;
	u32 pfn_num = 0;
	int ret = 0;
	int i;

	pfn_num = get_du_mem_size(mem);
	if (!pfn_num) {
		LOG_ERR("DU size is invalid.\n");
		return -EINVAL;
	}

	pfn_htbl = vmalloc(sizeof(*pfn_htbl));
	if (!pfn_htbl) {
		LOG_ERR("Failed to allocate memory for PFNs hashtable.\n");
		return -ENOMEM;
	}

	nodes = vmalloc(pfn_num * sizeof(*node));
	if (!nodes) {
		ret = -ENOMEM;
		LOG_ERR("Failed to allocate memory for PFNs nodes.\n");
		goto err;
	}

	pfn_table = vmalloc(pfn_num * sizeof(u64));
	if (!pfn_table) {
		ret = -ENOMEM;
		LOG_ERR("Failed to allocate memory for PFNs table.\n");
		goto err;
	}

	if (copy_from_user(pfn_table, (void __user *)mem->sgt, pfn_num * sizeof(u64))) {
		ret = -EFAULT;
		goto err;
	}

	hash_init(pfn_htbl->tbl);

	/* Calculate unique PFNs using hash table */
	for (i = 0; i < pfn_num; i++) {
		u64 pfn = pfn_table[i];
		u32 hash_index = hash_64(pfn, 10);
		bool found = false;

		/* Iterate over the bucket corresponding to the PFN's hash index */
		hash_for_each_possible(pfn_htbl->tbl, node, hash, hash_index) {
			if (node->pfn == pfn) {
				found = true;
				break;
			}
		}

		if (found)
			continue;

		if (for_cpu)
			dma_sync_single_for_cpu(drv->dev, (ulong)__pfn_to_phys(pfn), PAGE_SIZE, dir);
		else
			dma_sync_single_for_device(drv->dev, (ulong)__pfn_to_phys(pfn), PAGE_SIZE, dir);

		/* Insert the unique PFN into the hash table */
		node = &nodes[i];
		node->pfn = pfn;
		hash_add(pfn_htbl->tbl, &node->hash, hash_index);
	}
err:
	if (pfn_table)
		vfree(pfn_table);
	if (nodes)
		vfree(nodes);
	if (pfn_htbl)
		vfree(pfn_htbl);

	return 0;
}

/**
 * aml_dhp_mem_begin_cpu_access - Begins CPU access for a memory region.
 * @drv: Pointer to the DHP driver structure.
 * @mem: Pointer to the memory descriptor structure.
 * @dir: Direction of data transfer (e.g., DMA_FROM_DEVICE or DMA_TO_DEVICE).
 *
 * This function prepares a memory region for CPU access by performing necessary
 * synchronization. It handles different memory types, including physical addresses,
 * PFNs, and scatter-gather tables. Synchronization ensures consistency of data for CPU reads/writes.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int aml_dhp_mem_begin_cpu_access(struct aml_dhp_drv *drv,
	struct aml_du_mem *mem,
	enum dma_data_direction dir)
{
	int ret = -1;

	LOG_DEBUG("[%u]: %s, memtype:%d, addr:%lx, size:%u, uncached:%d, dirt %d\n",
		drv->uid, __func__, mem->type, get_du_mem_addr(mem),
		mem->size, mem->uncached, dir);

	switch (mem->type) {
	case AML_MEM_TYPE_PFN:
		dma_sync_single_for_cpu(drv->dev, get_du_mem_addr(mem), mem->size, dir);
		break;
	case AML_MEM_TYPE_PHY_ADDR:
		dma_sync_single_for_cpu(drv->dev, mem->addr, mem->size, dir);
		break;
	case AML_MEM_TYPE_SG_TBL:
		/* Get memory size for data unit (DU) */
		ret = __aml_dhp_sgt_sync(drv, mem, dir, true);
		if (ret) {
			LOG_ERR("Failed to memory sync, ret=%d.\n", ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * aml_dhp_mem_end_cpu_access - Ends CPU access for a memory region.
 * @drv: Pointer to the DHP driver structure.
 * @mem: Pointer to the memory descriptor structure.
 * @dir: Direction of data transfer (e.g., DMA_FROM_DEVICE or DMA_TO_DEVICE).
 *
 * This function concludes CPU access for a memory region and prepares the memory
 * for device access by performing the necessary synchronization. It ensures data
 * consistency for device operations after the CPU is done accessing the memory.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int aml_dhp_mem_end_cpu_access(struct aml_dhp_drv *drv,
	struct aml_du_mem *mem,
	enum dma_data_direction dir)
{
	int ret = -1;

	LOG_DEBUG("[%u]: %s, memtype:%d, addr:%lx, size:%u, uncached:%d, dirt %d\n",
		drv->uid, __func__, mem->type, get_du_mem_addr(mem),
		mem->size, mem->uncached, dir);

	switch (mem->type) {
	case AML_MEM_TYPE_PFN:
		dma_sync_single_for_device(drv->dev, get_du_mem_addr(mem), mem->size, dir);
		break;
	case AML_MEM_TYPE_PHY_ADDR:
		dma_sync_single_for_device(drv->dev, mem->addr, mem->size, dir);
		break;
	case AML_MEM_TYPE_SG_TBL:
		/* Get memory size for data unit (DU). */
		ret = __aml_dhp_sgt_sync(drv, mem, dir, false);
		if (ret) {
			LOG_ERR("Failed to memory sync, ret=%d.\n", ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * aml_dhp_mem_sync - Synchronizes memory for CPU or device access based on command.
 * @drv: Pointer to the DHP driver structure.
 * @cmd: Command specifying the type of synchronization (e.g., read or write).
 * @arg: User-space pointer to the ioctl data structure.
 *
 * This function provides a unified interface for synchronizing memory access between
 * CPU and device. It validates input parameters, determines the synchronization direction,
 * and delegates to the appropriate helper function based on the command (e.g., begin or end access).
 *
 * Return: 0 on success or a negative error code on failure.
 */
static long aml_dhp_mem_sync(struct aml_dhp_drv *drv, u32 cmd, ulong arg)
{
	struct aml_dhp_ioctl_data __user *uarg = (void *)arg;
	struct aml_dhp_ioctl_data io;
	struct aml_du_mem *mem = &io.mem;
	enum dma_data_direction direction;
	int ret;

	if (copy_from_user(&io, (void __user *) uarg, sizeof(io)))
		return -EFAULT;

	if (mem->syncflag & ~DHP_MEM_SYNC_VALID_FLAGS_MASK)
		return -EINVAL;

	switch (mem->syncflag & DHP_MEM_SYNC_RW) {
	case DHP_MEM_SYNC_READ:
		direction = DMA_FROM_DEVICE;
		break;
	case DHP_MEM_SYNC_WRITE:
		direction = DMA_TO_DEVICE;
		break;
	case DHP_MEM_SYNC_RW:
		direction = DMA_BIDIRECTIONAL;
		break;
	default:
		return -EINVAL;
	}

	if (io.mem.syncflag & DHP_MEM_SYNC_END)
		ret = aml_dhp_mem_end_cpu_access(drv, mem, direction);
	else
		ret = aml_dhp_mem_begin_cpu_access(drv, mem, direction);

	return ret;
}

/**
 * aml_dhp_ioctl - Handle IOCTL commands for the driver.
 * @file: Pointer to the file structure.
 * @cmd: IOCTL command.
 * @arg: Argument passed from user space.
 *
 * This function processes IOCTL commands for allocating FDs, mapping memory, etc.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static long aml_dhp_ioctl(struct file *file, u32 cmd, ulong arg)
{
	struct aml_dhp_drv *drv = file->private_data;
	int ret = 0;

	switch (cmd) {
	case IOCTL_DHP_GET_FD: {
		ret = du_alloc_fd(drv, arg);
		if (ret)
			LOG_ERR("Failed to allocate fd.\n");
		break;
	}
	case IOCTL_DHP_MMAP: {
		ret = du_mmap(drv, arg);
		if (ret)
			LOG_ERR("Failed to mmap.\n");
		break;
	}
	case IOCTL_DHP_SGT_MAP: {
		ret = du_sgt_mmap(drv, arg);
		if (ret)
			LOG_ERR("Failed to sgt mmap.\n");
		break;
	}
	case IOCTL_DHP_MEM_SYNC: {
		ret = aml_dhp_mem_sync(drv, cmd, arg);
		if (ret)
			LOG_ERR("Failed to memory sync.\n");
		break;
	}
	case IOCTL_DHP_SET_TASK: {
		//TODO
		break;
	}

	default:
		return -EINVAL;
	}

	return ret;
}

/**
 * aml_dhp_poll - Poll function for the DHP driver.
 * @file: Pointer to the file structure.
 * @wait: Pointer to the poll table.
 *
 * This function checks if there are any pending tasks in the driver's task queue.
 * It is used to enable non-blocking I/O for the DHP driver by providing a way to
 * check for task completion using the `poll()` system call.
 *
 * Return: A bitmask indicating if data is available for reading (POLLIN, POLLRDNORM).
 */
static unsigned int aml_dhp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct aml_dhp_drv *drv = file->private_data;

	poll_wait(file, &drv->du_wq, wait);

	mutex_lock(&drv->du_mutex);
	if (drv->du_pending) {
		mask |= POLLIN | POLLRDNORM;
		drv->du_pending = 0;
	}
	mutex_unlock(&drv->du_mutex);

	return mask;
}

/**
 * aml_dhp_mmap - Mmap function for the DHP driver.
 * @file: Pointer to the file structure.
 * @vma: Pointer to the vm_area_struct representing the VMA region.
 *
 * This function currently does not map any memory regions but provides a placeholder
 * for future mmap functionality if required by the DHP driver.
 *
 * Return: 0 (success).
 */
static int aml_dhp_mmap(struct file *file, struct vm_area_struct *vma)
{
	/* Currently, no specific mmap functionality is implemented */
	return 0;
}

/**
 * aml_dhp_open - Open function for the DHP driver.
 * @inode: Pointer to the inode structure.
 * @file: Pointer to the file structure.
 *
 * This function is called when a user-space application opens the DHP device file.
 * It allocates resources for a new DHP driver instance, including task pools, mutexes,
 * and wait queues, and adds the instance to the device's instance list.
 *
 * Return: 0 on success or a negative error code on failure.
 */
static int aml_dhp_open(struct inode *inode, struct file *file)
{
	struct aml_dhp_dev *dev = g_dev;
	struct aml_dhp_drv *drv = file->private_data;
	u8 ver[32] = { 0 };
	int i;

	drv = kzalloc(sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	get_task_comm(drv->user, current);
	mutex_init(&drv->du_mutex);
	INIT_LIST_HEAD(&drv->du_head);
	INIT_LIST_HEAD(&drv->node);
	init_waitqueue_head(&drv->du_wq);
	kref_init(&drv->ref);
	drv->dev = dev->dev;

	INIT_KFIFO(drv->du_free);
	INIT_KFIFO(drv->du_done);
	ver_to_string(DHP_DRV_VER, ver);

	drv->du_pool = vmalloc(DU_SIZE * sizeof(*drv->du_pool));
	if (!drv->du_pool) {
		LOG_ERR("Alloc task pool fail.\n");
		kfree(drv);
		return -ENOMEM;
	}

	for (i = 0 ; i < DU_SIZE ; i++) {
		struct data_unit *du = &drv->du_pool[i];

		du->dev = dev->dev;
		du->priv = drv;
		init_completion(&du->comp);
		mutex_init(&du->lock);
		INIT_LIST_HEAD(&du->attachments);

		kfifo_put(&drv->du_free, du);
	}

	file->private_data = drv;

	mutex_lock(&dev->mutex);
	drv->uid = dev->inst_cnt++;
	list_add(&drv->node, &dev->inst_head);
	mutex_unlock(&dev->mutex);

	LOG_INFO("[%u]: %s-%px open %s(%s) success.\n",
		drv->uid, drv->user, drv, dev_name(g_dev->dev), ver);

	return 0;
}

/**
 * __aml_dhp_release - Release function for the DHP driver.
 * @kref: Pointer to the reference count structure.
 *
 * This function is called when the reference count of the driver instance reaches zero.
 * It frees all resources associated with the DHP driver, including task pools and memory.
 */
void __aml_dhp_release(struct kref *kref)
{
	struct aml_dhp_drv *drv = container_of(kref, struct aml_dhp_drv, ref);

	LOG_INFO("[%u]: %s-%px destroyed.\n",
		drv->uid, drv->user, drv);

	vfree(drv->du_pool);

	kfree(drv);
}

/**
 * aml_dhp_release - Release function for the DHP driver.
 * @inode: Pointer to the inode structure.
 * @file: Pointer to the file structure.
 *
 * This function is called when a user-space application closes the DHP device file.
 * It removes the driver instance from the device's instance list, and if there are
 * no more instances, it ensures that all pending tasks are completed. The driver
 * instance is released when the reference count reaches zero.
 *
 * Return: 0 on success.
 */
static int aml_dhp_release(struct inode *inode, struct file *file)
{
	struct aml_dhp_drv *drv = file->private_data;
	int i;

	LOG_INFO("[%u]: %s-%px release %s success.\n",
		drv->uid, drv->user, drv, dev_name(g_dev->dev));

	mutex_lock(&g_dev->mutex);
	list_del_init(&drv->node);

	if (list_empty(&g_dev->inst_head)) {
		for (i = 0 ; i < DU_SIZE ; i++) {
			struct data_unit *du = &drv->du_pool[i];

			if (!completion_done(&du->comp)) {
				complete_all(&du->comp);
			}
		}
	}
	mutex_unlock(&g_dev->mutex);

	kref_put(&drv->ref, __aml_dhp_release);

	return 0;
}

static ssize_t info_show(KV_CLASS_CONST struct class *class,
			KV_CLASS_ATTR_CONST struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct aml_dhp_drv *drv;
	struct list_head *pos;

	mutex_lock(&g_dev->mutex);

	if (list_empty(&g_dev->inst_head)) {
		pbuf += sprintf(pbuf, "No dhp service.\n");
		goto out;
	}

	list_for_each(pos, &g_dev->inst_head) {
		drv = list_entry(pos, struct aml_dhp_drv, node);
		LOG_INFO("[%u]: Inst:%px.\n",
			drv->uid, drv);
		LOG_INFO("[%u]: DU free:%d, done:%d.\n",
			drv->uid, kfifo_len(&drv->du_free),
			kfifo_len(&drv->du_done));
	}
out:
	mutex_unlock(&g_dev->mutex);

	return pbuf - buf;
}

static ssize_t debug_show(KV_CLASS_CONST struct class *cls,
	KV_CLASS_ATTR_CONST struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", debug);
}

static ssize_t debug_store(KV_CLASS_CONST struct class *cls,
	KV_CLASS_ATTR_CONST struct class_attribute *attr, const char *buf, size_t count)
{
	//struct aml_dhp_drv *drv = NULL;
	//struct list_head *pos;

	if (kstrtoint(buf, 0, &debug) < 0)
		return -EINVAL;

	mutex_lock(&g_dev->mutex);

	if (list_empty(&g_dev->inst_head)) {
		LOG_ERR("No dhp service.\n");
		goto out;
	}

	// TODO
out:
	mutex_unlock(&g_dev->mutex);

	return count;
}


static CLASS_ATTR_RO(info);
static CLASS_ATTR_RW(debug);

static struct attribute *dhp_class_attrs[] = {
	&class_attr_info.attr,
	&class_attr_debug.attr,
	NULL
};

ATTRIBUTE_GROUPS(dhp_class);

static struct class dhp_class = {
	.name = CLASS_NAME,
	.class_groups = dhp_class_groups,
};

#ifdef CONFIG_COMPAT
static long aml_dhp_compat_ioctl(struct file *file, u32 cmd, ulong arg)
{
	return aml_dhp_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif

static struct file_operations aml_dmp_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_dhp_open,
	.release	= aml_dhp_release,
	.unlocked_ioctl	= aml_dhp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= aml_dhp_compat_ioctl,
#endif
	.mmap		= aml_dhp_mmap,
	.poll		= aml_dhp_poll,
};

static int __init aml_dhp_init(void)
{
	struct aml_dhp_dev *dev = NULL;
	int ret = -1;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->dev_no = MKDEV(AML_DHP_MAJOR, 0);

	ret = register_chrdev_region(dev->dev_no, 1, DEVICE_NAME);
	if (ret < 0) {
		LOG_ERR("Can't get major number %d.\n", AML_DHP_MAJOR);
		goto err0;
	}

	cdev_init(&dev->cdev, &aml_dmp_fops);
	dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&dev->cdev, dev->dev_no, 1);
	if (ret) {
		LOG_ERR("Error %d adding cdev fail.\n", ret);
		goto err1;
	}

	ret = class_register(&dhp_class);
	if (ret < 0) {
		LOG_ERR("Failed in creating class.\n");
		goto err2;
	}

	dev->dev = device_create(&dhp_class, NULL,
		dev->dev_no, NULL, DEVICE_NAME);
	if (!dev->dev) {
		LOG_ERR("Create device failed.\n");
		ret = -ENODEV;
		goto err3;
	}

	ret = dma_coerce_mask_and_coherent(dev->dev, DMA_BIT_MASK(64));
	if (ret) {
		LOG_ERR("Set dma mask fail.\n");
		goto err4;
	}

	INIT_LIST_HEAD(&dev->inst_head);
	mutex_init(&dev->mutex);
	dhp_func_reg(__aml_dhp_task);
	g_dev = dev;

	LOG_INFO("DHP driver init success.\n");

	return 0;
err4:
	device_destroy(&dhp_class, dev->dev_no);
err3:
	class_unregister(&dhp_class);
err2:
	cdev_del(&dev->cdev);
err1:
	unregister_chrdev_region(dev->dev_no, 1);
err0:
	kfree(dev);

	return ret;
}

static void __exit aml_dhp_exit(void)
{
	struct aml_dhp_dev *dev = g_dev;

	if (dev) {
		cdev_del(&dev->cdev);
		device_destroy(&dhp_class, dev->dev_no);
		class_unregister(&dhp_class);
		unregister_chrdev_region(dev->dev_no, 1);
		dhp_func_unreg();
		kfree(dev);
	}

	LOG_INFO("DHP driver exit success.\n");
}

module_init(aml_dhp_init);
module_exit(aml_dhp_exit);

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n set debug level \n");

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);

