/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
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
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "vcodec.h"

#define u32 unsigned int
#define u64 unsigned long long

struct vframe_qos_s {
	u32 num;
	u32 type;
	u32 size;
	u32 pts;
	int max_qp;
	int avg_qp;
	int min_qp;
	int max_skip;
	int avg_skip;
	int min_skip;
	int max_mv;
	int min_mv;
	int avg_mv;
	int decode_buffer;
} /*vframe_qos */;


struct vframe_comm_s {
	int vdec_id;
	char vdec_name[16];
	u32 vdec_type;
};


struct vframe_counter_s {
	struct vframe_qos_s qos;
	u32  decode_time_cost;/*us*/
	u32 frame_width;
	u32 frame_height;
	u32 frame_rate;
	u32 bit_depth_luma;//original bit_rate;
	u32 frame_dur;
	u32 bit_depth_chroma;//original frame_data;
	u32 error_count;
	u32 status;
	u32 frame_count;
	u32 error_frame_count;
	u32 drop_frame_count;
	u64 total_data;//this member must be 8 bytes alignment
	u32 double_write_mode;//original samp_cnt;
	u32 offset;
	u32 ratio_control;
	u32 vf_type;
	u32 signal_type;
	u32 pts;
	u64 pts_us64;
};

/*This is a versioning structure, the key member is the struct_size.
 *In the 1st version it is not used,but will have its role in future.
 *https://bytes.com/topic/c/answers/811125-struct-versioning
 */
struct av_param_mvdec_t {
	int vdec_id;

	/*This member is used for versioning this structure.
	 *When passed from userspace, its value must be
	 *sizeof(struct av_param_mvdec_t)
	 */
	int struct_size;

	int slots;

	struct vframe_comm_s comm;
	struct vframe_counter_s minfo[8];
};

#define _A_M  'S'
#define AMSTREAM_IOC_GET_MVDECINFO _IOR((_A_M), 0xcb, int)

int main(int argc, char *argv[])
{
	unsigned int j;
	int r,fd,i;
	unsigned int *c;
	int *q;
	char *fn = "/dev/amstream_userdata";//This file should not affect other dev file
	struct av_param_mvdec_t para;
	int vdec_id = 0;

//	printf("%u %u %u \n",sizeof(struct vframe_comm_s),
//		sizeof(struct vframe_counter_s),sizeof(struct av_param_mvdec_t));

	printf("%s using %s\n", argv[0],fn);
	if (argc > 1)
		vdec_id = atoi(argv[1]);
	fd = open(fn, O_RDONLY);
	if (fd <= 0)
		return 0;

	while (1) {
		memset(&para.minfo, 0, 8*sizeof(struct vframe_counter_s));

		/* Prepare the parameter */
		para.vdec_id = vdec_id;
		para.struct_size = sizeof(struct av_param_mvdec_t);
		//printf("para.struct_size=%u\n",para.struct_size);

		/* Get the vdec frames information via ioctl */
		r = ioctl(fd, AMSTREAM_IOC_GET_MVDECINFO, &para);
		if (r < 0) {
			printf("ioctl error for vdec %d, ret=%d \n", vdec_id, r);
			break;
		}
		else if (para.slots == 0) {
			//The decoder does not provide the info yet, wait sometime
			usleep(100*1000);
			continue;
		}


		/* Print the vdec frames related informations */
		printf("got %d frames info, vdec_id=%d\n",para.slots, vdec_id);
		for (i=0; i<para.slots; i++) {
			printf("vdec_name : %s\n", para.comm.vdec_name);
			printf("vdec_type : %d\n", para.comm.vdec_type);

			printf("frame_width : %s\n", para.minfo[i].frame_width);
			printf("frame_height : %s\n", para.minfo[i].frame_height);
			printf("pts_us64 : %s\n", para.minfo[i].pts_us64);

			printf("qos.type : %s\n", para.minfo[i].qos.type);

			printf("----------------\n");
		}
	}

	return 0;
}

