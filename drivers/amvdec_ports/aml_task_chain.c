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

#include <linux/list.h>
#include <linux/spinlock.h>

#include "aml_vcodec_drv.h"
#include "aml_task_chain.h"

struct task_item_name_s {
	enum task_type_e	type;
	const u8		*name;
};

static const struct task_item_name_s iname[] = {
	{TASK_TYPE_DEC,		"dec"},
	{TASK_TYPE_VPP,		"vpp"},
	{TASK_TYPE_V4L_SINK,	"v4l-sink"},
	{TASK_TYPE_MAX,		"unknown"},
};

static const u8 *type_to_name(enum task_type_e type)
{
	const u8 *name = "unknown";
	int i, size = ARRAY_SIZE(iname);

	for (i = 0; i < size; i++) {
		if (type == iname[i].type)
			name = iname[i].name;
	}

	return name;
}

static struct task_item_s *find_task_item(struct task_chain_s *task,
					  enum task_type_e type)
{
	struct task_item_s *item = NULL;
	ulong flags;

	spin_lock_irqsave(&task->slock, flags);

	if (!list_empty(&task->list_item)) {
		struct task_item_s *p;

		list_for_each_entry(p, &task->list_item, node) {
			if (p->ops->type == type) {
				item = p;
				break;
			}
		}
	}

	if (item)
		kref_get(&item->ref);

	spin_unlock_irqrestore(&task->slock, flags);

	return item;
}

static void task_item_release(struct kref *kref);

static struct task_item_s *task_item_get(struct task_chain_s *task,
				  enum task_type_e type)
{
	struct task_item_s *item = NULL;

	item = find_task_item(task, type);
	if (!item) {
		v4l_dbg(task->ctx, V4L_DEBUG_CODEC_ERROR,
			"TASK_CHAIN:%d get item:%d fail.\n", task->id, type);
	}

	return item;
}

static int task_item_put(struct task_item_s *item)
{
	return kref_put(&item->ref, task_item_release);
}

static void task_buffer_submit(struct task_chain_s *task,
			       enum task_type_e type)
{
	struct vdec_v4l2_buffer *fb =
		(struct vdec_v4l2_buffer *)task->obj;
	struct task_item_s *item = NULL;
	struct task_item_s *item2 = NULL;
	struct vframe_s *vf = NULL;

	item = task_item_get(task, type);
	if (item) {
		item->ops->get_vframe(item->caller, &vf);
		fb->vframe = item->vframe = (void *)vf;
		item->is_active = false;

		item2 = task_item_get(task, task->map[0][type]);
		if (item2) {
			item2->is_active = true;
			item2->ops->fill_buffer(task->ctx, fb);

			v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
				"TASK_CHAIN:%d, vf:%px, phy:%lx, submit %d => %d.\n",
				task->id, vf, fb->m.mem[0].addr,
				type, task->map[0][type]);

			task->direction = TASK_DIR_SUBMIT;
			task_item_put(item2);
		}
		task_item_put(item);
	}
}

static void task_buffer_recycle(struct task_chain_s *task,
			       enum task_type_e type)
{
	struct vdec_v4l2_buffer *fb =
		(struct vdec_v4l2_buffer *)task->obj;
	struct task_item_s *item = NULL;
	struct task_item_s *item2 = NULL;

	item = task_item_get(task, type);
	if (item) {
		item->is_active = false;

		item2 = task_item_get(task, task->map[1][type]);
		if (item2) {
			item2->is_active = true;
			item2->ops->put_vframe(item2->caller, item2->vframe);

			v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
				"TASK_CHAIN:%d, vf:%px, phy:%lx, recycle %d => %d.\n",
				task->id, item2->vframe, fb->m.mem[0].addr,
				type, task->map[1][type]);

			task->direction = TASK_DIR_RECYCLE;
			task_item_put(item2);
		}
		task_item_put(item);
	}
}

void task_chain_show(struct task_chain_s *task)
{
	struct task_item_s *item = NULL;
	char buf[128] = {0};
	char *pbuf = buf;
	ulong flags;

	if (!task || !task->ctx)
		return;

	spin_lock_irqsave(&task->slock, flags);

	if (!list_empty(&task->list_item)) {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)task->obj;

		list_for_each_entry(item, &task->list_item, node) {
			pbuf += sprintf(pbuf, "%s(%d)",
				item->name, item->is_active);
			if (item->node.next != &task->list_item) {
				if (task->direction == TASK_DIR_SUBMIT)
					pbuf += sprintf(pbuf, " ==> ");
				else
					pbuf += sprintf(pbuf, " <== ");
			}
		}
		v4l_dbg(task->ctx, V4L_DEBUG_CODEC_PRINFO,
			"vb:%2d, phy:%lx  %s\n",
			task->id, fb->m.mem[0].addr, buf);
	}

	spin_unlock_irqrestore(&task->slock, flags);
}
EXPORT_SYMBOL(task_chain_show);

static void task_chain_destroy(struct kref *kref)
{
	struct task_chain_s *task;

	task = container_of(kref, struct task_chain_s, ref);

	task->cur_type = TASK_TYPE_MAX;
	memset(task->map, 0, sizeof(task->map));

	v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
		"TASK_CHAIN:%d task chain has been destroyed.\n", task->id);

	kfree(task);
}

static void task_item_release(struct kref *kref)
{
	struct task_item_s *item;

	item = container_of(kref, struct task_item_s, ref);
	list_del(&item->node);
	kref_put(&item->task->ref, task_chain_destroy);

	v4l_dbg(item->task->ctx, V4L_DEBUG_TASK_CHAIN,
		"TASK_CHAIN:%d task item:(%px,%d) has been released.\n",
		item->task->id, item, item->ops->type);

	kfree(item);
}

void task_chain_clean(struct task_chain_s *task)
{
	struct task_item_s *item, *tmp;
	ulong flags;

	spin_lock_irqsave(&task->slock, flags);

	if (!list_empty(&task->list_item)) {
		list_for_each_entry_safe(item, tmp, &task->list_item, node)
			kref_put(&item->ref, task_item_release);
	}

	spin_unlock_irqrestore(&task->slock, flags);

	v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
		"TASK_CHAIN:%d task chain clean.\n", task->id);
}
EXPORT_SYMBOL(task_chain_clean);

void task_chain_release(struct task_chain_s *task)
{
	if (kref_read(&task->ref))
		kref_put(&task->ref, task_chain_destroy);
}
EXPORT_SYMBOL(task_chain_release);

void task_order_attach(struct task_chain_s *task,
			 struct task_ops_s *ops,
			 void *caller)
{
	struct task_item_s *item;

	item = kzalloc(sizeof(struct task_item_s), GFP_ATOMIC);
	if (!item) {
		v4l_dbg(task->ctx, V4L_DEBUG_CODEC_ERROR,
			"TASK_CHAIN:%d alloc item fail.\n", task->id);
		return;
	}

	item->task	= task;
	item->ops	= ops;
	item->caller	= caller;
	item->name	= type_to_name(ops->type);
	kref_init(&item->ref);

	task->map[0][ops->type] = task->cur_type;
	task->map[1][task->cur_type] = ops->type;
	task->cur_type = ops->type;
	kref_get(&task->ref);

	list_add(&item->node, &task->list_item);

	v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
		"TASK_CHAIN:%d attach item:(%px,%d).\n",
		task->id, item, ops->type);
}
EXPORT_SYMBOL(task_order_attach);

void task_chain_init(struct task_chain_s **task_out,
			    void *v4l_ctx,
			    void *obj,
			    int vb_idx)
{
	struct task_chain_s *task;

	task = kzalloc(sizeof(struct task_chain_s), GFP_ATOMIC);
	if (!task) {
		v4l_dbg(task->ctx, V4L_DEBUG_CODEC_ERROR,
			"TASK_CHAIN:%d alloc task fail.\n", task->id);
		return;
	}

	task->id	= vb_idx;
	task->obj	= obj;
	task->ctx	= v4l_ctx;
	kref_init(&task->ref);
	spin_lock_init(&task->slock);
	INIT_LIST_HEAD(&task->list_item);

	task->attach	= task_order_attach;
	task->submit	= task_buffer_submit;
	task->recycle	= task_buffer_recycle;

	*task_out = task;

	v4l_dbg(task->ctx, V4L_DEBUG_TASK_CHAIN,
		"TASK_CHAIN:%d task chain creat success.\n", task->id);
}
EXPORT_SYMBOL(task_chain_init);

