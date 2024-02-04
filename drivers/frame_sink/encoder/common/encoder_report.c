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

#include <linux/list.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <linux/amlogic/media/resource_mgr/resourcemanage.h>

#include "encoder_report.h"

#define DEFAULT_TITTLE "default"
#define DEBUG_LEVEL_TITTLE "debuglevel"
#define ENC_TITTLE "Video_Enc"

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

enum {
	ENC_SYS_LOGLEVEL_NONE			= 0,
	ENC_SYS_LOGLEVEL_ERROR			= 1,
	ENC_SYS_LOGLEVEL_WARN			= 2,
	ENC_SYS_LOGLEVEL_INFO			= 3,
	ENC_SYS_LOGLEVEL_DEBUG_0		= 4,
	ENC_SYS_LOGLEVEL_DEBUG_1		= 5,
	ENC_SYS_LOGLEVEL_DEBUG_2		= 6,
	ENC_SYS_LOGLEVEL_VERBOSE_0		= 7,
	ENC_SYS_LOGLEVEL_VERBOSE_1		= 8,
	ENC_SYS_LOGLEVEL_VERBOSE_2		= 9,
	ENC_SYS_LOGLEVEL_TRACE			= 10
};

struct module_debug_node {
	struct list_head list;
	char *module;
	enc_set_debug_level_func set_debug_level_notify;
};

static struct list_head debug_head;
static struct mutex debug_lock;

static struct module_debug_node *get_debug_module(const char *module)
{
	struct module_debug_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;

	if (module) {
		list_for_each_safe(pos, tmp, &debug_head) {
			node = list_entry(pos, struct module_debug_node, list);
			if (node && node->module && strlen(module) == strlen(node->module)) {
				if (!memcmp(module, node->module, strlen(module)))
					break;
			}
			node = NULL;
		}
	}

	return node;
}

int enc_register_set_debug_level_func(const char *module, enc_set_debug_level_func func)
{
	int res = 0;
	struct module_debug_node *node = NULL;

	if (!module || !func)
		return -EINVAL;

	mutex_lock(&debug_lock);
	node = get_debug_module(module);
	if (!node) {
		node = kzalloc(sizeof(struct module_debug_node), GFP_KERNEL);
		if (!node) {
			pr_info("failed allocate debug node for %s\n", module);
			res = -ENOMEM;
			goto error;
		}

		node->module = kzalloc(strlen(module) + 1, GFP_KERNEL);
		if (!node) {
			pr_info("failed allocate module for %s\n", module);
			res = -ENOMEM;
			goto error;
		}

		memcpy(node->module, module, strlen(module));
		node->set_debug_level_notify = func;
		list_add_tail(&node->list, &debug_head);
	}
	mutex_unlock(&debug_lock);

	return 0;
error:
	kfree(node->module);
	kfree(node);
	mutex_unlock(&debug_lock);
	return res;
}
EXPORT_SYMBOL(enc_register_set_debug_level_func);

static int get_configs(const char *configs, const char *need, int *val)
{
	const char *str;
	const char *str_2;
	int ret = 0;
	int lval = 0;
	*val = 0;

	if (!configs || !need) {
		ret = -1;
		goto exit;
	}

	/*find v4l configs*/
	str = strstr(configs, need);
	if (str != NULL) {
		if (str > configs && str[-1] != ',') {
			ret = -2;
			goto exit;
		}
		str += strlen(need);
		if (str[0] != ':' || str[1] == '\0') {
			if (str[0] != '_' || str[1] != 'v') {
				ret = -3;
				goto exit;
			}
		} else {
			if (str[1] == '0' &&  str[2] == 'x') {
				if (sscanf(str, ":0x%x", &lval) == 1) {
					*val = lval;
					ret = 0;
					goto exit;
				} else {
					ret = -4;
					goto exit;
				}
			} else {
				if (sscanf(str, ":%d", &lval) == 1) {
					*val = lval;
					ret = 0;
					goto exit;
				} else {
					ret = -5;
					goto exit;
				}
			}
		}
	} else {
		ret = -6;
		goto exit;
	}

	/*find non v4l configs*/
	str_2 = strstr(str, need);
	if (str_2 != NULL) {
		if (str_2 > str && str_2[-1] != ',') {
			ret = -7;
			goto exit;
		}
		str_2 += strlen(need);
		if (str_2[0] != ':' || str_2[1] == '\0') {
			ret = -8;
			goto exit;
		} else {
			if (str_2[1] == '0' && str_2[2] == 'x') {
				if (sscanf(str_2, ":0x%x", &lval) == 1) {
					*val = lval;
					ret = 0;
					goto exit;
				} else {
					ret = -9;
					goto exit;
				}
			} else {
				if (sscanf(str_2, ":%d", &lval) == 1) {
					*val = lval;
					ret = 0;
					goto exit;
				} else {
					ret = -10;
					goto exit;
				}
			}
		}
	}

exit:
	return ret;
}

static int cur_configs(const char *configs, const char *title, char *cur_str)
{
	const char *str = NULL;
	const char *cur = NULL;
	int ret = 0;
	char *tmpbuf = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	char *tmpptr = tmpbuf;

	if (!configs || !title) {
		ret = -1;
		goto configs_done;
	}

	/*cur configs*/
	str = strstr(configs, title);
	if (str != NULL) {
		if (str > configs && str[-1] != ';') {
			ret = -2;
			goto configs_done;
		}

		str += strlen(title);
		if (str[0] != ':' || str[1] == '\0') {
			ret = -3;
			goto configs_done;
		}

		str += 1;

		strcpy(tmpbuf, str);
		cur = strsep(&tmpptr, ";");
	}

	if (!cur) {
		ret = -4;
		goto configs_done;
	}

	strcpy(cur_str, cur);
	ret = 0;

configs_done:
	kfree(tmpbuf);
	return ret;
}

static void set_default_mode(int debug_level)
{
	int log_level = LOG_ERROR;
	struct module_debug_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;
	switch (debug_level) {
		case ENC_SYS_LOGLEVEL_NONE:
		case ENC_SYS_LOGLEVEL_ERROR:
			log_level = LOG_ERROR;
			break;
		case ENC_SYS_LOGLEVEL_WARN:
		case ENC_SYS_LOGLEVEL_INFO:
		case ENC_SYS_LOGLEVEL_DEBUG_0:
		case ENC_SYS_LOGLEVEL_DEBUG_1:
		case ENC_SYS_LOGLEVEL_DEBUG_2:
			log_level = LOG_DEBUG;
			break;
		case ENC_SYS_LOGLEVEL_VERBOSE_0:
		case ENC_SYS_LOGLEVEL_VERBOSE_1:
		case ENC_SYS_LOGLEVEL_VERBOSE_2:
			log_level = LOG_INFO;
			break;
		case ENC_SYS_LOGLEVEL_TRACE:
			log_level = LOG_ALL;
			break;
		default:
			log_level = LOG_ALL;
			break;
	}

	list_for_each_safe(pos, tmp, &debug_head) {
		node = list_entry(pos, struct module_debug_node, list);
		node->set_debug_level_notify(node->module, log_level);
	}
}

void enc_set_debug_configs(const char *module, const char *debug, int len)
{
	int config_val;
	char *default_str = kzalloc(sizeof(char) * 128, GFP_KERNEL);
	char *dec_str = kzalloc(sizeof(char) * 2048, GFP_KERNEL);

	if (cur_configs(debug, ENC_TITTLE, dec_str) == 0) {
		struct module_debug_node *node = NULL;
		struct list_head *pos = NULL, *tmp = NULL;
		pr_info("Video_Enc:%s\n", dec_str);

		if (get_configs(dec_str, DEFAULT_TITTLE, &config_val) == 0) {
			set_default_mode(config_val);
		} else {
			list_for_each_safe(pos, tmp, &debug_head) {
				node = list_entry(pos, struct module_debug_node, list);
				if (get_configs(dec_str, node->module, &config_val) == 0) {
					node->set_debug_level_notify(node->module, config_val);
				}
			}
		}
	} else {
		if (cur_configs(debug, DEFAULT_TITTLE, default_str) == 0) {
			pr_info("default:%s\n", default_str);
			if (get_configs(default_str, DEBUG_LEVEL_TITTLE, &config_val) == 0) {
				set_default_mode(config_val);
			}
		}
	}

	kfree(default_str);
	kfree(dec_str);
}
EXPORT_SYMBOL(enc_set_debug_configs);

int enc_report_init(void)
{
	INIT_LIST_HEAD(&debug_head);
	mutex_init(&debug_lock);
	resman_register_debug_callback("VideoEncoder", enc_set_debug_configs);
	return 0;
}

void enc_report_exit(void)
{
	struct module_debug_node *node = NULL;

	while (!list_empty(&debug_head)) {
		node = list_entry(debug_head.next,
			struct module_debug_node, list);
		mutex_lock(&debug_lock);
		if (node) {
			list_del(&node->list);
			kfree(node->module);
			kfree(node);
		}
		mutex_unlock(&debug_lock);
	}
}

static s32 __init encoder_common_init(void)
{
	enc_report_init();
	return 0;
}

static void __exit encoder_common_exit(void)
{
	enc_report_exit();
}


module_init(encoder_common_init);
module_exit(encoder_common_exit);

MODULE_DESCRIPTION("AMLOGIC Video Encoder Bug Report Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaoya.lin <xiaoya.lin@amlogic.com>");
