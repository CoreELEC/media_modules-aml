/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdarg.h>
#include "vcodec_utils.h"

#define CRC_LE_BITS 64
#define LE_TABLE_ROWS (CRC_LE_BITS / 8)
#define LE_TABLE_SIZE 256
#define CRC32_POLYNOMIAL_LE 0xEDB88320UL
#define unlikely(x) __builtin_expect(!!(x), 0)

static uint32_t crc32table_le[LE_TABLE_ROWS][256];
static int init_table_flag = 0;

extern int g_log_level;

static void crc32init_le_generic(const uint32_t polynomial,
				 uint32_t (*tab)[256])
{
	unsigned i, j;
	uint32_t crc = 1;

	tab[0][0] = 0;

	for (i = LE_TABLE_SIZE >> 1; i; i >>= 1) {
		crc = (crc >> 1) ^ ((crc & 1) ? polynomial : 0);
		for (j = 0; j < LE_TABLE_SIZE; j += 2 * i)
			tab[0][i + j] = crc ^ tab[0][j];
	}

	for (i = 0; i < LE_TABLE_SIZE; i++) {
		crc = tab[0][i];
		for (j = 1; j < LE_TABLE_ROWS; j++) {
			crc = tab[0][crc & 0xff] ^ (crc >> 8);
			tab[j][i] = crc;
		}
	}
}

static inline uint32_t crc32_body(uint32_t crc, unsigned char const *buf,
	size_t len, const uint32_t (*tab)[256])
{
#define DO_CRC(x) crc = t0[(crc ^ (x)) & 255] ^ (crc >> 8)
#define DO_CRC4 (t3[(q) & 255] ^ t2[(q >> 8) & 255] ^ \
			t1[(q >> 16) & 255] ^ t0[(q >> 24) & 255])
#define DO_CRC8 (t7[(q) & 255] ^ t6[(q >> 8) & 255] ^ \
			t5[(q >> 16) & 255] ^ t4[(q >> 24) & 255])

	const uint32_t *b;
	size_t rem_len;
	const uint32_t *t0 = tab[0], *t1 = tab[1], *t2 = tab[2], *t3 = tab[3];
	const uint32_t *t4 = tab[4], *t5 = tab[5], *t6 = tab[6], *t7 = tab[7];
	uint32_t q;

	/* Align it */
	if (unlikely((long)buf & 3 && len)) {
		do {
			DO_CRC(*buf++);
		} while ((--len) && ((long)buf) & 3);
	}

	rem_len = len & 7;
	len = len >> 3;

	b = (const uint32_t *)buf;

	for (--b; len; --len) {
		q = crc ^ *++b; /* use pre increment for speed */
		crc = DO_CRC8;
		q = *++b;
		crc ^= DO_CRC4;
	}

	len = rem_len;

	/* And the last few bytes */
	if (len) {
		uint8_t *p = (uint8_t *)(b + 1) - 1;
		do {
			DO_CRC(*++p); /* use pre increment for speed */
		} while (--len);
	}
	return crc;
#undef DO_CRC
#undef DO_CRC4
#undef DO_CRC8
}

static inline uint32_t crc32_le_generic(uint32_t crc, unsigned char const *p,
	size_t len, const uint32_t (*tab)[256],
	uint32_t polynomial)
{
	if (!init_table_flag)
		crc32init_le_generic(CRC32_POLYNOMIAL_LE, crc32table_le);

	crc = crc32_body(crc, p, len, tab);
	return crc;
}

uint32_t crc32_le(uint32_t crc, unsigned char const *p, int len)
{
	return crc32_le_generic(crc, p, len,
		(const uint32_t (*)[256])crc32table_le, CRC32_POLYNOMIAL_LE);
}

int debug_print(int flag, const char *fmt, ...)
{
	if ((flag == 0) ||
		(g_log_level & flag)) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
	return 0;
}

