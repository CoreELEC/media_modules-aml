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
#ifndef __ENC_REPORT_H__
#define __ENC_REPORT_H__

#define DEBUG_AMVENC_264     "amvenc_264"
#define DEBUG_AMVENC_265     "amvenc_265"
#define DEBUG_AMVENC_MULIT   "amvenc_multi"
#define DEBUG_AMVENC_VERS    "amvenc_vers"
#define DEBUG_AMVENC_JPEG    "amvenc_jpeg"

typedef void (*enc_set_debug_level_func)(const char *module, int level);

int enc_register_set_debug_level_func(const char *module, enc_set_debug_level_func func);

#endif

