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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "media_utils.h"

static dhp_func g_dhp_fun;

inline void *aml_media_mem_alloc(size_t size, gfp_t flags)
{
	return size >= SZ_8K ? vzalloc(size) : kzalloc(size, flags);
}
EXPORT_SYMBOL(aml_media_mem_alloc);

inline void aml_media_mem_free(const void *addr)
{
	kvfree(addr);
}
EXPORT_SYMBOL(aml_media_mem_free);

int dhp_func_reg(dhp_func fn)
{
	if (g_dhp_fun) {
		pr_err("error!!,g_dhp_fun have register\n");
		return -1;
	}
	g_dhp_fun = fn;

	return 0;
}
EXPORT_SYMBOL(dhp_func_reg);

int dhp_func_unreg(void)
{
	g_dhp_fun = NULL;

	return 0;
}
EXPORT_SYMBOL(dhp_func_unreg);

int dhp_func_request(void *src, void *dst, void *meta, int size)
{
	if (g_dhp_fun) {
		return g_dhp_fun(src, dst, meta, size);
	}

	pr_err("DHP task request error.\n");

	return -1;
}
EXPORT_SYMBOL(dhp_func_request);

