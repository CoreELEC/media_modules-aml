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
void *dhp_dbuf_mmap(int fd, unsigned int len, int prot, int flags, unsigned int offset);

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
void *dhp_page_mmap(void *priv, unsigned int pfn);

/*
 * dhp_dbuf_munmap() - Unmaps a previously mapped memory region.
 *
 * @vaddr: Pointer to the memory region to unmap.
 * @len: Length of the memory region.
 *
 * This function unmaps a memory region that was previously mapped using `mmap`.
 */
void dhp_dbuf_munmap(void *vaddr, unsigned int len);

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


