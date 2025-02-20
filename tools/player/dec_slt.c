/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**************************************************
* example based on multi instance frame check code
**************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>

#include "dec_slt_res.h"
#include "vcodec.h"

#define READ_SIZE       (64 * 1024)
#define EXTERNAL_PTS    (1)
#define SYNC_OUTSIDE    (2)
#define UNIT_FREQ       96000
#define PTS_FREQ        90000
#define AV_SYNC_THRESH  PTS_FREQ*30
#define BUFFER_SIZE (1024*1024*2)

#define TEST_CASE_HEVC 0
#define TEST_CASE_VDEC 1
#define TEST_CASE_HEVC_VP9 2
#define TEST_CASE_HEVC_AV1 3

#define RETRY_TIME     3


#define LPRINT0
#define LPRINT1(...)        printf(__VA_ARGS__)

#define ERRP(con, rt, p, ...) do {  \
    if (con) {                      \
        LPRINT##p(__VA_ARGS__); \
        rt;                         \
    }                               \
} while(0)

static vcodec_para_t v_codec_para;
static vcodec_para_t *pcodec, *vpcodec;
int id;
char *name;

int set_tsync_enable(int enable)
{
    int fd;
    char *path = "/sys/class/tsync/enable";
    char  bcmd[16];
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        sprintf(bcmd, "%d", enable);
        ssize_t bytes = write(fd, bcmd, strlen(bcmd));
        if (bytes == -1) {
            printf("write error, bytes: %d", bytes);
        }
        close(fd);
        return 0;
    }

    return -1;
}

int set_cmd(const char *str, const char *path)
{
    int fd;
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t bytes = write(fd, str, strlen(str));
        if (bytes == -1) {
            printf("write error, bytes: %d", bytes);
        }
        close(fd);
        printf("[success]: %s > %s\n", str, path);
        return 0;
    }
    printf("[failed]: %s > %s\n", str, path);
    return -1;
}

bool is_video_file_type_ivf(FILE *fp, int video_type, char *buffer)
{
    if (fp && video_type == VFORMAT_AV1) {
        ssize_t bytes = fread(buffer, 1, 4, fp);
        if (bytes == -1) {
            printf("write error, bytes: %d", bytes);
        }
        fseek(fp, 0, SEEK_SET);
        if ((buffer[0] == 0x44) &&
            (buffer[1] == 0x4B) &&
            (buffer[2] == 0x49) &&
            (buffer[3] == 0x46))
            return true;
    } else if (video_type == VFORMAT_AV1) {
        if ((buffer[0] == 0x44) &&
            (buffer[1] == 0x4B) &&
            (buffer[2] == 0x49) &&
            (buffer[3] == 0x46))
            return true;
    }
    return false;
}

int send_buffer_to_device(char *buffer, int Readlen)
{
    int isize = 0;
    int ret;

    do {
        ret = vcodec_write(pcodec, (buffer + isize), (Readlen - isize));
        if (ret < 0) {
            if (errno != EAGAIN) {
                printf("write data failed, errno %d\n", errno);
                return -1;
            } else {
                continue;
            }
        } else {
            isize += ret;
            //printf("write %d, cur isize %d\n", ret, isize);
        }
        //printf("ret %d, isize %d\n", ret, isize);
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

int uleb_decode(const uint8_t *buffer, size_t available, uint64_t *value,
    size_t *length) {
    if (buffer && value) {
        *value = 0;
        for (size_t i = 0; i < kMaximumLeb128Size && i < available; ++i) {
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

int uleb_encode(uint64_t value, size_t available, uint8_t *coded_value,
    size_t *coded_size) {
    const size_t leb_size = uleb_size_in_bytes(value);
    if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
        leb_size > available || !coded_value || !coded_size) {
        return -1;
    }

    for (size_t i = 0; i < leb_size; ++i) {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

        *(coded_value + i) = byte;
    }

    *coded_size = leb_size;
    return 0;
}

int uleb_encode_fixed_size(uint64_t value, size_t available,
    size_t pad_to_size, uint8_t *coded_value,
    size_t *coded_size) {
    if (value > kMaximumLeb128Value || !coded_value || !coded_size ||
        available < pad_to_size || pad_to_size > kMaximumLeb128Size) {
        return -1;
    }
    const uint64_t limit = 1ULL << (7 * pad_to_size);
    if (value >= limit) {
        // Can't encode 'value' within 'pad_to_size' bytes
        return -1;
    }

    for (size_t i = 0; i < pad_to_size; ++i) {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (i < pad_to_size - 1) byte |= 0x80;  // Signal that more bytes follow.

        *(coded_value + i) = byte;
    }

    *coded_size = pad_to_size;
    return 0;
}

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type) {
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

static int rb_read_bit(struct read_bit_buffer *rb) {
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

static int rb_read_literal(struct read_bit_buffer *rb, int bits) {
    int value = 0, bit;
    for (bit = bits - 1; bit >= 0; bit--) value |= rb_read_bit(rb) << bit;
    return value;
}

static int read_obu_size(const uint8_t *data,
                                     size_t bytes_available,
                                     size_t *const obu_size,
                                     size_t *const length_field_size) {
  uint64_t u_obu_size = 0;
  if (uleb_decode(data, bytes_available, &u_obu_size, length_field_size) !=
      0) {
    return -1;
  }

  if (u_obu_size > UINT32_MAX) return -1;
  *obu_size = (size_t)u_obu_size;
  return 0;
}

// Parses OBU header and stores values in 'header'.
static int read_obu_header(struct read_bit_buffer *rb,
                                       int is_annexb, ObuHeader *header) {
  if (!rb || !header) return -1;

  const ptrdiff_t bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
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
                                             size_t bytes_available,
                                             int is_annexb,
                                             ObuHeader *obu_header,
                                             size_t *const payload_size,
                                             size_t *const bytes_read) {
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

int parser_frame(
    int is_annexb,
    uint8_t *data,
    const uint8_t *data_end,
    uint8_t *dst_data,
    uint32_t *frame_len,
    uint8_t *meta_buf,
    uint32_t *meta_len) {
    int frame_decoding_finished = 0;
    uint32_t obu_size = 0;
    ObuHeader obu_header;
    memset(&obu_header, 0, sizeof(obu_header));
    int seen_frame_header = 0;
    int next_start_tile = 0;
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

        int status =
            read_obu_header_and_size(data, bytes_available, is_annexb,
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

        printf("\tobu %s len %zu+%zu\n", obu_type_name[obu_header.type], bytes_read, payload_size);

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
            next_start_tile = 0;
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
            }
            else {
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
            printf("\t meta type %s %zu+%zu\n", meta_type_name[type], bytes_read, payload_size - bytes_read);

            if (meta_type == OBU_METADATA_TYPE_ITUT_T35) {
#if 0 /* for dumping original obu payload */
                for (i = 0; i < payload_size - bytes_read; i++) {
                    printf("%02x ", p[i]);
                    if (i % 16 == 15) printf("\n");
                }
                if (i % 16 != 0) printf("\n");
#endif
                if ((p[0] == 0xb5) /* country code */
                    && ((p[1] == 0x00) && (p[2] == 0x3b)) /* terminal_provider_code */
                    && ((p[3] == 0x00) && (p[4] == 0x00) && (p[5] == 0x08) && (p[6] == 0x00))) { /* terminal_provider_oriented_code */
                    printf("\t\t dolby vision rpu\n");
                    meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
                    meta_buf[3] = 0x01;    meta_buf[4] = 0x19;

                    if (p[11] & 0x10) {
                        rpu_size = 0x100;
                        rpu_size |= (p[11] & 0x0f) << 4;
                        rpu_size |= (p[12] >> 4) & 0x0f;
                        if (p[12] & 0x08) {
                            printf("\t meta rpu in obu exceed 512 bytes\n");
                            break;
                        }
                        for (i = 0; i < rpu_size; i++) {
                            meta_buf[5 + i] = (p[12 + i] & 0x07) << 5;
                            meta_buf[5 + i] |= (p[13 + i] >> 3) & 0x1f;
                        }
                        rpu_size += 5;
                    }
                    else {
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
            }
            else if (meta_type == OBU_METADATA_TYPE_HDR_CLL) {
                printf("\t\t hdr10 cll:\n");
                printf("\t\t max_cll = %x\n", (p[0] << 8) | p[1]);
                printf("\t\t max_fall = %x\n", (p[2] << 8) | p[3]);
            }
            else if (meta_type == OBU_METADATA_TYPE_HDR_MDCV) {
                printf("\t\t hdr10 primaries[r,g,b] = \n");
                for (i = 0; i < 3; i++) {
                    printf("\t\t %x, %x\n",
                        (p[i * 4] << 8) | p[i * 4 + 1],
                        (p[i * 4 + 2] << 8) | p[i * 4 + 3]);
                }
                printf("\t\t white point = %x, %x\n", (p[12] << 8) | p[13], (p[14] << 8) | p[15]);
                printf("\t\t maxl = %x\n", (p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19]);
                printf("\t\t minl = %x\n", (p[20] << 24) | (p[21] << 16) | (p[22] << 8) | p[23]);
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

int ivf_write_dat(FILE *src_fp, uint8_t *src_buffer)
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
        printf("fail to alloc meta buf\n");
        return -1;
    }
    if (fread(src_buffer, 1, 32, src_fp) != 32) {
        printf("read input file error!\n");
        return -1;
    }
    p_size = (unsigned int *)(src_buffer + 24);
    process_count = *p_size;
    printf("frame number = %d\n", process_count);
    while (frame_count < process_count) {
        if (fread(src_buffer, 1, 12, src_fp) != 12) {
            printf("end of file!\n");
            break;
        }
        p_size = (unsigned int *)src_buffer;
        src_frame_size = *p_size;
        printf("frame %d, size %d\n", frame_count, src_frame_size);

        if (fread(src_buffer, 1, src_frame_size, src_fp) != src_frame_size) {
            printf("read input file error %d!\n", src_frame_size);
            break;
        }

        dst_buffer = calloc(1, src_frame_size + 4096);
        if (!dst_buffer) {
            printf("failed to alloc frame buf\n");
            break;
        }
        dst_frame_size = 0;
        meta_size = 0;

        parser_frame(0, src_buffer, src_buffer + src_frame_size, dst_buffer, &dst_frame_size, meta_buffer, &meta_size);
        if (dst_frame_size) {
            printf("\toutput len=%d\n", dst_frame_size);
            if (send_buffer_to_device((char *)dst_buffer, dst_frame_size) < 0) {
                free(dst_buffer);
                break;
            }
        }
        if (meta_size) {
            printf("\t meta len=%d\n", meta_size);
            /* dump meta here */
        }
        free(dst_buffer);
        frame_count++;
    }
    printf("Process %d frame\n", frame_count);
    free(meta_buffer);
    return 0;
}

int ivf_write_dat_with_size(uint8_t *src_buffer,unsigned int size)
{
    int frame_count = 0;
    unsigned int src_frame_size = 0;
    unsigned int dst_frame_size = 0;
    unsigned int meta_size = 0;
    uint8_t *dst_buffer = NULL;
    uint8_t *meta_buffer = NULL;
    int process_count = -1;
    unsigned int *p_size;
    uint8_t* buffer = NULL;

    buffer = malloc(BUFFER_SIZE);
    if (NULL == buffer) {
        printf("malloc write buffer fail\n");
        return -1;
    }
    memset(buffer, 0, BUFFER_SIZE);

    meta_buffer = calloc(1, 1024);
    if (!meta_buffer) {
        printf("fail to alloc meta buf\n");
        return -1;
    }
    size = 0;

    memcpy(buffer, src_buffer, 32);
    src_buffer += 32;

    p_size = (unsigned int *)(buffer + 24);
    process_count = *p_size;
    printf("frame number = %d\n", process_count);
    while (frame_count < process_count) {
        memcpy(buffer, src_buffer, 12);
        src_buffer += 12;
        p_size = (unsigned int *)buffer;
        src_frame_size = *p_size;
        printf("frame %d, size %d\n", frame_count, src_frame_size);

        memcpy(buffer, src_buffer, src_frame_size);
        src_buffer += src_frame_size;

        dst_buffer = calloc(1, src_frame_size + 4096);
        if (!dst_buffer) {
            printf("failed to alloc frame buf\n");
            break;
        }
        dst_frame_size = 0;
        meta_size = 0;

        parser_frame(0, buffer, buffer + src_frame_size, dst_buffer, &dst_frame_size, meta_buffer, &meta_size);
        if (dst_frame_size) {
            printf("\toutput len=%d\n", dst_frame_size);
            if (send_buffer_to_device((char *)dst_buffer, dst_frame_size) < 0) {
                free(dst_buffer);
                break;
            }
        }
        if (meta_size) {
            printf("\t meta len=%d\n", meta_size);
            /* dump meta here */
        }
        free(dst_buffer);
        frame_count++;
    }
    printf("Process %d frame\n", frame_count);
    free(meta_buffer);
    return 0;
}


/****************** end of ivf parer *************/


void get_vdec_id()
{
    int cfd;
    char buf[128];
    int count = 0, num = 0;
    cfd = open("/sys/class/vdec/core", O_RDONLY);
    if (cfd < 0) {
        printf("open /sys/class/vdec/core failed\n");
        printf("use id = 0\n");
    } else {
        lseek(cfd, 0, SEEK_SET);
        memset(buf, 0, sizeof(buf));
        count = 0;
        while ((num = read(cfd, buf, 128)) > 0) {
            int i = 0;
            for ( i = 0 ; i < 128 ; i++ )
            {
                if (buf[i] == '\n')
                {
                    count = count + 1;
                }
            }
            memset(buf,0,128);
        }
        id = count - 1;
        printf("count = %d,id = %d\n",count,id);
    }
}

/*
 * set tee load disable for some code tee is not updated;
 * set double write for hevc;
 * set frame check enable;
 * set check when 4 frame ready.
 */
static int slt_test_set(int tcase, int enable)
{
    if (enable) {
        // set_cmd("1",      "/sys/module/tee/parameters/disable_flag");
        if (tcase == TEST_CASE_HEVC) {
            set_cmd("0x8000000", "/sys/module/amvdec_h265/parameters/debug");
            set_cmd("1",         "/sys/module/amvdec_h265/parameters/double_write_mode");
        } else if (tcase == TEST_CASE_HEVC_VP9) {
            set_cmd("2000", "/sys/module/amvdec_vp9/parameters/start_decode_buf_level");
            set_cmd("0x80000001", "/sys/module/amvdec_vp9/parameters/double_write_mode");
        } else if (tcase == TEST_CASE_HEVC_AV1) {
            set_cmd("0x80000001", "/sys/module/amvdec_av1/parameters/double_write_mode");
        }
    } else {
        // set_cmd("0",      "/sys/module/tee/parameters/disable_flag");
        if (tcase == TEST_CASE_HEVC) {
            set_cmd("0",  "/sys/module/amvdec_h265/parameters/debug");
            set_cmd("0",  "/sys/module/amvdec_h265/parameters/double_write_mode");
        } else if (tcase == TEST_CASE_HEVC_VP9) {
            set_cmd("32768", "/sys/module/amvdec_vp9/parameters/start_decode_buf_level");
            set_cmd("0",  "/sys/module/amvdec_vp9/parameters/double_write_mode");
        } else if (tcase == TEST_CASE_HEVC_AV1) {
            set_cmd("0", "/sys/module/amvdec_av1/parameters/double_write_mode");
        }
    }
    printf("%s test %s\n", name,
        (enable == 0)?"end":"start");
    return 0;
}

static int do_video_decoder(int tcase)
{
    dec_sysinfo_t slt_sysinfo[4] = {
        [0] = {
            .format = VIDEO_DEC_FORMAT_HEVC,
            .width = 192,
            .height = 200,
            .rate = 96000/30,
            .param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE),
        },
        [1] = {
            .format = VIDEO_DEC_FORMAT_H264,
            .width = 80,
            .height = 96,
            .rate = 96000/30,
            .param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE),
        },
        [2] = {
            .format = VIDEO_DEC_FORMAT_VP9,
            .width = 192,
            .height = 192,
            .rate = 96000/30,
            .param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE),
        },
        [3] = {
            .format = VIDEO_DEC_FORMAT_AV1,
            .width = 426,
            .height = 240,
            .rate = 96000/30,
            .param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE),
        }
    };
    int ret = CODEC_ERROR_NONE;
    char buffer[READ_SIZE];

    unsigned int read_len, isize, padding_size, rest_size;
    unsigned int wait_cnt = 0;
    char *vstream = hevc_stream;

    get_vdec_id();

    set_cmd("rm default", "/sys/class/vfm/map");
    set_cmd("add default decoder ionvideo", "/sys/class/vfm/map");
    if (id == 0)
        set_cmd("0 1", "/sys/class/vdec/frame_check");
    else if (id == 1) {
        set_cmd("1 1", "/sys/class/vdec/frame_check");
    }


    vpcodec = &v_codec_para;
    memset(vpcodec, 0, sizeof(vcodec_para_t));
    vpcodec->has_video = 1;
    vpcodec->stream_type = STREAM_TYPE_ES_VIDEO;
    vpcodec->am_sysinfo = slt_sysinfo[tcase];
    //vpcodec->video_type = (tcase == TEST_CASE_HEVC)?11:2;    //hevc ? vdec
    if (tcase == TEST_CASE_HEVC)
        vpcodec->video_type = 11;
    else if (tcase == TEST_CASE_VDEC)
        vpcodec->video_type = 2;
    else if (tcase == TEST_CASE_HEVC_VP9)
        vpcodec->video_type = 14;
    else if (tcase == TEST_CASE_HEVC_AV1)
        vpcodec->video_type = 16;

    if (vpcodec->video_type == VFORMAT_AV1)
        vpcodec->mode = FRAME_MODE;

    ret = vcodec_init(vpcodec);
    ERRP((ret != CODEC_ERROR_NONE), goto tst_error_0,
        1, "codec init failed, ret=-0x%x", -ret);

    set_tsync_enable(0);
    printf("vcodec init ok, set cmp crc !\n");

    if (tcase == TEST_CASE_HEVC) {
        vstream = hevc_stream;
        rest_size = sizeof(hevc_stream);
        vcodec_set_frame_cmp_crc(vpcodec, hevc_crc,
            sizeof(hevc_crc)/(sizeof(int)*2), id);
    } else if (tcase == TEST_CASE_VDEC)  {
        vstream = h264_stream;
        rest_size = sizeof(h264_stream);
        vcodec_set_frame_cmp_crc(vpcodec, h264_crc,
            sizeof(h264_crc)/(sizeof(int)*2), id);
    } else if (tcase == TEST_CASE_HEVC_VP9) {
        /*
        video:
        android-cts-media-1.5/TestVectorsIttiam/vp9/yuv420/
        8bit/resolution/crowd_192x192p50f32_200kbps.webm
        */
        vstream = vp9_stream;
        rest_size = sizeof(vp9_stream);
        vcodec_set_frame_cmp_crc(vpcodec, vp9_crc,
            sizeof(vp9_crc)/(sizeof(int)*2), id);
    } else if (tcase == TEST_CASE_HEVC_AV1) {
        /*
        video:
        Animation_1c9d_240p.ivf
        */
        vstream = av1_stream;
        rest_size = sizeof(av1_stream);
        vcodec_set_frame_cmp_crc(vpcodec, av1_crc,
            sizeof(av1_crc)/(sizeof(int)*2), id);
    }

    pcodec = vpcodec;
    padding_size = 4096;

    if (is_video_file_type_ivf(NULL, vpcodec->video_type, vstream)) {
        printf("input video file is ivf with av1.\n");
        ivf_write_dat_with_size((uint8_t *)vstream, rest_size);
    } else {
        while (1) {
            if (padding_size) {
                if (rest_size <= READ_SIZE) {
                    memcpy(buffer, vstream, rest_size);
                    read_len = rest_size;
                    rest_size = 0;
                } else {
                    memcpy(buffer, vstream, READ_SIZE);
                    read_len = READ_SIZE;
                    rest_size -= read_len;
                }
            }
            isize = 0;
            wait_cnt = 0;
            do {
                ret = vcodec_write(pcodec, buffer + isize, read_len-isize);
                //printf("write ret = %d, isize = %d, read_len = %d\n", ret, isize, read_len);
                if (ret <= 0) {
                    if (errno != EAGAIN) {
                        printf("write data failed, errno %d\n", errno);
                        goto tst_error_1;
                    } else {
                        usleep(1000);
                        if (++wait_cnt > 255) {
                          break;
                        }
                        continue;
                    }
                } else {
                    isize += ret;    /*all write size*/
                    wait_cnt = 0;
                }
            } while (isize < read_len);

            if ((!rest_size) && (!padding_size))
                break;

            if (!rest_size) {
                memset(buffer, 0, padding_size);
                read_len = padding_size;
                padding_size = 0;
            }
        }
    }

    usleep(300000);   //sleep 0.3s

    /* wait cmp crc to end */
    wait_cnt = 0;
    do {
        ret = is_crc_cmp_ongoing(vpcodec, id);
        if (ret < 0)
            goto tst_error_1;
        usleep(10000);
        if (++wait_cnt > 99) {
            printf("wait decode end timeout.\n");
            ret = -1;
            goto tst_error_1;
        }
    } while (ret);

    ret = vcodec_get_crc_check_result(vpcodec, id);

    vcodec_close(vpcodec);

    return ret;

tst_error_1:
    vcodec_close(vpcodec);
tst_error_0:
    return ((ret < 0)?ret:-1);
}

int main(int argc, char *argv[])
{
    int tcase = TEST_CASE_HEVC;
    int ret = 0;
    int retry = 0;

    name = "hevc core:h265";
    if (argc > 1) {
        ret = atoi(argv[1]);
        if (ret == 1) {
            tcase = TEST_CASE_VDEC;
            name = "vdec core:h264";
        } else if (ret == 2) {
            tcase = TEST_CASE_HEVC_VP9;
            name = "hevc core:vp9";
        } else if (ret == 3) {
            tcase = TEST_CASE_HEVC_AV1;
            name = "hevc core:av1";
        }
    }
    printf("Decoder SLT Test [%s]\n",name);

    /* set cmd for test */
    slt_test_set(tcase, 1);

    retry = 0;
    do {
        if (retry > 0)
            usleep(10000);

        retry++;
        ret = do_video_decoder(tcase);
        if (ret < 0) {
            if (retry > (RETRY_TIME - 1))
                break;
            continue;
        }
    } while ((ret != 0) && (retry < RETRY_TIME));

    if (retry > 1)
        printf("ret: %d, retry: %d \n\n", ret, retry);

    /* set restore */
    slt_test_set(tcase, 0);
    set_cmd("rm default", "/sys/class/vfm/map");
    set_cmd("add default decoder ppmgr deinterlace amvideo", "/sys/class/vfm/map");

    printf("\n\n{\"result\": %s, \"item\": %s}\n\n\n",
        ret?"false":"true", name);

    return 0;
}

