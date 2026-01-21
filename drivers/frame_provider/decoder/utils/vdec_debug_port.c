#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/nmi.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/compat.h>


#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "vdec_debug_port.h"
#include "vdec.h"
#include "frame_check.h"
#include <linux/crc32.h>

/*
before    : v0.0
2025-10-25: v1.0
2026-03-05: v1.1
*/
#define VDEC_DEBUG_DRV_VERSION  (1)
#define VDEC_DEBUG_SUB_VERSION  (1)

#define DEVICE_NAME "vdec_debug"
#define CLASS_NAME  "vdec_debug"

#define LPRINT0
#define LPRINT1(...)        printk(__VA_ARGS__)

#define ERRP(con, rt, p, ...) do {    \
    if (con) {                        \
        LPRINT##p(__VA_ARGS__);       \
        rt;                           \
    }                                 \
} while(0)


static struct vdec_debug_export_core *mcore;

static u32 dec_debug_buf_size;
module_param(dec_debug_buf_size, uint, 0664);

static u32 debug;
module_param(debug, uint, 0664);

#define PR_TEST(fmt, args...)  {if (debug & 0x1) pr_info(fmt, ##args);}

#define PR_EMG(fmt, args...)  {if (debug & 0x2) pr_info(fmt, ##args);}

static bool is_vdec_releasing(void)
{
	struct vdec_s *vdec;
	int id = mcore->export->vdec_id;

	if (id < 0)
		return false;

	vdec = vdec_get_vdec_by_id(id);
	if (vdec) {
		if (vdec->next_status == VDEC_STATUS_DISCONNECTED)
			return true;
	}

	return false;
}

static void vdec_export_ready_notify(void)
{
	struct vdec_dbg_export_s *export = mcore->export;

	if (export->cnt_ready || atomic_read(&export->ue_ready_flag))
		wake_up_interruptible(&mcore->poll_wait);
}

static struct vdec_dbg_local_buf *local_buf_init(u32 size)
{
	struct vdec_dbg_local_buf *buf;

	if (!size)
		return NULL;

	buf = (struct vdec_dbg_local_buf *)vzalloc(sizeof(struct vdec_dbg_local_buf));
	if (!buf)
		return NULL;

	if (dec_debug_buf_size) {
		size = dec_debug_buf_size;
		pr_info("%s, force local buf 0x%x\n", __func__, size);
	}

	size = round_up(size, PAGE_SIZE);

	buf->phyadr = codec_mm_alloc_for_dma(
			"VDEC_DEBUG",
			size / PAGE_SIZE,
			4 + PAGE_SHIFT,
			(CODEC_MM_FLAGS_CMA_FIRST |
			CODEC_MM_FLAGS_DMA |
			CODEC_MM_FLAGS_FOR_VDECODER));
	if (!buf->phyadr)
		pr_err("%s, alloc buf phy failed\n", __func__);
	else
		buf->start = codec_mm_vmap(buf->phyadr, size);

	if (!buf->start) {
		if (buf->phyadr)
			codec_mm_free_for_dma("VDEC_DEBUG", buf->phyadr);
		buf->phyadr = 0;
		buf->start = (char *)vzalloc(size);
		if (!buf->start) {
			pr_err("%s, alloc local buf failed\n", __func__);
			vfree(buf);
			return NULL;
		}
	}
	buf->size = size;
	buf->rp = buf->start;
	buf->wp = buf->start;
	buf->last_wp = buf->wp;
	buf->end = buf->start + size;
	buf->filled = 0;
	pr_info("%s, vmaped buf %px, phy %lx, size %x\n", __func__,
		buf->start, buf->phyadr, size);

	mutex_init(&buf->lock);
	init_waitqueue_head(&buf->wqt);

	return buf;
}

static void local_buf_release(struct vdec_dbg_local_buf *buf)
{
	if (!buf)
		return;

	if (buf->phyadr) {
		if (buf->start) {
			codec_mm_unmap_phyaddr(buf->start);
			buf->start = NULL;
		}
		codec_mm_free_for_dma("VDEC_DEBUG", buf->phyadr);
		buf->phyadr = 0;
	} else {
		if (buf->start)
			vfree(buf->start);
		buf->start = NULL;
	}

	buf->size = 0;
	buf->filled = 0;
	buf->rp = NULL;
	buf->wp = NULL;
	buf->end = NULL;

	vfree(buf);
}

static void local_buf_reset(struct vdec_dbg_local_buf *buf)
{
	mutex_lock(&buf->lock);
	buf->rp = buf->start;
	buf->wp = buf->start;
	buf->end = buf->start + buf->size;
	buf->last_wp = buf->start;
	buf->filled = 0;
	mutex_unlock(&buf->lock);

	pr_info("%s\n", __func__);
}

static u32 local_buf_remain(struct vdec_dbg_local_buf *buf)
{
	return (buf->size - buf->filled);
}

static int local_buf_space_wait(struct vdec_dbg_local_buf *buf, u32 size)
{
	u32 timeout_cnt = 0;
	long ret;

	if (size > buf->size) {
		pr_err("%s, buf_size %x, data %x\n",
			__func__, buf->size, size);
		return -ENOMEM;
	}

	while (1) {
		if (is_vdec_releasing())
			return -EAGAIN;

		if (local_buf_remain(buf) >= size)
			break;

		vdec_export_ready_notify();

		ret = wait_event_interruptible_timeout(buf->wqt,
				(local_buf_remain(buf) >= size),
				msecs_to_jiffies(200));
		if (ret < 0)
			return -EINTR;
		else if (ret == 0) {
			if (++timeout_cnt > 3) {
				pr_err("%s timeout, size 0x%x < 0x%x\n",
					__func__, local_buf_remain(buf), size);
				timeout_cnt = 0;
				return -ENOMEM;
			}
		}
		PR_TEST("%s, buf start %px, rp %px, wp %px\n",
			__func__, buf->start, buf->rp, buf->wp);
	}

	return 0;
}

static int local_buf_status(struct vdec_dbg_local_buf *buf)
{
	mutex_lock(&buf->lock);
	if ((buf->wp < buf->start) || (buf->wp > buf->end) ||
		(buf->rp < buf->start) || (buf->rp >= buf->end) ) {
			pr_err("%s, buf start %px, end %px, rp %px, wp %px \n",
				__func__, buf->start, buf->end, buf->rp, buf->wp);
			mutex_unlock(&buf->lock);
			return -EINVAL;
	}
	mutex_unlock(&buf->lock);

	return 0;
}

/* BUF_TYPE: VIRT, PHYS USER */
static u32 local_buf_write(struct vdec_dbg_local_buf *buf,
	const void *data, u32 size, COPY_FUNC copy)
{
	u32 left, total = 0, ret = 0;
	char *wp = buf->wp;
	void *src = (void *)data;

	if (wp < buf->rp) {
		if (buf->rp - wp < size) {
			pr_err("%s, over write wp %px, rp %px, size 0x%x\n",
				__func__, wp, buf->rp, size);
			return total;
		}
		ret = copy(wp, src, size);
		ERRP(ret < size, return 0, 1, "%s failed\n", __func__);
		wp += size;
		total = size;
	} else {
		left = buf->end - wp;   // space from wp to buffer end

		if (left < size) {
			// first chunk till buffer end
			ret = copy(wp, src, left);
			ERRP(ret < left, return 0, 1, "%s failed\n", __func__);
			total = left;

			wp = buf->start;
			src += left;
			size -= left;

			// second chunk from buffer start
			ret = copy(wp, src, size);
			ERRP(ret < size, goto wr_end, 1, "%s failed\n", __func__);
			wp += size;
			total += size;
		} else {
			ret = copy(wp, src, size);
			ERRP(ret < size, return 0, 1, "%s failed\n", __func__);
			wp += size;
			total = size;
		}
	}

wr_end:
	mutex_lock(&buf->lock);
	buf->last_wp = buf->wp;
	buf->wp = wp;
	buf->filled += total;
	mutex_unlock(&buf->lock);

	if (buf->filled > buf->size) {
		pr_err("%s, filled size larger than buf size. (%x > %x)\n",
			__func__, buf->filled, buf->size);
		local_buf_reset(buf);
		total = 0;
	}

	PR_EMG("update_wp, wp %px, filled %x\n", buf->wp, buf->filled);

	return total;
}

static int local_buf_rp_update(struct vdec_dbg_local_buf *buf, u32 size)
{
	bool reset_flag = 0;

	mutex_lock(&buf->lock);

	if ((buf->rp + size) >= buf->end)
		buf->rp = buf->rp + size - buf->end + buf->start;
	else
		buf->rp = buf->rp + size;

	if (buf->filled >= size)
		buf->filled -= size;
	else {
		pr_err("filled size is less than read data size (%d < %d)\n",
			buf->filled, size);
		reset_flag = 1;
	}
	mutex_unlock(&buf->lock);

	if (reset_flag)
		local_buf_reset(buf);

	local_buf_status(buf);

	PR_EMG("update_rp: rp %px, filled %x\n",
		buf->rp, buf->filled);

	return 0;
}

static unsigned long vdec_export_packet_lock(struct vdec_dbg_export_s *export)
{
	unsigned long flags;

	spin_lock_irqsave(&export->lock, flags);

	return flags;
}

static void vdec_export_packet_unlock(struct vdec_dbg_export_s *export, unsigned long flags)
{
	spin_unlock_irqrestore(&export->lock, flags);
}

static struct vdec_dbg_export_u *vdec_export_idle_unit_get(struct vdec_dbg_export_s *export)
{
	struct vdec_dbg_export_u *ue = NULL, *tmp;
	ulong flags;

	flags = vdec_export_packet_lock(export);
	if (!list_empty(&export->head_recycle)) {
		list_for_each_entry_safe(ue, tmp, &export->head_recycle, list) {
			list_del(&ue->list);
			break;
		}
		export->cnt_recycle--;
		memset(&ue->pusr, 0, sizeof(ue->pusr));
	}
	vdec_export_packet_unlock(export, flags);

	return ue;
}

static int vdec_export_local_buf_packet(struct vdec_dbg_export_s *export,
	struct vdec_dbg_local_buf *buf, char *file, int size)
{
	struct vdec_dbg_export_u *ue;
	struct vdec_dbg_ex_usr *pusr;
	ulong flags;

	ue = vdec_export_idle_unit_get(export);
	if (!ue) {
		ue = vzalloc(sizeof(struct vdec_dbg_export_u));
		if (!ue) {
			pr_err("%s, alloc node failed\n", __func__);
			return -ENOMEM;
		}
	}

	flags = vdec_export_packet_lock(export);
	pusr = &ue->pusr;
	pusr->header    = PACKET_HEADER;
	pusr->id        = export->total;
	pusr->type      = DEV_READ_LOCAL;
	pusr->data_size = size;
	pusr->offset    = (buf->last_wp == buf->end) ? 0 : (buf->last_wp - buf->start);
	pusr->addr_h    = 0;
	pusr->addr_l    = 0;
#if VDEC_EXPORT_VERIFY
	pusr->crc       = crc32_le(0, buf->last_wp, size);
#endif
	memcpy(pusr->file, file, 64);
	list_add_tail(&ue->list, &export->head_ready);
	export->cnt_ready++;
	export->total++;
	vdec_export_packet_unlock(export, flags);

	PR_TEST("packdata: id %d, size %x, offset %x, file %s\n",
		pusr->id, size, pusr->offset, file);

	return 0;
}

/* non-block mode, copy to local buf */
static u32 copy_phys_to_buf(char *to, const void *from, u32 size)
{
	u8 *virt = NULL;
	u32 map_size = SZ_1M;
	u32 stride = SZ_1M;
	u32 res = size;
	dos_addr_t phy = (dos_addr_t)from;
	u32 total = 0;
	int retry = 0;

	if (!size)
		return 0;

	do {
		stride = (res > map_size)? map_size : res;
		do {
			virt = codec_mm_vmap(phy, stride);
			if (!virt) {
				if (++retry > 10) {
					pr_err("%s map failed\n", __func__);
					return 0;
				}
				map_size >>= 1;
				stride = map_size;
			}
		} while(!virt);

		codec_mm_dma_flush(virt, stride, DMA_FROM_DEVICE);
		memcpy(to, virt, stride);
		phy += stride;
		to += stride;
		total += stride;
		res -= stride;
		codec_mm_unmap_phyaddr(virt);
		virt = NULL;
	} while (res);

	return total;
}

static u32 copy_virt_to_buf(char *to, const void *from, u32 size)
{
	if (!size)
		return 0;

	memcpy(to, from, size);

	return size;
}

static int vdec_export_non_block(char *file, const void *src, int size, int adr_type)
{
	int ret = 0;
	int res = 0;
	struct vdec_debug_export_core *core = mcore;
	COPY_FUNC copy;

	if ((!src) || (!size))
		return 0;

	ret = local_buf_space_wait(core->buf, size);
	if (ret < 0)
		return ret;

	PR_EMG("filled %x, start %px, wp %px, packcnt %d\n",
		core->buf->filled,
		core->buf->start,
		core->buf->wp, core->export->cnt_ready);

	if (adr_type == PHYS_ADDR_TYPE)
		copy = copy_phys_to_buf;
	else if (adr_type == VIRT_ADDR_TYPE)
		copy = copy_virt_to_buf;
	else {
		pr_err("%s, src addr is %s <%px>\n", __func__,
			access_ok(src, size)? "userbuf": "unknown", src);
		return -1;
	}

	res = local_buf_write(core->buf, src, size, copy);
	if (!res) {
		res = local_buf_write(core->buf, src, size, copy);
		if (!res)
			return -ENOMEM;
	}

	ret = local_buf_status(core->buf);
	if (ret < 0) {
		local_buf_reset(core->buf);
		return -EFAULT;
	}

	vdec_export_local_buf_packet(core->export, core->buf, file, res);

	vdec_export_ready_notify();

	return 0;
}


static int vdec_export_block(char *file, const char *src, u32 size, int adr_type)
{
	struct vdec_dbg_export_s *export = mcore->export;
	struct vdec_dbg_ex_usr *pusr = &export->ue_block.pusr;
	ulong flags;
	u64 addr = (u64)(uintptr_t)src;
	u32 timeout_cnt = 0;

	flags = vdec_export_packet_lock(export);
	pusr->header    = PACKET_HEADER;
	pusr->id        = export->total;
	pusr->data_size = size;
	pusr->offset    = 0;
	pusr->addr_h    = upper_32_bits(addr);
	pusr->addr_l    = lower_32_bits(addr);
	if (adr_type == PHYS_ADDR_TYPE) {
		pusr->type  = DEV_READ_PHYS;
		pusr->crc   = 0;
	} else {
		pusr->type  = DEV_READ_VIRT;
#if VDEC_EXPORT_VERIFY
		pusr->crc   = crc32_le(0, src, size);
#endif
	}
	memcpy(pusr->file, file, 64);
	export->total++;
	atomic_set(&export->ue_ready_flag, 1);
	vdec_export_packet_unlock(export, flags);

	PR_TEST("%s, id %d, size %x, addr %px, file %s\n",
		__func__, pusr->id, size, src, file);

	vdec_export_ready_notify();

	while (atomic_read(&export->ue_ready_flag)) {
		if (!wait_event_interruptible_timeout(export->wblk,
				(!atomic_read(&export->ue_ready_flag)),
				msecs_to_jiffies(50))) {

			if ((atomic_read(&export->ue_ready_flag) < 2) &&
				is_vdec_releasing()) {
				atomic_set(&export->ue_ready_flag, 0);
				timeout_cnt = 0;
			}

			if (++timeout_cnt > 100) {
				pr_err("%s timeout, ready flag %d, dump failed\n",
					__func__, atomic_read(&export->ue_ready_flag));
				timeout_cnt = 50;
				return -EBUSY;
			}
		}
	}

	return 0;
}

static int vdec_export_data(char *file, const void *src, int size,
	int addr_type, int vdec_id)
{
	struct vdec_debug_export_core *core = mcore;

	if (!core->busy_flag)
		pr_warn("WARN: please Launch vdec_debug app\n");

	core->export->vdec_id = vdec_id;

	if (size < (core->buf->size >> 3))
		vdec_export_non_block(file, src, size, addr_type);
	else
		vdec_export_block(file, src, size, addr_type);

	return 0;
}

static ssize_t vdec_dbg_export_write(struct file *file,
	const char __user *buf, size_t count, loff_t *offset)
{
	return count;
}

static __poll_t vdec_dbg_export_poll(struct file *file,
		poll_table *wait_table)
{
	struct vdec_debug_export_core *core =
		(struct vdec_debug_export_core *)file->private_data;
	struct vdec_dbg_export_s *export = core->export;

	if (core->busy_flag < 0)
		return POLLIN | POLLRDNORM;

	if (!export->cnt_ready && !atomic_read(&export->ue_ready_flag)) {
		poll_wait(file, &core->poll_wait, wait_table);
		return 0;
	}
	PR_TEST("poll wait done, packcnt %d, ue ready %d\n",
		export->cnt_ready, atomic_read(&export->ue_ready_flag));

	return POLLIN | POLLRDNORM;

}

static int vdec_dbg_export_unit_recycle(struct vdec_dbg_export_s *export)
{
	unsigned long flags;
	struct vdec_dbg_export_u *ue = export->cur_unit;

	flags = vdec_export_packet_lock(export);

	list_add_tail(&ue->list, &export->head_recycle);
	export->cnt_recycle++;
	export->cur_unit = NULL;

	vdec_export_packet_unlock(export, flags);

	return 0;
}

static u32 copy_phys_to_user(char __user *to, unsigned long padr, u32 size)
{
	u8 *adr;
	u32 len = size;
	u32 retry = 0;
	int ret;
	u32 sum = 0;;

	while (size) {
		len = (size > SZ_1M) ? SZ_1M : size;

		adr = codec_mm_vmap(padr, len);
		if (!adr) {
			retry++;
			if (retry > 10)
				break;
			continue;
		}
		retry = 0;

		ret = copy_to_user(to, adr, len);
		codec_mm_unmap_phyaddr(adr);

		if (ret) {
			u32 copied = len - ret;
			sum += copied;
			break;
		}

		sum += len;
		padr += len;
		to += len;
		size -= len;
	}

	return sum;
}

static ssize_t vdec_dbg_export_read(struct file *file,
	char __user *usr_buf, size_t count, loff_t *offset)
{
	struct vdec_debug_export_core *core =
		(struct vdec_debug_export_core *)file->private_data;
	struct vdec_dbg_export_s *export = core->export;
	struct vdec_dbg_ex_usr *pusr = &export->cur_unit->pusr;
	int res, ret = 0;
	ulong src;

	if (core->busy_flag < 0)
		return 0;

	if (!pusr)
		return -EFAULT;

	src = (ulong)(((u64)pusr->addr_l) | ((u64)pusr->addr_h << 32));

	if (pusr->type == DEV_READ_PHYS) {
		atomic_set(&export->ue_ready_flag, 3);

		ret = copy_phys_to_user(usr_buf, src, pusr->data_size);

		PR_TEST("%s, phys %lx, id %d, size %x, copied %x\n",
			__func__, src, pusr->id, pusr->data_size, ret);

		if (ret != pusr->data_size)
			pr_warn("%s, copied %x less than data size %x\n",
				__func__, ret, pusr->data_size);

		goto ret_copy;
	} else if (pusr->type == DEV_READ_VIRT) {
		atomic_set(&export->ue_ready_flag, 3);

		res = copy_to_user(usr_buf, (char *)src, pusr->data_size);
		ret = pusr->data_size - res;

		PR_TEST("%s, virt %lx, id %d, size %x, copied %x\n",
			__func__, src, pusr->id, pusr->data_size, ret);

		if (res)
			pr_warn("%s, copied %x less than size %x\n",
				__func__, pusr->data_size - res, pusr->data_size);

		goto ret_copy;
	} else if (pusr->type == DEV_READ_LOCAL) {
		struct vdec_dbg_local_buf *buf = core->buf;
		char *read_pos;
		u32 read_size;
		u32 to_copy;

		if (pusr->offset >= buf->size) {
			pr_warn("%s: invalid offset %x >= buf size %x\n",
				__func__, pusr->offset, buf->size);
			ret = 0;
			goto recycle;
		}

		read_pos = buf->start + pusr->offset;

		PR_TEST("%s, id %d, count %zx, rp %px, offset %x\n",
			__func__, pusr->id, count, buf->rp, pusr->offset);

		/* Determine how many bytes we can copy */
		to_copy = pusr->data_size;
		if (count < to_copy) {
			pr_debug("%s: count %zx < data_size %u, partial copy\n",
				__func__, count, pusr->data_size);
			to_copy = count;
		}

		if (to_copy > (buf->end - read_pos)) {
			read_size = buf->end - read_pos;
			if (read_size > to_copy)
				read_size = to_copy;
			res = copy_to_user(usr_buf, read_pos, read_size);
			if (!res && (to_copy - read_size) > 0)
				res = copy_to_user(usr_buf + read_size, buf->start, to_copy - read_size);
		} else
			res = copy_to_user(usr_buf, read_pos, to_copy);

		ret = to_copy - res;
		if (ret > 0) {
			local_buf_rp_update(buf, ret);
			wake_up_interruptible(&buf->wqt);
		} else {
			pr_debug("%s: no data copied, recycle\n", __func__);
		}

recycle:
		memset(pusr, 0, sizeof(struct vdec_dbg_ex_usr));
		vdec_dbg_export_unit_recycle(export);
	}

	return ret;

ret_copy:
	memset(pusr, 0, sizeof(struct vdec_dbg_ex_usr));
	export->cur_unit = NULL;
	atomic_set(&export->ue_ready_flag, 0);
	wake_up_interruptible(&export->wblk);

	return ret;
}

static int vdec_dbg_export_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;

#ifdef VMAP_LOCAL_BUF
	struct vdec_debug_export_core *core =
		(struct vdec_debug_export_core *)file->private_data;

	struct vdec_dbg_local_buf *buf = core->buf;

	if (!core || !core->buf) {
		pr_err("%s NULL buf\n", __func__);
		return -EFAULT;
	}
	buf = core->buf;

	if (vma->vm_pgoff > buf->size) {
		pr_info("%s pgoff: %lx, bufsize 0x%x\n", __func__,
			vma->vm_pgoff, buf->size);
		return -EFAULT;
	}

	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
	ret = remap_pfn_range(vma, vma->vm_start,
		(buf->phyadr >> PAGE_SHIFT) + vma->vm_pgoff,
		vma->vm_end - vma->vm_start,
		vma->vm_page_prot);
	pr_info("%s, ret %d, start %lx, end %lx, offset %lx\n",
		__func__, ret, vma->vm_start, vma->vm_end, vma->vm_pgoff);
#endif

#if 0
	struct vdec_dbg_ex_usr *pusr;
	ulong phys;

	if (!core || !core->export || !core->export->cur_unit)
		return -EFAULT;
	pusr = &core->export->cur_unit->pusr;
	if ((pusr->type == DEV_READ_MMAP) && (pusr->data_size)) {
		if (vma->vm_pgoff > pusr->data_size) {
			pr_info("%s pgoff: %lx, data size 0x%x\n", __func__,
				vma->vm_pgoff, pusr->data_size);
			return -EFAULT;
		}
		phys = (((ulong)pusr->addr_h << 32) | (ulong)pusr->addr_l);
		vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
		ret = remap_pfn_range(vma, vma->vm_start,
			(phys >> PAGE_SHIFT) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);
	}
#endif

	return ret;
}

static struct vdec_dbg_ex_usr *vdec_dbg_req_pusr_info(struct vdec_dbg_export_s *export)
{
	unsigned long flags;
	struct vdec_dbg_export_u *ue, *tmp;

	flags = vdec_export_packet_lock(export);

	if (list_empty(&export->head_ready) || !export->cnt_ready) {
		vdec_export_packet_unlock(export, flags);
		return NULL;
	}

	list_for_each_entry_safe(ue, tmp, &export->head_ready, list) {
		list_del(&ue->list);
		break;
	}
	export->cnt_ready--;
	export->cur_unit = ue;

	vdec_export_packet_unlock(export, flags);

	return &ue->pusr;
}

static long vdec_dbg_export_ioctl(struct file *file, unsigned int cmd, unsigned long param)
{
	long ret = 0;
	struct vdec_debug_export_core *core =
		(struct vdec_debug_export_core *)file->private_data;
	int version[2] = {VDEC_DEBUG_DRV_VERSION, VDEC_DEBUG_SUB_VERSION};
	struct vdec_dbg_export_s *export = core->export;
	struct vdec_dbg_ex_usr *pusr;

	switch (cmd) {
		case VDEC_EXPORT_VERSION:
			if (copy_to_user((void *)param, version, sizeof(version)))
				return -EFAULT;
			break;
		case VDEC_EXPORT_PACKET:
			if (core->busy_flag < 0) {
				pusr = &export->ue_block.pusr;
				memset(pusr->file, 0, sizeof(pusr->file));
				strcpy(pusr->file, "EXIT");
				if (copy_to_user((void *)param, pusr, sizeof(struct vdec_dbg_ex_usr)))
					return -EFAULT;
				return ret;
			}
#if 0
			if (export->cur_unit && (export->cur_unit != &export->ue_block)) {
				pr_warn("%s, drop exported data which haven't been read\n", __func__);
				vdec_dbg_export_unit_recycle(export);
			}
#endif
			if (atomic_read(&export->ue_ready_flag)) {
				atomic_set(&export->ue_ready_flag, 2);
				export->cur_unit = &export->ue_block;
				pusr = &export->cur_unit->pusr;
			} else
				pusr = vdec_dbg_req_pusr_info(core->export);
			if (!pusr)
				return -EFAULT;

			if (copy_to_user((void *)param, pusr, sizeof(struct vdec_dbg_ex_usr)))
				return -EFAULT;
			break;
		case VDEC_EXPORT_CONFIG:
			break;
		case VDEC_EXPORT_NOTIFY:
			wake_up_interruptible(&core->buf->wqt);
			break;
		case VDEC_EXPORT_LOCAL_BUF_RST:
			local_buf_reset(core->buf);
			break;

		default:
			break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vdec_dbg_export_compat_ioctl(struct file *file,
		unsigned int cmd, ulong arg)
{
	return vdec_dbg_export_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif

static int vdec_dbg_export_open(struct inode *inode, struct file *file)
{
	struct vdec_debug_export_core *core = mcore;

	if (core->busy_flag)
		return -EBUSY;

	core->busy_flag = 1;

	file->private_data = (void *)core;

	pr_info("%s\n", __func__);

	return 0;
}

static int vdec_dbg_export_close(struct inode *inode, struct file *file)
{
	struct vdec_debug_export_core *core =
		(struct vdec_debug_export_core *)file->private_data;

	pr_info("%s\n", __func__);

	core->busy_flag = 0;

	atomic_set(&core->export->ue_ready_flag, 0);

	return 0;
}

static const struct file_operations vdec_dbg_export_fops = {
	.owner   = THIS_MODULE,
	.open    = vdec_dbg_export_open,
	.read    = vdec_dbg_export_read,
	.write   = vdec_dbg_export_write,
	//.llseek  = no_llseek,
	.unlocked_ioctl = vdec_dbg_export_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vdec_dbg_export_compat_ioctl,
#endif
	.release = vdec_dbg_export_close,
	.poll    = vdec_dbg_export_poll,
	.mmap    = vdec_dbg_export_mmap,
};


static void vdec_dbg_export_free(struct vdec_dbg_export_s *export)
{
	unsigned long flags;
	struct vdec_dbg_export_u *ue, *tmp;

	while (!list_empty(&export->head_ready)) {
		flags = vdec_export_packet_lock(export);
		list_for_each_entry_safe(ue, tmp, &export->head_ready, list) {
			list_del(&ue->list);
			break;
		}
		vdec_export_packet_unlock(export, flags);
		vfree(ue);
	}
	export->cnt_ready = 0;

	while (!list_empty(&export->head_recycle)) {
		flags = vdec_export_packet_lock(export);
		list_for_each_entry_safe(ue, tmp, &export->head_recycle, list) {
			list_del(&ue->list);
			break;
		}
		vdec_export_packet_unlock(export, flags);
		vfree(ue);
	}
	export->cnt_recycle = 0;
	vfree(export);
}

static void vdec_dbg_export_destroy(struct vdec_debug_export_core *core)
{
	if (!core)
		return;

	if (atomic_read(&core->export->ue_ready_flag) == 1) {
		atomic_set(&core->export->ue_ready_flag, 0);
		usleep_range(20000, 21000);
	}

	/* unregister decoder common interface */
	vdec_debug_port_unregister();

	if (core->busy_flag) {
		pr_info("file port busy, force exit\n");
		core->busy_flag = -1;
		do {
			wake_up_interruptible(&core->poll_wait);
			usleep_range(10000, 15000);
		} while (core->busy_flag != 0);
	}

	local_buf_release(core->buf);
	core->buf = NULL;

	vdec_dbg_export_free(core->export);
	core->export = NULL;
}

static int vdec_dbg_export_init(struct vdec_debug_export_core *core)
{
	if (!core)
		return -1;

	core->export = vzalloc(sizeof(struct vdec_dbg_export_s));
	if (!core->export) {
		pr_err("%s, alloc export failed\n", __func__);
		return -ENOMEM;
	}

	spin_lock_init(&core->export->lock);

	INIT_LIST_HEAD(&core->export->head_ready);
	INIT_LIST_HEAD(&core->export->head_recycle);

	core->export->cnt_ready = 0;
	core->export->cnt_recycle = 0;
	init_waitqueue_head(&core->export->wblk);

	core->buf = local_buf_init(SZ_1M * 16);
	if (!core->buf) {
		vfree(core->export);
		return -ENOMEM;
	}

	init_waitqueue_head(&core->poll_wait);

	/* register interfaces to decoder_common */
	vdec_debug_port_register(vdec_export_data);

	return 0;
}

static int vdec_debug_dev_register(struct vdec_debug_export_core *core)
{
	int ret;
	struct dentry *entry_root;

	if (core == NULL)
		return -1;

	ret = alloc_chrdev_region(&core->dev_num, 0, 1, DEVICE_NAME);
	if (ret) {
		pr_err("Failed to allocate char dev region\n");
		return ret;
	}

	cdev_init(&core->cdev, &vdec_dbg_export_fops);
	ret = cdev_add(&core->cdev, core->dev_num, 1);
	if (ret) {
		pr_err("Failed to add cdev\n");
		goto unregister_chrdev;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 3, 13)
	core->class = class_create(THIS_MODULE, CLASS_NAME);
#else
	core->class = class_create(CLASS_NAME);
#endif
	if (IS_ERR(core->class)) {
		pr_err("Failed to create class\n");
		ret = PTR_ERR(core->class);
		goto del_cdev;
	}

	core->device = device_create(core->class, NULL, core->dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(core->device)) {
		pr_err("Failed to create device\n");
		ret = PTR_ERR(core->device);
		goto destroy_class;
	}

	pr_info("chardev initialized: /dev/%s\n", DEVICE_NAME);

	/* debug fs create */
	entry_root = debugfs_lookup("vdec_profile", NULL);
	if (!entry_root)
		pr_info("debugfs_lookup vdec_profile dir failed\n");

	core->entry = debugfs_create_file("debug_port",
		0666, entry_root, NULL, &vdec_dbg_export_fops);
	if (!core->entry)
		pr_info("%s create failed\n", __func__);
	else
		pr_info("vdec debugfs entry created\n");

	return 0;

destroy_class:
	class_destroy(core->class);
del_cdev:
	cdev_del(&core->cdev);
unregister_chrdev:
	unregister_chrdev_region(core->dev_num, 1);
	return ret;
}

static void vdec_debug_dev_unregister(struct vdec_debug_export_core *core)
{
	if (!core)
		return;

	if (core->entry) {
		debugfs_remove(core->entry);
		pr_info("vdec debugfs entry removed\n");
	}

	device_destroy(core->class, core->dev_num);

	class_destroy(core->class);

	cdev_del(&core->cdev);

	unregister_chrdev_region(core->dev_num, 1);
}

static int __init vdec_debug_module_init(void)
{
	int ret;
	struct vdec_debug_export_core *core;

	core = (struct vdec_debug_export_core *)vzalloc(
		sizeof(struct vdec_debug_export_core));
	if (core == NULL)
		return -ENOMEM;

	ret = vdec_debug_dev_register(core);
	if (ret < 0) {
		vfree(core);
		return ret;
	}

	ret = vdec_dbg_export_init(core);
	if (ret < 0) {
		vdec_debug_dev_unregister(core);
		vfree(core);
		return ret;
	}
	mcore = core;

	pr_info("VDEC DEBUG VERSION %d.%d\n", VDEC_DEBUG_DRV_VERSION, VDEC_DEBUG_SUB_VERSION);

	return 0;
}

static void __exit vdec_debug_module_exit(void)
{
	if (mcore) {
		vdec_dbg_export_destroy(mcore);

		vdec_debug_dev_unregister(mcore);

		vfree(mcore);
		mcore = NULL;
	}
}

module_init(vdec_debug_module_init);
module_exit(vdec_debug_module_exit);

MODULE_DESCRIPTION("AMLOGIC vdec debug Driver");
MODULE_LICENSE("GPL");
