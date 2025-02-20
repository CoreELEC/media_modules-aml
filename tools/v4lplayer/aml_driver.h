/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef AML_DRIVER_H__
#define AML_DRIVER_H__

#include <stdint.h>

#define V4L2_CONFIG_PARM_DECODE_CFGINFO (1 << 0)
#define V4L2_CONFIG_PARM_DECODE_PSINFO  (1 << 1)
#define V4L2_CONFIG_PARM_DECODE_HDRINFO (1 << 2)
#define V4L2_CONFIG_PARM_DECODE_CNTINFO (1 << 3)

struct aml_vdec_cfg_infos {
	uint32_t double_write_mode;
	uint32_t init_width;
	uint32_t init_height;
	uint32_t ref_buf_margin;
	uint32_t canvas_mem_mode;
	uint32_t canvas_mem_endian;
	uint32_t low_latency_mode;
	uint32_t uvm_hook_type;
	/*
	* bit 21	: buffer alloc flag. 0: dma heap, 1: ion heap.
	* bit 20	: di post flag.
	* bit 19	: no-surface flag.
	* bit 18	: release vpp early. 0:false, 1:true
	* bit 17	: force di permission.
	* bit 16	: force progressive output flag.
	* bit 15	: enable nr.
	* bit 14	: enable di local buff.
	* bit 13	: report downscale yuv buffer size flag.
	* bit 12	: for second field pts mode.
	* bit 11	: disable error policy.
	* bit 10	: dynamic bypass vpp.
	* bit 9	: disable ge2d wrapper.
	* bit 8	: disable vpp wrapper.
	* bit 1	: Non-standard dv flag.
	* bit 0	: dv two layer flag.
	*/
	uint32_t metadata_config_flag; // for metadata config flag
	uint32_t duration;
	uint32_t triple_write_mode;
	uint32_t amld_profile;
	uint32_t data[2];
};

#define SEI_PicTiming         1
#define SEI_MasteringDisplayColorVolume 137
#define SEI_ContentLightLevel 144
struct vframe_content_light_level_s {
    uint32_t present_flag;
    uint32_t max_content;
    uint32_t max_pic_average;
}; /* content_light_level from SEI */

struct vframe_master_display_colour_s {
    uint32_t present_flag;
    uint32_t primaries[3][2];
    uint32_t white_point[2];
    uint32_t luminance[2];
    struct vframe_content_light_level_s
        content_light_level;
}; /* master_display_colour_info_volume from SEI */

struct aml_vdec_hdr_infos {
    /*
     * bit 29   : present_flag
     * bit 28-26: video_format "component", "PAL", "NTSC", "SECAM", "MAC", "unspecified"
     * bit 25   : range "limited", "full_range"
     * bit 24   : color_description_present_flag
     * bit 23-16: color_primaries "unknown", "bt709", "undef", "bt601",
     *            "bt470m", "bt470bg", "smpte170m", "smpte240m", "film", "bt2020"
     * bit 15-8 : transfer_characteristic unknown", "bt709", "undef", "bt601",
     *            "bt470m", "bt470bg", "smpte170m", "smpte240m",
     *            "linear", "log100", "log316", "iec61966-2-4",
     *            "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
     *            "smpte-st-2084", "smpte-st-428"
     * bit 7-0  : matrix_coefficient "GBR", "bt709", "undef", "bt601",
     *            "fcc", "bt470bg", "smpte170m", "smpte240m",
     *            "YCgCo", "bt2020nc", "bt2020c"
     */
    uint32_t signal_type;
    struct vframe_master_display_colour_s color_parms;
};

struct aml_vdec_ps_infos {
    uint32_t visible_width;
	uint32_t visible_height;
	uint32_t coded_width;
	uint32_t coded_height;
	uint32_t profile;
	uint32_t mb_width;
	uint32_t mb_height;
	uint32_t dpb_size;
	uint32_t ref_frames;
	uint32_t dpb_frames;
	uint32_t dpb_margin;
	uint32_t field;
	uint32_t bitdepth;
	uint32_t data[2];
};

struct aml_vdec_cnt_infos {
    uint32_t bit_rate;
    uint32_t frame_count;
    uint32_t error_frame_count;
    uint32_t drop_frame_count;
    uint32_t total_data;
};

struct aml_dec_params {
    /* one of V4L2_CONFIG_PARM_DECODE_xxx */
    uint32_t parms_status;
    struct aml_vdec_cfg_infos   cfg;
    struct aml_vdec_ps_infos    ps;
    struct aml_vdec_hdr_infos   hdr;
    struct aml_vdec_cnt_infos   cnt;
};

/******************* DEC INFO *******************/
enum E_DECINFO_EVENT {
	AML_DECINFO_EVENT_STREAM = 0,
	AML_DECINFO_EVENT_STATISTIC,
	AML_DECINFO_EVENT_AFD,
	AML_DECINFO_EVENT_CC,
	AML_DECINFO_EVENT_HDR10,
	AML_DECINFO_EVENT_HDR10P,
	AML_DECINFO_EVENT_CUVA,
	AML_DECINFO_EVENT_AMDV,
	AML_DECINFO_EVENT_FRAME,
	AML_DECINFO_EVENT_COMPOSITE = 30,
	AML_DECINFO_EVENT_BOTTOM = 31,
};

struct v4l_dec_data_extension {
	unsigned long ptr;  /* for future extension */
	__u32 data_size;
};

struct aml_vdec_hdr_infos_ {
	/*
	 * bit 29   : present_flag
	 * bit 28-26: video_format "component", "PAL", "NTSC", "SECAM", "MAC", "unspecified"
	 * bit 25   : range "limited", "full_range"
	 * bit 24   : color_description_present_flag
	 * bit 23-16: color_primaries "unknown", "bt709", "undef", "bt601",
	 *            "bt470m", "bt470bg", "smpte170m", "smpte240m", "film", "bt2020"
	 * bit 15-8 : transfer_characteristic unknown", "bt709", "undef", "bt601",
	 *            "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *            "linear", "log100", "log316", "iec61966-2-4",
	 *            "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	 *            "smpte-st-2084", "smpte-st-428"
	 * bit 7-0  : matrix_coefficient "GBR", "bt709", "undef", "bt601",
	 *            "fcc", "bt470bg", "smpte170m", "smpte240m",
	 *            "YCgCo", "bt2020nc", "bt2020c"
	 */
	__u32 signal_type;
	struct vframe_master_display_colour_s color_parms;
};

struct aux_data_static_t {
	__u32 info_type;    /* HDR10 HLG FMM or IMAX */
	struct aml_vdec_hdr_infos_ hdr_info;
};

struct v4l_userdata_meta_data_t {
	__u32 poc_number;
	/************ flags bit definition ***********/
	/* * bit 0: 0: top_field_first_flag is not valid
	*  1: top_field_first_flag is valid
	* bit 1: //top_field_first bit val
	*/
	__u16 flags;
	/*  0,  VFORMAT_MPEG12
	*  1,  VFORMAT_MPEG4
	*  2,  VFORMAT_H264
	*  3,  VFORMAT_MJPEG
	*  4,  VFORMAT_REAL
	*  5,  VFORMAT_JPEG
	*  6,  VFORMAT_VC1
	*  7,  VFORMAT_AVS
	*  8,  VFORMAT_SW
	*  9,  VFORMAT_H264MVC
	*  10, VFORMAT_H264_4K2K
	*  11, VFORMAT_HEVC
	*  12, VFORMAT_H264_ENC
	*  13, VFORMAT_JPEG_ENC
	*  14, VFORMAT_VP9
	*/
	__u16 video_format;
	/*        bit 0:     //used for mpeg2
	*  1, group start
	*  0, not group start
	*          bit 1-2:    //used for mpeg2
	*  0, extension_and_user_data( 0 )
	*  1, extension_and_user_data( 1 )
	*  2, extension_and_user_data( 2 )
	*/
	__u16 extension_data;
	/*        0, Unknown Frame Type
	* 1, I Frame
	* 2, B Frame
	* 3, P Frame
	* 4, D_Type_MPEG2
	*/
	__u16  frame_type;
	__u32 vpts;         /*video frame pts*/
	/*
	* 0: pts is invalid, please use duration to calculate
	* 1: pts is valid
	*/
	__u32 vpts_valid;
	/*used for sync*/
	__u64 timestamp;
	/* how many records left in queue waiting to be read*/
	__u32 records_in_que;
	unsigned long long priv_data;
	__u32 padding_data[64];
};

struct sei_usd_param_s {
	__u32 info_type;    /* CC or AFD */
	__u32 data_size;    /* size of the data domain */
	__u64 data_ptr;    /*pointer to data domain. Need to be converted before use */
	__u64 v_addr_ptr;  /* used in kernel space. Need to be converted before use */
	struct v4l_userdata_meta_data_t meta_data;  /* meta_data */
};

struct dec_statistics_info_s {
	__u32 info_ype;
	__u32 total_decoded_frames;
	__u32 error_frames;
	__u32 drop_frames;
	__u32 i_decoded_frames;
	__u32 i_drop_frames;
	__u32 i_error_frames;
	__u32 p_decoded_frames;
	__u32 p_drop_frames;
	__u32 p_error_frames;
	__u32 b_decoded_frames;
	__u32 b_drop_frames;
	__u32 b_error_frames;
	__u32 av_resynch_counter;
	__u64 total_decoded_datasize;
	char reserved[64];
};

struct vframe_qos_s {
	uint32_t num;
	uint32_t type;
	uint32_t size;
	uint32_t pts;
	int32_t max_qp;
	int32_t avg_qp;
	int32_t min_qp;
	int32_t max_skip;
	int32_t avg_skip;
	int32_t min_skip;
	int32_t max_mv;
	int32_t min_mv;
	int32_t avg_mv;
	int32_t decode_buffer;//For padding currently
} /*vframe_qos */;

struct dec_frame_info_s {
	__u32 info_type;
	struct vframe_qos_s qos;      /* qos信息 */
	__u32 num;
	__u32 type;
	__s32 frame_poc;
	__u32 decode_time_cost;
	__u32 pic_width;
	__u32 pic_height;
	__u32 error_flag;
	__u32 status;
	__u32 bitrate;
	__u32 field_output_order;
	__u32 offset;
	__u32 ratio_control;
	__u32 vf_type;
	__u32 signal_type;
	__u32 ext_signal_type;
	__u32 pts;
	__u64 pts_us64;
	__u64 timestamp;
	__u32 frame_size;
	char reserved[64];
};

enum E_ASPECT_RATIO {
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_16_9,
	ASPECT_UNDEFINED = 255
};

struct aspect_ratio_size {
	__s32 sar_width; /* -1 :invalid value */
	__s32 sar_height; /* -1 :invalid value */
	__s32 dar_width; /* -1 :invalid value */
	__s32 dar_height; /* -1 :invalid value */
};

struct dec_stream_info_s {
	__u32 info_type;
	char vdec_name[32];    /* vdec driver name */
	__u32 vdec_type;             /* 1: frame 0: stream mode */
	__u32 dual_core_flag;      /* single or dual core */
	__u32 is_secure;
	__u32 profile_idc;
	__u32 level_idc;
	__u32 filed_flag;
	__u32 frame_width;
	__u32 frame_height;
	__u32 crop_top;
	__u32 crop_bottom;
	__u32 crop_left;
	__u32 crop_right;
	__u32 frame_rate;
	__u32 fence_enable;
	__u32 fast_output_enable;
	__u32 trick_mode;
	__u32 bit_depth;
	__u32 double_write_mode;
	__u32 error_handle_policy;
	enum E_ASPECT_RATIO eu_aspect_ratio;  /* aspect ratio (4:3 or 16:9) */
	struct aspect_ratio_size ratio_size;   /* sar width/height, dar width/height */
	__u32 frame_dur;
	char reserved[60];
};

/**
 * struct vdec_common_s - Structure used to post decoder infos to upper
 */
struct vdec_common_s {
	__u32 version_magic;    /* version number of the interface */
	__u32 vdec_id;   /* id of current instance */
	__u32 type;   /* type of the current info packet */
	union {
		struct dec_stream_info_s stream_info;
		struct dec_frame_info_s frame_info;
		struct dec_statistics_info_s decoder_statistics;
		struct sei_usd_param_s usd_param;
		struct aux_data_static_t aux_data;
		struct v4l_dec_data_extension data_ext;
		char  raw_data[512];    /* for  future extension */
	} u;  /* data domain */
	__u32 size;    /* size of this struct  */
};
/******************* DEC INFO END*******************/

#endif
