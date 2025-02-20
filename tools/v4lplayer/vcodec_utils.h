/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VCODEC_UTILS_H_
#define VCODEC_UTILS_H_

#include <stdint.h>

#define DEBUG_DEF 0
#define DEBUG_ERROR 0
#define DEBUG_STATE 0x1
#define DEBUG_FRAME 0x2
#define DEBUG_TO_DO 0x4

#define FRAME_PRINT_CRC 			1 << 0
#define FRAME_DUMP_YUV 				1 << 1
#define FRAME_DUMP_CRC 				1 << 2

#define DECINFO_DUMP_STREAM 		1 << 4
#define DECINFO_DUMP_STATISTIC 		1 << 5
#define DECINFO_DUMP_AFD 			1 << 6
#define DECINFO_DUMP_CC 			1 << 7
#define DECINFO_DUMP_HDR10 			1 << 8
#define DECINFO_DUMP_HDR10P 		1 << 9
#define DECINFO_DUMP_CUVA 			1 << 10
#define DECINFO_DUMP_AMDV 			1 << 11
#define DECINFO_DUMP_FRAME 			1 << 12

#define VDEC_MODE_MMU_DW_MASK	(0x20)
#define VDEC_MODE_10BIT_MASK	(0x10000)
#define VDEC_MODE_DW_MASK	(0xffff)

// video fromat
enum vformat_e {
	VFORMAT_UNKNOWN = -1,
	VFORMAT_MPEG12 = 0,
	VFORMAT_MPEG4 = 1,
	VFORMAT_H264 = 2,
	VFORMAT_MJPEG = 3,
	VFORMAT_REAL = 4,
	VFORMAT_JPEG = 5,
	VFORMAT_VC1 = 6,
	VFORMAT_AVS = 7,
	VFORMAT_SW = 8,		/* Use SW decoder */
	VFORMAT_H264MVC = 9,
	VFORMAT_H264_4K2K = 10,
	VFORMAT_HEVC = 11,
	VFORMAT_H264_ENC = 12,
	VFORMAT_JPEG_ENC = 13,
	VFORMAT_VP9 = 14,
	VFORMAT_AVS2 = 15,
	VFORMAT_AV1 = 16,
	VFORMAT_AVS3 = 17,
	VFORMAT_H266 = 18,
	VFORMAT_MAX
};

enum vdec_dec_mode {
	DM_INVALID		= 0,
	DM_AVBC_ONLY		= 0,
	DM_YUV_1_1_AVBC		= 1,
	DM_YUV_1_4_AVBC_A	= 2,
	DM_YUV_1_4_AVBC_B	= 3,
	DM_YUV_1_2_AVBC		= 4,
	DM_YUV_1_8_AVBC		= 8,
	DM_YUV_ONLY		= 0x10,
	DM_AVBC_1_1		= 0x21,
	DM_AVBC_1_4		= 0x22,
	DM_AVBC_1_2		= 0x24,
	DM_YUV_AUTO_1_2_AVBC	= 0x100,
	DM_YUV_AUTO_1_4_AVBC	= 0x200,
	DM_YUV_AUTO_1_2_AVBC_B	= 0x300,
	/* (0~540] 1/1, (540~1080] 1/4, (1080~4K] 1/16 */
	DM_YUV_AUTO_14_12_AVBC	= 0x400,
	DM_YUV_1_1_10BIT_AVBC	= 0x10001,
	DM_YUV_1_4_10BIT_AVBC	= 0x10003,
	DM_YUV_1_2_10BIT_AVBC	= 0x10004,
	DM_YUV_1_8_10BIT_AVBC	= 0x10008,
	/* (0~1080] 1/1, (1080~4K] 1/16 */
	DM_YUV_14_11_10BIT_AVBC	= 0x10200,
};

int debug_print(int flag, const char *fmt, ...);
uint32_t crc32_le(uint32_t crc, unsigned char const *p, int len);

#endif //CODEC_H_
