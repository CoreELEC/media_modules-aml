#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/crc32.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/scatterlist.h>
#include <linux/sched/clock.h>
#include <linux/highmem.h>
#include <linux/version.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VICP
#include <linux/amlogic/media/vicp/vicp.h>
#endif
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0))
#include <linux/amlogic/meson_uvm_allocator.h>
#else
#include <linux/amlogic/media/meson_uvm_allocator.h>
#endif
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/types.h>

#include "vdec_drv_if.h"
#ifdef CONFIG_AMLOGIC_MEDIA_WRAPPER
#include "aml_vcodec_avbc_wrapper.h"
#endif
#include "trigger_data.h"
#include "utils/common.h"
#include "../frame_provider/decoder/utils/aml_buf_helper.h"
#include "aml_vcodec_dec.h"
#include "../stream_input/amports/streambuf.h"
#include "../frame_provider/decoder/utils/vdec.h"
#include "../frame_provider/aml_dhp/aml_dhp_if.h"
#include "../common/chips/decoder_cpu_ver_info.h"
#include "../frame_provider/decoder/utils/vdec_ge2d_utils.h"
#include "../common//media_utils/media_cpufreq.h"

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)

#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K  0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K  0x120000

#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_GET_DATA         4
#define DEC_RESULT_GET_DATA_RETRY   5
#define DEC_RESULT_ERROR            6
#define DEC_RESULT_EOS              7
#define DEC_RESULT_ERROR_DATA       12

/*
 * enum AVBCD_FRAME_TYPE_FLAG - AVBCD frame type flag.
 *
 * @AVBCD_FLAG_P	: Indicates P frame.
 * @AVBCD_FLAG_I	: Indicates I frame.
 * @AVBCD_FLAG_EOS	: Indicates EOS frame.
 * @AVBCD_FLAG_BUF_BY_PASS
 *			: Bypass the frame.
 */
enum AVBCD_FRAME_TYPE_FLAG {
	AVBCD_FLAG_P		= 0x1,
	AVBCD_FLAG_I		= 0x2,
	AVBCD_FLAG_EOS		= 0x4,
	AVBCD_FLAG_BUF_BY_PASS	= 0x8,
	AVBCD_FLAG_MAX		= 0x7FFFFFFF,
};

/*
 * struct soft_data_t - Structure to define AVBCD processing parameters.
 *
 * @dst_addr	: Virtual address of the YUV output buffer.
 * @width	: Aligned width of the YUV buffer in pixels.
 * @height	: Aligned height of the YUV buffer in pixels.
 * @stride_w	: Byte stride (aligned width) for the YUV buffer.
 * @stride_h	: Byte stride (aligned height) for the YUV buffer.
 * @bitdepth	: Bit depth of the input stream (e.g., 8-bit, 10-bit).
 * @bitdepth_out : Bit depth of the output YUV frame (e.g., 8-bit, 10-bit).
 * @avbc_width	: Original width of the AVBC input stream in pixels.
 * @avbc_height	: Original height of the AVBC input stream in pixels.
 * @header_addr	: Physical address of the AVBCD header buffer.
 */
struct soft_data_t {
	char *dst_addr;
	u32 width;
	u32 height;
	u32 stride_w;
	u32 stride_h;
	u32 bitdepth;
	u32 bitdepth_out;
	u32 avbc_width;
	u32 avbc_height;
	ulong header_addr;
};

/*
 * struct aml_avbc_wrapper_buf - Parameter input buffer structure.
 *
 * @flag		: Buffer of flag, p, i, eos etc.
 * @aml_vb		: Point to aml_buf.
 * @vf			: vframe for copy to output.
 */
struct aml_avbc_wrapper_buf {
	u32 flag;
	struct aml_v4l2_buf *aml_vb;
	struct vframe_s vf;
};


struct aml_avbc_hw_buf {
	ulong mem_handle;
	dos_addr_t phy_addr;
	void *virt_addr;
	u32 size;
};

/*
 * struct aml_avbc_wrapper_s - Structure defining AVBCD wrapper context and parameters.
 *
 * @id              : Instance ID of the AVBCD wrapper context.
 * @ref             : Reference count for managing the AVBCD wrapper context lifecycle.
 * @mutex_lock      : Mutex lock to ensure thread-safe access and interface serialization.
 * @avbc_queue      : Task queue for AVBCD processing.
 * @port            : ES (Elementary Stream) input port context.
 * @format          : Stream protocol format (e.g., H.264, VP9).
 * @vdec            : Pointer to the video decoder (vdec) context.
 * @vdec_cb         : Callback function for vdec operations.
 * @vdec_cb_arg     : Argument passed to the vdec callback function.
 * @chunk           : vframe chunk used in frame-based processing.
 * @data_offset     : Address offset of the chunk for the current frame.
 * @data_size       : Size of the input frame in bytes.
 * @inputpool       : Input buffer pool for AVBCD processing.
 * @in              : Data information of the input frame.
 * @input           : KFIFO for storing input buffers.
 * @in_done_q       : KFIFO queue for completed input buffers (marked as done).
 * @out             : KFIFO queue for storing output buffers.
 * @avbc_workqueue  : Workqueue for handling post-interrupt processing tasks.
 * @avbc_work       : Work structure for post-interrupt tasks.
 * @dec_result      : Result of the AVBCD decoding process.
 * @avbc_done       : Completion structure to wait for AVBCD processing to finish.
 * @frame_count     : Total count of frames processed by AVBCD.
 * @stop_flag       : Flag indicating whether the stream has been stopped.
 */
struct aml_avbc_wrapper_s {
	ulong 				id;
	atomic_t			ref;
	struct mutex			mutex_lock;
	struct mutex			copy_lock;
	struct list_head		avbc_queue;
	struct stream_port_s 		port;
	int 				format;
	struct vdec_s 			*vdec;
	void (*vdec_cb)(struct vdec_s *, void *, int);
	void *vdec_cb_arg;
	struct vframe_chunk_s 		*chunk;
	u32				data_offset;
	u32				data_size;
	struct aml_avbc_wrapper_buf 	*inputpool;
	struct avbc_input		in;
	DECLARE_KFIFO_PTR(input, typeof(struct aml_avbc_wrapper_buf*));
	DECLARE_KFIFO(in_done_q, struct aml_avbc_wrapper_buf *, AVBCD_FRAME_SIZE);
	DECLARE_KFIFO(out, struct avbc_output *, AVBCD_FRAME_SIZE);
	struct workqueue_struct		*avbc_workqueue;
	struct work_struct 		avbc_work;
	struct work_struct 		copy_work;
	int				dec_result;
	struct completion 		avbc_done;
	struct completion 		seq_header_done;
	int				hard_mode;
	int				vicp_mode;
	u32				frame_count;
	u32				stop_flag;
	u32				width;
	u32				height;
	u32				bitdepth;
	u32				flag;
	struct aml_avbc_hw_buf		hw_buf;
	struct timeval 			start;
	struct timeval 			end;
	u32				wait_complete;
	struct vdec_ge2d 		*ge2d;
};

static DEFINE_MUTEX(avbc_mutex);
static DEFINE_MUTEX(task_mutex);

struct aml_avbc_wrapper_s *g_wrapper;
static struct freq_qos_request media_copy_qos_req;

extern int avbcd_work_mode;
extern int crc_dump;
extern int dump_avbcd_frame;
extern char dump_path[32];
extern int dec_i_frame_once;
extern int ge2d_copy;

static void copy_ge2d(struct aml_avbc_wrapper_s *wrapper, struct avbc_output *out)
{
	u32 src_w_stride, src_h_stride, dst_w_stride, dst_h_stride;
	struct vframe_s *vf = vzalloc(sizeof(struct vframe_s));
	struct canvas_config_s src_canvas_config[2], dst_canvas_config[2];
	struct vdec_ge2d_info ge2d_info = { 0 };

	src_w_stride = ALIGN(wrapper->vdec->avbc_info.avbc_width, 64);
	if (wrapper->vdec->avbc_info.bitdepth_dst == 10)
		src_w_stride = src_w_stride * 2;
	src_h_stride = ALIGN(wrapper->vdec->avbc_info.avbc_height, 64);

	dst_w_stride = out->img.rect.width;
	dst_h_stride = out->img.rect.height;

	src_canvas_config[0].phy_addr = wrapper->hw_buf.phy_addr;
	src_canvas_config[0].width = src_w_stride;
	src_canvas_config[0].height = src_h_stride;
	src_canvas_config[0].block_mode = 0;
	src_canvas_config[0].endian = 0;
	src_canvas_config[0].bit_depth = 0;

	src_canvas_config[1].phy_addr = wrapper->hw_buf.phy_addr + src_w_stride * src_h_stride;
	src_canvas_config[1].width = src_w_stride;
	src_canvas_config[1].height = src_h_stride;
	src_canvas_config[1].block_mode = 0;
	src_canvas_config[1].endian = 0;
	src_canvas_config[1].bit_depth = 0;

	dst_canvas_config[0] = src_canvas_config[0];
	dst_canvas_config[0].width = dst_w_stride;
	dst_canvas_config[0].height = dst_h_stride;
	dst_canvas_config[0].phy_addr = out->img.data;

	dst_canvas_config[1] = src_canvas_config[1];
	dst_canvas_config[1].width = dst_w_stride;
	dst_canvas_config[1].height = dst_h_stride;
	dst_canvas_config[1].phy_addr = out->img.data + dst_w_stride * dst_h_stride;

	vf->width = wrapper->vdec->avbc_info.avbc_width;
	vf->height = wrapper->vdec->avbc_info.avbc_height;
	vf->type = (out->img.format == AML_PIX_FMT_NV21) ?
			VIDTYPE_VIU_NV21 : VIDTYPE_VIU_NV12;
	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 2;
	vf->canvas0_config[0] = dst_canvas_config[0];
	vf->canvas0_config[1] = dst_canvas_config[1];
	vf->canvas1_config[0] = dst_canvas_config[0];
	vf->canvas1_config[1] = dst_canvas_config[1];
	ge2d_info.src_canvas0Addr = ge2d_info.src_canvas1Addr = -1;
	ge2d_info.dst_vf = vf;

	ge2d_info.src_canvas0_config[0] = src_canvas_config[0];
	ge2d_info.src_canvas0_config[1] = src_canvas_config[1];
	ge2d_info.src_canvas1_config[0] = src_canvas_config[0];
	ge2d_info.src_canvas1_config[1] = src_canvas_config[1];

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
		"data 0x%llx size %d, src_addr(0x%lx, 0x%lx), dst_addr(0x%lx, 0x%lx), src_stride(%ux%u), dst_stride(%ux%u)\n",
		out->img.data, out->img.size,
		src_canvas_config[0].phy_addr, src_canvas_config[1].phy_addr,
		dst_canvas_config[0].phy_addr, dst_canvas_config[1].phy_addr,
		src_w_stride, src_h_stride,
		dst_w_stride, dst_h_stride);
	if (!wrapper->ge2d) {
		int mode = (out->img.format == AML_PIX_FMT_NV21) ?
				GE2D_MODE_CONVERT_NV21 : GE2D_MODE_CONVERT_NV12;
		mode |= GE2D_MODE_CONVERT_LE;
		vdec_ge2d_init(&wrapper->ge2d, mode);
	}
	vdec_ge2d_copy_data(wrapper->ge2d, &ge2d_info);

	if (wrapper->ge2d) {
		vdec_ge2d_destroy(wrapper->ge2d);
		wrapper->ge2d = NULL;
	}

	vfree(vf);
}

static noinline void copy_yuv(struct aml_avbc_wrapper_s *wrapper, struct avbc_output *out)
{
	u8 *dst_addr, *src_addr, *src_addr_start, *dst_addr_start, *dump_addr;
	u32 src_w_stride, src_h_stride, dst_w_stride, dst_h_stride;
	int i;

	src_w_stride = ALIGN(wrapper->vdec->avbc_info.avbc_width, 64);
	if (wrapper->vdec->avbc_info.bitdepth_dst == 10)
		src_w_stride = src_w_stride * 2;
	src_h_stride = ALIGN(wrapper->vdec->avbc_info.avbc_height, 64);

	dst_w_stride = out->img.rect.width;
	dst_h_stride = out->img.rect.height;

	src_addr = wrapper->hw_buf.virt_addr;
try_map:
	dst_addr = codec_mm_vmap(out->img.data, out->img.size);
	if (!dst_addr) {
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
		"%s Map fail!Try again!\n", __func__);
		goto try_map;
	}

	src_addr_start = src_addr;
	dst_addr_start = dst_addr;

	dump_addr = dst_addr;

	preempt_disable();
	codec_mm_inval_cache(wrapper->hw_buf.phy_addr, wrapper->hw_buf.size);
	for (i = 0; i < src_h_stride; i++) {
		memcpy(dst_addr, src_addr, src_w_stride);
		dst_addr += dst_w_stride;
		src_addr += src_w_stride;
	}

	src_addr = src_addr_start + src_w_stride * src_h_stride;
	dst_addr = dst_addr_start + dst_w_stride * dst_h_stride;

	for (i = 0; i < src_h_stride / 2; i++) {
		memcpy(dst_addr, src_addr, src_w_stride);
		dst_addr += dst_w_stride;
		src_addr += src_w_stride;
	}
	codec_mm_dma_flush(dst_addr, out->img.size, DMA_TO_DEVICE);
	preempt_enable();
	media_recover_cpufreq(&media_copy_qos_req);

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
		"data 0x%llx size %d, dst_addr: 0x%px, src_addr: 0x%px, src_w_stride: %u, src_h_stride: %u, dst_w_stride:%u, dst_h_stride %u\n",
		 out->img.data, out->img.size, dst_addr, src_addr, src_w_stride, src_h_stride, dst_w_stride, dst_h_stride);

	codec_mm_unmap_phyaddr(dst_addr);
}

static void do_vframe_avbc_soft_decode(struct soft_data_t *soft_data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	int i, j, ret, y_size, free_cnt;
	unsigned int crc1, crc2, crc3, crc4;
	short *planes[4];
	int w, h;
	char *ybuf, *ubuf, *vbuf, *uvbuf;
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	short *y_dst_10, *vu_dst_10;
	int bit_10;
	int convert_to_8bit = 0;
	struct timeval start, end;
	struct fbc_decoder_param param;
	unsigned long time_use = 0;
	u32 wstride = soft_data->stride_w;
	u32 hstride = soft_data->stride_h;

	if ((soft_data->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	y_size = soft_data->avbc_width * soft_data->avbc_height * sizeof(short);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "width: %d, height: %d, compWidth: %u, compHeight: %u,bit10:%d.\n",
		 soft_data->width, soft_data->height, soft_data->avbc_width, soft_data->avbc_height, bit_10);

	if (bit_10 && soft_data->bitdepth_out == 8) {
		bit_10 = 0;
		convert_to_8bit  = 1;
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "memory not enough,convert 10bit to 8bit.\n");
	}

	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		if (!planes[i]) {
			free_cnt = i;
			v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "vmalloc fail in %s\n", __func__);
			goto free;
		}
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}
	free_cnt = 4;

	do_gettimeofday(&start);
	param.compHeadAddr = soft_data->header_addr;
	param.compWidth = soft_data->avbc_width;
	param.compHeight = soft_data->avbc_height;
	param.bitdepth = soft_data->bitdepth;
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, &param, 0, 0);
	if (ret < 0) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "amlogic_fbc_lib.ko error %d", ret);
		goto free;
	}
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	if (crc_dump) {
		ybuf = (unsigned char *)planes[0];
		ubuf = (unsigned char *)planes[1];
		vbuf = (unsigned char *)planes[2];
		uvbuf = (unsigned char *)planes[3];

		w = soft_data->avbc_width;
		h = soft_data->avbc_height;
		crc1 = 0;
		crc2 = 0;
		crc3 = 0;
		crc4 = 0;
		crc1 = crc32_le(0, ybuf, w * h * 2);
		crc2 = crc32_le(0, ubuf, w * h / 2);
		crc3 = crc32_le(0, vbuf, w * h / 2);
		crc4 = crc32_le(0, uvbuf, w * h * 2 / 2);
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "crc32_dump_avbc %08x %08x %08x %08x\n",
			crc1, crc4, crc2, crc3);
	}

	y_dst = soft_data->dst_addr;
	y_dst_10 = (short *)(soft_data->dst_addr);
	vu_dst = soft_data->dst_addr + wstride * hstride;
	vu_dst_10 = (short *)(soft_data->dst_addr + wstride * hstride);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "offset uv: %d\n", wstride * hstride);
	do_gettimeofday(&start);
	for (i = 0; i < soft_data->avbc_height; i++) {
		//PR_INIT(500);
		//pr_info("y[%d]\n", i);
		for (j = 0; j < soft_data->avbc_width; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst_10 + j) = *s2c << 6 & 0xFFC0;
			else if (convert_to_8bit)
				*(y_dst + j) = (*s2c >> 2) & 0xff;
			else
				*(y_dst + j) = tmp[0];
			//PR_FILL("%04hx ", *(y_dst_10 + j));
			//if (((j + 1) & 0xf) == 0)
				//PR_INFO(0);
		}
			//PR_INFO(0);
			y_dst += wstride;
			y_dst_10 = (short *)y_dst;
			y_src += soft_data->avbc_width;
	}
	for (i = 0; i < (soft_data->avbc_height / 2); i++) {
		//PR_INIT(500);
		//pr_info("uv[%d]\n", i);
		for (j = 0; j < soft_data->avbc_width; j += 2) {
			s2c = v_src + j / 2;
			s2c1 = u_src + j / 2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst_10 + j) = *s2c1 << 6 & 0xFFC0;
				*(vu_dst_10 + j + 1) = *s2c << 6 & 0xFFC0;
			} else if (convert_to_8bit) {
				*(vu_dst + j) = *s2c >> 2 & 0xff;
				*(vu_dst + j + 1) = *s2c1 >> 2 & 0xff;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
			//PR_FILL("%04hx ", *(vu_dst_10 + j));
			//PR_FILL("%04hx ", *(vu_dst_10 + j + 1));
			//if ((j & 0xf) == 0)
				//PR_INFO(0);
		}
		//PR_INFO(0);
		vu_dst += wstride;
		vu_dst_10 = (short *)vu_dst;
		u_src += (soft_data->avbc_width / 2);
		v_src += (soft_data->avbc_width / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "bitblk time: %ldms\n", time_use);

free:
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
#endif
}

int aml_avbcd_process_one_frame(struct avbc_input *input, struct avbc_output *output)
{
	int i, j, num_pages;
	struct mua_buffer *buffer;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;
	struct uvm_buf_obj *obj;
	struct uvm_handle *handle;
	struct uvm_alloc *ua;
	struct soft_data_t soft_data;
	struct dma_buf *dmabuf = (struct dma_buf *)(ulong)output->img.data;
	int ret = 0;

	handle = dmabuf->priv;
	ua = handle->ua;
	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	buffer = container_of(obj, struct mua_buffer, base);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "WxH: %dx%d, buffer(0x%p)->size:%zu realloc dmabuf->size=%zu\n",
			buffer->width, buffer->height, buffer, buffer->size, dmabuf->size);

	//start to do vmap
	if (!ua->sgt[0]) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "none uvm buffer allocated.\n");
		return -ENODEV;
	}

	ret = dma_buf_begin_cpu_access(buffer->idmabuf[0], DMA_BIDIRECTIONAL);
	if (ret) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "%s: Failed to get dma sg", __func__);
		return -ENOMEM;
	}
	src_sgt = ua->sgt[0];
	num_pages = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	page_array = tmp;

	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "buffer vaddr: %px.\n", vaddr);

	//start to filldata
	memset(&soft_data, 0, sizeof(soft_data));

	soft_data.dst_addr = vaddr;
	soft_data.width = buffer->width;
	soft_data.height = buffer->height;
	soft_data.stride_w = output->img.rect.width;
	soft_data.stride_h = output->img.rect.height;
	soft_data.bitdepth_out = output->img.bitdep;
	soft_data.header_addr = (ulong)input->img.data;
	soft_data.avbc_width = input->img.rect.width;
	soft_data.avbc_height = input->img.rect.height;
	soft_data.bitdepth = input->img.bitdep == 10 ? (BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10) :
				(BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s. width=%d height=%d byte_stride=%d buf-bounds(%d, %d, %d)\n",
			__func__, buffer->width, buffer->height,
			buffer->byte_stride, output->img.rect.width, output->img.rect.height, buffer->align);
	do_vframe_avbc_soft_decode(&soft_data);
	dma_buf_end_cpu_access(buffer->idmabuf[0], DMA_BIDIRECTIONAL);
	vunmap(vaddr);

	return 0;
}

static int aml_dhp_avbcd(struct avbc_output *output, struct avbc_input *input, u32 pts)
{
	struct aml_du_mem src = { 0 };
	struct aml_du_mem dst = { 0 };
	struct aml_du_avbcd	avbcd;
	ulong header = (ulong)input->img.data;
	u32 h_size = input->img.size;
	u32 width = input->img.rect.width;
	u32 height = input->img.rect.height;
	u32 depth = input->img.bitdep;
	u32 depth_out = output->img.bitdep;
	ulong buf = (ulong)output->img.data;
	u32 buf_size = output->img.size;
	u32 dst_f = output->img.format;
	u32 dst_x = output->img.rect.x;
	u32 dst_y = output->img.rect.y;
	u32 dst_w = output->img.rect.width;
	u32 dst_h = output->img.rect.height;
	int ret = 0;

	//fill avbcd pic info
	avbcd.type	= AML_DHP_TYPE_AVBCD;
	avbcd.width	= width;
	avbcd.height	= height;
	avbcd.crop.top	= input->img.crop.top;
	avbcd.crop.left	= input->img.crop.left;
	avbcd.crop.bottom = input->img.crop.bottom;
	avbcd.crop.right = input->img.crop.right;
	avbcd.pixel	= 0;
	avbcd.bitdep	= depth;
	avbcd.header	= header;
	avbcd.hsize	= h_size;
	avbcd.pts	= pts;

	//fill source memory information.
	src.type	= AML_MEM_TYPE_PHY_ADDR;
	src.addr	= header;
	src.size	= h_size;
	src.uncached	= 0;

	//fill destination memory information.
	dst.type	= AML_MEM_TYPE_PHY_ADDR;
	dst.addr	= buf;
	dst.size	= buf_size;
	dst.uncached	= 0;

	dst.img.format	= dst_f;
	dst.img.rect.x	= dst_x;
	dst.img.rect.y	= dst_y;
	dst.img.rect.width = dst_w;
	dst.img.rect.height = dst_h;
	dst.img.bitdep	= depth_out;

	ret = dhp_func_request(&src, &dst, &avbcd, sizeof(avbcd));
	if (ret)
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "Do dhp task fail. err:%d\n", ret);

	return ret;
}

static int get_header_size(int w, int h)
{
	w = ALIGN(w, 64);
	h = ALIGN(h, 64);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) && IS_8K_SIZE(w, h))
		return (MMU_COMPRESS_HEADER_SIZE_8K);
	if (IS_4K_SIZE(w, h))
		return (MMU_COMPRESS_HEADER_SIZE_4K);
	return (MMU_COMPRESS_HEADER_SIZE_1080P);
}

int aml_avbcd_submit_one_frame(struct avbc_input *input, struct avbc_output *output,
						struct aml_avbc_wrapper_s *wrapper)
{
	int ret = 0;
	input->img.size = get_header_size(input->img.rect.width, input->img.rect.height);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
		"h:%lx, hsize:%d, wxh:%ux%u, crop[t:%u,l:%u,b:%u,r:%u], dep:%d, dst-fmt:%x, buf:%lx, size:%u, dst-stride(%u, %u), dst-dep:%d, pts:%d.\n",
		(ulong)input->img.data,
		input->img.size,
		input->img.rect.width,
		input->img.rect.height,
		input->img.crop.top,
		input->img.crop.left,
		input->img.crop.bottom,
		input->img.crop.right,
		input->img.bitdep,
		output->img.format,
		(ulong)output->img.data,
		output->img.size,
		output->img.rect.width,
		output->img.rect.height,
		output->img.bitdep,
		wrapper->frame_count);

	ret = aml_dhp_avbcd(output, input, wrapper->frame_count);

	wrapper->frame_count++;

	return ret;
}

#ifdef CONFIG_AMLOGIC_MEDIA_VICP
int config_vicp_input_data(struct aml_avbc_wrapper_s *wrapper, struct avbc_output *out,
				struct vframe_s *vf, struct vicp_data_config_s *data_config)
{
	struct input_data_param_s *input_data = &data_config->input_data;
	struct canvas_config_s src_canvas_config[2];

	memset(input_data, 0, sizeof(struct input_data_param_s));

	src_canvas_config[0].width = ALIGN(wrapper->in.img.rect.width, 64);
	src_canvas_config[0].height = wrapper->in.img.rect.height;
	src_canvas_config[0].block_mode = 0;
	src_canvas_config[0].endian = 0;
	src_canvas_config[0].bit_depth = 0;

	src_canvas_config[1].width = ALIGN(wrapper->in.img.rect.width, 64);
	src_canvas_config[1].height = wrapper->in.img.rect.height;
	src_canvas_config[1].block_mode = 0;
	src_canvas_config[1].endian = 0;
	src_canvas_config[1].bit_depth = 0;

	vf->compBodyAddr = 0;
	vf->compHeadAddr = wrapper->in.img.data;

	vf->width = wrapper->in.img.rect.width;//dst_w_stride;
	vf->height = wrapper->in.img.rect.height;//dst_h_stride;

	vf->compWidth = wrapper->in.img.rect.width;
	vf->compHeight = wrapper->in.img.rect.height;
	vf->flag |= VFRAME_FLAG_VIDEO_LINEAR; //todo

	if (wrapper->in.img.bitdep == 10)
		vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
	if (wrapper->in.img.bitdep == 10 && out->img.bitdep == 8)
		vf->bitdepth |= BITDEPTH_SAVING_MODE;

	vf->type = (out->img.format == AML_PIX_FMT_NV21) ?
			VIDTYPE_VIU_NV21 : VIDTYPE_VIU_NV12;
	vf->type |= VIDTYPE_PROGRESSIVE |
		VIDTYPE_VIU_FIELD |
		VIDTYPE_COMPRESS |
		VIDTYPE_SCATTER;
	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 2;
	vf->canvas0_config[0] = src_canvas_config[0];
	vf->canvas0_config[1] = src_canvas_config[1];
	vf->canvas1_config[0] = src_canvas_config[0];
	vf->canvas1_config[1] = src_canvas_config[1];

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
				"%s wxh(%d x %d), flag(0x%x), bitdepth(0x%x), type(0x%x)\n",
				__func__,
				vf->compWidth,
				vf->compHeight,
				vf->flag,
				vf->bitdepth,
				vf->type);

	input_data->is_vframe = true;
	input_data->data_vf = vf;

	return 0;
}

int config_vicp_output_data(struct aml_avbc_wrapper_s *wrapper,
			struct avbc_output *out, struct vicp_data_config_s *data_config)
{
	struct output_data_param_s *output_data = &data_config->output_data;
	struct data_option_s *data_option = &data_config->data_option;
	memset(output_data, 0, sizeof(struct output_data_param_s));

	output_data->width = out->img.rect.width;
	output_data->height = out->img.rect.height;
	output_data->phy_addr[0] = out->img.data;
	output_data->stride[0] = out->img.rect.width;
	output_data->mif_out_en = 1;
	output_data->endian = 1;
	output_data->need_swap_cbcr = 1;
	output_data->out_sig_fmt = 0;

	output_data->mif_color_fmt = VICP_COLOR_FORMAT_YUV420;
	output_data->mif_color_dep = out->img.bitdep;

	data_option->rotation_mode = 0;
	data_option->crop_info.left = 0;
	data_option->crop_info.top = 0;
	data_option->crop_info.width = ALIGN(wrapper->in.img.rect.width, 64);
	data_option->crop_info.height = ALIGN(wrapper->in.img.rect.height, 64);
	data_option->output_axis.left = 0;
	data_option->output_axis.top = 0;
	data_option->output_axis.width = ALIGN(wrapper->in.img.rect.width, 64);
	data_option->output_axis.height = ALIGN(wrapper->in.img.rect.height, 64);
	data_option->shrink_mode = 0;
	data_option->rdma_enable = false;
	data_option->input_source_count = 1;
	data_option->input_source_number = 0;
	data_option->security_enable = 0;
	data_option->skip_mode = 0;

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
				"%s output(%d x %d), crop(%d x %d), addr(0x%lx), bitdepth(0x%x), stride(%d)\n",
				__func__,
				output_data->width,
				output_data->height,
				data_option->crop_info.width,
				data_option->crop_info.height,
				output_data->phy_addr[0],
				output_data->mif_color_dep,
				output_data->stride[0]);

	return 0;
}

void aml_avbcd_vicp_process(struct aml_avbc_wrapper_s *wrapper,
					struct avbc_output *out)
{
	int ret;
	struct vicp_data_config_s *data_config = vzalloc(sizeof(struct vicp_data_config_s));
	struct vframe_s *src_vf = vzalloc(sizeof(struct vframe_s));
	unsigned long time_use = 0;

	do_gettimeofday(&wrapper->start);

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s start!\n", __func__);

	config_vicp_input_data(wrapper, out, src_vf, data_config);

	config_vicp_output_data(wrapper, out, data_config);

	ret = vicp_process(data_config);
	if (ret < 0)
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "vicp_process failed\n");

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s end!\n", __func__);

	vfree(data_config);
	vfree(src_vf);

	do_gettimeofday(&wrapper->end);
	time_use = (wrapper->end.tv_sec - wrapper->start.tv_sec) * 1000 * 1000 +
		(wrapper->end.tv_usec - wrapper->start.tv_usec);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s time_use:%lu us\n", __func__, time_use);
}
#endif

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct vframe_chunk_s *chunk = NULL;

	chunk = vdec_input_next_chunk(&vdec->input);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "avbc wrapper %s: mask 0x%lx chunk %px\n",
		__func__, mask, chunk);
	if (!chunk)
		return 0;

	if (vdec->avbc_mode &
		(AVBCD_HARDWARE_LEGACY_MODE | AVBCD_HARDWARE_STANDARD_MODE))
		return CORE_MASK_HEVC;
	else
		return CORE_MASK_AVBCD_SOFT;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *, int), void *arg)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)vdec->wrapper;
	int r = 0;
	u8 *data = NULL;
	struct avbc_output *out = NULL;

	if (callback && arg) {
		wrapper->vdec_cb = callback;
		wrapper->vdec_cb_arg = arg;
	}

	wrapper->chunk = vdec_input_next_chunk(&vdec->input);
	r = wrapper->chunk->size;
	if ((vdec_frame_based(vdec)) &&
		(wrapper->chunk != NULL)) {
		wrapper->data_offset = wrapper->chunk->offset;
		wrapper->data_size = r;
	}

	wrapper->dec_result = DEC_RESULT_NONE;

	data = ((u8 *)wrapper->chunk->block->start_virt) +
		wrapper->data_offset;

	memcpy(&wrapper->in, data, sizeof(struct avbc_input));

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: size 0x%x  header_addr 0x%lx, header_size %u, width %u, height %u, bitdepth %u\n",
		__func__, r,
		(ulong)wrapper->in.img.data,
		wrapper->in.img.size,
		wrapper->in.img.rect.width,
		wrapper->in.img.rect.height,
		wrapper->in.img.bitdep);


	if (!wrapper->stop_flag &&
		wrapper->vicp_mode &&
		kfifo_peek(&wrapper->out, &out)) {
#ifdef CONFIG_AMLOGIC_MEDIA_VICP
		aml_avbcd_vicp_process(wrapper, out);
#endif
		goto out;
	}

	if (!wrapper->stop_flag && wrapper->hard_mode) {
		if (dec_i_frame_once && wrapper->frame_count &&
			wrapper->width && wrapper->height &&
			 wrapper->in.img.rect.width && wrapper->in.img.rect.height &&
			(wrapper->width != wrapper->in.img.rect.width ||
			wrapper->height != wrapper->in.img.rect.height)) {
			vdec->pic0_done = 0;
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: resolution change: (%u x %u)==>(%u x %u)\n",
					__func__,
					wrapper->width,
					wrapper->height,
					wrapper->in.img.rect.width,
					wrapper->in.img.rect.height);
		}
		memset(data, 0, r);

		if (vdec->pic0_done) {
			memcpy(data, trigger_p_1080, P_FRAME_SIZE);
			if (wrapper->hard_mode && vdec->pic0_done &&
				kfifo_peek(&wrapper->out, &out)) {
				vdec->avbc_info.avbc_y_addr = (ulong)out->img.data;
				vdec->avbc_info.format = out->img.format != AML_PIX_FMT_NV21 ?
								AML_PIX_FMT_NV12 : AML_PIX_FMT_NV21;
			}
			if (wrapper->flag & AVBC_FLAG_IO_BLOCKING) {
				u32 size = ALIGN(vdec->avbc_info.avbc_width, 64) * ALIGN(vdec->avbc_info.avbc_height, 64) * 3 /2;
				if (!((out->img.rect.width == ALIGN(vdec->avbc_info.avbc_width, 64)) &&
					(out->img.rect.height == ALIGN(vdec->avbc_info.avbc_height, 64)))) {
					if (vdec->avbc_info.bitdepth_dst == 10)
						size = size * 2;
					wrapper->hw_buf.size = size;
					wrapper->hw_buf.phy_addr = codec_mm_alloc_for_dma("AVBCD_BUF", PAGE_COUNT(size), 0, 0);
					wrapper->hw_buf.virt_addr = codec_mm_vmap(wrapper->hw_buf.phy_addr, size);
					vdec->avbc_info.avbc_y_addr = (ulong)wrapper->hw_buf.phy_addr;
				}
			}
			if (dec_i_frame_once && wrapper->frame_count) {
				vdec->avbc_info.avbc_width = wrapper->in.img.rect.width;
				vdec->avbc_info.avbc_height = wrapper->in.img.rect.height;
				vdec->avbc_info.bitdepth_src = wrapper->in.img.bitdep;
				vdec->avbc_info.align_w = 64;
				vdec->avbc_info.align_h = 64;
				vdec->avbc_info.avbc_header_addr = (ulong)wrapper->in.img.data;
			}
			if (out)
				v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: p frame, avbc_wxh(%dx%d), dst_addr(0x%lx), y_addr(0x%lx), size(%u), avbc_header_addr(0x%lx), format(0x%x)\n",
						__func__,
						vdec->avbc_info.avbc_width,
						vdec->avbc_info.avbc_height,
						(ulong)out->img.data,
						vdec->avbc_info.avbc_y_addr ? vdec->avbc_info.avbc_y_addr : (ulong)out->img.data,
						wrapper->hw_buf.size,
						vdec->avbc_info.avbc_header_addr,
						vdec->avbc_info.format);
		} else {
			vdec->avbc_info.avbc_width = wrapper->in.img.rect.width;
			vdec->avbc_info.avbc_height = wrapper->in.img.rect.height;
			vdec->avbc_info.bitdepth_src = wrapper->in.img.bitdep;
			vdec->avbc_info.align_w = 64;
			vdec->avbc_info.align_h = 64;
			vdec->avbc_info.avbc_header_addr = (ulong)wrapper->in.img.data;
			if (kfifo_peek(&wrapper->out, &out))
				vdec->avbc_info.bitdepth_dst = out->img.bitdep;

			if ((wrapper->vdec->avbc_mode & AVBCD_HARDWARE_STANDARD_MODE) &&
				kfifo_peek(&wrapper->out, &out)) {
				vdec->avbc_info.avbc_y_addr = (ulong)out->img.data;
				vdec->avbc_info.format = out->img.format != AML_PIX_FMT_NV21 ?
								AML_PIX_FMT_NV12 : AML_PIX_FMT_NV21;
				vdec->avbc_info.bitdepth_dst = out->img.bitdep;
				if (wrapper->flag & AVBC_FLAG_IO_BLOCKING) {
					u32 size = ALIGN(vdec->avbc_info.avbc_width, 64) * ALIGN(vdec->avbc_info.avbc_height, 64) * 3 /2;
					if (!((out->img.rect.width == ALIGN(vdec->avbc_info.avbc_width, 64)) &&
						(out->img.rect.height == ALIGN(vdec->avbc_info.avbc_height, 64)))) {
						if (vdec->avbc_info.bitdepth_dst == 10)
							size = size * 2;
						wrapper->hw_buf.size = size;
						wrapper->hw_buf.phy_addr = codec_mm_alloc_for_dma("AVBCD_BUF", PAGE_COUNT(size), 0, 0);
						wrapper->hw_buf.virt_addr = codec_mm_vmap(wrapper->hw_buf.phy_addr, size);
						vdec->avbc_info.avbc_y_addr = (ulong)wrapper->hw_buf.phy_addr;
					}
				}
			}

			wrapper->width = wrapper->in.img.rect.width;
			wrapper->height = wrapper->in.img.rect.height;

			memcpy(data, trigger_i_1080, I_FRAME_SIZE);
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: init the first frame! avbc_header_addr %lx avbc_y_addr  %lx \n", __func__, vdec->avbc_info.avbc_header_addr, vdec->avbc_info.avbc_y_addr);
			do_gettimeofday(&wrapper->start);
		}
	}

out:
	if (wrapper->stop_flag) {
		wrapper->dec_result = DEC_RESULT_ERROR;
		queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
	} else {
		if (wrapper->hard_mode && wrapper->in.img.data) {
			vdec->run_avbc(vdec, mask, wrapper->vdec_cb, wrapper->vdec_cb_arg);
			if (vdec->avbc_mode & AVBCD_HARDWARE_STANDARD_MODE) {
				wrapper->dec_result = DEC_RESULT_DONE;
				if (!wrapper->in.img.data)
					wrapper->dec_result = DEC_RESULT_EOS;
				queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
			}
		} else {
			wrapper->dec_result = DEC_RESULT_DONE;
			if (!wrapper->in.img.data)
				wrapper->dec_result = DEC_RESULT_EOS;
			queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
		}
	}
}

void aml_avbc_wrapper_reset(void *priv)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)priv;
	struct vdec_s *vdec = wrapper->vdec;
	int i;

	vdec_reset(vdec);

	mutex_lock(&avbc_mutex);

	flush_workqueue(wrapper->avbc_workqueue);
	wrapper->dec_result = DEC_RESULT_NONE;

	kfifo_reset(&wrapper->input);
	kfifo_reset(&wrapper->in_done_q);
	kfifo_reset(&wrapper->out);

	for (i = 0 ; i < AVBCD_FRAME_SIZE ; i++) {
		memset(&wrapper->inputpool[i], 0, sizeof(struct aml_avbc_wrapper_buf ));
		kfifo_put(&wrapper->input, &wrapper->inputpool[i]);
	}

	mutex_unlock(&avbc_mutex);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success!\n", __func__);
}
EXPORT_SYMBOL(aml_avbc_wrapper_reset);

void aml_avbc_wrapper_start(void *priv)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)priv;

	mutex_lock(&avbc_mutex);

	wrapper->stop_flag = false;

	mutex_unlock(&avbc_mutex);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success!\n", __func__);
}
EXPORT_SYMBOL(aml_avbc_wrapper_start);

void aml_avbc_wrapper_stop(void *priv)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)priv;

	mutex_lock(&avbc_mutex);

	wrapper->stop_flag = true;

	mutex_unlock(&avbc_mutex);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success!\n", __func__);
}
EXPORT_SYMBOL(aml_avbc_wrapper_stop);


static void aml_buf_avbcd_worker(struct work_struct *work)
{
	struct aml_avbc_wrapper_s *wrapper =
		container_of(work, struct aml_avbc_wrapper_s, avbc_work);
	struct avbc_output *out = NULL;
	int ret = 0;

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s dec_result: %d, len: %d\n",
			__func__, wrapper->dec_result, kfifo_len(&wrapper->out));

	if (wrapper->stop_flag || !kfifo_len(&wrapper->out))
		goto out;

	if (wrapper->dec_result == DEC_RESULT_DONE) {
		if (wrapper->vicp_mode) {
			if (!kfifo_get(&wrapper->out, &out))
				goto out;
		} else if (!wrapper->hard_mode && !wrapper->vicp_mode) {
			if (!kfifo_get(&wrapper->out, &out))
				goto out;
			if (avbcd_work_mode & AVBCD_SOFT_KERNEL_MODE)
				aml_avbcd_process_one_frame(&wrapper->in, out);
			else {
				ret = aml_avbcd_submit_one_frame(&wrapper->in, out, wrapper);
				if (ret)
					wrapper->dec_result = DEC_RESULT_ERROR;
			}
		} else if (wrapper->hard_mode) {
			if (!wrapper->hw_buf.phy_addr) {
				if (!kfifo_get(&wrapper->out, &out))
					goto out;
			} else {
				wrapper->wait_complete = true;
				queue_work(wrapper->avbc_workqueue, &wrapper->copy_work);
			}
		} else {
			v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "%s Get out fifo fail!\n", __func__);
			goto out;
		}

		if (out && out->done_func && (!wrapper->hard_mode ||
			(wrapper->hard_mode && wrapper->frame_count) ||
			wrapper->vdec->avbc_mode & AVBCD_HARDWARE_STANDARD_MODE))
			out->done_func(out);
	} else if (wrapper->dec_result == DEC_RESULT_EOS) {
		if (kfifo_get(&wrapper->out, &out)) {
			if (out->done_func)
				out->done_func(out);
		}
	} else if (wrapper->dec_result == DEC_RESULT_ERROR) {
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s Drop frame!\n", __func__);
	}

out:
	vdec_vframe_dirty(wrapper->vdec, wrapper->chunk);
	wrapper->chunk = NULL;

	if (wrapper->hard_mode)
		vdec_core_finish_run(wrapper->vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(wrapper->vdec, CORE_MASK_AVBCD_SOFT);

	if (wrapper->vdec_cb)
		wrapper->vdec_cb(wrapper->vdec, wrapper->vdec_cb_arg, CORE_MASK_HEVC);

	mutex_lock(&wrapper->copy_lock);
	if (!wrapper->wait_complete)
		complete(&wrapper->avbc_done);
	mutex_unlock(&wrapper->copy_lock);
}

static void aml_buf_copy_worker(struct work_struct *work)
{
	struct aml_avbc_wrapper_s *wrapper =
		container_of(work, struct aml_avbc_wrapper_s, copy_work);
	struct avbc_output *out;

	mutex_lock(&wrapper->copy_lock);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s start!\n", __func__);

	if (kfifo_get(&wrapper->out, &out)) {
		if (ge2d_copy)
			copy_ge2d(wrapper, out);
		else
			copy_yuv(wrapper, out);
	}

	if (wrapper->hw_buf.phy_addr) {
		codec_mm_unmap_phyaddr(wrapper->hw_buf.virt_addr);
		codec_mm_free_for_dma("AVBCD_BUF", wrapper->hw_buf.phy_addr);
		wrapper->hw_buf.phy_addr = 0;
		wrapper->hw_buf.virt_addr = 0;
		wrapper->hw_buf.size = 0;
	}
	wrapper->wait_complete = false;
	complete(&wrapper->avbc_done);

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s end!\n", __func__);
	mutex_unlock(&wrapper->copy_lock);
}

static irqreturn_t avbc_irq_cb(struct vdec_s *vdec, int irq)
{
	return vdec->avbc_irq_handler(vdec, irq);
}

u8 *aml_avbcd_wrapper_yuv_dump(struct file *fp, u8 *start_addr, u32 real_width, u32 real_height, u32 align, u32 bitdepth)
{
	u32 coded_width = ALIGN(real_width, align);
	u32 coded_height = ALIGN(real_height, align);
	u8 *yuv_data_addr = start_addr;
	u32 stride = coded_width;
	u32 size;

	 if (bitdepth == 10)
		stride = stride * 2;

	size = stride * coded_height * 3 / 2;
	media_write(fp, yuv_data_addr, size, 0);

	return (start_addr + size);
}

static irqreturn_t avbc_isr_thread_fn(int irq, void *data)
{
	struct vdec_s *vdec = (struct vdec_s *)data;
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)vdec->wrapper;
	ulong time_use = 0;

	vdec->avbc_threaded_irq_handler(vdec, irq);

	if (vdec->pic_end) {
		vdec->pic_end = 0;
		if (!vdec->pic0_done) {
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s init\n", __func__);
			vdec->pic0_done = 1;
			wrapper->chunk = NULL;
			vdec->run(vdec, 0, NULL, NULL);
			/* boost CPU frequency for the efficiency of copy_yuv() */
			if (wrapper->hw_buf.phy_addr)
				media_set_cpufreq(&media_copy_qos_req, media_get_max_cpufreq(),
					MEDIA_BOOST_FREQ_MIN);
		} else {
			do_gettimeofday(&wrapper->end);
			time_use = (wrapper->end.tv_sec - wrapper->start.tv_sec) * 1000 * 1000 +
				(wrapper->end.tv_usec - wrapper->start.tv_usec);
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s done! time_use:%lu us\n", __func__, time_use);
			if (!dec_i_frame_once)
				vdec->pic0_done = 0;
			wrapper->dec_result = DEC_RESULT_DONE;
			if (!wrapper->in.img.data)
				wrapper->dec_result = DEC_RESULT_EOS;

			do {
				struct vdec_s *vdec = wrapper->vdec;
				struct file *fp;
				char file_name[64] = {0};

				if (!dump_avbcd_frame)
					break;

				snprintf(file_name, 64, "%s/avbcd_dump_%ux%u_%u.yuv",
					dump_path, vdec->avbc_info.avbc_width, vdec->avbc_info.avbc_height, dump_avbcd_frame);

				fp = media_open(file_name,
						O_CREAT | O_RDWR | O_LARGEFILE | O_APPEND, 0666);

				if (!IS_ERR(fp)) {
					u32 length = ALIGN(vdec->avbc_info.avbc_width, vdec->avbc_info.align_w) *
						ALIGN(vdec->avbc_info.avbc_height, vdec->avbc_info.align_h) * 3 / 2;
					void *y_vaddr;

					if (vdec->avbc_info.bitdepth_dst == 10)
						length = length * 2;
					y_vaddr = codec_mm_vmap(vdec->avbc_info.avbc_y_addr, length);

					aml_avbcd_wrapper_yuv_dump(fp, (u8 *)y_vaddr,
						vdec->avbc_info.avbc_width, vdec->avbc_info.avbc_height, 64, vdec->avbc_info.bitdepth_dst);
					codec_mm_unmap_phyaddr(y_vaddr);

					pr_info("dump idx: %d %dx%d\n", dump_avbcd_frame, vdec->avbc_info.avbc_width, vdec->avbc_info.avbc_height);
					dump_avbcd_frame--;
					media_close(fp, NULL);
				}
			} while(0);
			wrapper->frame_count++;
			queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t avbc_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	return avbc_isr_thread_fn(0, vdec);
}

static void reset(struct vdec_s *vdec)
{

}

static int avbcd_wrapper_probe(struct vdec_s *vdec)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)vdec->wrapper;

	vdec->reset = reset;
	vdec->run_ready = run_ready;
	vdec->run = run;
	vdec->irq_handler = avbc_irq_cb;
	vdec->threaded_irq_handler = avbc_threaded_irq_cb;

	//vdec->dump_state = avbc_dump_state;

	vdec_set_prepare_level(vdec, 1);
	//hevc_source_changed(VFORMAT_AV1, 4096, 2048, 60);

	if (!wrapper->hard_mode)
		vdec_core_request(vdec, CORE_MASK_AVBCD_SOFT);

	return 0;
}

static int aml_avbc_enable_hardware(struct stream_port_s *port)
{
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6)
		return -1;

	amports_switch_gate("demux", 1);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		amports_switch_gate("parser_top", 1);

	if (port->type & PORT_TYPE_VIDEO) {
		amports_switch_gate("vdec", 1);

		if (has_hevc_vdec()) {
			if (port->type & PORT_TYPE_HEVC)
				vdec_poweron(VDEC_HEVC);
			else
				vdec_poweron(VDEC_1);
		} else {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
				vdec_poweron(VDEC_1);
		}
	}

	return 0;
}

static int aml_avbc_disable_hardware(struct stream_port_s *port)
{
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6)
		return -1;

	if (port->type & PORT_TYPE_VIDEO) {
		if (has_hevc_vdec()) {
			if (port->type & PORT_TYPE_HEVC)
				vdec_poweroff(VDEC_HEVC);
			else
				vdec_poweroff(VDEC_1);
		}

		amports_switch_gate("vdec", 0);
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		amports_switch_gate("parser_top", 0);

	amports_switch_gate("demux", 0);

	return 0;
}

int aml_avbc_wrapper_init(void **pwrapper, void *para)
{
	struct vdec_s *vdec = NULL;
	struct aml_avbc_wrapper_s *wrapper = NULL;
	struct avbc_input *avbc_in = (struct avbc_input *)para;
	int ret = -1;
	int i;
	int cpu_ver_id = get_cpu_major_id();

	mutex_lock(&avbc_mutex);
	if (g_wrapper) {
		*pwrapper = g_wrapper;
		mutex_unlock(&avbc_mutex);
		return 0;
	}

	wrapper = aml_media_mem_alloc(sizeof(struct aml_avbc_wrapper_s), GFP_KERNEL);
	if (!wrapper) {
		goto err_no_mem;
	}

	if (!(avbcd_work_mode & 0x8000)) {
		switch (cpu_ver_id) {
		case AM_MESON_CPU_MAJOR_ID_S6:
		case AM_MESON_CPU_MAJOR_ID_S5:
		case AM_MESON_CPU_MAJOR_ID_T3X:
			avbcd_work_mode = AVBCD_VICP_MODE;
			break;
		case AM_MESON_CPU_MAJOR_ID_S7:
		case AM_MESON_CPU_MAJOR_ID_T6D:
			avbcd_work_mode = AVBCD_AUTO_MODE;
			break;
		case AM_MESON_CPU_MAJOR_ID_T6W:
		case AM_MESON_CPU_MAJOR_ID_T6X:
			avbcd_work_mode = AVBCD_HARDWARE_STANDARD_MODE;
			break;
		default:
			avbcd_work_mode = AVBCD_SOFT_USER_MODE;
		}
	}

	if ((avbcd_work_mode & AVBCD_AUTO_MODE)) {
		u32 src_w = 0;
		u32 src_h = 0;

		if (avbc_in) {
			src_w = avbc_in->img.rect.width;
			src_h = avbc_in->img.rect.height;
		}

		if ((src_w == 2560 && src_h == 1090) ||
			(src_w == 3840 && src_h == 1634) ||
			(src_w == 256 && src_h == 108) ||
			(src_w == 854 && src_h == 362) ||
			(src_w == 1920 && src_h == 818))
			avbcd_work_mode |= AVBCD_SOFT_USER_MODE;
		else
			avbcd_work_mode |= AVBCD_HARDWARE_LEGACY_MODE;
	}

	init_completion(&wrapper->avbc_done);
	atomic_set(&wrapper->ref, 1);
	mutex_init(&wrapper->copy_lock);
	wrapper->avbc_workqueue =
		alloc_ordered_workqueue("avbcd-worker",
			__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_HIGHPRI);
	if (!wrapper->avbc_workqueue) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "Failed to create avbc workqueue\n");
		goto err_queue_alloc_fail;
	}

	/* create the vdec instance.*/
	vdec = vdec_create(&wrapper->port, NULL, 0);
	if (IS_ERR_OR_NULL(vdec))
		goto vdec_create_fail;

	wrapper->format 	= VFORMAT_AVBCD;
	vdec->port		= &wrapper->port;
	vdec->format		= wrapper->format;

	vdec->sys_info_store.format = vdec->format;
	vdec->sys_info_store.width = 1920;
	vdec->sys_info_store.height = 1088;
	vdec->sys_info_store.rate = 3200;

	vdec->wrapper = wrapper;

	/* set video format, sys info and vfm map.*/
	vdec->port->vformat = vdec->format;
	vdec->port->type |= PORT_TYPE_VIDEO;
	vdec->port_flag |= (vdec->port->flag | PORT_FLAG_VFORMAT);

	vdec->type = VDEC_TYPE_FRAME_BLOCK;
	vdec->port->type |= PORT_TYPE_FRAME;
	vdec->port->type &= ~PORT_TYPE_ES;

	vdec->port->flag = vdec->port_flag;
	wrapper->vdec = vdec;
	vdec->disable_vfm = true;
	vdec->avbc_mode = avbcd_work_mode;
	if (vdec->avbc_mode &
		(AVBCD_HARDWARE_LEGACY_MODE | AVBCD_HARDWARE_STANDARD_MODE))
		wrapper->hard_mode = 1;
	else if (vdec->avbc_mode & AVBCD_VICP_MODE)
		wrapper->vicp_mode = 1;
	aml_avbc_enable_hardware(&wrapper->port);
	vdec_init(vdec, 0, 0);
	avbcd_wrapper_probe(vdec);

	/* connect vdec at the end after all HW initialization */
	vdec_connect(vdec);

	INIT_WORK(&wrapper->avbc_work, aml_buf_avbcd_worker);
	INIT_WORK(&wrapper->copy_work, aml_buf_copy_worker);

	INIT_KFIFO(wrapper->input);
	INIT_KFIFO(wrapper->in_done_q);
	INIT_KFIFO(wrapper->out);

	ret = kfifo_alloc(&wrapper->input, AVBCD_FRAME_SIZE, GFP_KERNEL);
	if (ret) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "alloc input fifo fail.\n");
		goto input_fifo_alloc_fail;
	}

	wrapper->inputpool = vzalloc(AVBCD_FRAME_SIZE * sizeof(*wrapper->inputpool));
	if (!wrapper->inputpool) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "alloc input vb pool fail.\n");
		ret = -1;
		goto pool_alloc_fail;
	}

	for (i = 0 ; i < AVBCD_FRAME_SIZE ; i++) {
		kfifo_put(&wrapper->input, &wrapper->inputpool[i]);
	}

	g_wrapper = wrapper;
	*pwrapper = wrapper;

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success! avbcd_work_mode 0x%x\n", __func__, avbcd_work_mode);

	mutex_unlock(&avbc_mutex);

	return 0;

pool_alloc_fail:
	kfifo_free(&wrapper->input);
input_fifo_alloc_fail:
	vdec_destroy(vdec);
vdec_create_fail:
	destroy_workqueue(wrapper->avbc_workqueue);
err_queue_alloc_fail:
	aml_media_mem_free(wrapper);
err_no_mem:
	mutex_unlock(&avbc_mutex);

	return ret;
}
EXPORT_SYMBOL(aml_avbc_wrapper_init);

void aml_avbc_wrapper_destroy(void *priv)
{
	struct aml_avbc_wrapper_s *wrapper;
	struct vdec_s *vdec;

	if (!priv)
		return;

	mutex_lock(&avbc_mutex);
	wrapper = (struct aml_avbc_wrapper_s *)priv;
	if (atomic_dec_return(&wrapper->ref)) {
		mutex_unlock(&avbc_mutex);
		return;
	}
	vdec = wrapper->vdec;

	flush_workqueue(wrapper->avbc_workqueue);
	destroy_workqueue(wrapper->avbc_workqueue);

	if (!wrapper->hard_mode)
		vdec_core_release(vdec, CORE_MASK_AVBCD_SOFT);

	vdec_release(vdec);
	aml_avbc_disable_hardware(&wrapper->port);
	vfree(wrapper->inputpool);
	kfifo_free(&wrapper->input);

	aml_media_mem_free(wrapper);
	g_wrapper = NULL;
	mutex_unlock(&avbc_mutex);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success!\n", __func__);
}
EXPORT_SYMBOL(aml_avbc_wrapper_destroy);

int aml_avbc_decode(struct avbc_output *out, struct avbc_input *in, u32 flag)
{
	struct aml_avbc_wrapper_s *wrapper;
	int ret = -1;

	mutex_lock(&task_mutex);

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "is_support_avbc_wrapper(%d). avbcd_work_mode(%d)\n",
		is_support_avbc_wrapper(), avbcd_work_mode);

	if (!is_support_avbc_wrapper() && !(avbcd_work_mode & 0x8000))
		goto out;

	ret = aml_avbc_wrapper_init((void**)&wrapper, in);
	if (ret) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "[ERR] aml_avbc_wrapper_init fail.\n");
		goto out;
	}

	kfifo_put(&wrapper->out, out);

	wrapper->flag = flag;
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s in:  header_addr 0x%lx, header_size %u, width %u, height %u, bitdepth %u flag %d\n",
		__func__,
		(ulong)in->img.data,
		in->img.size,
		in->img.rect.width,
		in->img.rect.height,
		in->img.bitdep,
		flag);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s out:  header_addr 0x%lx, header_size %u, width %u, height %u, bitdepth %u flag %d\n",
		__func__,
		(ulong)out->img.data,
		out->img.size,
		out->img.rect.width,
		out->img.rect.height,
		out->img.bitdep,
		flag);

	ret = vdec_write_vframe(wrapper->vdec, (const char *)in, P_FRAME_SIZE, NULL, NULL, NULL);
	if (ret < 0) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "[ERR] %s fail!\n", __func__);
		goto out;
	}

	ret = 0;
	if (flag & AVBC_FLAG_IO_BLOCKING) {
		if(!wait_for_completion_timeout(&wrapper->avbc_done,
			msecs_to_jiffies(20000))) {
			ret = -1;
			goto out;
		}
	}
	if (wrapper->dec_result == DEC_RESULT_ERROR)
		ret = -1;

out:
	mutex_unlock(&task_mutex);

	return ret;
}
EXPORT_SYMBOL(aml_avbc_decode);

static void avbc_vf_get(void *caller, struct vframe_s *vf_out)
{
	struct aml_avbc_wrapper_s *wrapper = (struct aml_avbc_wrapper_s *)caller;
	struct aml_avbc_wrapper_buf *avbcd_buf = NULL;
	struct aml_buf *am_buf = NULL;
	struct vframe_s *vf = NULL;
	bool bypass = false;
	bool eos = false;

	if (!wrapper) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"fatal %s %d wrapper:%px\n",
			__func__, __LINE__, wrapper);
		return;
	}

	if (kfifo_get(&wrapper->in_done_q, &avbcd_buf)) {
		am_buf	= avbcd_buf->aml_vb->aml_buf;
		vf	= &avbcd_buf->vf;
		eos	= (avbcd_buf->flag & AVBCD_FLAG_EOS);
		bypass	= (avbcd_buf->flag & AVBCD_FLAG_BUF_BY_PASS);

		if (eos) {
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s %d got eos\n",
				__func__, __LINE__);
			vf->type |= VIDTYPE_V4L_EOS;
			vf->flag = VFRAME_FLAG_EMPTY_FRAME_V4L;
		}

		memcpy(vf_out, vf, sizeof(struct vframe_s));
		kfifo_put(&wrapper->input, avbcd_buf);

		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: vf:%px, index:%d, flag:%x, ts:%lld\n",
			__func__, vf,
			vf->index,
			vf->flag,
			vf->timestamp);
	}else
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "%s: in_done_q is empty!\n", __func__);
}

static void avbc_vf_put(void *caller, struct vframe_s *vf)
{

}

static int aml_v4l2_avbc_push_vframe(struct aml_avbc_wrapper_s *wrapper, struct vframe_s *vf)
{
	struct aml_avbc_wrapper_buf *in_buf;

	if (!wrapper)
		return -EINVAL;

	if (!kfifo_get(&wrapper->input, &in_buf)) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "can not get free input buffer.\n");
		return -1;
	}
	if (vf->type & VIDTYPE_V4L_EOS)
		in_buf->flag |= AVBCD_FLAG_EOS;

	memcpy(&in_buf->vf, vf, sizeof(struct vframe_s));

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "avbc_push_vframe: vf:%px, idx:%d, type:%x, ts:%lld flag:%x, avbcd(flag: 0x%x)\n",
		vf, vf->index, vf->type, vf->timestamp, vf->flag, in_buf->flag);

	kfifo_put(&wrapper->in_done_q, in_buf);

	return 0;
}

static void fill_avbc_buf_cb(void *v4l_ctx, void *fb_ctx)
{
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)v4l_ctx;
	struct aml_buf *am_buf = (struct aml_buf *)fb_ctx;
	struct aml_avbc_wrapper_s *wrapper;
	int ret = -1;

	wrapper = (struct aml_avbc_wrapper_s *)ctx->avbc_wrapper;
	ret = aml_v4l2_avbc_push_vframe(wrapper, &am_buf->vframe);
	if (ret < 0) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR,
			"avbc push vframe err, ret: %d\n", ret);
	}
}

struct aml_avbc_wrapper_s *aml_avbc_get_wrapper(void)
{
	return g_wrapper;
}
EXPORT_SYMBOL(aml_avbc_get_wrapper);

static struct task_ops_s avbc_ops = {
	.type		= TASK_TYPE_AVBCD,
	.get_vframe	= avbc_vf_get,
	.put_vframe	= avbc_vf_put,
	.fill_buffer	= fill_avbc_buf_cb,
};

struct task_ops_s *get_avbc_ops(void)
{
	return &avbc_ops;
}
EXPORT_SYMBOL(get_avbc_ops);

