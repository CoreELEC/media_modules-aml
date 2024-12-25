/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_IMAGE_INFO_H__
#define __AML_IMAGE_INFO_H__

#define MAKE_FOURCC(a, b, c, d)\
	((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

/* AVBC: Amlogic Video Buffer Compression Data. */
#define AML_PIX_FMT_AVBC	MAKE_FOURCC('A', 'V', 'B', 'C')

/* NV12: YUV 4:2:0 semi-planar format, Y plane followed by interleaved UV plane */
#define AML_PIX_FMT_NV12	MAKE_FOURCC('N', 'V', '1', '2')

/* NV21: YUV 4:2:0 semi-planar format, Y plane followed by interleaved VU plane */
#define AML_PIX_FMT_NV21	MAKE_FOURCC('N', 'V', '2', '1')

/* P010: 10-bit YUV 4:2:0 semi-planar format, Y plane followed by interleaved UV plane */
#define AML_PIX_FMT_P010	MAKE_FOURCC('P', '0', '1', '0')

/**
 * fourcc_to_string - Converts a FOURCC value to a string.
 *
 * @fourcc: The FOURCC value to convert.
 * @return : A pointer to a statically allocated string containing the result.
 */
static inline const char *fourcc_to_string(u32 fourcc)
{
	static char str[5];
	union {
		u32 value;
		char chars[4];
	} u = { .value = fourcc };

	str[0] = u.chars[0];
	str[1] = u.chars[1];
	str[2] = u.chars[2];
	str[3] = u.chars[3];
	str[4] = '\0';

	return str;
}

/*
 * struct image_rect_s - Defines a rectangular region within an image.
 *
 * @x	     : X-coordinate of the top-left corner.
 * @y	     : Y-coordinate of the top-left corner.
 * @width   : Width of the rectangle.
 * @height  : Height of the rectangle.
 */
typedef struct image_rect_s {
	u32	x;
	u32	y;
	u32	width;
	u32	height;
} ImageRect;

/*
 * struct image_crop_s - Frame cropping parameters.
 *
 * @top     : Pixels cropped from the top.
 * @left    : Pixels cropped from the left.
 * @bottom  : Pixels cropped from the bottom.
 * @right   : Pixels cropped from the right.
 */
typedef struct image_crop_s {
	u32	top;
	u32	left;
	u32	bottom;
	u32	right;
} ImageCrop;

/*
 * struct image_info_s - Image metadata for processing.
 *
 * @format   : Image format (e.g., RGB, YUV).
 * @bitdep   : Bit depth (e.g., 8-bit, 10-bit).
 * @mtype    : Memory type of the buffer.
 * @data     : Pointer to the starting address of the image buffer.
 * @plane    : Array of pointers to image planes (up to 3 planes for formats like YUV).
 * @size     : Total size of the image data buffer in bytes.
 * @plane_size: Array of sizes for each plane, applicable for planar formats.
 * @uncached : 1 if uncached, 0 if cached.
 * @rect     : Region of interest (ROI) in the image.
 * @crop     : Crop parameters for the image.
 */
typedef struct image_info_s {
	u32		format;
	u32		bitdep;
	u32		mtype;
	union {
		u64	plane[3];
		u64	data;
	};
	union {
		u32	plane_size[3];
		u32	size;
	};
	u32		uncached;
	ImageRect	rect;
	ImageCrop	crop;
} ImageInfo;

#endif //__AML_IMAGE_INFO_H__
