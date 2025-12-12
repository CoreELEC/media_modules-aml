/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _SEC_EXTRACTOR_H_
#define _SEC_EXTRACTOR_H_

#define EXTRACTOR_CONFIG(x, y) ((x) & (EXTRACTOR_CONFIG_ ## y))
#define EXTRACTOR_CONFIG_VAL(x, y) \
	(((x) & (EXTRACTOR_CONFIG_ ## y)) >> (EXTRACTOR_CONFIG_ ## y ## _SHIFT))

#define EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE_MASK		0x07
#define EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE_SHIFT		2

enum {
	EXTRACTOR_CONFIG_MAP_REG_DST_SHARED_MEM = 1 << 0,
	EXTRACTOR_CONFIG_ALLOC_REG_DST_SHARED_MEM = 1 << 1,
	EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE = \
	EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE_MASK << EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE_SHIFT,
};

/* Combined mask for all valid configuration bits */
#define EXTRACTOR_CFG_MASK \
	(EXTRACTOR_CONFIG_MAP_REG_DST_SHARED_MEM | \
	 EXTRACTOR_CONFIG_ALLOC_REG_DST_SHARED_MEM | \
	 EXTRACTOR_CONFIG_DST_SHARED_MEM_SIZE)

enum {
	EXTRACTOR_DBG_COMMON =		1 << 0,
	EXTRACTOR_DBG_TIME_COST =	1 << 1,
	EXTRACTOR_DBG_PARAM =		1 << 2,
};

/**
 * struct sec_downscale_config - Configuration for downscale operation
 *
 * This structure contains all parameters needed for a secure downscale operation.
 * The operation is performed by the Trusted Application in secure world using
 * hardware-accelerated GE2D engine.
 *
 * @downscale_param: Pointer to GE2D downscale parameters (source/dest dimensions,
 *	format, etc.). Must be valid for the duration of the call.
 * @downscale_param_size: Size of the downscale_param structure in bytes.
 * @dst_buf_phy_addr: Physical address of the destination buffer where the
 *	downscaled output will be written. Must be contiguous and accessible.
 * @dst_buf_size: Total size of the destination buffer in bytes. Must be large
 *	enough to hold the downscaled output.
 * @version: Structure version for forward compatibility. Set to 0 for current version.
 * @flags: Operation control flags (reserved for future use).
 * @reserved: Reserved for future expansion. Must be zeroed.
 */
struct sec_downscale_config {
	void *downscale_param;
	u32 downscale_param_size;
	u64 dst_buf_phy_addr;
	u32 dst_buf_size;

	u32 version;
	u32 flags;
	u64 reserved[4];
};

/**
 * sec_extractor_open() - Initialize and open the TEE session
 *
 * This function must be called before any other sec_extractor operations.
 * It performs the following:
 * - Opens a TEE context
 * - Opens a session with the Trusted Application
 * - Allocates shared memory for parameter exchange
 * - Allocates or registers destination shared memory based on configuration
 *
 * The function is idempotent - calling it multiple times without an intervening
 * sec_extractor_close() will succeed on subsequent calls.
 *
 * Return: 0 on success, negative error code on failure:
 *         -EACCES: TEE context/session open failed
 *         -ENOMEM: Memory allocation failed
 *         -EINVAL: Invalid configuration
 *         -ENODEV: OP-TEE not enabled in kernel
 */
int sec_extractor_open(void);

/**
 * sec_extractor_close() - Deinitialize and release the TEE session
 *
 * This function cleans up all resources allocated by sec_extractor_open():
 * - Frees shared memory buffers
 * - Closes the TA session
 * - Closes the TEE context
 *
 * The function is idempotent - calling it multiple times is safe.
 *
 * Return: 0 on success, negative error code on failure:
 *         -ENODEV: OP-TEE not enabled in kernel
 */
int sec_extractor_close(void);

/**
 * sec_extractor_downscale() - Perform secure video buffer downscaling
 * @cfg: Pointer to configuration structure containing operation parameters
 *
 * This function invokes the Trusted Application to perform hardware-accelerated
 * downscaling using the secure GE2D engine. The source image data is assumed
 * to be already configured in the secure world through previous TA calls.
 *
 * The destination buffer handling depends on the configuration flags:
 * - MAP_REG_DST_SHARED_MEM: User buffer is registered for direct TEE access
 * - ALLOC_REG_DST_SHARED_MEM: Pre-allocated CMA buffer is used, result copied back
 * - Default: TEE-allocated buffer is used, result copied back
 *
 * Context: Must be called after sec_extractor_open(). Caller must ensure
 *          @cfg remains valid for the duration of the call.
 *
 * Return: 0 on success, negative error code on failure:
 *         -EINVAL: Invalid parameters or TEE not initialized
 *         -ENOMEM: Memory mapping/registration failed
 *         -EBUSY: Shared memory access failed
 *         -EIO: TEE command execution failed
 *         -ENODEV: OP-TEE not enabled in kernel
 */
int sec_extractor_downscale(struct sec_downscale_config *cfg);

#endif /* _SEC_EXTRACTOR_H_ */
