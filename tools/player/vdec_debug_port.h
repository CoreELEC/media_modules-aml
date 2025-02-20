#ifndef __MM_DEC_DEBUG_PORT__
#define __MM_DEC_DEBUG_PORT__


typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long ulong;

#define LPRINT0
#define LPRINT1(...)        printf(__VA_ARGS__)

#define ERRP(con, rt, p, ...) do {    \
    if (con) {                        \
        LPRINT##p(__VA_ARGS__);       \
        rt;                           \
    }                                 \
} while(0)


#define MAX_PIC_SIZE  (4096 * 2304)

/* max stream buffer size */
#define MALLOC_BUF_SIZE (1024 * 1024 * 16)

#define FMT_NV12_SIZE(w, h)  ((w * h * 3) << 1)

#define VDBG_POLL_TIMEOUT 1000

#define DEC_DEBUG_PORT_DEV  "/sys/kernel/debug/vdec_profile/debug_port"

#define DUMP_FILE_PATH "/data/tmp/"


enum vformat_e {
	VFORMAT_MPEG12 = 0,
	VFORMAT_MPEG4,
	VFORMAT_H264,
	VFORMAT_MJPEG,
	VFORMAT_REAL,
	VFORMAT_JPEG,
	VFORMAT_VC1,
	VFORMAT_AVS,
	VFORMAT_YUV,		/* Use SW decoder */
	VFORMAT_H264MVC,
	VFORMAT_H264_4K2K,
	VFORMAT_HEVC,
	VFORMAT_H264_ENC,
	VFORMAT_JPEG_ENC,
	VFORMAT_VP9,
	VFORMAT_AVS2,
	VFORMAT_AV1,
	VFORMAT_AVS3,
	VFORMAT_MAX
};


#define _VDP_  'P'
#define VDBG_IOC_PORT_CFG      _IOW((_VDP_), 0x01, int)
#define VDBG_IOC_GET_DATA      _IOW((_VDP_), 0x02, int)
#define VDBG_IOC_DATA_DONE     _IOW((_VDP_), 0x03, int)

#define VDBG_IOC_BUF_RESET     _IOW((_VDP_), 0x0a, int)
#define VDBG_IOC_GET_VINFO     _IOW((_VDP_), 0x0b, int)

#define CMD_DUMP_YUV   0x1
#define CMD_DUMP_CRC   0x2
#define CMD_DUMP_ES    0x4
#define CMD_DUMP_AUX   0x10

struct debug_config_param {
	int type;
	int id;
	u32 pic_start;  // yuv dump start pic
	u32 pic_num;    // yuv dump pic num
	u32 mode;       // dump es mode;

	char *buf;
	u32 buf_size;

	u32 reserved[32];
};

#define PADING_SIZE 1024
#define PACKET_HEADER  (0xaa55aa55)

/*
#define PORT_DATA_TYPE_YUV 1
#define PORT_DATA_TYPE_CRC 2
#define PORT_DATA_TYPE_ES  3
*/
#define MAX_INSTANCE_NUM 9

enum data_type{
	TYPE_INFO,
	TYPE_YUV,
	TYPE_CRC,
	TYPE_ES,
	TYPE_AUX,
	TYPE_MAX
};

struct port_data_packet {
	int header;
	int id;
	int type;
	u32 data_size;
	u32 crc;
	u32 private_data_size;
	char private[0];   //private to PADING_SIZE end
};

struct port_vdec_info {
	int id;
	int format;
	int double_write;
	int stream_w;
	int stream_h;
	int dw_w;
	int dw_h;
	int plane_num;
	int bitdepth;
	int is_interlace;
	u32 reserved[32];
};

#endif /* __MM_DEC_DEBUG_PORT__ */

