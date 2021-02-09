/*
 * drivers/amlogic/media/frame_sync/tsync_pcr.c
 *
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
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/major.h>
#include "media_sync_core.h"
#include "media_sync_dev.h"

#define MEDIASYNC_DEVICE_NAME   "mediasync"
static struct device *mediasync_dev;

typedef struct alloc_para {
	s32 mDemuxId;
	s32 mPcrPid;
} mediasync_alloc_para;

typedef struct systime_para {
       s64 mStcUs;
       s64 mSystemTimeUs;
}mediasync_systime_para;

typedef struct updatetime_para {
	int64_t mMediaTimeUs;
	int64_t mSystemTimeUs;
	bool mForceUpdate;
}mediasync_updatetime_para;

typedef struct arthortime_para {
	int64_t mMediaTimeUs;
	int64_t mSystemTimeUs;
	int64_t mStcTimeUs;
}mediasync_arthortime_para;

typedef struct priv_s {
	s32 mSyncInsId;
	mediasync_ins *mSyncIns;
}mediasync_priv_s;

static int mediasync_open(struct inode *inode, struct file *file)
{
	mediasync_priv_s *priv = {0};
	priv = kzalloc(sizeof(mediasync_priv_s), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	priv->mSyncInsId = -1;
	priv->mSyncIns = NULL;
	file->private_data = priv;
	return 0;
}

static int mediasync_release(struct inode *inode, struct file *file)
{
	long ret = 0;
	mediasync_priv_s *priv = (mediasync_priv_s *)file->private_data;
	if (priv == NULL) {
		return -ENOMEM;
	}

	if (priv->mSyncInsId != -1) {
		ret = mediasync_ins_unbinder(priv->mSyncInsId);
		priv->mSyncInsId = -1;
		priv->mSyncIns = NULL;
	}
	kfree(priv);
	return 0;
}

static long mediasync_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	mediasync_speed SyncSpeed = {0};
	s32 SyncInsId = -1;
	s32 SyncPaused = 0;
	s32 SyncMode = -1;
	s64 NextVsyncSystemTime = 0;
	s64 TrackMediaTime = 0;

	mediasync_priv_s *priv = (mediasync_priv_s *)file->private_data;
	mediasync_ins *SyncIns = NULL;
	mediasync_alloc_para parm = {0};
	mediasync_arthortime_para ArthorTime = {0};
	mediasync_updatetime_para UpdateTime = {0};
	mediasync_systime_para SystemTime = {0};
	switch (cmd) {
		case MEDIASYNC_IOC_INSTANCE_ALLOC:
			if (copy_from_user ((void *)&parm,
						(void *)arg,
						sizeof(parm)))
				return -EFAULT;
			if (mediasync_ins_alloc(parm.mDemuxId,
						parm.mPcrPid,
						&SyncInsId,
						&SyncIns) < 0) {
				return -EFAULT;
			}
			if (SyncIns == NULL) {
				return -EFAULT;
			}
			if (priv != NULL) {
				priv->mSyncInsId = SyncInsId;
				priv->mSyncIns = SyncIns;
				priv->mSyncIns->mRef++;
			}

		break;
		case MEDIASYNC_IOC_INSTANCE_GET:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			SyncInsId = priv->mSyncInsId;
			if (copy_to_user((void *)arg,
					&SyncInsId,
					sizeof(SyncInsId))) {
				return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_INSTANCE_BINDER:
			if (copy_from_user((void *)&SyncInsId,
						(void *)arg,
						sizeof(parm))) {
				return -EFAULT;
			}
			ret = mediasync_ins_binder(SyncInsId, &SyncIns);
			if (SyncIns == NULL) {
				return -EFAULT;
			}

			priv->mSyncInsId = SyncInsId;
			priv->mSyncIns = SyncIns;
		break;

		case MEDIASYNC_IOC_UPDATE_MEDIATIME:
			if (copy_from_user((void *)&UpdateTime,
						(void *)arg,
						sizeof(UpdateTime))) {
				return -EFAULT;
			}
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_update_mediatime(priv->mSyncInsId,
							UpdateTime.mMediaTimeUs,
							UpdateTime.mSystemTimeUs,
							UpdateTime.mForceUpdate);
		break;

		case MEDIASYNC_IOC_GET_MEDIATIME:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}
			ret = mediasync_ins_get_anchor_time(priv->mSyncInsId,
												&(ArthorTime.mMediaTimeUs),
												&(ArthorTime.mStcTimeUs),
												&(ArthorTime.mSystemTimeUs));
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&ArthorTime,
						sizeof(ArthorTime))) {
					return -EFAULT;
				}
			}
		break;

		case MEDIASYNC_IOC_GET_SYSTEMTIME:

			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_get_systemtime(priv->mSyncInsId,
											&(SystemTime.mStcUs),
											&(SystemTime.mSystemTimeUs));
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SystemTime,
						sizeof(SystemTime))) {
					return -EFAULT;
				}
			}
		break;

		case MEDIASYNC_IOC_GET_NEXTVSYNC_TIME:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_nextvsync_systemtime(priv->mSyncInsId,
								&NextVsyncSystemTime);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&NextVsyncSystemTime,
						sizeof(NextVsyncSystemTime)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_SPEED:
			if (copy_from_user((void *)&SyncSpeed,
					(void *)arg,
					sizeof(SyncSpeed)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_mediatime_speed(priv->mSyncInsId,
								SyncSpeed);
		break;

		case MEDIASYNC_IOC_GET_SPEED:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_mediatime_speed(priv->mSyncInsId,
								&SyncSpeed);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncSpeed,
						sizeof(SyncSpeed)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_PAUSE:
			if (copy_from_user((void *)&SyncPaused,
						(void *)arg,
						sizeof(SyncPaused)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_paused(priv->mSyncInsId,
							SyncPaused);
		break;

		case MEDIASYNC_IOC_GET_PAUSE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_paused(priv->mSyncInsId,
							&SyncPaused);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncPaused,
						sizeof(SyncPaused)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_SYNCMODE:
			if (copy_from_user((void *)&SyncMode,
						(void *)arg,
						sizeof(SyncMode)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_syncmode(priv->mSyncInsId,
							SyncMode);
		break;

		case MEDIASYNC_IOC_GET_SYNCMODE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_syncmode(priv->mSyncInsId,
							&SyncMode);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncMode,
						sizeof(SyncMode)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_GET_TRACKMEDIATIME:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_get_trackmediatime(priv->mSyncInsId,
								&TrackMediaTime);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&TrackMediaTime,
						sizeof(TrackMediaTime))) {
					return -EFAULT;
				}
			}
		break;

		default:
			pr_info("invalid cmd:%d\n", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long mediasync_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	switch (cmd) {
		case MEDIASYNC_IOC_INSTANCE_ALLOC:
		case MEDIASYNC_IOC_INSTANCE_GET:
		case MEDIASYNC_IOC_INSTANCE_BINDER:
		case MEDIASYNC_IOC_UPDATE_MEDIATIME:
		case MEDIASYNC_IOC_GET_MEDIATIME:
		case MEDIASYNC_IOC_GET_SYSTEMTIME:
		case MEDIASYNC_IOC_GET_NEXTVSYNC_TIME:
		case MEDIASYNC_IOC_SET_SPEED:
		case MEDIASYNC_IOC_GET_SPEED:
		case MEDIASYNC_IOC_SET_PAUSE:
		case MEDIASYNC_IOC_GET_PAUSE:
		case MEDIASYNC_IOC_SET_SYNCMODE:
		case MEDIASYNC_IOC_GET_SYNCMODE:
		case MEDIASYNC_IOC_GET_TRACKMEDIATIME:
			return mediasync_ioctl(file, cmd, arg);
		default:
			return -EINVAL;
	}
	return ret;
}
#endif

static const struct file_operations mediasync_fops = {
	.owner = THIS_MODULE,
	.open = mediasync_open,
	.release = mediasync_release,
	.unlocked_ioctl = mediasync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mediasync_compat_ioctl,
#endif
};

static struct class_attribute mediasync_class_attrs[] = {
    __ATTR_NULL
};

static struct class mediasync_class = {
	.name = "mediasync",
	.class_attrs = mediasync_class_attrs,
};

static int __init mediasync_module_init(void)
{
	int r;

	r = class_register(&mediasync_class);

	if (r) {
		pr_err("mediasync class create fail.\n");
		return r;
	}

	/* create tsync device */
	r = register_chrdev(MEDIASYNC_MAJOR, "mediasync", &mediasync_fops);
	if (r < 0) {
		pr_info("Can't register major for tsync\n");
		goto err2;
	}

	mediasync_dev = device_create(&mediasync_class, NULL,
				MKDEV(MEDIASYNC_MAJOR, 0), NULL, MEDIASYNC_DEVICE_NAME);

	if (IS_ERR(mediasync_dev)) {
		pr_err("Can't create mediasync_dev device\n");
		goto err1;
	}
	return 0;

err1:
	unregister_chrdev(MEDIASYNC_MAJOR, "mediasync");
err2:
	class_unregister(&mediasync_class);

	return 0;
}

static void __exit mediasync_module_exit(void)
{
	device_destroy(&mediasync_class, MKDEV(MEDIASYNC_MAJOR, 0));
	unregister_chrdev(MEDIASYNC_MAJOR, "mediasync");
	class_unregister(&mediasync_class);
}

module_init(mediasync_module_init);
module_exit(mediasync_module_exit);

MODULE_DESCRIPTION("AMLOGIC media sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lifeng Cao <lifeng.cao@amlogic.com>");

