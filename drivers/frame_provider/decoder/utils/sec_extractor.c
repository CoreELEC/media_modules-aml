// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/ktime.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
#include <linux/amlogic/tee.h>
#include <linux/amlogic/tee_drv.h>
#endif

#include "sec_extractor.h"

#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
/* Module configuration parameters */
static int sec_extractor_config = 1;
/* Debug mask (0=off, 1=common, 2=timing, 4=params) */
static int sec_extractor_debug;

#define extractor_dbg_flag(_f, fmt, ...)\
	do {\
		if (sec_extractor_debug & (_f))\
			pr_info("%s:%d " fmt "\n",\
				__func__, __LINE__, ##__VA_ARGS__);\
	} while (0)

#define extractor_dbg(fmt, ...)\
	extractor_dbg_flag(EXTRACTOR_DBG_COMMON, fmt, ##__VA_ARGS__)
#define extractor_dbg_time(fmt, ...)\
	extractor_dbg_flag(EXTRACTOR_DBG_TIME_COST, fmt, ##__VA_ARGS__)
#define extractor_dbg_param(fmt, ...)\
	extractor_dbg_flag(EXTRACTOR_DBG_PARAM, fmt, ##__VA_ARGS__)

#define EXTRACTOR_CHECK_STATUS(cond, errcode, label, fmt, ...)\
	do {\
		if (cond) {\
			ret = (errcode);\
			pr_err("%s:%d " fmt "\n",\
				__func__, __LINE__, ##__VA_ARGS__);\
			goto label;\
		}\
	} while (0)

#define	EXTRACTOR_TA_UUID UUID_INIT(0x7f193930, 0xdbf5, 0x48ff, \
	0x87, 0x81, 0x0a, 0xa7, 0x73, 0xe8, 0xd9, 0xed)
#define	TEE_CMD_PARAM_NUM	4
#define	EXTRACTOR_PARAM_SHARED_MEM_SIZE	(4 * 1024)
#define	EXTRACTOR_SHARED_MEM_SIZE	(1 * 1024 * 1024)
#define	EXTRACTOR_CMD_DOWNSCALE	1

/* Global mutex to protect TEE operations */
static DEFINE_MUTEX(g_ca_mutex);

/* Global TEE context and session management */
static int g_teec_inited;
static struct tee_context *g_teec_context;
static struct tee_ioctl_open_session_arg g_teec_session;

/**
 * struct sec_extractor_shm - Shared memory information structure
 * @param_shm: Parameter shared memory handle
 * @dst_shm: Destination shared memory handle
 * @dst_paddr: Physical address of CMA buffer
 * @dst_vaddr: Virtual address of CMA buffer
 * @dst_size: Size of destination shared memory
 */
struct sec_extractor_shm {
	struct tee_shm *param_shm;
	struct tee_shm *dst_shm;
	ulong dst_paddr;
	u8 *dst_vaddr;
	u32 dst_size;
};

/* Global shared memory information */
static struct sec_extractor_shm g_extractor_shm = {
	.dst_size = EXTRACTOR_SHARED_MEM_SIZE,
};

static int extractor_tee_ctx_match(struct tee_ioctl_version_data *ver,
	const void *data)
{
	(void)data;	/* Unused parameter */
	return (ver && ver->impl_id == TEE_IMPL_ID_OPTEE);
}
/**
 * sec_extractor_open_ta_session - Open TEE context and session with TA
 * @context: Pointer to TEE context handle (output parameter)
 * @session: Pointer to TEE session structure (output parameter)
 *
 * Return: 0 on success, negative error code on failure
 */
static int sec_extractor_open_ta_session(struct tee_context **context,
		struct tee_ioctl_open_session_arg *session)
{
	int ret;
	uuid_t uuid = EXTRACTOR_TA_UUID;
	u64 start = ktime_get_ns();

	/* Validate parameters */
	EXTRACTOR_CHECK_STATUS(!context || !session, -EINVAL, error_exit,
		"Invalid parameters: context=%p, session=%p", context, session);

	/* Open TEE context */
	*context = tee_client_open_context(NULL, extractor_tee_ctx_match, NULL, NULL);
	EXTRACTOR_CHECK_STATUS(IS_ERR(*context), -EACCES, error_exit,
		"Failed to open TEE context: %ld", PTR_ERR(*context));

	/* Prepare session parameters */
	memcpy(session->uuid, uuid.b, TEE_IOCTL_UUID_LEN);
	session->clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	session->num_params = 0;

	/* Open session with Trusted Application */
	ret = tee_client_open_session(*context, session, NULL);
	EXTRACTOR_CHECK_STATUS(ret < 0 || session->ret != TEEC_SUCCESS, -EACCES, error_exit,
		"Failed to open TA session: ret=%d, ta_ret=0x%x, origin=0x%x",
		ret, session->ret, session->ret_origin);

	extractor_dbg_time("TA session opened in %llu us", (ktime_get_ns() - start) / 1000);
	return 0;

error_exit:
	if (context && *context) {
		tee_client_close_context(*context);
		*context = NULL;
	}
	return ret;
}

/**
 * sec_extractor_close_ta_session - Close TEE session and context
 * @context: TEE context handle to close
 * @session: TEE session structure containing session ID
 */
static void sec_extractor_close_ta_session(struct tee_context *context,
		struct tee_ioctl_open_session_arg *session)
{
	if (!context)
		return;

	if (!session) {
		pr_warn("NULL session, closing context only\n");
		tee_client_close_context(context);
		return;
	}

	tee_client_close_session(context, session->session);
	tee_client_close_context(context);
	extractor_dbg("TA session closed");
}

/**
 * sec_extractor_alloc_shm - Allocate shared memory (param and dst)
 * @context: TEE context handle
 * @config: Configuration flags
 * @shm: Pointer to SHM structure (output parameter)
 *
 * Return: 0 on success, negative error code on failure
 */
static int sec_extractor_alloc_shm(struct tee_context *context, u32 config,
		struct sec_extractor_shm *shm)
{
	int ret;
	u32 shm_size_MB;
	u32 dma_buf_size;
	u32 dma_buf_pages;
	int dma_buf_flags;

	/* Validate parameters */
	EXTRACTOR_CHECK_STATUS(!context || !shm, -EINVAL, error_exit,
		"Invalid parameters: context=%p, shm=%p", context, shm);

	/* Initialize SHM structure */
	shm->param_shm = NULL;
	shm->dst_shm = NULL;
	shm->dst_paddr = 0;
	shm->dst_vaddr = NULL;

	/* Allocate parameter shared memory */
	shm->param_shm = tee_shm_alloc_kernel_buf(context, EXTRACTOR_PARAM_SHARED_MEM_SIZE);
	EXTRACTOR_CHECK_STATUS(IS_ERR(shm->param_shm), -ENOMEM, error_exit,
		"Failed to allocate param SHM (%u bytes): %ld",
		EXTRACTOR_PARAM_SHARED_MEM_SIZE, PTR_ERR(shm->param_shm));
	extractor_dbg("Param SHM allocated: %zu bytes", shm->param_shm->size);

	/* Setup destination shared memory based on configuration */
	if (EXTRACTOR_CONFIG(config, MAP_REG_DST_SHARED_MEM)) {
		extractor_dbg("Skip dst SHM allocation (MAP_REG mode)");
		return 0;
	}

	/* Extract configured size from config bits */
	shm_size_MB = EXTRACTOR_CONFIG_VAL(config, DST_SHARED_MEM_SIZE);
	if (shm_size_MB != 0) {
		shm->dst_size = shm_size_MB * 1024 * 1024;
		extractor_dbg("Configured SHM size: %u MB", shm_size_MB);
	}

	/* Allocate from CMA and register with TEE */
	dma_buf_size = PAGE_ALIGN(shm->dst_size);
	dma_buf_pages = dma_buf_size / PAGE_SIZE;
	dma_buf_flags = CODEC_MM_FLAGS_CMA | CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER;

	shm->dst_paddr = codec_mm_alloc_for_dma("extractor_shm", dma_buf_pages, 0, dma_buf_flags);
	EXTRACTOR_CHECK_STATUS(!shm->dst_paddr, -ENOMEM, free_param_shm,
		"Failed to allocate CMA buffer (%u pages)", dma_buf_pages);

	shm->dst_vaddr = codec_mm_vmap_noncache(shm->dst_paddr, dma_buf_size);
	EXTRACTOR_CHECK_STATUS(!shm->dst_vaddr, -ENOMEM, free_cma,
		"Failed to map CMA buffer (paddr=0x%lx)", shm->dst_paddr);

	shm->dst_shm = tee_shm_register_kernel_buf(context, (void *)shm->dst_vaddr, dma_buf_size);
	EXTRACTOR_CHECK_STATUS(IS_ERR(shm->dst_shm) || shm->dst_shm == NULL, -ENOMEM, free_cma,
		"Failed to register CMA buffer with TEE");

	extractor_dbg("Registered CMA buffer: paddr=0x%lx, vaddr=%p, size=%zu",
		shm->dst_paddr, shm->dst_vaddr, shm->dst_shm->size);

	return 0;

free_cma:
	if (shm->dst_vaddr) {
		codec_mm_unmap_phyaddr(shm->dst_vaddr);
		shm->dst_vaddr = NULL;
	}

	if (shm->dst_paddr) {
		codec_mm_free_for_dma("sec_extractor_shm", shm->dst_paddr);
		shm->dst_paddr = 0;
	}
free_param_shm:
	if (shm->param_shm) {
		tee_shm_free(shm->param_shm);
		shm->param_shm = NULL;
	}
error_exit:
	return ret;
}

/**
 * sec_extractor_free_shm - Free shared memory (param and dst)
 * @shm: Pointer to SHM structure
 */
static void sec_extractor_free_shm(struct sec_extractor_shm *shm)
{
	if (!shm)
		return;

	if (shm->dst_shm) {
		tee_shm_free(shm->dst_shm);
		shm->dst_shm = NULL;
	}

	if (shm->dst_vaddr) {
		codec_mm_unmap_phyaddr(shm->dst_vaddr);
		shm->dst_vaddr = NULL;
	}

	if (shm->dst_paddr) {
		codec_mm_free_for_dma("sec_extractor_shm", shm->dst_paddr);
		shm->dst_paddr = 0;
	}

	if (shm->param_shm) {
		tee_shm_free(shm->param_shm);
		shm->param_shm = NULL;
	}
}
#endif

int sec_extractor_open(void)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	int ret;
	u64 start = ktime_get_ns();

	mutex_lock(&g_ca_mutex);
	extractor_dbg("config: 0x%x", sec_extractor_config);

	if (g_teec_inited) {
		mutex_unlock(&g_ca_mutex);
		return 0;
	}

	ret = sec_extractor_open_ta_session(&g_teec_context, &g_teec_session);
	if (ret)
		goto error_exit;

	ret = sec_extractor_alloc_shm(g_teec_context, sec_extractor_config,
		&g_extractor_shm);
	if (ret)
		goto error_close_ta;

	g_teec_inited = 1;

	extractor_dbg_time("Total open time: %llu us", (ktime_get_ns() - start) / 1000);
	mutex_unlock(&g_ca_mutex);
	return 0;

error_close_ta:
	sec_extractor_close_ta_session(g_teec_context, &g_teec_session);
	g_teec_context = NULL;
error_exit:
	mutex_unlock(&g_ca_mutex);
	return ret;
#else
	pr_err("Not support sec extractor\n");
	return -ENODEV;
#endif
}
EXPORT_SYMBOL(sec_extractor_open);

int sec_extractor_close(void)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	u64 start = ktime_get_ns();

	mutex_lock(&g_ca_mutex);

	sec_extractor_free_shm(&g_extractor_shm);
	sec_extractor_close_ta_session(g_teec_context, &g_teec_session);
	g_teec_context = NULL;
	g_teec_inited = 0;

	extractor_dbg_time("Total close time: %llu us", (ktime_get_ns() - start) / 1000);
	mutex_unlock(&g_ca_mutex);
	return 0;
#else
	return -ENODEV;
#endif
}
EXPORT_SYMBOL(sec_extractor_close);

int sec_extractor_downscale(struct sec_downscale_config *cfg)
{
#if IS_ENABLED(CONFIG_AMLOGIC_OPTEE)
	int ret;
	char *param_shm_data = NULL, *shm_data = NULL;
	struct tee_param param[TEE_CMD_PARAM_NUM] = { 0 };
	struct tee_ioctl_invoke_arg inv_arg = { 0 };
	struct tee_shm *reg_dst_shm = NULL;
	u8 *dst_buf = NULL;
	u8 use_reg_dst_shm = 0;
	u64 start = ktime_get_ns();

	mutex_lock(&g_ca_mutex);

	/* Validate input parameters */
	EXTRACTOR_CHECK_STATUS(!cfg, -EINVAL, error_exit, "NULL config pointer");
	EXTRACTOR_CHECK_STATUS(!cfg->downscale_param || cfg->downscale_param_size == 0,
		-EINVAL, error_exit, "Invalid downscale_param: ptr=%p, size=%u",
		cfg->downscale_param, cfg->downscale_param_size);
	EXTRACTOR_CHECK_STATUS(cfg->dst_buf_phy_addr == 0 || cfg->dst_buf_size == 0,
		-EINVAL, error_exit, "Invalid dst buffer: paddr=0x%llx, size=%u",
		cfg->dst_buf_phy_addr, cfg->dst_buf_size);

	/* Check TEE initialization status */
	EXTRACTOR_CHECK_STATUS(!g_teec_inited || !g_extractor_shm.param_shm, -EINVAL, error_exit,
		"TEE not initialized, call sec_extractor_open() first");

	/* Validate parameter size fits in shared memory */
	EXTRACTOR_CHECK_STATUS(cfg->downscale_param_size > g_extractor_shm.param_shm->size,
		-EINVAL, error_exit, "Parameter size too large: %u", cfg->downscale_param_size);

	/* Copy parameters to shared memory */
	param_shm_data = tee_shm_get_va(g_extractor_shm.param_shm, 0);
	EXTRACTOR_CHECK_STATUS(IS_ERR(param_shm_data), -EBUSY, error_exit,
		"Failed to get param SHM virtual address");
	memcpy(param_shm_data, cfg->downscale_param, cfg->downscale_param_size);

	/* Determine destination buffer handling strategy */
	use_reg_dst_shm = EXTRACTOR_CONFIG(sec_extractor_config, MAP_REG_DST_SHARED_MEM);
	EXTRACTOR_CHECK_STATUS(!use_reg_dst_shm && !g_extractor_shm.dst_shm, -EINVAL, error_exit,
		"No destination SHM available (config=0x%x)", sec_extractor_config);

	/* Map destination buffer for access */
	dst_buf = codec_mm_vmap((ulong)cfg->dst_buf_phy_addr, cfg->dst_buf_size);
	EXTRACTOR_CHECK_STATUS(!dst_buf, -ENOMEM, error_exit,
		"Failed to map dst buffer (paddr=0x%llx, size=%u)",
		cfg->dst_buf_phy_addr, cfg->dst_buf_size);

	extractor_dbg_param("dst_buf mapped: %p, size=%u", dst_buf, cfg->dst_buf_size);

	if (use_reg_dst_shm) {
		/* Register user buffer directly with TEE for zero-copy operation */
		reg_dst_shm = tee_shm_register_kernel_buf(g_teec_context,
				(void *)dst_buf, (size_t)cfg->dst_buf_size);
		EXTRACTOR_CHECK_STATUS(IS_ERR(reg_dst_shm) || reg_dst_shm == NULL, -ENOMEM,
			reg_shm_free, "Failed to register dst buffer with TEE");
		extractor_dbg("Registered dst buffer with TEE: size=%u", cfg->dst_buf_size);
		shm_data = (char *)dst_buf;
	} else if (EXTRACTOR_CONFIG(sec_extractor_config, ALLOC_REG_DST_SHARED_MEM)) {
		/* Use pre-allocated CMA buffer */
		shm_data = (char *)g_extractor_shm.dst_vaddr;
		extractor_dbg_param("Using CMA buffer: vaddr=%p", shm_data);
	} else {
		/* Use TEE-allocated buffer */
		shm_data = tee_shm_get_va(g_extractor_shm.dst_shm, 0);
		EXTRACTOR_CHECK_STATUS(IS_ERR(shm_data), -EBUSY, error_exit,
			"Failed to get dst SHM virtual address");
		extractor_dbg_param("Using TEE buffer: vaddr=%p", shm_data);
	}

	/* Prepare TEE parameters */
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = g_extractor_shm.param_shm;
	param[0].u.memref.size = cfg->downscale_param_size;
	param[0].u.memref.shm_offs = 0;

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[1].u.memref.shm = use_reg_dst_shm ? reg_dst_shm : g_extractor_shm.dst_shm;
	param[1].u.memref.size = cfg->dst_buf_size;
	param[1].u.memref.shm_offs = 0;

	/* Invoke TA for downscale operation */
	inv_arg.func = EXTRACTOR_CMD_DOWNSCALE;
	inv_arg.session = g_teec_session.session;
	inv_arg.num_params = TEE_CMD_PARAM_NUM;

	ret = tee_client_invoke_func(g_teec_context, &inv_arg, param);
	EXTRACTOR_CHECK_STATUS(ret, -EIO, reg_shm_free, "TEE invoke failed: %d", ret);
	EXTRACTOR_CHECK_STATUS(inv_arg.ret != TEEC_SUCCESS, -EIO, reg_shm_free,
		"TA command failed: ret=0x%x, origin=0x%x", inv_arg.ret, inv_arg.ret_origin);

	/* Copy result back to user buffer if not using direct access */
	if (!use_reg_dst_shm) {
		extractor_dbg_param("Copying result: %u bytes", cfg->dst_buf_size);
		memcpy((void *)dst_buf, shm_data, cfg->dst_buf_size);
	}

	ret = 0;	/* Success */

reg_shm_free:
	if (reg_dst_shm)
		tee_shm_free(reg_dst_shm);

error_exit:
	if (use_reg_dst_shm && dst_buf)
		codec_mm_unmap_phyaddr(dst_buf);

	extractor_dbg_time("Downscale time: %llu us", (ktime_get_ns() - start) / 1000);
	mutex_unlock(&g_ca_mutex);

	return ret;
#else
	return -ENODEV;
#endif
}

EXPORT_SYMBOL(sec_extractor_downscale);

