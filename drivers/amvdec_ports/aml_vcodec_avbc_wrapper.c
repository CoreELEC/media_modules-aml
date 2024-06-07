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
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/amlogic/media/meson_uvm_allocator.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/types.h>

#include "vdec_drv_if.h"
#include "aml_vcodec_avbc_wrapper.h"
#include "trigger_data.h"
#include "utils/common.h"
#include "../frame_provider/decoder/utils/aml_buf_helper.h"
#include "aml_vcodec_dec.h"
#include "../stream_input/amports/streambuf.h"
#include "../frame_provider/decoder/utils/vdec.h"
#include "../frame_provider/aml_dhp/aml_dhp_if.h"


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
 * struct aml_avbc_wrapper_buf - Parameter AVBCD structure.
 *
 * @dst_addr		: Yuv virtual address.
 * @byte_stride		: Align of width for yuv.
 * @width		: Aligned width of yuv buffer.
 * @height		: Aligned height of yuv buffer.
 * @align_w		: Align width for yuv buffer.
 * @align_h		: Align height for yuv buffer.
 * @bitdepth		: Bitdepth of stream.
 * @avbc_width		: Original width.
 * @avbc_height		: Original height.
 * @header_addr		: AVBCD header bufer physic address.
 */
struct soft_data_t {
	char *dst_addr;
	int byte_stride;
	u32 width;
	u32 height;
	u32 align_w;
	u32 align_h;
	u32 bitdepth;
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

/*
 * struct aml_avbc_wrapper_buf - Parameter AVBCD structure.
 *
 * @id			: Instance ID of AVBCD Wrapper context.
 * @ref			: Reference count of AVBCD Wrapper context.
 * @mutex_lock		: Lock is used to ensure interface serialization..
 * @avbc_queue		: Avbcd task queue for process.
 * @port		: ES input port context.
 * @format		: Stream Protocol.
 * @vdec		: Point to vdec context.
 * @vdec_cb		: Vdec callback interface.
 * @vdec_cb_arg		: Parameter of vdec callback interface.
 * @chunk		: vframe_chunk in frame mode.
 * @data_offset		: Chunk of address offset for one frame.
 * @data_size		: Frame size of input.
 * @inputpool		: Input buffer pool.
 * @in			: Data information of frame.
 * @input		: Kfifo of input container.
 * @out			: Kfifo of output buffer.
 * @avbc_workqueue	: Queue to procress work of Post-interrupt.
 * @avbc_work		: Work of Post-interrupt.
 * @dec_result		: AVBCD processed result.
 * @avbc_done		: Wait for finish of AVBCD process .
 * @frame_count		: AVBCD process frame count.
 * @stop_flag		: Stream off flag.
 */
struct aml_avbc_wrapper_s {
	ulong 				id;
	atomic_t			ref;
	struct mutex			mutex_lock;
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
	int				dec_result;
	struct completion 		avbc_done;
	int				hard_mode;
	u32				frame_count;
	u32				stop_flag;
};

static DEFINE_MUTEX(avbc_mutex);
struct aml_avbc_wrapper_s *g_wrapper;

extern int avbcd_work_mode;
extern int crc_dump;

static void do_vframe_avbc_soft_decode(struct soft_data_t *soft_data)
{
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
	u32 align_h = soft_data->align_h;
	u32 hstride = align_h ? ALIGN(soft_data->height, align_h) : soft_data->height;

	if ((soft_data->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	y_size = soft_data->avbc_width * soft_data->avbc_height * sizeof(short);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "width: %d, height: %d, compWidth: %u, compHeight: %u,bit10:%d.\n",
		 soft_data->width, soft_data->height, soft_data->avbc_width, soft_data->avbc_height, bit_10);

	if (soft_data->byte_stride == soft_data->width && bit_10 == 1) {
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
	vu_dst = soft_data->dst_addr + soft_data->byte_stride * hstride;
	vu_dst_10 = (short *)(soft_data->dst_addr + soft_data->byte_stride * hstride);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "offset uv: %d\n", soft_data->byte_stride * hstride);
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
			y_dst += soft_data->byte_stride;
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
		vu_dst += soft_data->byte_stride;
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
}

int aml_avbcd_process_one_frame(struct avbc_input *input, struct avbc_output	*output)
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
	struct dma_buf *dmabuf = output->m.dbuf;
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
	soft_data.byte_stride = buffer->byte_stride;
	soft_data.width = buffer->width;
	soft_data.height = buffer->height;
	soft_data.align_w = output->align_w;
	soft_data.align_h = output->align_h;
	soft_data.header_addr = input->header_addr;
	soft_data.avbc_width = input->width;
	soft_data.avbc_height = input->height;
	soft_data.bitdepth = input->bitdepth == 10 ? (BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10) :
				(BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s. width=%d height=%d byte_stride=%d align(%d, %d, %d)\n",
			__func__, buffer->width, buffer->height,
			buffer->byte_stride, output->align_w, output->align_h, buffer->align);
	do_vframe_avbc_soft_decode(&soft_data);
	dma_buf_end_cpu_access(buffer->idmabuf[0], DMA_BIDIRECTIONAL);
	vunmap(vaddr);

	return 0;
}

static int aml_dhp_avbcd(struct avbc_output *output, struct avbc_input *input, u32 pts)
{
	struct aml_du_mem	src;
	struct aml_du_mem	dst;
	struct aml_du_avbcd	avbcd;
	ulong header = input->header_addr;
	u32 h_size = input->header_size;
	u32 width = input->width;
	u32 height = input->height;
	u32 depth = input->bitdepth;
	ulong buf = output->m.phy;
	u32 buf_size = output->length;
	u32 align_w = output->align_w;
	u32 align_h = output->align_h;
	int ret = 0;

	//fill avbcd pic info
	avbcd.type	= AML_DHP_TYPE_AVBCD;
	avbcd.width	= width;
	avbcd.height	= height;
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
	dst.w_align	= align_w;
	dst.h_align	= align_h;

	ret = dhp_func_request(&src, &dst, &avbcd, sizeof(avbcd));
	if (ret)
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "Do dhp task fail. err:%d\n", ret);

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
	input->header_size = get_header_size(input->width, input->height);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR,
		"h:%lx, hsize:%d, wxh:%ux%u, dep:%d, buf:%lx, size:%u, align(%u, %u), pts:%d.\n",
		input->header_addr,
		input->header_size,
		input->width,
		input->height,
		input->bitdepth,
		output->m.phy,
		output->length,
		output->align_w,
		output->align_h,
		wrapper->frame_count);

	aml_dhp_avbcd(output, input, wrapper->frame_count);

	wrapper->frame_count++;

	return 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct vframe_chunk_s *chunk = NULL;

	chunk = vdec_input_next_chunk(&vdec->input);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "avbc wrapper %s: mask 0x%lx chunk %px\n",
		__func__, mask, chunk);
	if (!chunk)
		return 0;

	if (vdec->parallel_dec == 1)
		return CORE_MASK_HEVC;
	else
		return (CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *, int), void *arg)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)vdec->wrapper;
	int r = 0;
	u8 *data = NULL;

	wrapper->vdec_cb = callback;
	wrapper->vdec_cb_arg = arg;

	wrapper->chunk = vdec_input_next_chunk(&vdec->input);
	r = wrapper->chunk->size;
	if ((vdec_frame_based(vdec)) &&
		(wrapper->chunk != NULL)) {
		wrapper->data_offset = wrapper->chunk->offset;
		wrapper->data_size = r;
	}

	wrapper->dec_result = DEC_RESULT_NONE;

	if (!wrapper->chunk->block->is_mapped)
		data = codec_mm_vmap(wrapper->chunk->block->start +
			wrapper->data_offset, r);
	else
		data = ((u8 *)wrapper->chunk->block->start_virt) +
			wrapper->data_offset;

	if (!wrapper->hard_mode || vdec->pic0_done) {
		struct avbc_output *out;
		memcpy(&wrapper->in, data, sizeof(struct avbc_input));
		vdec->avbc_header_addr = wrapper->in.header_addr;
		if (wrapper->hard_mode && kfifo_peek(&wrapper->out, &out))
			vdec->avbc_y_addr = out->m.phy;

		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: size 0x%x  header_addr 0x%lx, header_size %u, width %u, height %u y_addr 0x%lx\n",
			__func__, r,
			wrapper->in.header_addr,
			wrapper->in.header_size,
			wrapper->in.width,
			wrapper->in.height,
			vdec->avbc_y_addr);
	} else
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: init the first frame!\n", __func__);

	if (wrapper->hard_mode && vdec->pic0_done) {
		memset(data, 0, 623);
		memcpy(data, trigger_p_1080, 27);
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s: p frame\n", __func__);
	}
	if (!wrapper->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);

	if (wrapper->stop_flag) {
		wrapper->dec_result = DEC_RESULT_ERROR;
		queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
	} else {
		if (wrapper->hard_mode) {
			vdec->run_avbc(vdec, mask, callback, arg);
		} else {
			wrapper->dec_result = DEC_RESULT_DONE;
			if (!wrapper->in.header_addr)
				wrapper->dec_result = DEC_RESULT_EOS;
			queue_work(wrapper->avbc_workqueue, &wrapper->avbc_work);
		}
	}
}

void aml_avbc_wrapper_reset(void *priv)
{
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)priv;
	int i;

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
	struct avbc_output *out;

	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s dec_result %d \n",
			__func__, wrapper->dec_result);

	if (wrapper->stop_flag)
		goto out;

	if (wrapper->dec_result == DEC_RESULT_DONE) {
		if (kfifo_get(&wrapper->out, &out) && !wrapper->hard_mode) {
			if (avbcd_work_mode & AVBCD_SOFT_KERNEL_MODE)
				aml_avbcd_process_one_frame(&wrapper->in, out);
			else
				aml_avbcd_submit_one_frame(&wrapper->in, out, wrapper);
		} else {
			v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "%s Get out fifo fail!\n", __func__);
			goto out;
		}

		if (out->avbc_done && (!wrapper->hard_mode ||
			(wrapper->hard_mode && wrapper->frame_count)))
			out->avbc_done(out);
	} else if (wrapper->dec_result == DEC_RESULT_EOS) {
		if (kfifo_get(&wrapper->out, &out)) {
			if (out->avbc_done)
				out->avbc_done(out);
		}
	} else if (wrapper->dec_result == DEC_RESULT_ERROR) {
		v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s Drop frame!\n", __func__);
	}

out:
	vdec_vframe_dirty(wrapper->vdec, wrapper->chunk);
	wrapper->chunk = NULL;
	complete(&wrapper->avbc_done);

	if (wrapper->vdec->parallel_dec == 1)
		vdec_core_finish_run(wrapper->vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(wrapper->vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (wrapper->vdec_cb)
		wrapper->vdec_cb(wrapper->vdec, wrapper->vdec_cb_arg, CORE_MASK_HEVC);
}

static irqreturn_t avbc_irq_cb(struct vdec_s *vdec, int irq)
{
	return vdec->avbc_irq_handler(vdec, irq);
}

static irqreturn_t avbc_isr_thread_fn(int irq, void *data)
{
	struct vdec_s *vdec = (struct vdec_s *)data;
	struct aml_avbc_wrapper_s *wrapper =
		(struct aml_avbc_wrapper_s *)vdec->wrapper;

	vdec->avbc_threaded_irq_handler(vdec, irq);

	if (vdec->pic_end) {
		vdec->pic_end = 0;
		if (!vdec->pic0_done) {
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s init\n", __func__);
			vdec->pic0_done = 1;
			wrapper->chunk = NULL;
		} else {
			v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s done!\n", __func__);
			wrapper->dec_result = DEC_RESULT_DONE;
			if (!wrapper->in.header_addr)
				wrapper->dec_result = DEC_RESULT_EOS;
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

	if (!wrapper->hard_mode) {
		if (vdec->parallel_dec == 1)
			vdec_core_request(vdec, CORE_MASK_HEVC);
		else
			vdec_core_request(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);
	}

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

int aml_avbc_wrapper_init(void **pwrapper)
{
	struct vdec_s *vdec = NULL;
	struct aml_avbc_wrapper_s *wrapper = NULL;
	int ret = -1;
	int i;

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
	init_completion(&wrapper->avbc_done);
	atomic_set(&wrapper->ref, 1);
	wrapper->avbc_workqueue =
		alloc_ordered_workqueue("avbcd-worker",
			__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_HIGHPRI);
	if (!wrapper->avbc_workqueue) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "Failed to create avbc workqueue\n");
		goto err_queue_alloc_fail;
	}

	/* create the vdec instance.*/
	vdec = vdec_create(&wrapper->port, NULL);
	if (IS_ERR_OR_NULL(vdec))
		goto vdec_create_fail;

	wrapper->format 	= VFORMAT_HEVC;
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
	wrapper->hard_mode = 0;
	vdec->avbc_mode = avbcd_work_mode;
	aml_avbc_enable_hardware(&wrapper->port);
	vdec_init(vdec, 0, 0);
	avbcd_wrapper_probe(vdec);

	/* connect vdec at the end after all HW initialization */
	vdec_connect(vdec);

	INIT_WORK(&wrapper->avbc_work, aml_buf_avbcd_worker);

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

	if (wrapper->hard_mode)
		ret = vdec_write_vframe(wrapper->vdec, (const char *)trigger_i_1080, 623, NULL, NULL);
	v4l_dbg_avbcd(0, V4L_DEBUG_AVBCD_BUFMGR, "%s success! size %d\n", __func__, ret);

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

	vdec_core_release(vdec, CORE_MASK_HEVC);

	aml_avbc_disable_hardware(&wrapper->port);
	vdec_release(vdec);
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

	ret = aml_avbc_wrapper_init((void**)&wrapper);
	if (ret) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "[ERR] aml_avbc_wrapper_init fail.\n");
		return ret;
	}

	kfifo_put(&wrapper->out, out);

	ret = vdec_write_vframe(wrapper->vdec, (const char *)in, 69, NULL, NULL);
	if (ret < 0) {
		v4l_dbg_avbcd(0, V4L_DEBUG_CODEC_ERROR, "[ERR] %s fail!\n", __func__);
		goto out;
	}

	if (flag & AVBCD_IO_BLOCKING) {
		if(!wait_for_completion_timeout(&wrapper->avbc_done,
			msecs_to_jiffies(20000))) {
			ret = -1;
			goto out;
		}
	}

out:
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

