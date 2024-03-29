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
#ifndef AMPORTS_PRIV_HEAD_HH
#define AMPORTS_PRIV_HEAD_HH
#include "../amports/streambuf.h"
#include "../../common/media_clock/switch/amports_gate.h"
#include <linux/amlogic/media/vfm/vframe.h>
#include "../../include/regs/dos_registers.h"
#include <linux/amlogic/media/utils/log.h>

struct port_priv_s {
	struct vdec_s *vdec;
	struct stream_port_s *port;
	struct mutex mutex;
	bool is_4k;
};

struct stream_buf_s *get_buf_by_type(u32 type);

/*video.c provide*/
extern u32 trickmode_i;
struct amvideocap_req;
extern u32 set_blackout_policy(int policy);
extern u32 get_blackout_policy(void);
int calculation_stream_ext_delayed_ms(u8 type);
int ext_get_cur_video_frame(struct vframe_s **vf, int *canvas_index);
int ext_put_video_frame(struct vframe_s *vf);
int ext_register_end_frame_callback(struct amvideocap_req *req);
int amstream_request_firmware_from_sys(const char *file_name,
	char *buf, int size);
void set_vsync_pts_inc_mode(int inc);

void set_real_audio_info(void *arg);
void amstream_wakeup_userdata_poll(struct vdec_s *vdec);
#define dbg() pr_info("on %s,line %d\n", __func__, __LINE__);

struct device *amports_get_dma_device(void);
struct device *get_codec_cma_device(void);

#endif
