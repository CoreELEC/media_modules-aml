/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef __AML_DHP_DAEMON_H_
#define __AML_DHP_DAEMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#include "aml_dhp_common.h"
#include "../aml_dhp_if.h"

#define DEVICE_FILE "/dev/aml_dhp_dev"

/*
 * struct Device - Represents a device to be managed by the DHP system.
 *
 * @fd:   File descriptor for the device, used for I/O operations.
 * @pfd:  Polling structure for monitoring device events using poll().
 */
typedef struct {
    int             fd;
    struct pollfd   pfd;
} Device;

/*
 * struct DhpManager - Manages the overall DHP system, including device and thread pool.
 *
 * @pool: Pointer to the ThreadPool structure, used for task scheduling and execution.
 * @dev:  Pointer to the Device structure, representing the device being managed.
 */
typedef struct {
    ThreadPool      *pool;
    Device          *dev;
} DhpManager;

/*
 * dhp_dbuf_mmap() - Maps a file descriptor to memory.
 *
 * @fd: File descriptor of the device or file to be mapped.
 * @len: Length of the memory region to map.
 * @prot: Memory protection flags (e.g., PROT_READ, PROT_WRITE).
 * @flags: Mapping flags (e.g., MAP_SHARED, MAP_PRIVATE).
 * @offset: Offset within the file descriptor to start mapping from.
 *
 * This function maps the memory associated with the file descriptor into the
 * process's address space. If the mapping fails, it returns NULL.
 *
 * Return: Pointer to the mapped memory on success, or NULL on failure.
 */
void *dhp_dbuf_mmap(int fd, u32 len, int prot, int flags, u32 offset);

/*
 * dhp_dbuf_munmap() - Unmaps a previously mapped memory region.
 *
 * @vaddr: Pointer to the memory region to unmap.
 * @len: Length of the memory region.
 *
 * This function unmaps a memory region that was previously mapped using `mmap`.
 * It removes the mapping between the virtual address and the physical memory,
 * making the region no longer accessible by the process.
 */
void dhp_dbuf_munmap(void *vaddr, u32 len);

/*
 * dhp_dbuf_sync() - Synchronizes the memory buffer with the device or CPU.
 *
 * @fd: File descriptor associated with the memory buffer.
 * @flags: Flags indicating the synchronization type (e.g., read/write).
 *
 * This function ensures that the memory buffer associated with the file descriptor
 * is properly synchronized, ensuring that changes are visible to the device or CPU.
 */
void dhp_dbuf_sync(int fd, u32 flags);

/*
 * dhp_page_mmap() - Maps a physical frame number (PFN) into virtual memory.
 *
 * @priv: Pointer to the Device structure.
 * @pfn: Physical frame number to be mapped.
 *
 * This function maps a page identified by its physical frame number (PFN) into the
 * process's virtual address space using an IOCTL call. It logs an error if the IOCTL
 * operation fails.
 *
 * Return: Pointer to the mapped memory on success, or NULL on failure.
 */
void *dhp_page_mmap(void *priv, u32 pfn, u32 uncached);

/*
 * dhp_mem_mmap() - Maps a memory region into virtual memory.
 *
 * @priv: Pointer to the Device structure.
 * @addr: Starting address of the memory region to map.
 * @size: Size of the memory region to map.
 * @uncached: Flag indicating whether the memory should be mapped as uncached.
 *
 * This function maps the specified memory region into the process's virtual address
 * space. The mapping can optionally be made uncached based on the `uncached` flag.
 *
 * Return: Pointer to the mapped memory on success, or NULL on failure.
 */
void *dhp_mem_mmap(void *priv, u64 addr, u32 size, u32 uncached);

/*
 * dhp_mem_sync() - Synchronizes a memory region between device and CPU.
 *
 * @priv: Pointer to the Device structure.
 * @addr: Starting address of the memory region to synchronize.
 * @size: Size of the memory region to synchronize.
 * @flags: Flags indicating the synchronization type (e.g., read/write).
 *
 * This function ensures memory consistency between the device and the CPU for the
 * specified memory region. The `flags` parameter determines whether the memory
 * should be synchronized for reading, writing, or both.
 */
void dhp_mem_sync(void *priv, u64 addr, u32 size, u32 flags);

/*
 * dhp_mem_sgt_mmap() - Maps a list of physical frame numbers (PFNs) into virtual memory.
 *
 * @priv: Pointer to the Device structure.
 * @uptr_array: Array of user-space pointers to map.
 * @pfn_array: Array of physical frame numbers to map.
 * @num: Number of entries in the `uptr_array` and `pfn_array`.
 * @uncached: Flag indicating whether the memory should be mapped as uncached.
 *
 * This function maps a list of physical frame numbers (PFNs) into the process's
 * virtual address space using an array of user-space pointers and physical
 * frame numbers. The mapping can be made uncached based on the `uncached` flag.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int dhp_mem_sgt_mmap(void *priv, u64 *uptr_array, u64 *pfn_array, u32 num, u32 uncached);

/*
 * dhp_mem_sgt_sync() - Synchronizes a list of memory regions between device and CPU.
 *
 * @priv: Pointer to the Device structure.
 * @pfn_array: Array of physical frame numbers to synchronize.
 * @num: Number of entries in the `pfn_array`.
 * @flags: Flags indicating the synchronization type (e.g., read/write).
 *
 * This function ensures memory consistency between the device and the CPU for a
 * list of memory regions specified by the `pfn_array`. The `flags` parameter
 * determines whether the memory should be synchronized for reading, writing, or both.
 */
void dhp_mem_sgt_sync(void *priv, u64 *pfn_array, u32 num, u32 flags);

/*
 * dhp_mem_munmap() - Unmaps a previously mapped memory region.
 *
 * @priv: Pointer to the Device structure.
 * @vaddr: Pointer to the memory region to unmap.
 * @len: Length of the memory region to unmap.
 *
 * This function unmaps a previously mapped memory region, removing the mapping
 * between the virtual address and the physical memory.
 */
void dhp_mem_munmap(void *priv, u8 *vaddr, u32 len);

/*
 * dhp_dev_ioctl() - Performs an IOCTL operation on the device.
 *
 * @dev: Pointer to the Device structure.
 * @request: The IOCTL request code.
 * @arg: Pointer to the argument passed to the IOCTL.
 *
 * This function checks if the device is initialized and valid, then performs an
 * IOCTL operation on the device. It logs errors if the device is not valid or if
 * the IOCTL operation fails.
 *
 * Return: 0 on success, or -1 on failure.
 */
int dhp_dev_ioctl(Device *dev, int request, void *arg);

#endif //__AML_DHP_DAEMON_H_


