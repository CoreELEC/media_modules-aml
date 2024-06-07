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
#ifndef _AML_VCODEC_AVBC_WRAPPER_H_
#define _AML_VCODEC_AVBC_WRAPPER_H_

#include "aml_task_chain.h"

#define AVBCD_FRAME_SIZE 64

#define AVBCD_SOFT_KERNEL_MODE	(1 << 0)
#define AVBCD_SOFT_USER_MODE	(1 << 1)
#define AVBCD_HARDWARE_MODE	(1 << 2)

/*
 * enum avbc_io_type_e - AVBCD processing mode.
 *
 * @AVBCD_IO_BLOCKING	: Block waiting for avbcd processing to complete.
 * @AVBCD_IO_NON_BLOCKING
 *			: Submit avbcd task without blocking.
 */
enum avbc_io_type_e {
	AVBCD_IO_BLOCKING,
	AVBCD_IO_NON_BLOCKING
};

/*
 * enum avbc_memory_type_e - AVBCD output buffer type.
 *
 * @AVBCD_MEM_VIRTADDR	: Output buffer type is virtual address.
 * @AVBCD_MEM_PHYADDR	: Output buffer type is physical address.
 * @AVBCD_MEM_DMABUF	: Output buffer type is dma buffer.
 */
enum avbc_memory_type_e {
	AVBCD_MEM_VIRTADDR,
	AVBCD_MEM_PHYADDR,
	AVBCD_MEM_DMABUF,
	AVBCD_MEM_MAX
};

 /*
  * struct avbc_output - Parameters of output for AVBCD.
  *
  * @type		: AVBCD output buffer type.
  * @phy		: AVBCD output buffer physic address.
  * @virt		: AVBCD output buffer virtual address.
  * @dbuf		: AVBCD output buffer dma buffer.
  * @length		: YUV buffer size for AVBCD.
  * @avbc_done		: AVBCD callback after completing for AVBCD_IO_NON_BLOCKING mode.
  */
 struct avbc_output {
	enum avbc_memory_type_e type;
	union {
		ulong           phy;
		void            *virt;
		struct dma_buf  *dbuf;
	} m;
	u32 length;
	u32 align_w;
	u32 align_h;

	void (*avbc_done)(struct avbc_output *);
};

/*
 * struct avbc_input - Parameters of input for AVBCD.
 *
 * @header_addr		: AVBCD header buffer physic address.
 * @header_size		: AVBCD header buffer size.
 * @width		: Original width.
 * @height		: Original height.
 * @bitdepth		: Bitdepth of stream.
 */
struct avbc_input {
	ulong header_addr;
	u32 header_size;
	u32 width;
	u32 height;
	u32 bitdepth;
};

/*
 * aml_avbc_wrapper_init() - AVBC Wrapper context init.
 *
 * Used to init AVBC Wrapper context
 */
int aml_avbc_wrapper_init(void**);

/*
 * aml_avbc_wrapper_destroy() - AVBC Wrapper context destroy.
 *
 * Used to destroy AVBC Wrapper context
 */
void aml_avbc_wrapper_destroy(void *);

/*
 * aml_avbc_wrapper_reset() - AVBC Wrapper context reset.
 *
 * Used to reset AVBC Wrapper context
 */
void aml_avbc_wrapper_reset(void *);

/*
 * aml_avbc_wrapper_start() - AVBC Wrapper work enable.
 *
 * @priv	: pointer to AVBC Wrapper context.
 * Used to enable AVBC Wrapper work
 */
void aml_avbc_wrapper_start(void *priv);

/*
 * aml_avbc_wrapper_init() - AVBC Wrapper work disable.
 *
 * @priv	: pointer to AVBC Wrapper context.
 * Used to disable AVBC Wrapper work
 */
void aml_avbc_wrapper_stop(void *priv);

/*
 * aml_avbc_decode() - AVBC Wrapper process interface.
 *
 * @out		: Parameters of output.
 * @in		: Parameters of input.
 * @flag	: Blocking mode of processing frame.
 * Used to post AVBC process task
 */
int aml_avbc_decode(struct avbc_output *out, struct avbc_input *in, u32 flag);

struct task_ops_s *get_avbc_ops(void);

#endif /* _AML_VCODEC_AVBC_WRAPPER_H_ */

