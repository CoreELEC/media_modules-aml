/*
 * Copyright (c) 2016 Amlogic, Inc. All rights reserved.
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 * Description:
 */

#ifndef __AML_AVBC_DEC_IF_H__
#define __AML_AVBC_DEC_IF_H__

/**
 * @brief Memory mapping function type definition.
 *
 * This function type defines the prototype for memory mapping functions that
 * can be used by the decoder.
 */
typedef void *(*mem_map_func)(void *priv, unsigned int pfn);

/**
 * @brief Decodes a YUV frame using the AVBC decoder.
 *
 * This function performs decoding of YUV frames from the provided header,
 * width, height, and other parameters. It stores the decoded frame in the
 * provided destination buffer.
 *
 * @param header Pointer to the AVBC frame header.
 * @param width Width of the frame in pixels.
 * @param height Height of the frame in pixels.
 * @param stride Stride of the frame (bytes per row).
 * @param bitdepth Bit depth of the YUV frame.
 * @param dst_yuv Pointer to the destination buffer where the decoded YUV
 * frame will be stored.
 * @param dst_size Size of the destination buffer in bytes.
 * @param mMap Memory mapping function for handling memory mapped I/O.
 * @param priv Private data for the memory mapping function.
 *
 * @return Returns 0 on success, or -1 on failure.
 */
int aml_avbc_decode(void *header,
                    unsigned int width,
                    unsigned int height,
                    unsigned int stride,
                    unsigned int bitdepth,
                    unsigned char *dst_yuv,
                    unsigned int dst_size,
                    mem_map_func mMap,
                    void *priv);

/**
 * @example test_avbc_decode.c
 * @brief Sample test code to use the aml_avbc_decode function.
 *
 * This file demonstrates how to use the aml_avbc_decode function in a test
 * scenario. It sets up the necessary input parameters, performs decoding,
 * and handles memory mapping and clean-up.
 *
 * @code
 * #include "aml_avbc_decoder.h"
 * #include <stdio.h>
 *
 * int main() {
 *     // Sample parameters
 *     unsigned int width = 1920;
 *     unsigned int height = 1080;
 *     unsigned int stride = width;  // assuming 8-bit YUV
 *     unsigned int bitdepth = 10;
 *     unsigned char *dst_yuv;
 *     unsigned int dst_size = stride * height * 3 / 2;
 *     void *header = get_avbc_header();  // hypothetical function to get the header
 *
 *     // Memory mapping function (example)
 *     mem_map_func my_mmap = my_memory_mapper;  // hypothetical memory mapper function
 *
 *     // Decode the frame
 *     int ret = aml_avbc_decode(header, width, height, stride, bitdepth, dst_yuv, dst_size, my_mmap, NULL);
 *     if (ret == 0) {
 *         printf("Decoding successful!\n");
 *     } else {
 *         printf("Decoding failed!\n");
 *     }
 *
 *     return 0;
 * }
 * @endcode
 */

#endif //__AML_AVBC_DEC_IF_H__

