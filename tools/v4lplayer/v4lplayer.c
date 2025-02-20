
/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

/**************************************************
* example based on amcodec
**************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <semaphore.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>

#include "vcodec_utils.h"
#include "v4l2_dec.h"

//#define DEBUG_FRAME

#define BUFFER_SIZE (1024*1024*4)

#define AML_VP9_HEADER_SIZE 16
#define VP9_IVF_HEAD_SIZE 32
#define VP9_IVF_FRAME_HEAD_SIZE 12

#define is_ivf_type(type)	\
		(type == VFORMAT_AV1)

static int video_type;
FILE* fp = NULL;
static char *filename;
static char *frame_size_file;
int g_dw_mode = 16;
int g_dump_dec_info_num = 5;
static sem_t wait_for_end;
int g_log_level = 0;
int g_output_flag = 0;

static int write_es_data(const uint8_t *data, int size)
{
	v4l2_dec_write_es(data, size);
	v4l2_dec_frame_done();
	return size;
}

int send_buffer_to_device(char *buffer, int Readlen)
{
	int isize = 0;
	int ret;

	do {
		ret = write_es_data((const uint8_t *)(buffer + isize), (Readlen - isize));
		if (ret < 0) {
			if (errno != EAGAIN) {
				debug_print(DEBUG_ERROR, "write data failed, errno %d\n", errno);
				return -1;
			} else {
				continue;
			}
		} else {
			isize += ret;
			debug_print(DEBUG_FRAME, "write %d, cur isize %d\n", ret, isize);
		}
		debug_print(DEBUG_FRAME, "ret %d, isize %d\n", ret, isize);
	} while (isize < Readlen);

	return 0;
}

/***************** ivf parser *******************/
#define MAX_SIZE 0x200000

/*!\brief OBU types. */
typedef enum ATTRIBUTE_PACKED {
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME_HEADER = 3,
	OBU_TILE_GROUP = 4,
	OBU_METADATA = 5,
	OBU_FRAME = 6,
	OBU_REDUNDANT_FRAME_HEADER = 7,
	OBU_TILE_LIST = 8,
	OBU_PADDING = 15,
} OBU_TYPE;

/*!\brief OBU metadata types. */
typedef enum {
	OBU_METADATA_TYPE_RESERVED_0 = 0,
	OBU_METADATA_TYPE_HDR_CLL = 1,
	OBU_METADATA_TYPE_HDR_MDCV = 2,
	OBU_METADATA_TYPE_SCALABILITY = 3,
	OBU_METADATA_TYPE_ITUT_T35 = 4,
	OBU_METADATA_TYPE_TIMECODE = 5,
} OBU_METADATA_TYPE;

typedef struct {
	size_t size;  // Size (1 or 2 bytes) of the OBU header (including the
	              // optional OBU extension header) in the bitstream.
	OBU_TYPE type;
	int has_size_field;
	int has_extension;
	// The following fields come from the OBU extension header and therefore are
	// only used if has_extension is true.
	int temporal_layer_id;
	int spatial_layer_id;
} ObuHeader;

static const size_t kMaximumLeb128Size = 8;
static const uint8_t kLeb128ByteMask = 0x7f;  // Binary: 01111111

// Disallow values larger than 32-bits to ensure consistent behavior on 32 and
// 64 bit targets: value is typically used to determine buffer allocation size
// when decoded.
static const uint64_t kMaximumLeb128Value = UINT32_MAX;

size_t uleb_size_in_bytes(uint64_t value) {
	size_t size = 0;
	do {
		++size;
	} while ((value >>= 7) != 0);
	return size;
}

int uleb_decode(const uint8_t *buffer, size_t available, uint64_t *value, size_t *length)
{
	uint32_t i = 0;
	if (buffer && value) {
		*value = 0;
		for (i = 0; i < kMaximumLeb128Size && i < available; ++i) {
			const uint8_t decoded_byte = *(buffer + i) & kLeb128ByteMask;
			*value |= ((uint64_t)decoded_byte) << (i * 7);
			if ((*(buffer + i) >> 7) == 0) {
				if (length) {
					*length = i + 1;
				}

				// Fail on values larger than 32-bits to ensure consistent behavior on
				// 32 and 64 bit targets: value is typically used to determine buffer
				// allocation size.
				if (*value > UINT32_MAX) return -1;

				return 0;
			}
		}
	}

	// If we get here, either the buffer/value pointers were invalid,
	// or we ran over the available space
	return -1;
}

int uleb_encode(uint64_t value, size_t available, uint8_t *coded_value, size_t *coded_size)
{
	uint32_t i = 0;
	const size_t leb_size = uleb_size_in_bytes(value);
	if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
		leb_size > available || !coded_value || !coded_size) {
		return -1;
	}

	for (i = 0; i < leb_size; ++i) {
		uint8_t byte = value & 0x7f;
		value >>= 7;

		if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

		*(coded_value + i) = byte;
	}

	*coded_size = leb_size;
	return 0;
}

int uleb_encode_fixed_size(uint64_t value, size_t available,
	size_t pad_to_size, uint8_t *coded_value, size_t *coded_size)
{
	uint32_t i;
	if (value > kMaximumLeb128Value || !coded_value || !coded_size ||
		available < pad_to_size || pad_to_size > kMaximumLeb128Size) {
		return -1;
	}
	const uint64_t limit = 1ULL << (7 * pad_to_size);
	if (value >= limit) {
		// Can't encode 'value' within 'pad_to_size' bytes
		return -1;
	}

	for (i = 0; i < pad_to_size; ++i) {
		uint8_t byte = value & 0x7f;
		value >>= 7;

		if (i < pad_to_size - 1) byte |= 0x80;  // Signal that more bytes follow.

		*(coded_value + i) = byte;
	}

	*coded_size = pad_to_size;
	return 0;
}

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type)
{
	int valid_type = 0;
	switch (obu_type) {
		case OBU_SEQUENCE_HEADER:
		case OBU_TEMPORAL_DELIMITER:
		case OBU_FRAME_HEADER:
		case OBU_TILE_GROUP:
		case OBU_METADATA:
		case OBU_FRAME:
		case OBU_REDUNDANT_FRAME_HEADER:
		case OBU_TILE_LIST:
		case OBU_PADDING: valid_type = 1; break;
		default: break;
	}
	return valid_type;
}

char obu_type_name[16][32] = {
	"UNKNOWN",
	"OBU_SEQUENCE_HEADER",
	"OBU_TEMPORAL_DELIMITER",
	"OBU_FRAME_HEADER",
	"OBU_TILE_GROUP",
	"OBU_METADATA",
	"OBU_FRAME",
	"OBU_REDUNDANT_FRAME_HEADER",
	"OBU_TILE_LIST",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"OBU_PADDING"
};

char meta_type_name[6][32] = {
	"OBU_METADATA_TYPE_RESERVED_0",
	"OBU_METADATA_TYPE_HDR_CLL",
	"OBU_METADATA_TYPE_HDR_MDCV",
	"OBU_METADATA_TYPE_SCALABILITY",
	"OBU_METADATA_TYPE_ITUT_T35",
	"OBU_METADATA_TYPE_TIMECODE"
};

struct read_bit_buffer {
	const uint8_t *bit_buffer;
	const uint8_t *bit_buffer_end;
	uint32_t bit_offset;
};

typedef struct DataBuffer {
	const uint8_t *data;
	size_t size;
} DataBuffer;

static int rb_read_bit(struct read_bit_buffer *rb)
{
	const uint32_t off = rb->bit_offset;
	const uint32_t p = off >> 3;
	const int q = 7 - (int)(off & 0x7);
	if (rb->bit_buffer + p < rb->bit_buffer_end) {
		const int bit = (rb->bit_buffer[p] >> q) & 1;
		rb->bit_offset = off + 1;
		return bit;
	}
	else {
		return 0;
	}
}

static int rb_read_literal(struct read_bit_buffer *rb, int bits)
{
	int value = 0, bit;
	for (bit = bits - 1; bit >= 0; bit--) value |= rb_read_bit(rb) << bit;
	return value;
}

static int read_obu_size(const uint8_t *data,
	size_t bytes_available,
	size_t *const obu_size,
	size_t *const length_field_size)
{
	uint64_t u_obu_size = 0;
	if (uleb_decode(data, bytes_available, &u_obu_size, length_field_size) != 0) {
		return -1;
	}

	if (u_obu_size > UINT32_MAX) return -1;
	*obu_size = (size_t)u_obu_size;
	return 0;
}

// Parses OBU header and stores values in 'header'.
static int read_obu_header(struct read_bit_buffer *rb, int is_annexb, ObuHeader *header)
{
	if (!rb || !header) return -1;

	//const ptrdiff_t bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
	const long int bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
	if (bit_buffer_byte_length < 1) return -1;

	header->size = 1;

	if (rb_read_bit(rb) != 0) {
		// Forbidden bit. Must not be set.
		return -1;
	}

	header->type = (OBU_TYPE)rb_read_literal(rb, 4);

	if (!valid_obu_type(header->type))
		return -1;

	header->has_extension = rb_read_bit(rb);
	header->has_size_field = rb_read_bit(rb);

	if (!header->has_size_field && !is_annexb) {
		// section 5 obu streams must have obu_size field set.
		return -1;
	}

	if (rb_read_bit(rb) != 0) {
		// obu_reserved_1bit must be set to 0.
		return -1;
	}

	if (header->has_extension) {
		if (bit_buffer_byte_length == 1) return -1;

		header->size += 1;
		header->temporal_layer_id = rb_read_literal(rb, 3);
		header->spatial_layer_id = rb_read_literal(rb, 2);
		if (rb_read_literal(rb, 3) != 0) {
			// extension_header_reserved_3bits must be set to 0.
			return -1;
		}
	}

	return 0;
}

int read_obu_header_and_size(const uint8_t *data,
	size_t bytes_available, int is_annexb, ObuHeader *obu_header,
	size_t *const payload_size, size_t *const bytes_read)
{
	size_t length_field_size_obu = 0;
	size_t length_field_size_payload = 0;
	size_t obu_size = 0;
	int status = 0;

	if (is_annexb) {
		// Size field comes before the OBU header, and includes the OBU header
		status =
		read_obu_size(data, bytes_available, &obu_size, &length_field_size_obu);

		if (status != 0) return status;
	}

	struct read_bit_buffer rb = { data + length_field_size_obu,
		data + bytes_available, 0};

	status = read_obu_header(&rb, is_annexb, obu_header);
	if (status != 0) return status;

	if (!obu_header->has_size_field) {
		// Derive the payload size from the data we've already read
		if (obu_size < obu_header->size) return -1;

		*payload_size = obu_size - obu_header->size;
	} else {
		// Size field comes after the OBU header, and is just the payload size
		status = read_obu_size(
		data + length_field_size_obu + obu_header->size,
		bytes_available - length_field_size_obu - obu_header->size,
		payload_size, &length_field_size_payload);
		if (status != 0) return status;
	}

	*bytes_read =
	length_field_size_obu + obu_header->size + length_field_size_payload;
	return 0;
}

int parser_frame(int is_annexb, uint8_t *data, const uint8_t *data_end,
	uint8_t *dst_data, uint32_t *frame_len, uint8_t *meta_buf, uint32_t *meta_len)
{
	int frame_decoding_finished = 0;
	uint32_t obu_size = 0;
	ObuHeader obu_header;
	memset(&obu_header, 0, sizeof(obu_header));
	int seen_frame_header = 0;
	//int next_start_tile = 0;
	DataBuffer obu_size_hdr;
	uint8_t header[20] = {
		0x00, 0x00, 0x01, 0x54,
		0xFF, 0xFF, 0xFE, 0xAB,
		0x00, 0x00, 0x00, 0x01,
		0x41, 0x4D, 0x4C, 0x56,
		0xD0, 0x82, 0x80, 0x00
	};
	uint8_t *p = NULL;
	uint32_t rpu_size = 0;

	// decode frame as a series of OBUs
	while (!frame_decoding_finished) {
		//      struct read_bit_buffer rb;
		size_t payload_size = 0;
		size_t header_size = 0;
		size_t bytes_read = 0;
		size_t bytes_written = 0;
		const size_t bytes_available = data_end - data;
		unsigned int i;
		OBU_METADATA_TYPE meta_type;
		uint64_t type;

		if (bytes_available == 0 && !seen_frame_header) {
			break;
		}

		int status = read_obu_header_and_size(data, bytes_available, is_annexb,
				&obu_header, &payload_size, &bytes_read);

		if (status != 0) {
			return -1;
		}

		// Record obu size header information.
		obu_size_hdr.data = data + obu_header.size;
		obu_size_hdr.size = bytes_read - obu_header.size;

		// Note: read_obu_header_and_size() takes care of checking that this
		// doesn't cause 'data' to advance past 'data_end'.

		if ((size_t)(data_end - data - bytes_read) < payload_size) {
			return -1;
		}
#ifdef DEBUG_FRAME
		debug_print(DEBUG_FRAME, "\tobu %s len %zu+%zu\n", obu_type_name[obu_header.type], bytes_read, payload_size);
#endif
		obu_size = bytes_read + payload_size + 4;

		if (!is_annexb) {
			obu_size = bytes_read + payload_size + 4;
			header_size = 20;
			uleb_encode_fixed_size(obu_size, 4, 4, header + 16, &bytes_written);
		}
		else {
			obu_size = bytes_read + payload_size;
			header_size = 16;
		}
		header[0] = ((obu_size + 4) >> 24) & 0xff;
		header[1] = ((obu_size + 4) >> 16) & 0xff;
		header[2] = ((obu_size + 4) >> 8) & 0xff;
		header[3] = ((obu_size + 4) >> 0) & 0xff;
		header[4] = header[0] ^ 0xff;
		header[5] = header[1] ^ 0xff;
		header[6] = header[2] ^ 0xff;
		header[7] = header[3] ^ 0xff;
		memcpy(dst_data, header, header_size);
		dst_data += header_size;
		memcpy(dst_data, data, bytes_read + payload_size);
		dst_data += bytes_read + payload_size;

		data += bytes_read;
		*frame_len += 20 + bytes_read + payload_size;

		switch (obu_header.type) {
			case OBU_TEMPORAL_DELIMITER:
				seen_frame_header = 0;
				//next_start_tile = 0;
				break;
			case OBU_SEQUENCE_HEADER:
				// The sequence header should not change in the middle of a frame.
				if (seen_frame_header) {
					return -1;
				}
				break;
			case OBU_FRAME_HEADER:
				if (data_end == data + payload_size) {
					frame_decoding_finished = 1;
				}
				else {
					seen_frame_header = 1;
				}
				break;
			case OBU_REDUNDANT_FRAME_HEADER:
			case OBU_FRAME:
				if (obu_header.type == OBU_REDUNDANT_FRAME_HEADER) {
					if (!seen_frame_header) {
						return -1;
					}
				} else {
					// OBU_FRAME_HEADER or OBU_FRAME.
					if (seen_frame_header) {
						return -1;
					}
				}
				if (obu_header.type == OBU_FRAME) {
					if (data_end == data + payload_size) {
						frame_decoding_finished = 1;
						seen_frame_header = 0;
					}
				}
				break;
			case OBU_TILE_GROUP:
				if (!seen_frame_header) {
					return -1;
				}
				if (data + payload_size == data_end)
					frame_decoding_finished = 1;
				if (frame_decoding_finished)
					seen_frame_header = 0;
				break;
			case OBU_METADATA:
				uleb_decode(data, 8, &type, &bytes_read);
				if (type < 6)
					meta_type = type;
				else
					meta_type = 0;
				p = data + bytes_read;
				debug_print(DEBUG_FRAME, "\t meta type %s %zu+%zu\n", meta_type_name[type], bytes_read, payload_size - bytes_read);

				if (meta_type == OBU_METADATA_TYPE_ITUT_T35) {
#if 0 /* for dumping original obu payload */
					for (i = 0; i < payload_size - bytes_read; i++) {
					debug_print(DEBUG_FRAME, "%02x ", p[i]);
					if (i % 16 == 15) debug_print(DEBUG_FRAME, "\n");
					}
					if (i % 16 != 0) debug_print(DEBUG_FRAME, "\n");
#endif
					if ((p[0] == 0xb5) /* country code */
						&& ((p[1] == 0x00) && (p[2] == 0x3b)) /* terminal_provider_code */
						&& ((p[3] == 0x00) && (p[4] == 0x00) && (p[5] == 0x08) && (p[6] == 0x00))) { /* terminal_provider_oriented_code */
						meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
						meta_buf[3] = 0x01;    meta_buf[4] = 0x19;

						if (p[11] & 0x10) {
							rpu_size = 0x100;
							rpu_size |= (p[11] & 0x0f) << 4;
							rpu_size |= (p[12] >> 4) & 0x0f;
							if (p[12] & 0x08) {
								debug_print(DEBUG_FRAME, "\t meta rpu in obu exceed 512 bytes\n");
								break;
							}
							for (i = 0; i < rpu_size; i++) {
								meta_buf[5 + i] = (p[12 + i] & 0x07) << 5;
								meta_buf[5 + i] |= (p[13 + i] >> 3) & 0x1f;
							}
							rpu_size += 5;
						} else {
							rpu_size = (p[10] & 0x1f) << 3;
							rpu_size |= (p[11] >> 5) & 0x07;
							for (i = 0; i < rpu_size; i++) {
								meta_buf[5 + i] = (p[11 + i] & 0x0f) << 4;
								meta_buf[5 + i] |= (p[12 + i] >> 4) & 0x0f;
							}
							rpu_size += 5;
						}
						*meta_len = rpu_size;
					}
				} else if (meta_type == OBU_METADATA_TYPE_HDR_CLL) {
					debug_print(DEBUG_FRAME, "\t\t hdr10 cll:\n");
					debug_print(DEBUG_FRAME, "\t\t max_cll = %x\n", (p[0] << 8) | p[1]);
					debug_print(DEBUG_FRAME, "\t\t max_fall = %x\n", (p[2] << 8) | p[3]);
				} else if (meta_type == OBU_METADATA_TYPE_HDR_MDCV) {
					debug_print(DEBUG_FRAME, "\t\t hdr10 primaries[r,g,b] = \n");
					for (i = 0; i < 3; i++) {
						printf("\t\t %x, %x\n", (p[i * 4] << 8) | p[i * 4 + 1],
							(p[i * 4 + 2] << 8) | p[i * 4 + 3]);
					}
					debug_print(DEBUG_FRAME, "\t\t white point = %x, %x\n", (p[12] << 8) | p[13], (p[14] << 8) | p[15]);
					debug_print(DEBUG_FRAME, "\t\t maxl = %x\n", (p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19]);
					debug_print(DEBUG_FRAME, "\t\t minl = %x\n", (p[20] << 24) | (p[21] << 16) | (p[22] << 8) | p[23]);
				}
				break;
			case OBU_TILE_LIST:
			break;
			case OBU_PADDING:
			break;
			default:
			// Skip unrecognized OBUs
			break;
		}

		data += payload_size;
	}

	return 0;
}

bool is_video_file_type_ivf(FILE *fp, char *buffer)
{
	if (fp && is_ivf_type(video_type)) {
		fread(buffer, 1, 4, fp);
		fseek(fp, 0, SEEK_SET);
		if ((buffer[0] == 0x44) &&
			(buffer[1] == 0x4B) &&
			(buffer[2] == 0x49) &&
			(buffer[3] == 0x46))
			return true;
	} else if (is_ivf_type(video_type)) {
		if ((buffer[0] == 0x44) &&
			(buffer[1] == 0x4B) &&
			(buffer[2] == 0x49) &&
			(buffer[3] == 0x46))
			return true;
	}
	return false;
}

int av1_ivf_write_dat(FILE *src_fp, uint8_t *src_buffer)
{
	int frame_count = 0;
	unsigned int src_frame_size = 0;
	unsigned int dst_frame_size = 0;
	unsigned int meta_size = 0;
	uint8_t *dst_buffer = NULL;
	uint8_t *meta_buffer = NULL;
	int process_count = -1;
	unsigned int *p_size;

	meta_buffer = calloc(1, 1024);
	if (!meta_buffer) {
		debug_print(DEBUG_ERROR, "fail to alloc meta buf\n");
		return -1;
	}
	if (fread(src_buffer, 1, 32, src_fp) != 32) {
		debug_print(DEBUG_ERROR, "read input file error!\n");
		free(meta_buffer);
		return -1;
	}
	p_size = (unsigned int *)(src_buffer + 24);
	process_count = *p_size;
	debug_print(DEBUG_DEF, "av1 ivf frame number = %d\n", process_count);
	/*
	* no frame count limit
	*/
	/* coverity[tainted_data:SUPPRESS] */
	while (frame_count < process_count) {
		if (fread(src_buffer, 1, 12, src_fp) != 12) {
			printf("end of file!\n");
			break;
		}
		p_size = (unsigned int *)src_buffer;
		src_frame_size = *p_size;
		if (src_frame_size > BUFFER_SIZE)
			src_frame_size = BUFFER_SIZE;

#ifdef DEBUG_FRAME
		debug_print(DEBUG_FRAME, "frame %d, size %d\n", frame_count, src_frame_size);
#endif
		if (fread(src_buffer, 1, src_frame_size, src_fp) != src_frame_size) {
			debug_print(DEBUG_ERROR, "read input file error %d!\n", src_frame_size);
			break;
		}

		dst_buffer = calloc(1, src_frame_size + 4096);
		if (!dst_buffer) {
			debug_print(DEBUG_ERROR, "failed to alloc frame buf\n");
			break;
		}
		dst_frame_size = 0;
		meta_size = 0;

		parser_frame(0, src_buffer, src_buffer + src_frame_size, dst_buffer, &dst_frame_size, meta_buffer, &meta_size);
		if (dst_frame_size) {
#ifdef DEBUG_FRAME
			debug_print(DEBUG_FRAME, "\toutput len=%d\n", dst_frame_size);
#endif
			if (send_buffer_to_device((char *)dst_buffer, dst_frame_size) < 0) {
				free(dst_buffer);
				break;
			}
		}
		if (meta_size) {
			debug_print(DEBUG_FRAME, "\t meta len=%d\n", meta_size);
			/* dump meta here */
		}
		free(dst_buffer);
		frame_count++;
	}
	debug_print(DEBUG_STATE, "Process %d frame\n", frame_count);
	free(meta_buffer);
	return 0;
}

/****************** end of ivf parer *************/

#if 1
//AV1
static int obu_frame_frame_head_come_after_tile = 0;
//static int frame_decoded = 0;
static int decoding_data_flag = 0;
#endif

unsigned char is_picture_start(int nal_unit_type)
{
	unsigned char ret = 0;
	if (video_type == VFORMAT_AVS) {
		if (nal_unit_type == 0xB3 || //I_PICTURE_START_CODE
			nal_unit_type == 0xB6  //PB_PICTURE_START_CODE
			)
			ret = 1;
	} else if (video_type == VFORMAT_VP9) {
		/*to do*/
		goto check_av1;
	} else if (video_type == VFORMAT_AV1) {
check_av1:
		if (nal_unit_type == OBU_FRAME_HEADER ||
		nal_unit_type == OBU_FRAME) {
			ret = 1;
			obu_frame_frame_head_come_after_tile = 1;
			if (nal_unit_type == OBU_FRAME) { /*have tile group in this OBU*/
				obu_frame_frame_head_come_after_tile = 0;
				decoding_data_flag = 1;
			}
		}
	}
	else
	ret = 1;
	return ret;
}

unsigned char is_picture_end(int nal_unit_type)
{
	unsigned char ret = 0;
	if (video_type == VFORMAT_AVS) {
		if (nal_unit_type == 0xB3 || //I_PICTURE_START_CODE
		nal_unit_type == 0xB6 || //PB_PICTURE_START_CODE
		nal_unit_type == 0xB0 || //SEQUENCE_HEADER_CODE
		nal_unit_type == 0xB1  //SEQUENCE_END_CODE
		)
		ret = 1;
	} else if (video_type == VFORMAT_VP9) {
		/*to do*/
		goto check_av1;
	} else if (video_type == VFORMAT_AV1) {
check_av1:
		if (nal_unit_type == OBU_TILE_GROUP) {
			obu_frame_frame_head_come_after_tile = 0;
			decoding_data_flag = 1;
		} else if (nal_unit_type == OBU_FRAME_HEADER ||
			nal_unit_type == OBU_FRAME) {
			if (decoding_data_flag)
				ret = 1;
			decoding_data_flag = 0;
			obu_frame_frame_head_come_after_tile = 1;
			if (nal_unit_type == OBU_FRAME) { /*have tile group in this OBU*/
				obu_frame_frame_head_come_after_tile = 0;
				decoding_data_flag = 1;
			}
		} else if (nal_unit_type == OBU_REDUNDANT_FRAME_HEADER &&
			obu_frame_frame_head_come_after_tile == 0) {
			decoding_data_flag = 0;
			debug_print(DEBUG_ERROR, "Warning, OBU_REDUNDANT_FRAME_HEADER come without OBU_FRAME or OBU_FRAME_HEAD\n");
		}
	} else
		ret = 1;
	return ret;
}

typedef struct
{
	int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned max_size;            //! Nal Unit Buffer size
	int forbidden_bit;            //! should be always FALSE
	int nal_reference_idc;        //! NALU_PRIORITY_xxxx
	int nal_unit_type;            //! NALU_TYPE_xxxx
	unsigned char *buf;                    //! contains the first byte followed by the EBSP
	unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

NALU_t *AllocNALU(int buffersize)
{
	NALU_t *n;

	if ((n = (NALU_t*)calloc (1, sizeof (NALU_t))) == NULL) {
		debug_print(DEBUG_ERROR, "AllocNALU: n");
		exit(0);
	}

	n->max_size=buffersize;

	if ((n->buf = (unsigned char*)calloc (buffersize, sizeof (char))) == NULL) {
		free (n);
		debug_print(DEBUG_ERROR, "AllocNALU: n->buf");
		exit(0);
	}

	return n;
}

void FreeNALU(NALU_t *n)
{
	if (n) {
		if (n->buf) {
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
}

static int FindStartCode2 (unsigned char *Buf)
{
	if ((Buf[0] != 0) || (Buf[1] != 0) || (Buf[2] != 1)) return 0; //0x000001
	else return 1;
}

static int FindStartCode3 (unsigned char *Buf)
{
	if ((Buf[0] != 0) || (Buf[1] != 0) || (Buf[2] != 0) || (Buf[3] != 1)) return 0;//0x00000001
	else return 1;
}

static int FindAmlStartCode(unsigned char *Buf)
{
	unsigned int len;
	unsigned int len2;
	len = (Buf[0] << 24) | (Buf[1] << 16) | (Buf[2] << 8) | (Buf[3] << 0);
	len2 = (Buf[4] << 24) | (Buf[5] << 16) | (Buf[6] << 8) | (Buf[7] << 0);
	if (((len + len2) == 0xffffffff) &&
		(Buf[8] == 0 && Buf[9] == 0 && Buf[10] == 0 && Buf[11] == 1) &&
		(Buf[12] == 0x41 && Buf[13] == 0x4d && Buf[14] == 0x4c && Buf[15] == 0x56)) {
		return 1;
	}
	return 0;
}

int fgetc_direct(FILE *fe)
{
	int ret;

	ret = fgetc(fe);
	return (ret != EOF) ? ret : '0';
}

int GetAnnexbNALU (FILE* fe, NALU_t *nalu)
{
	int pos = 0;
	int StartCodeFound, rewind;
	unsigned char *Buf;
	int info2 = 0, info3 = 0;
	int i;
	int prefix_len = 4;
	if (video_type == VFORMAT_VP9 || video_type == VFORMAT_AV1)
		prefix_len = 16;
	if ((Buf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL) {
		debug_print(DEBUG_ERROR, "GetAnnexbNALU: Could not allocate Buf memory\n");
		return -1;
	}

	nalu->startcodeprefix_len=0;
	while (!feof(fe)) {
		if (nalu->startcodeprefix_len<prefix_len) {
			Buf[nalu->startcodeprefix_len++] = fgetc_direct(fe);
		} else{
			if (prefix_len == 4) {
				for (i = 0; i < 3; i++)
				Buf[i] = Buf[i+1];
			} else {
				for (i = 0; i < prefix_len; i++)
					Buf[i] = Buf[i+1];
			}
			Buf[nalu->startcodeprefix_len - 1] = fgetc_direct(fe);
		}
		if (prefix_len == 4) {
			if (nalu->startcodeprefix_len >= 3) {
				if (FindStartCode2(Buf)) {
					pos = nalu->startcodeprefix_len;
					nalu->startcodeprefix_len = 3;
					break;
				}
			}
			if (nalu->startcodeprefix_len == 4) {
				if (FindStartCode3(Buf)) {
					pos = nalu->startcodeprefix_len;
					break;
				}
			}
		} else {
			if (nalu->startcodeprefix_len == prefix_len) {
				if (FindAmlStartCode(Buf)) {
					pos = nalu->startcodeprefix_len;
					break;
				}
			}
		}
	}
	StartCodeFound = 0;
	info2 = 0;
	info3 = 0;

	while (!StartCodeFound) {
		if (feof (fe)) {
			rewind = -1;
			goto fill_data;
		}
		Buf[pos++] = fgetc_direct (fe);
		if (prefix_len == 4) {
			info3 = FindStartCode3(&Buf[pos - 4]);
			if (info3 != 1)
				info2 = FindStartCode2(&Buf[pos - 3]);
			StartCodeFound = (info2 == 1 || info3 == 1);
		} else {
			StartCodeFound = FindAmlStartCode(&Buf[pos-prefix_len]);
		}
	}

	// Here, we have found another start code (and read length of startcode bytes more than we should
	// have.  Hence, go back in the file
	if (prefix_len == 4)
		rewind = (info3 == 1) ? -4 : -3;
	else
		rewind = -prefix_len;

	if (0 != fseek (fe, rewind, SEEK_CUR))
		debug_print(DEBUG_ERROR, "GetAnnexbNALU: Cannot fseek in the bit stream file");

	// Here the Start code, the complete NALU, and the next start code is in the Buf.
	// The size of Buf is pos, pos+rewind are the number of bytes excluding the next
	// start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code
fill_data:
	nalu->len = (pos + rewind);
	if (nalu->len > MAX_SIZE) {
		debug_print(DEBUG_ERROR, "%d: Error: to many data to copy %d\n", __LINE__, nalu->len);
		exit(0);
	}
	memcpy (nalu->buf, &Buf[0], nalu->len);//copy nalu, include 0x000001 or 0x00000001
	if (video_type == VFORMAT_VP9 || video_type == VFORMAT_AV1) {
		unsigned char* p = &nalu->buf[nalu->startcodeprefix_len];
		while ((*p++)&0x80) {

		}
		nalu->nal_reference_idc = 0;
		nalu->forbidden_bit = *p & 0x80;
		nalu->nal_unit_type = (*p>>3)&0xf;
	} else {
		nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80; //1 bit
		nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60; // 2 bit
		//nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
		nalu->nal_unit_type = (nalu->buf[nalu->startcodeprefix_len]);
	}
	free(Buf);

	return (pos + rewind);
}

static void dump(NALU_t *n)
{
	if (!n)return;
	/*
	printf("a new nal:");

	printf(" len: %d  ", n->len);
	printf("nal_unit_type: %x\n", n->nal_unit_type);
	*/
}

int av1_frame_mode_write_dat(FILE *fp, char *buffer)
{
	NALU_t *n = AllocNALU(MAX_SIZE);
	int buf_pos = 0;


	//unsigned char pic_head_found = 0;
	while (!feof(fp)) {
		GetAnnexbNALU(fp, n);
		dump(n);
		if ((buf_pos + n->len) > BUFFER_SIZE) {
			debug_print(DEBUG_ERROR, "%d: Error: to many data to copy %d\n", __LINE__, buf_pos + n->len);
			exit(0);
		}
		memcpy(&buffer[buf_pos], n->buf, n->len);
		buf_pos += n->len;
		if (is_picture_start(n->nal_unit_type)) {
			break;
		}
	}

	while (!feof(fp)) {
		GetAnnexbNALU(fp, n);
		if (is_picture_end(n->nal_unit_type)) {
			//printf("Send Data Size =%d\n", buf_pos);
#ifndef TEST_ON_PC
			/*
			int delay_time = 0;
			if (delay_time > 0)
			delay(delay_time);
			*/
			if (send_buffer_to_device(buffer, buf_pos) < 0)
				goto error_ret;
#endif
			buf_pos = 0;
		}

		if ((buf_pos + n->len) > BUFFER_SIZE) {
			debug_print(DEBUG_ERROR, "%d: Error: to many data to copy %d\n", __LINE__, buf_pos + n->len);
			goto error_ret;
		}

		memcpy(&buffer[buf_pos], n->buf, n->len);
		buf_pos += n->len;
		dump(n);
	}
error_ret:
	free(n);
	return 0;
}

int frame_mode_write_dat(FILE *fp, FILE *fszp, char *buffer)
{
	char frame_size_str[32];
	char *s_rt;
	int frame_size;
	int ret;

	while (!feof(fp)) {
		memset(frame_size_str, 0, sizeof(frame_size_str));
		s_rt = fgets(frame_size_str, 32, fszp);
		if (s_rt == NULL)
		break;
		frame_size = atoi(frame_size_str);
		if (frame_size) {
			memset(buffer, 0, BUFFER_SIZE);
			ret = fread(buffer, 1, frame_size, fp);
#ifdef DEBUG_FRAME
			debug_print(DEBUG_FRAME, "read size %d/%d, %x %x %x %x %x %x %x %x ...\n", ret, frame_size,
				buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
#endif
			if (ret < frame_size)
				debug_print(DEBUG_ERROR, "read back size %d, less than frame size %d\n", ret, frame_size);
			else {
				if (send_buffer_to_device(buffer, ret) < 0) {
					debug_print(DEBUG_ERROR, "send data failed\n");
					break;
				}
			}
		} else
			debug_print(DEBUG_ERROR, "error: read frame size 0\n");
	}

	return 0;
}

static void usage()
{
	printf("Command help:\n");
	printf("1. To play a video: v4lplayer -i <file> -f <format> -d <dw_mode> -s <FSZ> -l <log_level> -o <dump> -n <num>\n");
	printf(" -i, --ifile,  input es file\n");
	printf(" -f, --format, video format\n");
	printf("\t0:mpeg12\t1:mpeg4\t\t2:h264\t\t3:mjpeg\n");
	printf("\t5:jpeg\t\t6:vcl\t\t7:avs\t\t11:hevc\n");
	printf("\t14:vp9\t\t15:avs2\t\t16:av1\t\t17:avs3\n");
	printf("\t18:h266\n");
	printf(" -s, --size,   frame size file\n");
	printf(" -d, --dw_mode: (No dw mode will be forced to dw 16)\n");
	printf("\t0: no DW compressed only\n");
	printf("\t1: DW with 1:1 ratio\n");
	printf("\t2: DW with 1:4 down sample\n");
	printf("\t3: DW with 1:4 down sample\n");
	printf("\t4: DW with 1:2 down sample\n");
	printf("\t16: DW only\n");
	printf(" -l, --log_level, enable more log\n");
	printf("\tbit 0: decoder state\tbit 1: debug frame\n");
	printf(" -o, --output, output yuv and crc\n");
	printf("\tbit 0: print crc\n");
	printf("\tbit 1: dump yuv to /data/tmp\n");
	printf("\tbit 2: dump crc to /data/tmp\n");
	printf("\tbit 3: reserved\n");
	printf("\tbit 4: dump stream info to /data/tmp\n");
	printf("\tbit 5: dump statistic info to /data/tmp\n");
	printf("\tbit 6: dump afd info to /data/tmp\n");
	printf("\tbit 7: dump cc info to /data/tmp\n");
	printf("\tbit 8: dump hdr10 info to /data/tmp\n");
	printf("\tbit 9: dump hdr10p info to /data/tmp\n");
	printf("\tbit 10: dump cuva info to /data/tmp\n");
	printf("\tbit 11: dump amdv info to /data/tmp\n");
	printf("\tbit 12: dump frame info to /data/tmp\n");
	printf("\tBefore dumping, run 'mkdir -p /data/tmp -m 777;setenforce 0;rm /data/tmp/* -rf' command\n");
	printf(" -n, --number, dump decoder info num\n");
	printf(" -h, --help,   usage\n");
	printf("example : v4lplayer -f 2 -d 16 -i /data/h264.es -s /data/h264.fsz\n");
}

static const char short_options[] = "i:s:d:f:l:o:n:h";

static const struct option
long_options[] = {
		{ "sizefile",  required_argument, NULL, 's' },
		{ "ifile",	required_argument, NULL, 'i' },
        { "double_write", required_argument, NULL, 'd' },
        { "format", required_argument, NULL, 'f' },
        { "log",  required_argument, NULL, 'l' },
		{ "output",  required_argument, NULL, 'o' },
		{ "num",  required_argument, NULL, 'n' },
        { "help",   no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
};

static int parse_para(int argc, char *argv[])
{
	for (;;) {
		int idx;
		int c;
		unsigned long value = 0;
		char *endptr;

		c = getopt_long(argc, argv, short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
			case 0: /* getopt_long() flag */
				break;
			case 'l':
				g_log_level = atoi(optarg);
				break;
			case 'd':
				g_dw_mode = atoi(optarg);
				if (g_dw_mode != 0 &&
					g_dw_mode != 1 &&
					g_dw_mode != 2 &&
					g_dw_mode != 3 &&
					g_dw_mode != 4 &&
					g_dw_mode != 16) {
					debug_print(DEBUG_ERROR, "invalid dw_mode %d\n", g_dw_mode);
					exit(1);
				}
				break;
			case 'f':
				video_type = atoi(optarg);
				break;
			case 'i':
				filename = strdup(optarg);
				break;
			case 's':
				frame_size_file = strdup(optarg);
				break;
			case 'o':
				if (strncmp(optarg, "0x", 2) == 0 || strncmp(optarg, "0X", 2) == 0) {
					value = strtoul(optarg, &endptr, 16);
				} else {
					value = strtoul(optarg, &endptr, 10);
				}

				if (*endptr != '\0') {
					debug_print(DEBUG_ERROR, "Invalid value -o: %s\n", optarg);
					break;
				}

				g_output_flag = value;
				debug_print(DEBUG_DEF, "set output: %d\n", value);
				break;
			case 'n':
				g_dump_dec_info_num = atoi(optarg);
				break;
			case 'h':
				usage();
				return -1;
			default:
				usage();
			return -1;
		}
	}
	return 0;
}

static void decode_finish()
{
	sem_post(&wait_for_end);
}

static int start_decoder(enum vformat_e type)
{
	int ret;
	ret = v4l2_dec_init(type, decode_finish);
	if (ret) {
		debug_print(DEBUG_ERROR, "FATAL: start_decoder error:%d\n",ret);
		exit(1);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	FILE* frame_size_fp = NULL;
	char *buffer = NULL;

	if (argc < 6) {
		usage();
		return -1;
	}

	g_dw_mode = 16;
	if (parse_para(argc, argv))
		return -1;

	buffer = malloc(BUFFER_SIZE);
	if (NULL == buffer) {
		debug_print(DEBUG_ERROR, "malloc write buffer fail\n");
		return -1;
	}
	memset(buffer, 0, BUFFER_SIZE);

	debug_print(DEBUG_STATE, "set dw mode:%d\n", g_dw_mode);
	if (((frame_size_fp = fopen(frame_size_file, "rb")) == NULL) &&
		video_type != VFORMAT_AV1) {
		debug_print(DEBUG_ERROR, "open file %s error!, force stream mode\n", frame_size_file);
		return -1;
	}

	debug_print(DEBUG_DEF, "\n*********CODEC PLAYER DEMO************\n\n");
	debug_print(DEBUG_DEF, "file %s to be played\n", filename);

	if ((fp = fopen(filename, "rb")) == NULL) {
		debug_print(DEBUG_ERROR, "open file error!\n");
		goto free_buff;
	}

	start_decoder(video_type);

	if (is_video_file_type_ivf(fp, buffer)) {
		if (video_type == VFORMAT_AV1) {
			debug_print(DEBUG_DEF, "input video file is ivf with av1.\n");
			av1_ivf_write_dat(fp, (uint8_t *)buffer);
		}
	} else {
		if ((video_type == VFORMAT_AV1) && (frame_size_fp == NULL))
			av1_frame_mode_write_dat(fp, buffer);
		else {
			if (!frame_size_file) {
				debug_print(DEBUG_ERROR, "open file %s error!, force stream mode\n", frame_size_file);
				goto free_buff;
			}
			frame_mode_write_dat(fp, frame_size_fp, buffer);
		}
	}

	v4l2_dec_eos();
	sem_wait(&wait_for_end);

error:
	v4l2_dec_destroy();
	sem_destroy(&wait_for_end);
	fclose(fp);
free_buff:
	free(buffer);
	return 0;
}

