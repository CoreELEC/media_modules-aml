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

#include <linux/amlogic/media/avbc_wrapper_interface.h>
#include "aml_task_chain.h"

#define AVBCD_FRAME_SIZE 64

#define AVBCD_SOFT_KERNEL_MODE	(1 << 0)
#define AVBCD_SOFT_USER_MODE	(1 << 1)
#define AVBCD_HARDWARE_MODE	(1 << 2)

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

