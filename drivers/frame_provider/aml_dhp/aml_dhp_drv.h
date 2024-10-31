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
#ifndef _AML_DHP_DRV_H_
#define _AML_DHP_DRV_H_

#include "aml_dhp_if.h"

#define DU_SIZE	(64)

/*
 * struct du_vma_hdr - Handles the state of the dma-buf VMA during mmap operations.
 *
 * @refcnt : Pointer to the reference count, tracking the number of mappings to the dma-buf.
 * @put    : Callback function invoked during unmap to release resources or perform cleanup.
 * @arg    : Argument passed to the put() callback function.
 */
struct du_vma_hdr {
	refcount_t	*refcnt;
	void		(*put)(void *arg);
	void		*arg;
};

/*
 * struct data_unit - Represents a data unit managed by the Data Handler Proxy (DHP).
 *
 * @attachments: List of additional resources or components attached to this data unit,
 *               such as associated metadata or linked resources.
 * @lock       : Mutex to ensure thread-safe access and modification of the data unit's members.
 * @sg_tbl     : Scatter-gather table containing memory descriptors for DMA operations,
 *               providing efficient handling of non-contiguous memory regions.
 * @dbuf       : Pointer to the DMA buffer associated with this data unit,
 *               used for transferring data between the CPU and a device.
 * @vmah       : Virtual Memory Area (VMA) handler containing metadata for memory mappings
 *               and helper functions for managing user-space memory views.
 * @uncached   : Indicates whether the memory associated with this data unit is uncached.
 *               If true, memory access bypasses the CPU cache for coherent operations.
 * @mapped     : Flag indicating whether the data unit has been mapped to a virtual memory
 *               address space for device or user-space access.
 * @refcnt     : Reference counter to manage the lifecycle of the data unit.
 *               Ensures the structure remains valid until all references are released.
 * @node       : List node for linking this data unit into a list,
 *               allowing management of multiple data units as a collection.
 * @comp       : Completion synchronization primitive used to notify waiting threads
 *               when processing of the data unit is complete.
 * @dev        : Pointer to the DHP (The Data Handler Proxy) device associated with this data unit,
 *               used for managing hardware-specific operations and resources.
 * @priv       : Pointer to private context information specific to the DHP implementation,
 *               allowing customization or extension for specific use cases.
 * @pb         : Pointer to the base structure `aml_du_base`, which provides shared information
 *               and common configuration for the data unit.
 * @src        : Source memory or data structure representing the input for processing by this data unit.
 *               Typically includes data addresses, sizes, and flags.
 * @dst        : Destination memory or data structure where the processed output of this data unit
 *               will be stored. Includes similar attributes as the source.
 */
struct data_unit {
	struct list_head	attachments;
	struct mutex		lock;
	struct sg_table		sg_tbl;
	struct dma_buf		*dbuf;
	struct du_vma_hdr	vmah;
	bool			uncached;
	bool			mapped;

	refcount_t		refcnt;
	struct list_head	node;
	struct completion	comp;
	struct device		*dev;
	void			*priv;

	struct aml_du_base	*pb;
};

/*
 * struct aml_dhp_drv - Represents the driver context for The Data Handler Proxy (DHP).
 *
 * @ref         : Reference count for managing the lifetime of the driver instance.
 *                Ensures the driver context remains valid while in use.
 * @user        : Name of the user task that created this driver instance, stored in a character array.
 *                Useful for debugging and tracking the task that initiated the driver.
 * @uid         : Unique identifier for the driver instance, used for tracking and distinguishing
 *                between multiple instances of the driver.
 * @dev         : Pointer to the underlying device structure associated with this driver instance.
 *                Represents the hardware context managed by the driver.
 * @node        : List node entry for linking this driver instance within a global or subsystem-specific
 *                list of DHP driver instances.
 * @du_head     : Head of the list containing active data units (DUs) associated with this driver.
 *                Manages the lifecycle and operations on active DUs.
 * @du_mutex    : Mutex for synchronizing access to the DU list and related operations.
 *                Ensures thread-safe management of data units.
 * @du_wq       : Wait queue for managing pending data unit tasks and signaling when operations complete.
 *                Supports efficient task coordination.
 * @du_pending  : Indicates if there are pending tasks associated with data units.
 *                Used as a counter or flag to track the number of unfinished tasks.
 * @du_pool     : Pointer to an array of preallocated data units (DUs) managed by this driver.
 *                Acts as a resource pool to minimize dynamic memory allocations during runtime.
 * @du_free     : FIFO queue for tracking available data units that can be reused.
 *                Optimizes resource usage by recycling completed DUs.
 * @du_done     : FIFO queue for tracking completed data units.
 *                Allows the driver to efficiently handle post-processing or release operations.
 */
struct aml_dhp_drv {
	struct kref		ref;
	char			user[TASK_COMM_LEN];
	u32			uid;
	struct device		*dev;
	struct list_head	node;
	struct list_head	du_head;
	struct mutex		du_mutex;
	wait_queue_head_t	du_wq;
	int			du_pending;

	/* DU pool info. */
	struct data_unit	*du_pool;
	DECLARE_KFIFO(du_free, struct data_unit *, DU_SIZE);
	DECLARE_KFIFO(du_done, struct data_unit *, DU_SIZE);
};

#endif /* _AML_DHP_DRV_H_ */

