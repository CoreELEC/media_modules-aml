// SPDX-License-Identifier: GPL-2.0
/***************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    COPYRIGHT (C) 2019 VERISILICON ALL RIGHTS RESERVED
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    COPYRIGHT (C) 2019 VERISILICON ALL RIGHTS RESERVED
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices in your
 *    version of this file.
 *
 *****************************************************************************
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include "vc8000_driver.h"
#include <linux/of.h>

#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/moduleparam.h>

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/cpu_cooling.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/amlogic/power_domain.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"

#define VERSENC_TS_WAIT 5


static u32 vcmd_supported;

int hantroenc_normal_init(void);
int hantroenc_vcmd_init(struct platform_device *pf_dev);
void hantroenc_normal_cleanup(void);
void hantroenc_vcmd_cleanup(struct platform_device *pf_dev);

extern void vers_resume_hw(u32 on);

static int vc9000e_vce_probe(struct platform_device *pf_dev);

static void hantroenc_cleanup(struct platform_device *pf_dev)
{
    if (vcmd_supported == 0)
        hantroenc_normal_cleanup();
    else
        hantroenc_vcmd_cleanup(pf_dev);
}

static int vc9000e_vce_remove(struct platform_device *pf_dev)
{
    hantroenc_cleanup(pf_dev);

    return 0;
}

static void meson_versenc_control(struct platform_device *pdev, bool on)
{
    vers_resume_hw(on);
}

static int meson_versenc_suspend(struct device *dev)
{
    meson_versenc_control(to_platform_device(dev), false);
    return 0;
}

static int meson_versenc_resume(struct device *dev)
{
    meson_versenc_control(to_platform_device(dev), true);
    return 0;
}

static const struct of_device_id amlogic_vce_dt_match[] = {{
                                                               .compatible = "vc9000e_rev",
                                                           },
                                                           {}};

static const struct dev_pm_ops meson_versenc_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(meson_versenc_suspend, meson_versenc_resume)
};

static struct platform_driver mbd_vce_driver = {.probe = vc9000e_vce_probe,
                                                .remove = vc9000e_vce_remove,
                                                .driver = {
                                                    .name = "vc9000e_rev",
                                                    .owner = THIS_MODULE,
                                                    .of_match_table = amlogic_vce_dt_match,
#ifdef CONFIG_PM
                                                    .pm = &meson_versenc_pm_ops,
#endif
                                                }};

static int hantroenc_init(struct platform_device *pf_dev)
{
    pr_info("vc8000_vcmd_driver: hantroenc_init\n");
    vcmd_supported = 1;
    if (vcmd_supported == 0)
        return hantroenc_normal_init();
    else
        return hantroenc_vcmd_init(pf_dev);
}

static int vc9000e_vce_probe(struct platform_device *pf_dev)
{
    return hantroenc_init(pf_dev);
}

int __init enc_mem_init(void)
{
    if ((get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S5)
        && (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S6)){
        //pr_info("The chip is not support vers!!\n");
        return -1;
    }
    pr_info("vc8000_vcmd_driver: enc_mem_init\n");
    return platform_driver_register(&mbd_vce_driver);
}

void __exit enc_mem_exit(void)
{
    pr_info("vc8000_vcmd_driver: enc_mem_exit\n");
    platform_driver_unregister(&mbd_vce_driver);
}

module_param(vcmd_supported, uint, 0664);
MODULE_PARM_DESC(vcmd_supported, "\n vcmd_supported\n");

module_init(enc_mem_init);
module_exit(enc_mem_exit);

/* module description */
/*MODULE_LICENSE("Proprietary");*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("VC8000 Vcmd driver");
