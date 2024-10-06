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
 * @sg_tbl  : Scatter-gather table containing the memory scatter list information for DMA operations.
 * @dbuf    : Pointer to the DMA buffer associated with this data unit.
 * @vmah    : VMA handler information, including reference count and functions for memory mapping.
 * @uncached: Indicates if the data unit is using uncached memory mode (true if enabled).
 * @refcnt  : Reference count for managing the lifetime of the DMA buffer.
 * @node    : List node entry for linking this data unit within a list.
 * @comp    : Completion synchronization primitive to signal when the data unit has finished processing.
 * @dev     : Pointer to the GDHP device associated with this data unit, used for device operations.
 * @priv    : Pointer to private context information specific to the GDHP implementation.
 * @pb      : Pointer to the base structure containing additional data unit-related information.
 * @src     : Source information structure containing data to be processed.
 * @dst     : Destination information structure where the processed data will be stored.
 */
struct data_unit {
	struct sg_table		sg_tbl;
	struct dma_buf		*dbuf;
	struct du_vma_hdr	vmah;
	bool			uncached;

	refcount_t		refcnt;
	struct list_head	node;
	struct completion	comp;
	struct device		*dev;
	void			*priv;

	struct aml_du_base	*pb;
};

/*
 * struct aml_dhp_drv - Represents the driver context for the Data Handler Proxy (DHP).
 *
 * @ref         : Reference count for managing the lifetime of the driver instance.
 * @user        : Name of the user task that created this driver instance, stored in a character array.
 * @uid         : Unique identifier for the driver instance, used for tracking.
 * @node        : List node entry for linking this driver instance within a list of DHP instances.
 * @du_head     : Head of the list containing active data units (DUs) associated with this driver.
 * @du_mutex    : Mutex for synchronizing access to the DU list and related operations.
 * @du_wq       : Wait queue for managing pending data unit tasks and signaling completion.
 * @du_pending  : Indicates if there are pending tasks associated with data units.
 * @du_pool     : Pointer to an array of data units (DUs) managed by this driver.
 * @du_free     : FIFO queue for tracking available data units that can be reused.
 * @du_done     : FIFO queue for tracking completed data units.
 */
struct aml_dhp_drv {
	struct kref		ref;
	char			user[TASK_COMM_LEN];
	u32			uid;
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

