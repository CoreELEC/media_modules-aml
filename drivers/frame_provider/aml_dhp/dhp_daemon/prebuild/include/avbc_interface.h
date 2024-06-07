/*
 * Copyright (c) 2016 Amlogic, Inc. All rights reserved.
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 * Description:
 */

#ifndef __AML_AVBC_DEC_IF_H__
#define __AML_AVBC_DEC_IF_H__

/*
 * Memory synchronization flags for the Data Handler Proxy (DHP) driver:
 *
 * - DHP_MEM_SYNC_READ: Synchronize memory for reading (device-to-CPU), ensuring
 *   data in device memory is visible to the CPU.
 *
 * - DHP_MEM_SYNC_WRITE: Synchronize memory for writing (CPU-to-device), ensuring
 *   data in CPU memory is visible to the device.
 *
 * - DHP_MEM_SYNC_RW: Combines both read and write synchronization, allowing
 *   bi-directional cache coherency.
 *
 * - DHP_MEM_SYNC_START: Indicates synchronization at the start of memory usage,
 *   preparing memory for operations.
 *
 * - DHP_MEM_SYNC_END: Indicates synchronization at the end of memory usage, finalizing
 *   memory operations and ensuring data integrity.
 *
 * - DHP_MEM_SYNC_VALID_FLAGS_MASK: Defines the valid combination of flags for memory
 *   synchronization operations, restricting flags to only supported values.
 */
#define DHP_MEM_SYNC_READ	(1 << 0)
#define DHP_MEM_SYNC_WRITE	(2 << 0)
#define DHP_MEM_SYNC_RW		(DHP_MEM_SYNC_READ | DHP_MEM_SYNC_WRITE)
#define DHP_MEM_SYNC_START	(0 << 2)
#define DHP_MEM_SYNC_END	(1 << 2)
#define DHP_MEM_SYNC_VALID_FLAGS_MASK \
    (DHP_MEM_SYNC_RW | DHP_MEM_SYNC_END)

/**
 * @brief Memory operations for handling device memory in the Data Handler Proxy (DHP).
 *
 * This structure defines the memory operations used for mapping, synchronizing,
 * and unmapping device memory in the context of the Data Handler Proxy (DHP).
 * These operations allow for flexible handling of memory for devices, enabling
 * functions such as memory mapping, synchronization, and unmapping in both
 * user space and device space.
 *
 * The structure provides the following operations:
 * - `mmap`: Maps a memory region into the address space.
 * - `msync`: Synchronizes memory between the device and CPU, ensuring data consistency.
 * - `sgt_mmap`: Maps a scatter-gather table (SGT) of memory regions.
 * - `sgt_msync`: Synchronizes a scatter-gather table of memory regions.
 * - `unmmap`: Unmaps a previously mapped memory region.
 */
typedef struct dhp_mem_ops {
    /**
     * @brief Memory mapping function.
     *
     * This function maps a memory region into the process's virtual address space.
     *
     * @param priv Pointer to private data, typically device-specific context.
     * @param addr Address to be mapped.
     * @param size Size of the memory region to be mapped.
     * @param uncached Flag to indicate whether the memory should be uncached.
     *
     * @return Pointer to the mapped memory region, or NULL on failure.
     */
    void *(*mmap)(void *priv, u64 addr, u32 size, u32 uncached);

    /**
     * @brief Memory synchronization function.
     *
     * This function ensures that the specified memory region is synchronized
     * between the device and CPU, making sure that data is consistent.
     *
     * @param priv Pointer to private data, typically device-specific context.
     * @param addr Address of the memory region to synchronize.
     * @param size Size of the memory region to synchronize.
     * @param flags Flags specifying the type of synchronization (e.g., read, write).
     */
    void (*msync)(void *priv, u64 addr, u32 size, u32 flags);

    /**
     * @brief Scatter-gather table memory mapping function.
     *
     * This function maps a set of memory regions specified by scatter-gather tables
     * into the process's address space.
     *
     * @param priv Pointer to private data, typically device-specific context.
     * @param uptr_array Array of user-space addresses to be mapped.
     * @param pfn_array Array of physical frame numbers corresponding to the memory.
     * @param num Number of memory regions in the scatter-gather table.
     * @param uncached Flag to indicate whether the memory should be uncached.
     *
     * @return The actual valid length of memory successfully mapped to user-space on success,
     *         or a negative error code on failure.
     */
    int (*sgt_mmap)(void *priv, u64 *uptr_array, u64 *pfn_array, u32 num, u32 uncached);

    /**
     * @brief Scatter-gather table memory synchronization function.
     *
     * This function synchronizes a set of memory regions specified by scatter-gather
     * tables, ensuring data consistency between the CPU and device.
     *
     * @param priv Pointer to private data, typically device-specific context.
     * @param pfn_array Array of physical frame numbers corresponding to the memory.
     * @param num Number of memory regions to synchronize.
     * @param flags Flags specifying the type of synchronization (e.g., read, write).
     */
    void (*sgt_msync)(void *priv, u64 *pfn_array, u32 num, u32 flags);

    /**
     * @brief Unmapping function.
     *
     * This function unmaps a previously mapped memory region, releasing the
     * resources associated with it.
     *
     * @param priv Pointer to private data, typically device-specific context.
     * @param uptr Pointer to the user-space address of the memory to unmap.
     * @param size Size of the memory region to unmap.
     */
    void (*unmmap)(void *priv, u8 *uptr, u32 size);
} DhpMemOps;


/**
 * @brief Decodes a YUV frame using the AVBC decoder.
 *
 * This function performs decoding of YUV frames from the provided header,
 * width, height, and other parameters. It processes the input data and
 * stores the decoded frame in the provided destination buffer.
 *
 * The function leverages memory operations (mapping and synchronization)
 * through the `DhpMemOps` structure, which allows for flexible handling
 * of memory and device resources. This is useful when dealing with large
 * video frame buffers or when optimizations related to uncached memory
 * are required.
 *
 * @param header Pointer to the AVBC frame header, containing information
 *               about the encoded frame and its format.
 * @param width Width of the frame in pixels.
 * @param height Height of the frame in pixels.
 * @param wstride Stride of the frame (bytes per row), used to calculate
 *               the memory layout.
 * @param hstride Stride of the frame (bytes per vertical column), used to
 * 		 calculate the memory layout.
 * @param bitdepth Bit depth of the YUV frame (e.g., 8, 10 bits per channel).
 * @param dst_yuv Pointer to the destination buffer where the decoded YUV
 *                frame will be stored. The buffer must be large enough
 *                to hold the entire decoded frame.
 * @param dst_size Size of the destination buffer in bytes. This should
 *                 be at least the size required for the decoded frame.
 * @param uncached Flag to specify whether the memory should be uncached.
 *                 This is typically set to 1 for performance-critical
 *                 operations or when interacting with certain hardware devices.
 * @param mOps Pointer to a `DhpMemOps` structure that defines the memory
 *             operations (e.g., memory mapping, synchronization) to be used
 *             for the decoding process. This allows the decoder to handle
 *             memory efficiently across different platforms and configurations.
 * @param priv Pointer to private data that may be needed for the memory
 *             operations. This could include device-specific context or
 *             state required by the memory operations functions.
 *
 * @return Returns 0 on success, indicating the frame was successfully decoded
 *         and stored in the destination buffer. Returns -1 on failure,
 *         indicating an error during decoding.
 */
int aml_avbc_decode(void *header,
                    u32 width,
                    u32 height,
                    u32 wstride,
                    u32 hstride,
                    u32 bitdepth,
                    u8 *dst_yuv,
                    u32 dst_size,
                    u32 uncached,
                    DhpMemOps *mOps,
                    void *priv);

/**
 * @example test_avbc_decode.c
 * @brief Sample test code to use the aml_avbc_decode function.
 *
 * This file demonstrates how to use the aml_avbc_decode function in a test
 * scenario. It sets up the necessary input parameters, performs decoding,
 * and handles memory mapping and clean-up.
 *
 * @code
 * #include "aml_avbc_decoder.h"
 * #include <stdio.h>
 *
 * int main() {
 *     // Sample parameters
 *     u32 width = 1920;
 *     u32 height = 1080;
 *     u32 stride = width;  // assuming 8-bit YUV
 *     u32 bitdepth = 10;
 *     u8 *dst_yuv;
 *     u32 dst_size = stride * height * 3 / 2;
 *     void *header = get_avbc_header();  // hypothetical function to get the header
 *     void *priv = get_priv_context();
 *     u32 uncached = get_uncached_mode();
 *
 *     // Memory operation function (example)
 *     DhpMemOps mOps = { .sgt_mmap = dhp_mem_sgt_mmap,
 *                        .sgt_msync = dhp_mem_sgt_sync,
 *                        .unmmap = dhp_mem_munmap };
 *     // Decode the frame
 *     int ret = aml_avbc_decode(header,
                                width,
                                height,
                                stride,
                                bitdepth,
                                dst_yuv,
                                dst_size,
                                uncached,
                                &mOps,
                                priv);
 *     if (ret == 0) {
 *         printf("Decoding successful!\n");
 *     } else {
 *         printf("Decoding failed!\n");
 *     }
 *
 *     return 0;
 * }
 * @endcode
 */

#endif //__AML_AVBC_DEC_IF_H__

