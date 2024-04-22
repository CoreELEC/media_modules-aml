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

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/resource_mgr/resourcemanage.h>

#include "vdec.h"
#include "decoder_report.h"

#define BUFF_SIZE 1024 * 4 *  4
#define USER_BUFF_SIZE 1024 * 4

#define DEFAULT_TITTLE "default"
#define DEC_TITTLE "Video_Dec"

struct aml_dec_report_dev {
	void *v4l_dev;
	dump_v4ldec_state_func dump_v4ldec_state_notify;
	dump_amstream_bufs_func dump_amstream_bufs_notify;
};

struct module_debug_node {
	struct list_head list;
	char *module;
	set_debug_flag_func set_debug_flag_notify;
};

static struct list_head debug_head;
struct mutex debug_lock;

static struct aml_dec_report_dev *report_dev;

static ssize_t dump_v4ldec_state(char *buf)
{
	char *pbuf = buf;

	if (report_dev->dump_v4ldec_state_notify == NULL)
		return 0;
	if (report_dev->v4l_dev == NULL)
		return 0;

	pbuf += sprintf(pbuf, "\n============ cat /sys/class/v4ldec/status:\n");
	pbuf += report_dev->dump_v4ldec_state_notify(report_dev->v4l_dev, pbuf);

	return pbuf - buf;
}

void register_dump_v4ldec_state_func(void *dev, dump_v4ldec_state_func func)
{
	report_dev->v4l_dev = dev;
	report_dev->dump_v4ldec_state_notify = func;
}
EXPORT_SYMBOL(register_dump_v4ldec_state_func);

static ssize_t dump_amstream_bufs(char *buf)
{
	char *pbuf = buf;
	char *tmpbuf = (char *)kzalloc(BUFF_SIZE, GFP_KERNEL);
	char *ptmpbuf = tmpbuf;
	if (report_dev->dump_amstream_bufs_notify == NULL)
		return 0;

	ptmpbuf += report_dev->dump_amstream_bufs_notify(tmpbuf);

	if (ptmpbuf - tmpbuf) {
		pbuf += sprintf(pbuf, "\n============ cat /sys/class/amstream/bufs:\n");
		pbuf += sprintf(pbuf, "%s", tmpbuf);
	}

	kfree(tmpbuf);
	return pbuf - buf;
}

void register_dump_amstream_bufs_func(dump_amstream_bufs_func func)
{
	report_dev->dump_amstream_bufs_notify = func;
}
EXPORT_SYMBOL(register_dump_amstream_bufs_func);

static void buff_show(ssize_t size, char *buf, int buff_size)
{
	if (size && buf) {
		char *tmpbuf = kzalloc(sizeof(char) * size, GFP_KERNEL);
		char *tmpptr = tmpbuf;
		const char *cur;

		if (!tmpptr) {
			pr_err("failed alloc buf for buff_show\n");
			return;
		}

		strcpy(tmpbuf, buf);
		while (1) {
			cur = strsep(&tmpptr, "\n");
			if (!cur)
				break;
			pr_info("%s", cur);
		}

		memset(buf, 0, buff_size);
		kfree(tmpbuf);
	}
}

static ssize_t status_show(KV_CLASS_CONST struct class *cls,
	KV_CLASS_ATTR_CONST struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	char *tmpbuf = NULL;
	char *ptmpbuf = NULL;
	ssize_t size = 0;

	if (!report_dev)
		return 0;

	tmpbuf = (char *)kzalloc(BUFF_SIZE, GFP_KERNEL);
	if (!tmpbuf) {
		pr_err("failed alloc buf for status_show\n");
		return -ENOMEM;
	}
	ptmpbuf = tmpbuf;

	pr_info("\n============ cat /sys/class/vdec/dump_decoder_state:\n");
	buff_show(dump_decoder_state(tmpbuf), tmpbuf, BUFF_SIZE);

	ptmpbuf += dump_v4ldec_state(ptmpbuf);

	ptmpbuf+= sprintf(ptmpbuf, "\n============ cat /sys/class/vdec/debug:\n");
	ptmpbuf += dump_vdec_debug(ptmpbuf);

	ptmpbuf += sprintf(ptmpbuf, "\n============ cat /sys/class/vdec/dump_vdec_chunks:\n");
	ptmpbuf += dump_vdec_chunks(ptmpbuf);

	ptmpbuf += dump_amstream_bufs(ptmpbuf);

	ptmpbuf += sprintf(ptmpbuf, "\n============ cat /sys/class/vdec/core:\n");
	ptmpbuf += dump_vdec_core(ptmpbuf);

	size = ptmpbuf - tmpbuf;
	if (size > USER_BUFF_SIZE) {
		buff_show(size, tmpbuf, BUFF_SIZE);
	} else {
		pbuf+= sprintf(pbuf, "%s\n", tmpbuf);
	}

	kfree(tmpbuf);
	return pbuf - buf;
}

static CLASS_ATTR_RO(status);

static struct attribute *report_class_attrs[] = {
	&class_attr_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(report_class);

static struct class report_class = {
	.name = "dec_report",
	.class_groups = report_class_groups,
};


static struct platform_driver report_driver = {
	.driver = {
		.name = "dec_report",
	}
};

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

int register_set_debug_flag_func(const char *module, set_debug_flag_func func)
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
		if (!node->module) {
			pr_info("failed allocate module for %s\n", module);
			res = -ENOMEM;
			goto error;
		}

		memcpy(node->module, module, strlen(module));
		node->set_debug_flag_notify = func;
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
EXPORT_SYMBOL(register_set_debug_flag_func);

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
	char *tmpbuf = NULL;
	char *tmpptr = NULL;

	if (!configs || !title) {
		return -1;
	}

	tmpbuf = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (!tmpbuf) {
		pr_err("failed alloc buf for cur_configs\n");
		return -ENOMEM;
	}
	tmpptr = tmpbuf;
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

static void set_debug_flag(const char *module, int debug_flags)
{
	struct module_debug_node *node = NULL;
	node = get_debug_module(module);
	if (node) {
		node->set_debug_flag_notify(module, debug_flags);
	}
}

static void set_simple_dec_log(void) {
	set_debug_flag(DEBUG_AMVDEC_PORTS, 0x899);
	set_debug_flag(DEBUG_AMVDEC_H265, 0x20004001);
	set_debug_flag(DEBUG_AMVDEC_H264, 0x43);
	set_debug_flag(DEBUG_AMVDEC_VP9, 0x20000011);
	set_debug_flag(DEBUG_AMVDEC_AV1, 0x20000041);
	set_debug_flag(DEBUG_AMVDEC_AVS2, 0x20000009);
	set_debug_flag(DEBUG_AMVDEC_AVS3, 0x60000001);
	set_debug_flag(DEBUG_AMVDEC_MPEG12, 0x803);
	set_debug_flag(DEBUG_AMVDEC_MPEG4, 0x23);
	set_debug_flag(DEBUG_AMVDEC_AVS, 0x11);
	set_debug_flag(DEBUG_AMVDEC_MJPEG, 0x83);
	set_debug_flag(DEBUG_AMVDEC_H265_V4L, 0x20004001);
	set_debug_flag(DEBUG_AMVDEC_H264_V4L, 0x43);
	set_debug_flag(DEBUG_AMVDEC_VP9_V4L, 0x20000011);
	set_debug_flag(DEBUG_AMVDEC_AV1_V4L, 0x20000041);
	set_debug_flag(DEBUG_AMVDEC_AVS2_V4L, 0x20000009);
	set_debug_flag(DEBUG_AMVDEC_AVS3_V4L, 0x60000001);
	set_debug_flag(DEBUG_AMVDEC_MPEG12_V4L, 0x803);
	set_debug_flag(DEBUG_AMVDEC_MPEG4_V4L, 0x23);
	set_debug_flag(DEBUG_AMVDEC_AVS_V4L, 0x11);
	set_debug_flag(DEBUG_AMVDEC_MJPEG_V4L, 0x83);
	set_debug_flag(DEBUG_AMVDEC_H265_FB, 0x20004001);
	set_debug_flag(DEBUG_AMVDEC_VP9_FB, 0x20000011);
	set_debug_flag(DEBUG_AMVDEC_AV1_FB, 0x20000041);
	set_debug_flag(DEBUG_AMVDEC_AVS2_FB, 0x20000009);
	set_debug_flag(DEBUG_AMVDEC_H265_FB_V4L, 0x20004001);
	set_debug_flag(DEBUG_AMVDEC_VP9_FB_V4L, 0x20000011);
	set_debug_flag(DEBUG_AMVDEC_AV1_FB_V4L, 0x20000041);
	set_debug_flag(DEBUG_AMVDEC_AVS2_FB_V4L, 0x20000009);
}

static void set_normal_dec_log(void)
{
	set_debug_flag(DEBUG_AMVDEC_PORTS, 0xfff);
	set_debug_flag(DEBUG_AMVDEC_H265, 0x60004003);
	set_debug_flag(DEBUG_AMVDEC_H264, 0x90ff);
	set_debug_flag(DEBUG_AMVDEC_VP9, 0x70000011);
	set_debug_flag(DEBUG_AMVDEC_AV1, 0x6001007d);
	set_debug_flag(DEBUG_AMVDEC_AVS2, 0x600300ff);
	set_debug_flag(DEBUG_AMVDEC_AVS3, 0x600300ff);
	set_debug_flag(DEBUG_AMVDEC_MPEG12, 0x18ff);
	set_debug_flag(DEBUG_AMVDEC_MPEG4, 0x843);
	set_debug_flag(DEBUG_AMVDEC_AVS, 0x83f);
	set_debug_flag(DEBUG_AMVDEC_MJPEG, 0x83);
	set_debug_flag(DEBUG_AMVDEC_H265_V4L, 0x60004003);
	set_debug_flag(DEBUG_AMVDEC_H264_V4L, 0x90ff);
	set_debug_flag(DEBUG_AMVDEC_VP9_V4L, 0x70000011);
	set_debug_flag(DEBUG_AMVDEC_AV1_V4L, 0x6001007d);
	set_debug_flag(DEBUG_AMVDEC_AVS2_V4L, 0x600300ff);
	set_debug_flag(DEBUG_AMVDEC_AVS3_V4L, 0x600300ff);
	set_debug_flag(DEBUG_AMVDEC_MPEG12_V4L, 0x18ff);
	set_debug_flag(DEBUG_AMVDEC_MPEG4_V4L, 0x843);
	set_debug_flag(DEBUG_AMVDEC_AVS_V4L, 0x83f);
	set_debug_flag(DEBUG_AMVDEC_MJPEG_V4L, 0x83);
	set_debug_flag(DEBUG_AMVDEC_H265_FB, 0x60006803);
	set_debug_flag(DEBUG_AMVDEC_VP9_FB, 0x74000013);
	set_debug_flag(DEBUG_AMVDEC_AV1_FB, 0x600100ff);
	set_debug_flag(DEBUG_AMVDEC_AVS2_FB, 0x600300ff);
	set_debug_flag(DEBUG_AMVDEC_H265_FB_V4L, 0x60006803);
	set_debug_flag(DEBUG_AMVDEC_VP9_FB_V4L, 0x74000013);
	set_debug_flag(DEBUG_AMVDEC_AV1_FB_V4L, 0x600100ff);
	set_debug_flag(DEBUG_AMVDEC_AVS2_FB_V4L, 0x600300ff);
}

static void set_default_mode(const char *module, int debug_flags)
{
	struct module_debug_node *node = NULL;
	struct list_head *pos = NULL, *tmp = NULL;
	switch (debug_flags) {
		case 0:
			list_for_each_safe(pos, tmp, &debug_head) {
				node = list_entry(pos, struct module_debug_node, list);
				node->set_debug_flag_notify(node->module, 0);
			}
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			set_simple_dec_log();
			break;
		default:
			set_normal_dec_log();
			break;
	}
}

void set_debug_configs(const char *module, const char *debug, int len)
{
	int config_val;
	char *default_str = kzalloc(sizeof(char) * 128, GFP_KERNEL);
	char *dec_str = kzalloc(sizeof(char) * 2048, GFP_KERNEL);

	if (cur_configs(debug, DEC_TITTLE, dec_str) == 0) {
		struct module_debug_node *node = NULL;
		struct list_head *pos = NULL, *tmp = NULL;
		pr_info("Video_Dec:%s\n", dec_str);

		list_for_each_safe(pos, tmp, &debug_head) {
			node = list_entry(pos, struct module_debug_node, list);
			if (get_configs(dec_str, node->module, &config_val) == 0) {
				node->set_debug_flag_notify(node->module, config_val);
			}
		}

		if (get_configs(dec_str, DUMP_DECODER_STATE, &config_val) == 0) {
			char *tmpbuf = (char *)vzalloc(BUFF_SIZE);
			int size = status_show(NULL, NULL, tmpbuf);
			if (size <= USER_BUFF_SIZE) {
				pr_info("%s\n", tmpbuf);
			}
			vfree(tmpbuf);
		}
	} else if (cur_configs(debug, DEFAULT_TITTLE, default_str) == 0) {
		pr_info("default:%s\n", default_str);
		if (get_configs(default_str, "debuglevel", &config_val) == 0) {
			set_default_mode("debuglevel", config_val);
		}
	}

	kfree(dec_str);
	kfree(default_str);
}
EXPORT_SYMBOL(set_debug_configs);

int report_module_init(void)
{
	int ret = -1;
	report_dev = (struct aml_dec_report_dev *)vzalloc(sizeof(struct aml_dec_report_dev));

	if (platform_driver_register(&report_driver)) {
		pr_info("failed to register decoder report module\n");
		goto err;
	}

	ret = class_register(&report_class);
	if (ret < 0) {
		pr_info("Failed in creating class.\n");
		goto unregister;
	}

	INIT_LIST_HEAD(&debug_head);
	mutex_init(&debug_lock);
	resman_register_debug_callback("VideoDecoder", set_debug_configs);
	return 0;
unregister:
	platform_driver_unregister(&report_driver);
err:
	vfree(report_dev);
	return ret;
}
EXPORT_SYMBOL(report_module_init);

void report_module_exit(void)
{
	struct module_debug_node *node = NULL;

	vfree(report_dev);
	platform_driver_unregister(&report_driver);
	class_unregister(&report_class);

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
EXPORT_SYMBOL(report_module_exit);

MODULE_DESCRIPTION("AMLOGIC bug report driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kuan Hu <kuan.hu@amlogic.com>");

