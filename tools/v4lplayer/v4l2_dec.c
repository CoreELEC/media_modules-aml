/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#ifdef WITHOUT_KERNEL
#include "include/videodev.h"
#else
#include <linux/videodev2.h>
#endif

#include "aml_driver.h"
#include "vcodec_utils.h"
#include "v4l2_dec.h"
#include "aml_uvm.h"

static const char* video_dev_name = "/dev/video26";
static int video_fd;
static bool get_1st_data = false;
static int cur_output_index = -1;
static decode_finish_fn decode_finish_cb;
static bool res_evt_pending;
static bool eos_evt_pending;
static bool eos_received;
extern int g_dw_mode;
extern int g_dump_dec_info_num;
extern int g_output_flag;
extern int g_log_level;
static pthread_mutex_t res_lock;
static enum v4l2_memory sInMemMode;
static uint8_t* es_buf;
//#define DEBUG_FRAME
#ifdef DEBUG_FRAME
static int frame_checksum;
#endif
FILE *yuv_fp;
FILE *crc_fp;

struct decoder_info_config {
	uint8_t* buff;
	FILE *fp;
	enum E_DECINFO_EVENT event;
	bool is_used;
	int output_count;
};

#define DEC_INFO_BUF_SIZE (1024)
#define DEC_INFO_COUNT (9)

struct timer_config {
	timer_t timerid;
	struct sigevent sev;
	struct itimerspec its;
	bool is_start;
};

struct timer_config statistic_info_timer;

struct decoder_info_config dec_info[DEC_INFO_COUNT];

static const char *dec_info_name[] = {
	"aml_stream_info",
	"aml_statistic_info",
	"aml_afd_info",
	"aml_cc_info",
	"aml_hdr10_info",
	"aml_hrr10p_info",
	"aml_cuva_info",
	"aml_amdv_info",
	"aml_frame_info",
};

static int mjpeg_flag = 0;
static int canvas_h = 0;
static int canvas_w = 0;
static int real_yuv_h = 0;
static int real_yuv_w = 0;

#define ES_BUF_SIZE (6*1024*1024)

static pthread_t dec_thread;
bool quit_thread;

struct frame_buffer {
	struct v4l2_buffer v4lbuf;
	struct v4l2_plane v4lplane[2];
	uint8_t *vaddr[2];
	int gem_fd[2];
	bool queued;

	/* output only */
	uint32_t used;
	struct secmem* smem;

	/* capture only */
	bool free_on_recycle;
};

struct port_config {
	enum v4l2_buf_type type;
	uint32_t pixelformat;
	struct v4l2_format sfmt;
	int buf_num;
	int plane_num;
	struct frame_buffer** buf;
	pthread_mutex_t lock;
	pthread_cond_t wait;
	bool is_stream_on;
};

#define OUTPUT_BUF_CNT 6
static struct port_config output_p;
static struct port_config capture_p;

static int d_o_push_num;
static int d_o_rec_num;
static int d_c_push_num;
static int d_c_rec_num;

struct profile {
	bool res_change;
	struct timeval last_flag;
	struct timeval buffer_done;
	struct timeval first_frame;
};
static struct profile res_profile;

static enum vformat_e v4l2_fourcc_to_vtype(uint32_t fourcc) {
	switch (fourcc) {
		case V4L2_PIX_FMT_H264:
			return VFORMAT_H264;
		case V4L2_PIX_FMT_HEVC:
			return VFORMAT_HEVC;
		case V4L2_PIX_FMT_VP9:
			return VFORMAT_VP9;
		case V4L2_PIX_FMT_MPEG2:
			return VFORMAT_MPEG12;
		case V4L2_PIX_FMT_MPEG4:
			return VFORMAT_MPEG4;
		case V4L2_PIX_FMT_MJPEG:
			return VFORMAT_MJPEG;
		case V4L2_PIX_FMT_AV1:
			return VFORMAT_AV1;
		case V4L2_PIX_FMT_AVS:
			return VFORMAT_AVS;
		case V4L2_PIX_FMT_AVS2:
			return VFORMAT_AVS2;
		case V4L2_PIX_FMT_AVS3:
			return VFORMAT_AVS3;
		case V4L2_PIX_FMT_VC1_ANNEX_G:
			return VFORMAT_VC1;
		case V4L2_PIX_FMT_H266:
			return VFORMAT_H266;
	}
	return VFORMAT_MAX;
}

#define V4L2_CID_USER_AMLOGIC_BASE (V4L2_CID_USER_BASE + 0x1100)
#define AML_V4L2_DEC_PARMS_CONFIG (V4L2_CID_USER_AMLOGIC_BASE + 7)
#define AML_V4L2_SET_STREAM_MODE (V4L2_CID_USER_AMLOGIC_BASE + 9)
#define AML_V4L2_GET_DECINFO_SET (V4L2_CID_USER_AMLOGIC_BASE + 13)

#define V4L2_EVENT_PRIVATE_EXT_VSC_BASE (V4L2_EVENT_PRIVATE_START + 0x2000)
#define V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO (V4L2_EVENT_PRIVATE_EXT_VSC_BASE + 4)

static uint32_t get_driver_min_buffers (int fd, bool capture_port)
{
	struct v4l2_control control = { 0, };

	if (!capture_port)
		control.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
	else
		control.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

	if (!ioctl(fd, VIDIOC_G_CTRL, &control))
		return control.value;

	return 0;
}

static int setup_output_port(int fd)
{
	int i,ret;
	struct v4l2_requestbuffers req;

	req.count = OUTPUT_BUF_CNT;
	req.memory = sInMemMode;
	req.type = output_p.type;

	ret = ioctl(fd, VIDIOC_REQBUFS, &req);
	if (ret < 0) {
		debug_print(DEBUG_ERROR, "output VIDIOC_REQBUFS fail ret:%d\n", ret);
		return 1;
	}
	output_p.buf_num = req.count;
	debug_print(DEBUG_STATE, "output gets %d buf\n", req.count);

	output_p.buf = calloc(req.count, sizeof(struct frame_buffer *));
	if (!output_p.buf) {
		debug_print(DEBUG_ERROR, "%d oom\n", __LINE__);
		return 2;
	}
	for (i = 0 ; i < req.count ; i++)
		output_p.buf[i] = calloc(1, sizeof(struct frame_buffer));

	for (i = 0; i < req.count; i++) {
		int j;
		struct frame_buffer* pb = output_p.buf[i];
		pb->v4lbuf.index = i;
		pb->v4lbuf.type = output_p.type;
		pb->v4lbuf.memory = sInMemMode;
		pb->v4lbuf.length = output_p.plane_num;
		pb->v4lbuf.m.planes = pb->v4lplane;

		ret = ioctl(fd, VIDIOC_QUERYBUF, &pb->v4lbuf);
		if (ret) {
			debug_print(DEBUG_ERROR, "VIDIOC_QUERYBUF %dth buf fail ret:%d\n", i, ret);
			return 3;
		}

		if (sInMemMode != V4L2_MEMORY_MMAP)
			continue;
		for (j = 0; j < output_p.plane_num; j++) {
			void *vaddr;
			vaddr = mmap(NULL, pb->v4lplane[j].length,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			fd, pb->v4lplane[j].m.mem_offset);
			if (vaddr == MAP_FAILED) {
				debug_print(DEBUG_ERROR, "%s mmap failed len:%d offset:%x\n", __func__,
					pb->v4lplane[j].length, pb->v4lplane[j].m.mem_offset);
				return 4;
			}
			pb->vaddr[j] = (uint8_t *)vaddr;
		}

	}

	pthread_mutex_init(&output_p.lock, NULL);
	pthread_cond_init(&output_p.wait, NULL);

	if (sInMemMode == V4L2_MEMORY_DMABUF) {
		es_buf = malloc(ES_BUF_SIZE);
		if (!es_buf) {
			debug_print(DEBUG_ERROR, "%d OOM\n", __LINE__);
			return 5;
		}
	}
	return 0;
}

static int destroy_output_port(int fd) {
	int i;
	struct v4l2_requestbuffers req = {
		.memory = sInMemMode,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.count = 0,
	};
	pthread_mutex_destroy(&output_p.lock);
	pthread_cond_destroy(&output_p.wait);
	ioctl(fd, VIDIOC_REQBUFS, &req);
	for (i = 0 ; i < req.count ; i++) {
		struct frame_buffer *buf = output_p.buf[i];
		free(buf);
	}
	free(output_p.buf);
	return 0;
}

static int get_size_ratio(int dec_mode)
{
	int ratio = 1;
	uint32_t dm = dec_mode & VDEC_MODE_DW_MASK;

	switch (dm) {
	case DM_YUV_1_4_AVBC_A:
	case DM_YUV_1_4_AVBC_B:
		ratio = 4;
		break;
	case DM_YUV_1_2_AVBC:
		ratio = 2;
		break;
	case DM_YUV_1_8_AVBC:
		ratio = 8;
		break;
	default:
		break;
	}

	return ratio;
}

static void detect_res_change(struct v4l2_format *old, struct v4l2_format *new)
{
	if ((old->fmt.pix_mp.width != 0 &&
		old->fmt.pix_mp.height != 0) &&
		(old->fmt.pix_mp.width != new->fmt.pix_mp.width ||
		old->fmt.pix_mp.height != new->fmt.pix_mp.height)) {
		debug_print(DEBUG_DEF, "res_change: (%dx%d) --> (%dx%d)\n", old->fmt.pix_mp.width,
			old->fmt.pix_mp.height, new->fmt.pix_mp.width,
			new->fmt.pix_mp.height);
	}
}

static int setup_capture_port(int fd)
{
	int i,ret;
	int ratio;
	uint32_t coded_w, coded_h;
	struct v4l2_requestbuffers req;
	struct v4l2_selection selection;
	struct v4l2_format old;

	old.fmt.pix_mp.width = capture_p.sfmt.fmt.pix_mp.width;
	old.fmt.pix_mp.height = capture_p.sfmt.fmt.pix_mp.height;
	/* coded size should be ready now */
	memset(&capture_p.sfmt, 0, sizeof(struct v4l2_format));
	capture_p.sfmt.type = capture_p.type;
	ret = ioctl(video_fd, VIDIOC_G_FMT, &capture_p.sfmt);
	if (ret) {
		debug_print(DEBUG_ERROR, "%d VIDIOC_G_FMT fail :%d\n", __LINE__, ret);
		return -1;
	}
	coded_w = capture_p.sfmt.fmt.pix_mp.width;
	coded_h = capture_p.sfmt.fmt.pix_mp.height;

	detect_res_change(&old, &capture_p.sfmt);
	selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	selection.target = V4L2_SEL_TGT_CROP_DEFAULT;
	ret = ioctl(video_fd, VIDIOC_G_SELECTION, &selection);
	if (ret) {
		debug_print(DEBUG_ERROR, "%d VIDIOC_G_SELECTION fail :%d\n", __LINE__, ret);
		return 1;
	}

	ratio = get_size_ratio(g_dw_mode);
	real_yuv_w = selection.r.width / ratio;
	real_yuv_h = selection.r.height / ratio;

	canvas_w = coded_w / ratio;
	canvas_h = coded_h / ratio;
	debug_print(DEBUG_DEF, "capture port visible(%dx%d), coded(%dx%d), dw(%d)\n",
		selection.r.width, selection.r.height, coded_w, coded_h, g_dw_mode);

	capture_p.sfmt.fmt.pix_mp.pixelformat =
		(output_p.pixelformat == V4L2_PIX_FMT_MJPEG) ?
		V4L2_PIX_FMT_YUV420 : V4L2_PIX_FMT_NV21;

	ret = ioctl(video_fd, VIDIOC_S_FMT, &capture_p.sfmt);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_S_FMT fail %d\n", ret);
		return -1;
	}
	debug_print(DEBUG_DEF, "set capture port to %s\n",
		(output_p.pixelformat == V4L2_PIX_FMT_MJPEG) ?
		(g_dw_mode == VDEC_DW_AFBC_ONLY)?"I420":"I420M" :
		(g_dw_mode == VDEC_DW_AFBC_ONLY)?"NV21":"NV21M");

	capture_p.plane_num = 1;

	memset(&req, 0 , sizeof(req));
	req.memory = V4L2_MEMORY_DMABUF;
	req.type = capture_p.type;
	req.count = get_driver_min_buffers(fd, true);
	if (!req.count) {
		debug_print(DEBUG_ERROR, "get min buffers fail\n");
		return -1;
	}

	debug_print(DEBUG_DEF, "req capture count:%d\n", req.count);
	ret = ioctl(fd, VIDIOC_REQBUFS, &req);
	if (ret < 0) {
		debug_print(DEBUG_ERROR, "capture VIDIOC_REQBUFS fail ret:%d\n", ret);
		return 1;
	}
	capture_p.buf_num = req.count;
	debug_print(DEBUG_DEF, "capture gets %d buf\n", req.count);

	capture_p.buf = calloc(req.count, sizeof(struct frame_buffer *));
	if (!capture_p.buf) {
		debug_print(DEBUG_ERROR, "%d oom\n", __LINE__);
		return 2;
	}

	for (i = 0 ; i < req.count ; i++) {
		capture_p.buf[i] = calloc(1, sizeof(struct frame_buffer));
		if (!capture_p.buf[i]) {
			debug_print(DEBUG_ERROR, "%d oom\n", __LINE__);
			return 2;
		}
	}

	for (i = 0; i < req.count; i++) {
		struct frame_buffer* pb = capture_p.buf[i];
		int fd_capture;

		pb->v4lbuf.index = i;
		pb->v4lbuf.type = capture_p.type;
		pb->v4lbuf.memory = V4L2_MEMORY_DMABUF;
		pb->v4lbuf.length = capture_p.plane_num;
		pb->v4lbuf.m.planes = pb->v4lplane;

		/* allocate DRM-GEM buffers */
		ret = alloc_uvm_buffer(canvas_w, canvas_h, (void**)&pb->vaddr[0], i, &fd_capture);
		if (ret) {
			debug_print(DEBUG_ERROR, "alloc_uvm_buffer %dth fail\n", i);
			return 2;
		}

		ret = ioctl(video_fd, VIDIOC_QUERYBUF, &pb->v4lbuf);
		if (ret) {
			debug_print(DEBUG_ERROR, "VIDIOC_QUERYBUF %dth buf fail ret:%d\n", i, ret);
			return 3;
		}

		pb->v4lbuf.m.fd = fd_capture;

		ret = ioctl(video_fd, VIDIOC_QBUF, &capture_p.buf[i]->v4lbuf);
		if (ret) {
			debug_print(DEBUG_ERROR, "VIDIOC_QBUF %dth buf fail ret:%d\n", i, ret);
			return 5;
		}
		capture_p.buf[i]->queued = true;
	}
	gettimeofday(&res_profile.buffer_done, NULL);

	return 0;
}

static int destroy_capture_port(int fd) {
	int i;
	struct v4l2_requestbuffers req = {
		.memory = V4L2_MEMORY_DMABUF,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.count = 0,
	};
	pthread_mutex_destroy(&capture_p.lock);
	pthread_cond_destroy(&capture_p.wait);
	ioctl(fd, VIDIOC_REQBUFS, &req);
	for (i = 0 ; i < req.count ; i++) {
		/* release GEM buf */
		free_uvm_buffers();
		free(capture_p.buf[i]);
	}
	free(capture_p.buf);
	return 0;
}

static void handle_res_change()
{
	int i, ret;
	int free_cnt = 0, delay_cnt = 0;
	struct v4l2_requestbuffers req = {
		.memory = V4L2_MEMORY_DMABUF,
		.type = capture_p.type,
		.count = 0,
	};

	if (capture_p.is_stream_on) {
		/* stop capture port */
		ret = ioctl(video_fd, VIDIOC_STREAMOFF, &capture_p.type);
		if (ret) {
			debug_print(DEBUG_ERROR, "VIDIOC_STREAMOFF fail ret:%d\n",ret);
			goto error;
		}
		capture_p.is_stream_on = false;
		pthread_mutex_lock(&res_lock);
		/* return all DRM-GEM buffers */
		ret = ioctl(video_fd, VIDIOC_REQBUFS, &req);
		if (ret) {
			debug_print(DEBUG_ERROR, "VIDIOC_REQBUFS fail ret:%d\n",ret);
			pthread_mutex_unlock(&res_lock);
			goto error;
		}
		for (i = 0 ; i < capture_p.buf_num; i++) {
			if (capture_p.buf[i]->queued) {
				free_uvm_buffers();
				free(capture_p.buf[i]);
				free_cnt++;
			} else {
				capture_p.buf[i]->free_on_recycle = true;
				delay_cnt++;
			}
		}
		pthread_mutex_unlock(&res_lock);
		free(capture_p.buf);
		debug_print(DEBUG_DEF, "f/d/total: (%d/%d/%d)\n", free_cnt, delay_cnt, capture_p.buf_num);
	}

	/* setup capture port again */
	ret = setup_capture_port(video_fd);
	if (ret) {
		debug_print(DEBUG_ERROR, " setup_capture_port fail ret:%d\n",ret);
		goto error;
	}

	/* start capture again */
	ret = ioctl(video_fd, VIDIOC_STREAMON, &capture_p.type);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_STREAMON fail ret:%d\n",ret);
		goto error;
	}
	capture_p.is_stream_on = true;
	return;

error:
	debug_print(DEBUG_ERROR, "debug...\n");
	while (1)
		sleep(1);
}

static uint32_t v4l2_get_event(int fd, struct v4l2_event *evt)
{
	int ret;
	//struct v4l2_event evt = { 0 };

	ret = ioctl(fd, VIDIOC_DQEVENT, evt);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_DQEVENT fail, ret:%d\n", ret);
		return 0;
	}
	return evt->type;
}

#define DUMP_FILE_PATH "/data/tmp/"
#define ALIGN(x, align) (((x) + (align) - 1) & ~((align) - 1))

static uint8_t *aml_yuv_dump(FILE *fp, uint8_t *start_addr,
	uint32_t real_width, uint32_t real_height, uint32_t align)
{
	uint32_t index;
	uint32_t coded_width = ALIGN(real_width, align);
	uint32_t coded_height = ALIGN(real_height, align);
	uint8_t *yuv_data_addr = start_addr;

	if (real_width != coded_width) {
		for (index = 0; index < real_height; index++) {
			fwrite(yuv_data_addr, 1, real_width, fp);
			yuv_data_addr += coded_width;
		}
	} else {
		fwrite(yuv_data_addr, 1, real_width * real_height, fp);
	}

	return (start_addr + coded_width * coded_height);
}

static int do_fream_check(struct frame_buffer * frame_buff)
{
	char file_name[64] = {0};
	uint8_t *y_vaddr = frame_buff->vaddr[0];
	uint8_t *uv_vaddr = frame_buff->vaddr[0]
		+ (ALIGN(real_yuv_w, 64) * ALIGN(real_yuv_h, 64));

	if (g_output_flag  & FRAME_DUMP_YUV) {
		if (!yuv_fp) {
			memset(file_name, 0, sizeof(file_name));
			snprintf(file_name, sizeof(file_name), "%s%dx%d.yuv",
				DUMP_FILE_PATH, real_yuv_w, real_yuv_h);
			debug_print(DEBUG_DEF, "dump yuv to %s\n", file_name);
			if ((yuv_fp = fopen(file_name, "wb")) == NULL) {
				debug_print(DEBUG_ERROR, "open yuv_fp file error!\n");
				return -1;
			}
		}

		uv_vaddr = aml_yuv_dump(yuv_fp, y_vaddr, real_yuv_w, real_yuv_h, 64);
		aml_yuv_dump(yuv_fp, uv_vaddr, real_yuv_w, real_yuv_h / 2, 64);
	}

	if ((g_output_flag & FRAME_PRINT_CRC) ||
		(g_output_flag & FRAME_DUMP_CRC)) {
		unsigned int crc_y = 0;
		unsigned int crc_uv = 0;

		if (real_yuv_w == canvas_w) {
			int size_y = real_yuv_w * real_yuv_h;
			int size_uv = size_y >> (1 + mjpeg_flag);

			crc_y = crc32_le(crc_y, y_vaddr, size_y);
			crc_uv = crc32_le(crc_uv, uv_vaddr, size_uv);
		} else {
			int i;
			int usize = real_yuv_w;
			int stride = canvas_w;

			for (i = 0; i < real_yuv_h; i++) {
				crc_y = crc32_le(crc_y, y_vaddr, usize);
				y_vaddr += stride;
			}
			for (i = 0; i < real_yuv_h /  2; i++) {
				crc_uv = crc32_le(crc_uv, uv_vaddr, usize);
				uv_vaddr += stride;
			}
		}

		if (g_output_flag & FRAME_PRINT_CRC)
			debug_print(DEBUG_DEF, "FRAME_CHECK: %08d: %08x %08x\n", d_c_rec_num, crc_y, crc_uv);

		if (g_output_flag & FRAME_DUMP_CRC) {
			if (!crc_fp) {
				memset(file_name, 0, sizeof(file_name));
				snprintf(file_name, sizeof(file_name), "%sframe_check.crc",
					DUMP_FILE_PATH);
				debug_print(DEBUG_DEF, "dump crc to %s\n", file_name);
				if ((crc_fp = fopen(file_name, "w")) == NULL) {
					debug_print(DEBUG_ERROR, "open crc_fp file error!\n");
					return -1;
				}
			}
			fprintf(crc_fp, "%08d: %08x %08x\n", d_c_rec_num, crc_y, crc_uv);
		}
	}
	return 0;
}

/******************* DEC INFO *******************/
int get_unused_dec_info_index(void)
{
	int i;
	for (i = 0; i < DEC_INFO_COUNT; i ++) {
		if (!dec_info[i].is_used)
			return i;
	}

	return -1;
}

int find_dec_info_index(enum E_DECINFO_EVENT event)
{
	int i;
	for (i = 0; i < DEC_INFO_COUNT; i ++) {
		if (dec_info[i].is_used &&
			dec_info[i].event == event)
			return i;
	}

	return -1;
}

int config_dec_info(enum E_DECINFO_EVENT event)
{
	int dec_info_index;
	char file_name[64] = {0};
	const char * name = dec_info_name[event];

	dec_info_index = find_dec_info_index(event);
	if (dec_info_index < 0) {
		dec_info_index = get_unused_dec_info_index();

		if (dec_info_index < 0) {
			debug_print(DEBUG_ERROR, "%s can't get unused info\n", name);
			return -1;
		}

		if (!dec_info[dec_info_index].fp) {
			memset(file_name, 0, sizeof(file_name));
			snprintf(file_name, sizeof(file_name), "%s%s.txt",
				DUMP_FILE_PATH, name);
			debug_print(DEBUG_DEF, "dump %s to %s\n", name, file_name);
			if ((dec_info[dec_info_index].fp = fopen(file_name, "w")) == NULL) {
				debug_print(DEBUG_ERROR, "open %s_fp file error!\n", name);
				return -2;
			}
		} else {
			debug_print(DEBUG_ERROR, "%s_fp has used\n", name);
			return -3;
		}

		dec_info[dec_info_index].is_used = true;
		dec_info[dec_info_index].event = event;
		dec_info[dec_info_index].output_count = 0;
	}

	if (event == AML_DECINFO_EVENT_AFD ||
		event == AML_DECINFO_EVENT_CC ||
		event == AML_DECINFO_EVENT_HDR10P ||
		event == AML_DECINFO_EVENT_CUVA ||
		event == AML_DECINFO_EVENT_AMDV) {
		if (dec_info[dec_info_index].buff) {
			memset(dec_info[dec_info_index].buff, 0, DEC_INFO_BUF_SIZE);
		} else {
			dec_info[dec_info_index].buff = malloc(DEC_INFO_BUF_SIZE);
			if (!dec_info[dec_info_index].buff) {
				debug_print(DEBUG_ERROR, "malloc %s_buf fail.\n", name);
				return -5;
			}
		}
	}

	return dec_info_index;
}

int get_dec_info(struct vdec_common_s *vdec_comm, int info_index)
{
	int ret = 0;
	struct v4l2_ext_control control;
	struct v4l2_ext_controls ctrls;
	const char * name = dec_info_name[dec_info[info_index].event];

	memset(&ctrls, 0, sizeof(ctrls));
	memset(&control, 0, sizeof(control));

	ctrls.count = 1;
	ctrls.controls = &control;

	vdec_comm->type = dec_info[info_index].event;
	control.ptr = vdec_comm;
	control.size = sizeof(struct vdec_common_s);
	control.id = AML_V4L2_GET_DECINFO_SET;

	if (vdec_comm->type == AML_DECINFO_EVENT_AFD ||
		vdec_comm->type == AML_DECINFO_EVENT_CC ||
		vdec_comm->type == AML_DECINFO_EVENT_HDR10P ||
		vdec_comm->type == AML_DECINFO_EVENT_CUVA ||
		vdec_comm->type == AML_DECINFO_EVENT_AMDV) {
		vdec_comm->u.usd_param.data_ptr = (uintptr_t)dec_info[info_index].buff;
		vdec_comm->u.usd_param.data_size = DEC_INFO_BUF_SIZE;
	}

	ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret) {
		debug_print(DEBUG_ERROR, "Ioctl set ext %s fail. ret:%d\n", name, ret);
		return ret;
	}

	ret = ioctl(video_fd, VIDIOC_G_EXT_CTRLS, &ctrls);
	if (ret) {
		debug_print(DEBUG_ERROR, "Ioctl get ext %s fail. ret:%d\n", name, ret);
		return ret;
	}

	return 0;
}

int save_stream_info(struct vdec_common_s *vdec_comm, int info_index)
{
	struct decoder_info_config *decoder_info = &dec_info[info_index];
	const char * name = dec_info_name[decoder_info->event];

	//dump
	if (decoder_info->fp &&
		decoder_info->output_count < g_dump_dec_info_num) {
		struct dec_stream_info_s info = vdec_comm->u.stream_info;
		decoder_info->output_count ++;

		fprintf(decoder_info->fp, "--------%s count:%d\n", name, decoder_info->output_count);
		fprintf(decoder_info->fp, "info_type:%d\n", info.info_type);
		fprintf(decoder_info->fp, "vdec_name:%s\n", info.vdec_name);
		fprintf(decoder_info->fp, "vdec_type:%d\n", info.vdec_type);
		fprintf(decoder_info->fp, "dual_core_flag:%d\n", info.dual_core_flag);
		fprintf(decoder_info->fp, "is_secure:%d\n", info.is_secure);
		fprintf(decoder_info->fp, "profile_idc:%d\n", info.profile_idc);
		fprintf(decoder_info->fp, "level_idc:%d\n", info.level_idc);
		fprintf(decoder_info->fp, "filed_flag:%d\n", info.filed_flag);
		fprintf(decoder_info->fp, "frame_width:%d\n", info.frame_width);
		fprintf(decoder_info->fp, "frame_height:%d\n", info.frame_height);
		fprintf(decoder_info->fp, "crop_top:%d\n", info.crop_top);
		fprintf(decoder_info->fp, "crop_bottom:%d\n", info.crop_bottom);
		fprintf(decoder_info->fp, "crop_left:%d\n", info.crop_left);
		fprintf(decoder_info->fp, "crop_right:%d\n", info.crop_right);
		fprintf(decoder_info->fp, "frame_rate:%d\n", info.frame_rate);
		fprintf(decoder_info->fp, "fence_enable:%d\n", info.fence_enable);
		fprintf(decoder_info->fp, "fast_output_enable:%d\n", info.fast_output_enable);
		fprintf(decoder_info->fp, "trick_mode:%d\n", info.trick_mode);
		fprintf(decoder_info->fp, "bit_depth:%d\n", info.bit_depth);
		fprintf(decoder_info->fp, "double_write_mode:0x%x\n", info.double_write_mode);
		fprintf(decoder_info->fp, "error_handle_policy:0x%x\n", info.error_handle_policy);
		fprintf(decoder_info->fp, "eu_aspect_ratio:%d\n", info.eu_aspect_ratio);
		fprintf(decoder_info->fp, "sar_width:%d\n", info.ratio_size.sar_width);
		fprintf(decoder_info->fp, "sar_height:%d\n", info.ratio_size.sar_height);
		fprintf(decoder_info->fp, "dar_width:%d\n", info.ratio_size.dar_width);
		fprintf(decoder_info->fp, "dar_height:%d\n", info.ratio_size.dar_height);
		fprintf(decoder_info->fp, "frame_dur:%d\n", info.frame_dur);
		fprintf(decoder_info->fp, "\n");

		debug_print(DEBUG_STATE, "save. %s count:%d\n", name, decoder_info->output_count);
	}

	return 0;
}

int save_statistic_info(struct vdec_common_s *vdec_comm, int info_index)
{
	struct decoder_info_config *decoder_info = &dec_info[info_index];
	const char * name = dec_info_name[decoder_info->event];

	//dump
	if (decoder_info->fp &&
		decoder_info->output_count < g_dump_dec_info_num) {
		struct dec_statistics_info_s statistics = vdec_comm->u.decoder_statistics;

		fprintf(decoder_info->fp, "--------%s count:%d\n", name, decoder_info->output_count);
		fprintf(decoder_info->fp, "info_type:%d\n", statistics.info_ype);
		fprintf(decoder_info->fp, "total_decoded_frames:%d\n", statistics.total_decoded_frames);
		fprintf(decoder_info->fp, "error_frames:%d\n", statistics.error_frames);
		fprintf(decoder_info->fp, "drop_frames:%d\n", statistics.drop_frames);
		fprintf(decoder_info->fp, "i_decoded_frames:%d\n", statistics.i_decoded_frames);
		fprintf(decoder_info->fp, "i_drop_frames:%d\n", statistics.i_drop_frames);
		fprintf(decoder_info->fp, "i_error_frames:%d\n", statistics.i_error_frames);
		fprintf(decoder_info->fp, "p_decoded_frames:%d\n", statistics.p_decoded_frames);
		fprintf(decoder_info->fp, "p_drop_frames:%d\n", statistics.p_drop_frames);
		fprintf(decoder_info->fp, "p_error_frames:%d\n", statistics.p_error_frames);
		fprintf(decoder_info->fp, "b_decoded_frames:%d\n", statistics.b_decoded_frames);
		fprintf(decoder_info->fp, "b_drop_frames:%d\n", statistics.b_drop_frames);
		fprintf(decoder_info->fp, "b_error_frames:%d\n", statistics.b_error_frames);
		fprintf(decoder_info->fp, "av_resynch_counter:%d\n", statistics.av_resynch_counter);
		fprintf(decoder_info->fp, "total_decoded_datasize:%d\n", statistics.total_decoded_datasize);
		fprintf(decoder_info->fp, "\n");

		debug_print(DEBUG_STATE, "save. %s count:%d\n", name, decoder_info->output_count);
		decoder_info->output_count ++;
	}

	return 0;
}

int save_hdr10_info(struct vdec_common_s *vdec_comm, int info_index)
{
	struct decoder_info_config *decoder_info = &dec_info[info_index];
	const char * name = dec_info_name[decoder_info->event];

	//dump
	if (decoder_info->fp &&
		decoder_info->output_count < g_dump_dec_info_num) {
		struct vframe_master_display_colour_s color_parms =
			vdec_comm->u.aux_data.hdr_info.color_parms;
		struct vframe_content_light_level_s level = color_parms.content_light_level;
		decoder_info->output_count ++;

		fprintf(decoder_info->fp, "--------%s count:%d\n", name, decoder_info->output_count);
		fprintf(decoder_info->fp, "info_type:%d\n", vdec_comm->u.aux_data.info_type);
		fprintf(decoder_info->fp, "signal_type:0x%x\n", vdec_comm->u.aux_data.hdr_info.signal_type);
		fprintf(decoder_info->fp, "present_flag:%d\n", color_parms.present_flag);
		fprintf(decoder_info->fp, "primaries_0_0:%d\n", color_parms.primaries[0][0]);
		fprintf(decoder_info->fp, "primaries_0_1:%d\n", color_parms.primaries[0][1]);
		fprintf(decoder_info->fp, "primaries_1_0:%d\n", color_parms.primaries[1][0]);
		fprintf(decoder_info->fp, "primaries_1_1:%d\n", color_parms.primaries[1][1]);
		fprintf(decoder_info->fp, "primaries_2_0:%d\n", color_parms.primaries[2][0]);
		fprintf(decoder_info->fp, "primaries_2_1:%d\n", color_parms.primaries[2][1]);
		fprintf(decoder_info->fp, "white_point_0:%d\n", color_parms.white_point[0]);
		fprintf(decoder_info->fp, "white_point_1:%d\n", color_parms.white_point[1]);
		fprintf(decoder_info->fp, "luminance_0:%d\n", color_parms.luminance[0]);
		fprintf(decoder_info->fp, "luminance_1:%d\n", color_parms.luminance[1]);
		fprintf(decoder_info->fp, "l_present_flag:%d\n", level.present_flag);
		fprintf(decoder_info->fp, "max_content:%d\n", level.max_content);
		fprintf(decoder_info->fp, "max_pic_average:%d\n", level.max_pic_average);
		fprintf(decoder_info->fp, "\n");

		debug_print(DEBUG_STATE, "save. %s count:%d\n", name, decoder_info->output_count);
	}

	return 0;
}

int save_frame_info(struct vdec_common_s *vdec_comm, int info_index)
{
	struct decoder_info_config *decoder_info = &dec_info[info_index];
	const char * name = dec_info_name[decoder_info->event];

	//dump
	if (decoder_info->fp &&
		decoder_info->output_count < g_dump_dec_info_num) {
		struct dec_frame_info_s info = vdec_comm->u.frame_info;
		decoder_info->output_count ++;

		fprintf(decoder_info->fp, "--------%s count:%d\n", name, decoder_info->output_count);
		fprintf(decoder_info->fp, "info_type:%d\n", info.info_type);
		fprintf(decoder_info->fp, "qos.num:%d\n", info.qos.num);
		fprintf(decoder_info->fp, "qos.type:%d\n", info.qos.type);
		fprintf(decoder_info->fp, "qos.size:%d\n", info.qos.size);
		fprintf(decoder_info->fp, "qos.pts:%d\n", info.qos.pts);
		fprintf(decoder_info->fp, "qos.max_qp:%d\n", info.qos.max_qp);
		fprintf(decoder_info->fp, "qos.avg_qp:%d\n", info.qos.avg_qp);
		fprintf(decoder_info->fp, "qos.min_qp:%d\n", info.qos.min_qp);
		fprintf(decoder_info->fp, "qos.max_skip:%d\n", info.qos.max_skip);
		fprintf(decoder_info->fp, "qos.avg_skip:%d\n", info.qos.avg_skip);
		fprintf(decoder_info->fp, "qos.min_skip:%d\n", info.qos.min_skip);
		fprintf(decoder_info->fp, "qos.max_mv:%d\n", info.qos.max_mv);
		fprintf(decoder_info->fp, "qos.min_mv:%d\n", info.qos.min_mv);
		fprintf(decoder_info->fp, "qos.avg_mv:%d\n", info.qos.avg_mv);
		fprintf(decoder_info->fp, "qos.decode_buffer:%d\n", info.qos.decode_buffer);
		fprintf(decoder_info->fp, "num:%d\n", info.num);
		fprintf(decoder_info->fp, "type:%d\n", info.type);
		fprintf(decoder_info->fp, "frame_poc:%d\n", info.frame_poc);
		fprintf(decoder_info->fp, "decode_time_cost:%d\n", info.decode_time_cost);
		fprintf(decoder_info->fp, "pic_width:%d\n", info.pic_width);
		fprintf(decoder_info->fp, "pic_height:%d\n", info.pic_height);
		fprintf(decoder_info->fp, "error_flag:%d\n", info.error_flag);
		fprintf(decoder_info->fp, "status:%d\n", info.status);
		fprintf(decoder_info->fp, "bitrate:%d\n", info.bitrate);
		fprintf(decoder_info->fp, "field_output_order:%d\n", info.field_output_order);
		fprintf(decoder_info->fp, "offset:%d\n", info.offset);
		fprintf(decoder_info->fp, "ratio_control:0x%x\n", info.ratio_control);
		fprintf(decoder_info->fp, "vf_type:0x%x\n", info.vf_type);
		fprintf(decoder_info->fp, "signal_type:0x%x\n", info.signal_type);
		fprintf(decoder_info->fp, "ext_signal_type:0x%x\n", info.ext_signal_type);
		fprintf(decoder_info->fp, "pts:%d\n", info.pts);
		fprintf(decoder_info->fp, "pts_us64:%d\n", info.pts_us64);
		fprintf(decoder_info->fp, "timestamp:%d\n", info.timestamp);
		fprintf(decoder_info->fp, "frame_size:%d\n", info.frame_size);
		fprintf(decoder_info->fp, "\n");

		debug_print(DEBUG_STATE, "save. %s count:%d\n", name, decoder_info->output_count);
	}

	return 0;
}

int dump_dec_info_buff_data(struct vdec_common_s *vdec_comm, int index)
{
	int buff_size = vdec_comm->u.usd_param.data_size;
	uint8_t* buff = (uint8_t*)(uintptr_t)(vdec_comm->u.usd_param.data_ptr);
	struct v4l_userdata_meta_data_t *mea_data = &vdec_comm->u.usd_param.meta_data;
	struct decoder_info_config *decoder_info = &dec_info[index];
	const char * name = dec_info_name[decoder_info->event];

	if (decoder_info->fp &&
		decoder_info->output_count < g_dump_dec_info_num) {
		int i;
		decoder_info->output_count ++;

		fprintf(decoder_info->fp, "--------%s count:%d\n", name, decoder_info->output_count);
		fprintf(decoder_info->fp, "meta_data:\n");
		fprintf(decoder_info->fp, "poc_number:%d\n", mea_data->poc_number);
		fprintf(decoder_info->fp, "flags:%d\n", mea_data->flags);
		fprintf(decoder_info->fp, "video_format:%d\n", mea_data->video_format);
		fprintf(decoder_info->fp, "extension_data:%d\n", mea_data->extension_data);
		fprintf(decoder_info->fp, "frame_type:%d\n", mea_data->frame_type);
		fprintf(decoder_info->fp, "vpts:%d\n", mea_data->vpts);
		fprintf(decoder_info->fp, "vpts_valid:%d\n", mea_data->vpts_valid);
		fprintf(decoder_info->fp, "timestamp:%lu\n", mea_data->timestamp);
		fprintf(decoder_info->fp, "records_in_que:%d\n", mea_data->records_in_que);
		fprintf(decoder_info->fp, "priv_data:%d\n", mea_data->priv_data);
		fprintf(decoder_info->fp, "buff_data: size:%d\n", buff_size);
		for (i = 0; i < buff_size; i++) {
			fprintf(decoder_info->fp, "%02x ", buff[i]);
			if (((i + 1) & 0xf) == 0)
				fprintf(decoder_info->fp, "\n");
		}
		fprintf(decoder_info->fp, "\n\n");

		debug_print(DEBUG_STATE, "save. %s count:%d\n", name, decoder_info->output_count);
	}

	return 0;
}


void process_statistic_info(union sigval v)
{
	int index = -1;
	int ret = 0;
	const char * name = dec_info_name[AML_DECINFO_EVENT_STATISTIC];
	struct vdec_common_s vdec_comm;
	memset(&vdec_comm, 0, sizeof(vdec_comm));

	index = config_dec_info(AML_DECINFO_EVENT_STATISTIC);
	if (index < 0) {
		debug_print(DEBUG_ERROR, "config_dec_info %s err. ret:%d\n", name);
		return;
	}

	ret = get_dec_info(&vdec_comm, index);
	if (ret < 0) {
		debug_print(DEBUG_ERROR, "get %s err. ret:%d\n", ret, name);
		return;
	}

	save_statistic_info(&vdec_comm, index);

	return;
}

int setup_statistic_info_timer()
{
	if (statistic_info_timer.is_start)
		return 0;

	memset(&statistic_info_timer.sev, 0, sizeof(struct sigevent));

	statistic_info_timer.sev.sigev_notify = SIGEV_THREAD;
	statistic_info_timer.sev.sigev_value.sival_ptr = &statistic_info_timer.timerid;
	statistic_info_timer.sev.sigev_notify_function = process_statistic_info;

	if (timer_create(CLOCK_REALTIME, &statistic_info_timer.sev, &statistic_info_timer.timerid) == -1) {
		debug_print(DEBUG_ERROR, "statistic_info_timer create err\n");
		return -1;
	}

	statistic_info_timer.its.it_value.tv_sec = 1;
	statistic_info_timer.its.it_value.tv_nsec = 0;
	statistic_info_timer.its.it_interval.tv_sec = 1;
	statistic_info_timer.its.it_interval.tv_nsec = 0;

	if (timer_settime(statistic_info_timer.timerid, 0, &statistic_info_timer.its, NULL) == -1) {
		debug_print(DEBUG_ERROR, "statistic_info_timer settime err\n");
		return -1;
	}

	statistic_info_timer.is_start = true;

	debug_print(DEBUG_STATE, "statistic_info_timer start\n");
	return 0;
}

int destroy_statistic_info_timer(void)
{
	if (!statistic_info_timer.is_start)
		return 0;

	timer_delete(statistic_info_timer.timerid);
	return 0;
}

int process_decoder_info(enum E_DECINFO_EVENT event)
{
	int index = -1;
	int ret = 0;
	const char * name = NULL;
	struct vdec_common_s vdec_comm;

	if (event > DEC_INFO_COUNT ||
		event < 0) {
		debug_print(DEBUG_ERROR, "process_decoder_info err. Unknown event:%d\n", event);
	}

	name = dec_info_name[event];
	memset(&vdec_comm, 0, sizeof(vdec_comm));

	index = config_dec_info(event);
	if (index < 0) {
		debug_print(DEBUG_ERROR, "config_dec_info %s err. ret:%d\n", name);
		return index;
	}

	ret = get_dec_info(&vdec_comm, index);
	if (ret < 0) {
		debug_print(DEBUG_ERROR, "get %s err. ret:%d\n", ret, name);
		return ret;
	}

	if (event == AML_DECINFO_EVENT_STREAM) {
		save_stream_info(&vdec_comm, index);
	} else if (event == AML_DECINFO_EVENT_STATISTIC) {
		setup_statistic_info_timer();
	} else if (event == AML_DECINFO_EVENT_HDR10) {
		save_hdr10_info(&vdec_comm, index);
	} else if (event == AML_DECINFO_EVENT_FRAME) {
		save_frame_info(&vdec_comm, index);
	} else if (event == AML_DECINFO_EVENT_AFD ||
		event == AML_DECINFO_EVENT_CC ||
		event == AML_DECINFO_EVENT_HDR10P ||
		event == AML_DECINFO_EVENT_CUVA ||
		event == AML_DECINFO_EVENT_AMDV) {
		dump_dec_info_buff_data(&vdec_comm, index);
	}

	return 0;
}

int destroy_dec_info(void)
{
	int i;
	for (i = 0; i < DEC_INFO_COUNT; i ++) {
		if (dec_info[i].fp)
			fclose(dec_info[i].fp);
		if (dec_info[i].buff)
			free(dec_info[i].buff);
		dec_info[i].is_used = false;
	}

	return 0;
}
/******************* DEC INFO END *******************/

static void *dec_thread_func(void * arg)
{
	int ret = 0;
	int error_count = 0;
	struct pollfd pfd = {
		/* default blocking capture */
		.events =  POLLIN | POLLRDNORM | POLLPRI | POLLOUT | POLLWRNORM,
		.fd = video_fd,
	};
	while (!quit_thread) {
		struct v4l2_buffer buf;
		struct v4l2_plane planes[2];

		for (;;) {
			ret = poll(&pfd, 1, 10);
			if (ret > 0)
				break;
			if (quit_thread)
				goto exit;
			if (errno == EINTR)
				continue;
		}

		/* error handling */
		if (pfd.revents & POLLERR) {
			error_count ++;
			if (error_count < 30) {
				debug_print(DEBUG_ERROR, "POLLERR received\n");
			} else if (error_count == 30) {
				debug_print(DEBUG_ERROR, "POLLERR conunt 30, pls check. \n");
			}
		}

		/* res change */
		if (pfd.revents & POLLPRI) {
			//uint32_t evt;
			struct v4l2_event evt = { 0 };
			v4l2_get_event(video_fd, &evt);
			if (evt.type == V4L2_EVENT_SOURCE_CHANGE &&
				evt.u.src_change.changes == V4L2_EVENT_SRC_CH_RESOLUTION) {
				res_evt_pending = true;
			} else if (evt.type == V4L2_EVENT_EOS) {
				eos_evt_pending = true;
			} else if (evt.type == V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO) {
				process_decoder_info(evt.id);
			}

			/* ignore res change for 1st frame */
			if (d_c_push_num == 0 && res_evt_pending) {
				res_evt_pending = false;
				handle_res_change();
			}
		}

		/* dqueue output buf */
		if (pfd.revents & (POLLOUT | POLLWRNORM)) {
			memset(&buf, 0, sizeof(buf));
			memset(planes, 0, sizeof(planes));
			buf.memory = sInMemMode;
			buf.type = output_p.type;
			buf.length = 2;
			buf.m.planes = planes;

			ret = ioctl(video_fd, VIDIOC_DQBUF, &buf);
			if (ret) {
				debug_print(DEBUG_ERROR, "output VIDIOC_DQBUF fail %d\n", ret);
			} else {
				struct frame_buffer *fb = output_p.buf[buf.index];
#ifdef DEBUG_FRAME
				debug_print(DEBUG_FRAME, "dqueue output %d\n", buf.index);
#endif
				pthread_mutex_lock(&output_p.lock);
				fb->queued = false;
				pthread_mutex_unlock(&output_p.lock);
				fb->used = 0;
				d_o_rec_num++;
				pthread_cond_signal(&output_p.wait);
			}
		}
		/* dqueue capture buf */
		if (pfd.revents & (POLLIN | POLLRDNORM)) {
			memset(&buf, 0, sizeof(buf));
			memset(planes, 0, sizeof(planes));
			buf.memory = sInMemMode;
			buf.type = capture_p.type;
			buf.length = 2;
			buf.m.planes = planes;

			ret = ioctl(video_fd, VIDIOC_DQBUF, &buf);
			if (ret) {
				debug_print(DEBUG_ERROR, "capture VIDIOC_DQBUF fail %d\n", ret);
			} else {
#ifdef DEBUG_FRAME
				debug_print(DEBUG_FRAME, "dqueue cap %d\n", buf.index);
#endif
				capture_p.buf[buf.index]->queued = false;
				d_c_push_num++;

				if (!(buf.flags & V4L2_BUF_FLAG_LAST)) {
					debug_print(DEBUG_STATE, "decoder work done :%d\n", d_c_rec_num);
					do_fream_check(capture_p.buf[buf.index]);
					capture_buffer_recycle(capture_p.buf[buf.index]);
				} else {
					//uint32_t evt = 0;
					struct v4l2_event evt = { 0 };
					if (!res_evt_pending && !eos_evt_pending)
						v4l2_get_event(video_fd, &evt);

					if (evt.type == V4L2_EVENT_SOURCE_CHANGE || res_evt_pending) {
						debug_print(DEBUG_DEF, "res changed\n");
						res_evt_pending = false;
						gettimeofday(&res_profile.last_flag, NULL);
						res_profile.res_change = true;
						/* this buffer will be freed in handle_res_change */
						capture_p.buf[buf.index]->queued = true;
						handle_res_change();
						d_c_rec_num++;
						continue;
					} else if (evt.type == V4L2_EVENT_EOS || eos_evt_pending) {
						debug_print(DEBUG_DEF, "EOS received\n");
						eos_received = true;
						eos_evt_pending = false;

						d_c_rec_num++;
						dump_v4l2_decode_state();
						decode_finish_cb();
						break;
					}
				}
			}
		}
	}

exit:
	/* stop output port */
	ret = ioctl(video_fd, VIDIOC_STREAMOFF, &output_p.type);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_STREAMOFF fail ret:%d\n",ret);
	}
	output_p.is_stream_on = false;

	/* stop capture port */
	ret = ioctl(video_fd, VIDIOC_STREAMOFF, &capture_p.type);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_STREAMOFF fail ret:%d\n",ret);
	}
	capture_p.is_stream_on = false;

	return NULL;
}

static int config_decoder(int fd, enum vformat_e type)
{
	int ret = -1;
	struct v4l2_ext_control control;
	struct v4l2_ext_controls ctrls;
	struct v4l2_streamparm para;
	struct v4l2_event_subscription sub;
	struct aml_dec_params *dec_p = (struct aml_dec_params *)para.parm.raw_data;

	memset(&ctrls, 0, sizeof(ctrls));
	memset(&control, 0, sizeof(control));
	memset(&para, 0 , sizeof(para));
	para.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	dec_p->parms_status = V4L2_CONFIG_PARM_DECODE_CFGINFO;
	dec_p->cfg.double_write_mode = g_dw_mode;

	dec_p->cfg.metadata_config_flag = 0x4000;

	/* number of extra buffer for display pipelien to run */
	/* MPEG will use hardcoded size in driver */
	if (type != VFORMAT_MPEG12)
		dec_p->cfg.ref_buf_margin = 7;

	control.id = AML_V4L2_DEC_PARMS_CONFIG;
	control.ptr = dec_p;
	control.size = sizeof(struct aml_dec_params);
	ctrls.count = 1;
	ctrls.controls = &control;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_S_EXT_CTRLS fails ret:%d\n", ret);
		return ret;
	}

	/* subscribe to resolution change and EOS event */
	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_SOURCE_CHANGE;
	ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (ret) {
		debug_print(DEBUG_ERROR, "subscribe V4L2_EVENT_SOURCE_CHANGE fail\n");
		return ret;
	}

	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_EOS;
	ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	if (ret) {
		debug_print(DEBUG_ERROR, "subscribe V4L2_EVENT_EOS fail\n");
		return ret;
	}

	/******** [DEC INFO] Optional according to needs, not required ********/
	if (g_output_flag & DECINFO_DUMP_STREAM) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_STREAM;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_STREAM fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_STATISTIC) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_STATISTIC;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_STATISTIC fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_AFD) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_AFD;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_AFD fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_CC) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_CC;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_HDR10) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_HDR10;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_HDR10P) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_HDR10P;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_CUVA) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_CUVA;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_AMDV) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_AMDV;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}

	if (g_output_flag & DECINFO_DUMP_FRAME) {
		memset(&sub, 0, sizeof(sub));
		sub.type = V4L2_EVENT_PRIVATE_EXT_REPORT_DECINFO;
		sub.id = AML_DECINFO_EVENT_FRAME;
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret) {
			debug_print(DEBUG_ERROR, "subscribe AML_DECINFO_EVENT_HDR10 fail\n");
			return ret;
		}
	}
	/******************** [DEC INFO END] *********************/

	return 0;
}

int config_sys_node(const char* path, const char* value)
{
	int fd;
	/* enable video plane */
	fd = open(path, O_RDWR);
	if (fd < 0) {
		debug_print(DEBUG_ERROR, "fail to open %s\n", path);
		return -1;
	}
	if (write(fd, value, strlen(value)) != strlen(value)) {
		debug_print(DEBUG_ERROR, "fail to write %s to %s\n", value, path);
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

int v4l2_dec_init(enum vformat_e type, decode_finish_fn cb)
{
	int ret = -1;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fdesc;
	bool supported;

	if (!cb)
		return 1;
	decode_finish_cb = cb;
	sInMemMode = V4L2_MEMORY_MMAP;

	/* check decoder mode */
	if ((type == VFORMAT_MPEG12 ||
		type == VFORMAT_MPEG4 ||
		type == VFORMAT_MJPEG) &&
		g_dw_mode != 16) {
		debug_print(DEBUG_ERROR, "mjpeg/mpeg2/mpeg4 only support DW 16 mode\n");
		goto error;
	}

	if (type == VFORMAT_MJPEG) {
		mjpeg_flag = 1;
	}
	pthread_mutex_init(&res_lock, NULL);
	video_fd = open(video_dev_name, O_RDWR | O_NONBLOCK, 0);
	if (video_fd < 0) {
		debug_print(DEBUG_ERROR, "Unable to open video node: %s\n", strerror(errno));
		goto error;
	}

	if ((ret = ioctl(video_fd, VIDIOC_QUERYCAP, &cap))) {
		debug_print(DEBUG_ERROR, "VIDIOC_QUERYCAP fails ret:%d\n", ret);
		goto error;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		debug_print(DEBUG_ERROR, "V4L2_CAP_STREAMING fails\n");
		goto error;
	}

	/* output fmt */
	output_p.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	supported = false;
	memset(&fdesc, 0, sizeof(fdesc));
	fdesc.type = output_p.type;
	for (;;) {
		ret = ioctl(video_fd, VIDIOC_ENUM_FMT, &fdesc);
		if (ret)
			break;
		if (v4l2_fourcc_to_vtype(fdesc.pixelformat) == type) {
			supported = true;
			output_p.pixelformat = fdesc.pixelformat;
			break;
		}
		fdesc.index++;
	}
	if (!supported) {
		debug_print(DEBUG_ERROR, "output format not supported:%d\n", type);
		goto error;
	}

	/* VC1 need set stream mode*/
	if (type == VFORMAT_VC1) {
	    struct v4l2_queryctrl queryctrl;
	    struct v4l2_control control;

	    memset (&queryctrl, 0, sizeof (queryctrl));
	    queryctrl.id = AML_V4L2_SET_STREAM_MODE;

	    if (-1 == ioctl (video_fd, VIDIOC_QUERYCTRL, &queryctrl)) {
	        printf ("AML_V4L2_SET_STREAM_MODE is not supported\n");
			goto error;
	    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
	        printf ("AML_V4L2_SET_STREAM_MODE is disable\n");
			goto error;
	    } else {
	        memset (&control, 0, sizeof (control));
	        control.id = AML_V4L2_SET_STREAM_MODE;
	        control.value = 1;

	        if (ioctl (video_fd, VIDIOC_S_CTRL, &control)) {
	            printf ("AML_V4L2_SET_STREAM_MODE fail\n");
				goto error;
	        }
	    }
	}

	if (config_decoder(video_fd, type)) {
		debug_print(DEBUG_ERROR, "config_decoder error\n");
		goto error;
	}

	output_p.sfmt.type = output_p.type;
	output_p.sfmt.fmt.pix_mp.pixelformat = output_p.pixelformat;
	/* 4K frame should fit into 2M */
	output_p.sfmt.fmt.pix_mp.plane_fmt[0].sizeimage = ES_BUF_SIZE;
	output_p.sfmt.fmt.pix_mp.num_planes = 1;
	ret = ioctl(video_fd, VIDIOC_S_FMT, &output_p.sfmt);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_S_FMT 0x%x fail\n", output_p.pixelformat);
		goto error;
	}
	output_p.plane_num = output_p.sfmt.fmt.pix_mp.num_planes;

	/* capture fmt */
	capture_p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	supported = false;
	memset(&fdesc, 0, sizeof(fdesc));
	fdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	for (;;) {
		ret = ioctl(video_fd, VIDIOC_ENUM_FMT, &fdesc);
		if (ret)
			break;
		if (fdesc.pixelformat == V4L2_PIX_FMT_NV21M ||
			fdesc.pixelformat == V4L2_PIX_FMT_NV21 ||
			fdesc.pixelformat == V4L2_PIX_FMT_NV12 ||
			fdesc.pixelformat == V4L2_PIX_FMT_NV12M) {
			supported = true;
			capture_p.pixelformat = fdesc.pixelformat;
			break;
		}
		fdesc.index++;
	}
	if (!supported) {
		debug_print(DEBUG_ERROR, "capture format not supported\n");
		goto error;
	}

	/* setup output port */
	ret = setup_output_port(video_fd);
	if (ret) {
		printf("setup_output_port fail\n");
		goto error;
	}

	/* start output port */
	ret = ioctl(video_fd, VIDIOC_STREAMON, &output_p.type);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_STREAMON fail ret:%d\n",ret);
		goto error;
	}
	output_p.is_stream_on = true;
	debug_print(DEBUG_DEF, "output stream on\n");
	return 0;

error:
	close(video_fd);
	video_fd = 0;
	return ret;
}

static int handle_first_frame()
{
	/* Wait for 1st video ES data and setup capture port */

	pthread_mutex_init(&capture_p.lock, NULL);
	pthread_cond_init(&capture_p.wait, NULL);

	if (pthread_create(&dec_thread, NULL, dec_thread_func, NULL)) {
		debug_print(DEBUG_ERROR, "dec thread fail\n");
		return -1;
	}
	return 0;
error:
	debug_print(DEBUG_ERROR, "handle_first_frame fatal\n");
	while (1)
		sleep(1);
	exit(1);
}

int v4l2_dec_destroy()
{
	destroy_statistic_info_timer();
	quit_thread = true;
	pthread_cond_signal(&output_p.wait);
	pthread_join(dec_thread, NULL);
	destroy_output_port(video_fd);
	destroy_capture_port(video_fd);
	get_1st_data = false;
	pthread_mutex_destroy(&res_lock);
	close(video_fd);
	destroy_dec_info();
	if (es_buf)
		free(es_buf);
	if (yuv_fp)
		fclose(yuv_fp);
	if (crc_fp)
		fclose(crc_fp);
	return 0;
}

static int get_free_output_buf()
{
	int i;
	pthread_mutex_lock(&output_p.lock);
	do {
		for (i = 0; i < output_p.buf_num; i++) {
			if (!output_p.buf[i]->queued) {
				break;
			}
		}

		if (i == output_p.buf_num) {
			pthread_cond_wait(&output_p.wait, &output_p.lock);
			if (quit_thread)
			break;
		} else {
			break;
		}
	} while (1);
	pthread_mutex_unlock(&output_p.lock);

	if (i == output_p.buf_num)
		return -1;

	return i;
}

int v4l2_dec_write_es(const uint8_t *data, int size)
{
	struct frame_buffer *p;

	if (cur_output_index == -1)
		cur_output_index = get_free_output_buf();

	if (cur_output_index < 0) {
		debug_print(DEBUG_ERROR, "%s %d can not get output buf\n", __func__, __LINE__);
		return 0;
	}

	p = output_p.buf[cur_output_index];
	if ((sInMemMode == V4L2_MEMORY_MMAP &&
		(p->used + size) > p->v4lplane[0].length) ||
		(sInMemMode == V4L2_MEMORY_DMABUF &&
		(p->used + size) > ES_BUF_SIZE)) {
		debug_print(DEBUG_ERROR, "fatal frame too big %d\n", size + p->used);
		return 0;
	}

	if (sInMemMode == V4L2_MEMORY_MMAP)
		memcpy(p->vaddr[0] + p->used, data, size);
	else
		memcpy(es_buf + p->used, data, size);

	p->used += size;
#ifdef DEBUG_FRAME
	if (g_log_level & DEBUG_FRAME) {
		int i;
		for (i = 0 ; i < size ; i++)
			frame_checksum += data[i];
	}
#endif

	return size;
}

int v4l2_dec_frame_done()
{
	int ret;
	struct frame_buffer *p;

	if (quit_thread)
		return 0;

	if (cur_output_index < 0 || cur_output_index >= output_p.buf_num) {
		debug_print(DEBUG_ERROR, "BUG %s %d idx:%d\n", __func__, __LINE__, cur_output_index);
		return 0;
	}
	p = output_p.buf[cur_output_index];
	p->v4lbuf.m.planes[0].bytesused = p->used;

	/* convert from ns to timeval */
	p->v4lbuf.timestamp.tv_sec = d_o_push_num;

	pthread_mutex_lock(&output_p.lock);
	p->queued = true;
	pthread_mutex_unlock(&output_p.lock);
	ret = ioctl(video_fd, VIDIOC_QBUF, &p->v4lbuf);
	if (ret) {
		debug_print(DEBUG_ERROR, "write es VIDIOC_QBUF %dth buf fail ret:%d\n",
		cur_output_index, ret);
		return 0;
	}
#ifdef DEBUG_FRAME
	debug_print(DEBUG_FRAME, "%s queue output %d frame_checksum:%x\n", __func__, cur_output_index, frame_checksum);
	frame_checksum = 0;
#endif
	d_o_push_num++;
	cur_output_index = -1;
	/* can setup capture port now */
	if (!get_1st_data) {
		debug_print(DEBUG_STATE, "%s 1st frame done\n", __func__);
		get_1st_data = true;
		handle_first_frame();
	} else {
	}
	return ret;
}

int capture_buffer_recycle(void* handle)
{
	int ret = 0;
	struct frame_buffer *frame = handle;

	d_c_rec_num++;
	pthread_mutex_lock(&res_lock);
	if (frame->free_on_recycle) {
		printf("free index:%d\n", frame->v4lbuf.index);
		free(frame);
		goto exit;
	}

	if (eos_received)
		goto exit;

	ret = ioctl(video_fd, VIDIOC_QBUF, &frame->v4lbuf);
	if (ret) {
		debug_print(DEBUG_ERROR, "VIDIOC_QBUF %dth buf fail ret:%d\n", frame->v4lbuf.index, ret);
		goto exit;
	} else {
#ifdef DEBUG_FRAME
		debug_print(DEBUG_FRAME, "queue cap %d\n", frame->v4lbuf.index);
#endif
		frame->queued = true;
	}
exit:
	pthread_mutex_unlock(&res_lock);
	return ret;
}

int v4l2_dec_eos()
{
	int ret;
	struct v4l2_decoder_cmd cmd = {
		.cmd = V4L2_DEC_CMD_STOP,
		.flags = 0,
	};

	debug_print(DEBUG_DEF, "%s EOS send\n", __func__);
	/* flush decoder */
	ret = ioctl(video_fd, VIDIOC_DECODER_CMD, &cmd);
	if (ret)
		debug_print(DEBUG_ERROR, "V4L2_DEC_CMD_STOP output fail ret:%d\n",ret);

	return ret;
}

void dump_v4l2_decode_state()
{
	debug_print(DEBUG_DEF, "-----------------------\n");
	debug_print(DEBUG_DEF, "output port:  push(%d) pop(%d)\n", d_o_push_num, d_o_rec_num);
	debug_print(DEBUG_DEF, "capture port: push(%d) pop(%d)\n", d_c_push_num, d_c_rec_num);
	debug_print(DEBUG_DEF, "-----------------------\n");
}
