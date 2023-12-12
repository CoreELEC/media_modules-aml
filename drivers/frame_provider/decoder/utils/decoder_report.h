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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef __DEC_REPORT_H__
#define __DEC_REPORT_H__

#define DEBUG_AMVDEC_PORTS "amvdec_ports"
#define DEBUG_AMVDEC_H265 "amvdec_h265"
#define DEBUG_AMVDEC_H265_V4L "amvdec_h265_v4l"
#define DEBUG_AMVDEC_H265_FB "amvdec_h265_fb"
#define DEBUG_AMVDEC_H265_FB_V4L "amvdec_h265_fb_v4l"
#define DEBUG_AMVDEC_H264 "amvdec_mh264"
#define DEBUG_AMVDEC_H264_V4L "amvdec_mh264_v4l"
#define DEBUG_AMVDEC_VP9 "amvdec_vp9"
#define DEBUG_AMVDEC_VP9_V4L "amvdec_vp9_v4l"
#define DEBUG_AMVDEC_VP9_FB "amvdec_vp9_fb"
#define DEBUG_AMVDEC_VP9_FB_V4L "amvdec_vp9_fb_v4l"
#define DEBUG_AMVDEC_AV1 "amvdec_av1"
#define DEBUG_AMVDEC_AV1_V4L "amvdec_av1_v4l"
#define DEBUG_AMVDEC_AV1_FB "amvdec_av1_fb"
#define DEBUG_AMVDEC_AV1_FB_V4L "amvdec_av1_fb_v4l"
#define DEBUG_AMVDEC_AV1_T5D_V4L "amvdec_av1_t5d_v4l"
#define DEBUG_AMVDEC_AVS "amvdec_mavs"
#define DEBUG_AMVDEC_AVS_V4L "amvdec_mavs_v4l"
#define DEBUG_AMVDEC_AVS2 "amvdec_avs2"
#define DEBUG_AMVDEC_AVS2_V4L "amvdec_avs2_v4l"
#define DEBUG_AMVDEC_AVS2_FB "amvdec_avs2_fb"
#define DEBUG_AMVDEC_AVS2_FB_V4L "amvdec_avs2_fb_v4l"
#define DEBUG_AMVDEC_AVS3 "amvdec_avs3"
#define DEBUG_AMVDEC_AVS3_V4L "amvdec_avs3_v4l"
#define DEBUG_AMVDEC_MPEG12 "amvdec_mmpeg12"
#define DEBUG_AMVDEC_MPEG12_V4L "amvdec_mmpeg12_v4l"
#define DEBUG_AMVDEC_MPEG4 "amvdec_mmpeg4"
#define DEBUG_AMVDEC_MPEG4_V4L "amvdec_mmpeg4_v4l"
#define DEBUG_AMVDEC_MJPEG "amvdec_mmjpeg"
#define DEBUG_AMVDEC_MJPEG_V4L "amvdec_mmjpeg_v4l"

#define DUMP_DECODER_STATE "dump_decoder_state"

int report_module_init(void);
void report_module_exit(void);

typedef ssize_t (*dump_v4ldec_state_func)(void *dev, char *);
void register_dump_v4ldec_state_func(void *dev, dump_v4ldec_state_func func);

typedef void (*set_debug_flag_func)(const char *module, int debug_mode);

int register_set_debug_flag_func(const char *module, set_debug_flag_func func);

typedef ssize_t (*dump_amstream_bufs_func)(char *);
void register_dump_amstream_bufs_func(dump_amstream_bufs_func func);

#endif

