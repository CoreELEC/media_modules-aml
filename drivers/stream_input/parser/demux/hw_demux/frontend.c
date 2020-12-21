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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
//#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/media/utils/amstream.h>
//#include <linux/clk.h>
#include "c_stb_define.h"
#include "c_stb_regs_define.h"
#include "../aml_dvb.h"
#include "dvb_reg.h"

#include "demod_gt.h"
#include "../../../../common/media_clock/switch/amports_gate.h"

#define pr_error(fmt, args...) printk("DVB: " fmt, ## args)

int frontend_probe(struct platform_device *pdev)
{
	int ret = 0;
#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
	struct aml_dvb *advb = aml_get_dvb_device();

	ret = dvb_extern_register_frontend(&advb->dvb_adapter);
	if (ret)
		pr_error("aml register dvb frontend failed\n");
#endif

	return ret;
}

int frontend_remove(void)
{
#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
	dvb_extern_unregister_frontend();
#endif

	return 0;
}

