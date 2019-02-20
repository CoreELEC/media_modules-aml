/***************************************************************************
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.                   *
 ***************************************************************************/

#include "swdemux_internal.h"

static SWDMX_UInt32 crc32_table[256];

static void
crc32_table_init(void)
{
	SWDMX_UInt32 i, j, k;

	for (i = 0; i < 256; i++) {
		k = 0;
		for (j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1)
			k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);
		crc32_table[i] = k;
	}
}

SWDMX_UInt32
swdmx_crc32 (
		const SWDMX_UInt8 *p,
		SWDMX_Int          len)
{
      SWDMX_UInt32 i_crc = 0xffffffff;

      if (!crc32_table[0])
	      crc32_table_init();

      while (len) {
	      i_crc = (i_crc << 8) ^ crc32_table[(i_crc >> 24) ^ *p];
	      p   ++;
	      len --;
      }

      return i_crc;
}

