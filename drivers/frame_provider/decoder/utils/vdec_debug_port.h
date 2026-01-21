#ifndef __VDEC_DEBUG_UTILS__
#define __VDEC_DEBUG_UTILS__

#include "vdec.h"
#include <linux/atomic.h>

/* the buf type from interface called outside */
#define VIRT_ADDR_TYPE 0
#define PHYS_ADDR_TYPE 1

struct vdec_dbg_local_buf {
	dos_addr_t phyadr;   //buf_start
	u32 size;            //buf_size
	char *start;         //buf_vaddr
	char *wp;
	char *rp;
	char *end;           //start + size

	struct mutex lock;
	wait_queue_head_t wqt;
	char *last_wp;
	u32 filled;
};


#define _VDP_  'P'
/*
-#define VDBG_IOC_PORT_CFG      _IOW((_VDP_), 0x01, int)
-#define VDBG_IOC_GET_DATA      _IOR((_VDP_), 0x02, int)
-#define VDBG_IOC_DATA_DONE     _IOW((_VDP_), 0x03, int)
-#define VDBG_IOC_BUF_RESET     _IOW((_VDP_), 0x0a, int)
-#define VDBG_IOC_GET_VINFO     _IOW((_VDP_), 0x0b, int)
*/
#define VDEC_EXPORT_PACKET           _IOR((_VDP_), 0x02, struct vdec_dbg_ex_usr)
#define VDEC_EXPORT_CONFIG           _IOW((_VDP_), 0x03, int)
#define VDEC_EXPORT_NOTIFY           _IOW((_VDP_), 0x04, int)
#define VDEC_EXPORT_LOCAL_BUF_RST    _IOW((_VDP_), 0x0a, int)   //to compat with version 0
#define VDEC_EXPORT_VERSION          _IOR((_VDP_), 0x0f, int)


#define VDEC_EXPORT_VERIFY 0

#define PACKET_HEADER  (0xaa55aa55)

/* type for read data inside */
#define DEV_READ_LOCAL  0
#define DEV_READ_VIRT   1
#define DEV_READ_PHYS   2
#define DEV_READ_MMAP   3

typedef u32 (*COPY_FUNC)(char *dst, const void *src, u32 size);

#define file_name_str(file, fmt, args... ) snprintf(file, sizeof(file), \
	fmt, ##args)


/* it must be same with user defined */
struct vdec_dbg_ex_usr {
	int header;
	int id;       //the packet count
	int type;     //0: local buf, 1: not local buf but virt, 2: phys buf direct
	u32 data_size;
	u32 crc;
	u32 offset;
	u32 addr_h;
	u32 addr_l;
	char file[64];     /* store to file name */
	u32 reserved[32];
};

struct vdec_dbg_export_u {
	struct list_head list;
	struct vdec_dbg_ex_usr pusr;
};

struct vdec_dbg_export_s {
	spinlock_t lock;

	struct list_head head_ready;
	struct list_head head_recycle;
	u32 cnt_ready;
	u32 cnt_recycle;

	struct vdec_dbg_export_u ue_block;
	atomic_t ue_ready_flag;  //ue_block ready and wait user read
	wait_queue_head_t wblk;

	u32 total;
	/* wait to data export */
	struct vdec_dbg_export_u *cur_unit;

	int vdec_id;
};


struct vdec_debug_export_core {

	struct vdec_dbg_export_s *export;

	struct vdec_dbg_local_buf *buf; //exp_buf;

	wait_queue_head_t poll_wait;

	int busy_flag;

	dev_t  dev_num;

	struct cdev cdev;

	struct class *class;

	struct device *device;

	struct dentry *entry;
};


#endif
