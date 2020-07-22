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
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/sched.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/major.h>
#include "../frame_provider/decoder/utils/vdec.h"
#include <linux/delay.h>

#define RECEIVER_NAME "fake-amvideo"
#define DEVICE_NAME "fake-amvideo"

static struct device *amvideo_dev = NULL;
struct work_struct worker;

static struct vframe_receiver_s fake_video_vf_recv;
static int video_receiver_event_fun(int type, void *data, void *);
static const struct vframe_receiver_op_s fake_video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static struct vframe_s *video_vf_peek(void)
{
	return vf_peek(RECEIVER_NAME);
}

static struct vframe_s *video_vf_get(void)
{
	struct vframe_s *vf = NULL;

	vf = vf_get(RECEIVER_NAME);

	if (vf) {
		atomic_set(&vf->use_cnt, 1);
		/*pr_err("Get vframe  w: %d, h: %d, fence: %lx, idx: %d\n",
			vf->width, vf->height, (ulong)vf->fence, vf->index & 0xff);*/
	}

	return vf;
}

static void video_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(RECEIVER_NAME);

	if (vfp && vf && atomic_dec_and_test(&vf->use_cnt)) {
		vf_put(vf, RECEIVER_NAME);
	}
}

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG: {
		break;
	}
	case VFRAME_EVENT_PROVIDER_START: {
		break;
	}
	case VFRAME_EVENT_PROVIDER_QUREY_STATE: {
		break;
	}
	case VFRAME_EVENT_PROVIDER_VFRAME_READY: {
		vdec_schedule_work(&worker);
		break;
	}
	default:
		break;
	}

	return 0;
}

void displayer_worker(struct work_struct *work)
{
	struct vframe_s *vf = NULL;

	if (video_vf_peek()) {
		vf = video_vf_get();
		if (!vf) {
			pr_err("receiver vf err.\n");
			return;
		}

		/* add delay to simulation pipeline time*/
		udelay(2000);

		if (vf->fence) {
			/* fence waiting until frame ready. */
			vdec_fence_wait(vf->fence, 2000);

			if (vdec_fence_status_get(vf->fence) == 1) {
				pr_info("[VDEC-FENCE]: Display, idx: %d, dec cost: %lld\n",
					vf->index & 0xff, local_clock() - get_sync_pt(vf->fence)->timestamp);
			} else {
				pr_err("[VDEC-FENCE]: Display invalid, fence status err.\n");
			}
		}

		video_vf_put(vf);
	}
}

static int amvideo_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int amvideo_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long amvideo_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	file->private_data = NULL;
	return 0;
}

#ifdef CONFIG_COMPAT
static long amvideo_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	file->private_data = NULL;
	return 0;
}
#endif

static const struct file_operations amvideo_fops = {
	.owner = THIS_MODULE,
	.open = amvideo_open,
	.release = amvideo_release,
	.unlocked_ioctl = amvideo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvideo_compat_ioctl,
#endif
	//.poll = amvideo_poll,
};

static struct class amvideo_class = {
	.name = "fake_video",
};

#define FAKEVIDEO_MAJOR		(21 + (AML_BASE))

static int __init fake_video_init(void)
{
	int ret = 0;

	ret = class_register(&amvideo_class);
	if (ret) {
		pr_err("create video class fail.\n");
		return 0;
	}


	/* create video device */
	ret = register_chrdev(FAKEVIDEO_MAJOR, DEVICE_NAME, &amvideo_fops);
	if (ret < 0) {
		pr_err("Can't register major for amvideo device\n");
		goto err1;
	}

	amvideo_dev = device_create(&amvideo_class, NULL,
		MKDEV(FAKEVIDEO_MAJOR, 0), NULL, DEVICE_NAME);
	if (IS_ERR(amvideo_dev)) {
		pr_err("Can't create amvideo device\n");
		goto err;
	}

	vf_receiver_init(&fake_video_vf_recv, RECEIVER_NAME,
		&fake_video_vf_receiver, NULL);
	vf_reg_receiver(&fake_video_vf_recv);


	INIT_WORK(&worker, displayer_worker);

	return 0;

err:
	unregister_chrdev(FAKEVIDEO_MAJOR, DEVICE_NAME);
err1:
	class_unregister(&amvideo_class);

	return ret;
}

static void __exit fake_video_exit(void)
{
	cancel_work_sync(&worker);

	vf_unreg_receiver(&fake_video_vf_recv);

	device_destroy(&amvideo_class, MKDEV(FAKEVIDEO_MAJOR, 0));
	unregister_chrdev(FAKEVIDEO_MAJOR, DEVICE_NAME);
	class_unregister(&amvideo_class);

}


module_init(fake_video_init);
module_exit(fake_video_exit);


MODULE_DESCRIPTION("AMLOGIC fake video output driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nanxin Qin <nanxin.qin@amlogic.com>");

