/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <getopt.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <linux/version.h>
#include <sys/mman.h>

#include "aml_dhp_core.h"
#include "aml_dhp_daemon.h"
#include "aml_dhp_common.h"
#include "avbc_interface.h"
#include "../aml_dhp_if.h"

extern unsigned int dump_data;
extern crc_ctx_t *g_crc;
extern unsigned int g_idx;

static void dump_yuv_data(unsigned int id,
                            unsigned int width,
                            unsigned int height,
                            unsigned int stride,
                            unsigned int depth,
                            unsigned char *src_yuv,
                            unsigned int src_size)
{
    char file_name[64];
    unsigned int yuv_size = stride * height * 3 / 2;

    if ((g_idx != -1) && (g_idx != id))
        return;

    crc_reset(g_crc);

    //file format: resolution-bitdepth-index-crc.yuv
    snprintf(file_name, sizeof(file_name),
        "/data/tmp/%dx%d-%dbit-%u-%x.yuv",
        width, height, depth, id, crc_compute(g_crc, src_yuv, yuv_size));

    LOG_INFO("%s, %s, width: %d, height: %d, stride: %d, size:%u\n",
        __func__, file_name, width, height, stride, yuv_size);

    dump_raw_data(file_name, src_yuv, yuv_size, 0);
}

int aml_avbcd_handle(void *dev, struct aml_du_base *base)
{
    const struct aml_dhp_ioctl_data *io =
        container_of(base, struct aml_dhp_ioctl_data, base);
    struct aml_du_avbcd *avbcd = (struct aml_du_avbcd *)base->meta;
    struct timeval t0, t1;

    gettimeofday(&t0, NULL);

    LOG_DEBUG("AVBCD handle: wxh: %dx%d, dep: %d, pts:%u\n",
        avbcd->width, avbcd->height, avbcd->bitdep, avbcd->pts);

    void *header = dhp_dbuf_mmap(io->fd, avbcd->hsize, PROT_READ | PROT_WRITE, MAP_SHARED, 0);
    if (!header) {
        perror("Header mmap failed");
        return -1;
    }

    // Perform decoding
    struct aml_dhp_ioctl_data iomem = { 0 };
    unsigned int stride = (avbcd->bitdep == 8) ? avbcd->width : avbcd->width * 2;
    unsigned int dst_size = stride * avbcd->height * 3 / 2;
    void *dst_yuv = NULL;

    iomem.mem.type = AML_MEM_TYPE_PHY_ADDR;
    iomem.mem.addr = io->base.dst.addr;
    iomem.mem.size = io->base.dst.size;

    if (dhp_dev_ioctl(dev, IOCTL_DHP_MMAP, &iomem)) {
        LOG_ERROR("IOCTL_DHP_MMAP failed, addr:%llx\n", iomem.mem.addr);
        dhp_dbuf_munmap(header, avbcd->hsize);
        return -1;
    }

    dst_yuv = (void *)iomem.mem.uptr;
    dst_size = iomem.mem.size;

    LOG_DEBUG("Mapping YUV buffer: %p, size: %u\n", dst_yuv, dst_size);

    aml_avbc_decode(header, avbcd->width, avbcd->height, stride, avbcd->bitdep, dst_yuv, dst_size, dhp_page_mmap, dev);

    gettimeofday(&t1, NULL);
    LOG_VERBOSE("%s, Total elapse: %lu ms.\n",
        __func__, elapse_time_ms(&t0, &t1));

    if (dump_data)
        dump_yuv_data(avbcd->pts, avbcd->width, avbcd->height, stride, avbcd->bitdep, dst_yuv, dst_size);

    dhp_dbuf_munmap(header, avbcd->hsize);
    dhp_dbuf_munmap(dst_yuv, dst_size);

    return 0;
}

int aml_data_handle(void *dev, unsigned int type, void *data)
{
    int ret = -1;

    switch (type) {
    case AML_DHP_TYPE_AVBCD: {
        ret = aml_avbcd_handle(dev, data);
    }
    case AML_DHP_TYPE_MEM:
        //TODO
        break;
    default:
        break;
    }

    return ret;
}

