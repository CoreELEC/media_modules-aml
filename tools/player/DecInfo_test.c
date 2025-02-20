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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

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
	FILE *vfp[9] = {NULL};
	unsigned int choices[9] = {999};
	unsigned int v;
	int r,fd,i,t,t2;
	struct vframe_counter_s *c;
	unsigned int w8=0,w0=0,wait_time=1;
	struct vframe_qos_s *q;
	char *fn = "/dev/amstream_userdata";//This file should not affect other dev file
	struct av_param_mvdec_t para;

	printf("%zu %zu %zu \n",sizeof(struct vframe_comm_s),
		sizeof(struct vframe_counter_s),sizeof(struct av_param_mvdec_t));
	int status = system("mkdir -p /data/tmp/mvdec");
	if (status == -1) {
		// Handle error: system call failed
		printf("system call (mkdir -p /data/tmp/mvdec) failed %d\n", __LINE__);
	} else {
		// Optionally handle specific command exit status
		int exit_status = WEXITSTATUS(status);
		if (exit_status != 0) {
			// Handle specific command failure
			printf("system call (mkdir -p /data/tmp/mvdec) failed %d\n", __LINE__);
		}
	}

	status = system("rm /data/tmp/mvdec/vdec*");
	if (status == -1) {
		// Handle error: system call failed
		printf("system call (mkdir -p /data/tmp/mvdec) failed %d\n", __LINE__);
	} else {
		// Optionally handle specific command exit status
		int exit_status = WEXITSTATUS(status);
		if (exit_status != 0) {
			// Handle specific command failure
			printf("system call (mkdir -p /data/tmp/mvdec) failed %d\n", __LINE__);
		}
	}

	for (i=0; i<9; i++) {
		vfp[i] = NULL;
		choices[i] = 999;
	}


	if (argc >1) {
		for (i=0; i<(argc-1) && i<9; i++)
			choices[i] = atoi(argv[i+1]);
	}

	printf("%s using %s\n", argv[0],fn);
	fd = open(fn, O_RDONLY);
	if (fd <= 0)
		return 0;

	while (1) {
		for (v=0;v<9;v++) {
			if (argc > 1) {
				/* If select some choices, only these choices will record*/
				int got_choice = 0;
				for (i=0; i<9; i++)
					if (v == choices[i]) {
						got_choice = 1;
						break;
					}
				if (got_choice != 1)
					continue;
			}
			while (1) {
				memset(&para.minfo, 0, 8*sizeof(struct vframe_counter_s));
				para.vdec_id = v;
				para.struct_size = sizeof(struct av_param_mvdec_t);
				//printf("para.struct_size=%u\n",para.struct_size);
				r = ioctl(fd, AMSTREAM_IOC_GET_MVDECINFO, &para);
				if (r < 0) {
					//printf("ioctl error, ret=%d \n", r);
					break;
				}
				else if (para.slots == 0) {
					if (++w0 >9) {
					/* Reading too fast, so that increasing wait time */
						w0 = 0;
						wait_time++;
						//printf("wait_time=%u\n",wait_time);
					}
					break;
				} else if (para.slots == 8 && ++w8 >9) {
					/* Reading too slow, so that decreasing wait time */
					w0 = 0;
					w8 = 0;
					if (wait_time > 1)
						wait_time--;
					else
						wait_time = 0;
					//printf("wait_time=%u\n",wait_time);
				}

				if (vfp[v] == NULL) {
					char tmpvdec[32] = {0};
					sprintf(tmpvdec, "/data/tmp/mvdec/vdec.%d.log", v);
					vfp[v] = fopen(tmpvdec, "w+");
					if (vfp[v] == NULL) {
						printf("fopen %s error!\n",tmpvdec);
						return 0;
					}
				}


				fprintf(vfp[v],"got %d frames vdec_id=%d\n",para.slots, v);
				for (i=0; i<para.slots; i++) {
					fprintf(vfp[v],"\n---------------------------------\n");
					fprintf(vfp[v],"vdec_id = %d\n", para.comm.vdec_id);
					fprintf(vfp[v],"vdec_name = %s\n", para.comm.vdec_name);
					t2 = para.comm.vdec_type;
					fprintf(vfp[v],"vdec_type = %s mode\n", t2==2?"frame":(t2==1?"stream":"single"));

					fprintf(vfp[v],"vframe_qos_s { //(compatible with single mode)\n");
					q = &para.minfo[i].qos;
					fprintf(vfp[v],"frame num = %u\n", q->num);
					fprintf(vfp[v],"frame type = %u\n", q->type);
					fprintf(vfp[v],"frame size = %u\n", q->size);
					fprintf(vfp[v],"frame pts = %u\n", q->pts);
					fprintf(vfp[v],"max_qp = %d\n", q->max_qp);
					fprintf(vfp[v],"avg_qp = %d\n", q->avg_qp);
					fprintf(vfp[v],"min_qp = %d\n", q->min_qp);
					fprintf(vfp[v],"max_skip = %d\n", q->max_skip);
					fprintf(vfp[v],"avg_skip = %d\n", q->avg_skip);
					fprintf(vfp[v],"min_skip = %d\n", q->min_skip);
					fprintf(vfp[v],"max_mv = %d\n", q->max_mv);
					fprintf(vfp[v],"min_mv = %d\n", q->min_mv);
					fprintf(vfp[v],"avg_mv = %d\n", q->avg_mv);
					//fprintf(vfp[v],"decode_buffer = %d\n", q->decode_buffer);
					fprintf(vfp[v],"}\n");

					fprintf(vfp[v],"other parameters:\n");
					c = &para.minfo[i];
					fprintf(vfp[v],"double_write_mode = %u\n", c->double_write_mode);
					t = c->decode_time_cost;
					fprintf(vfp[v],"decode_time_cost = %u.%u ms\n",t/1000000,t%1000000);
					fprintf(vfp[v],"frame_width = %u\n", c->frame_width);
					fprintf(vfp[v],"frame_height = %u\n", c->frame_height);
					fprintf(vfp[v],"frame_dur = %u\n", c->frame_dur);
					fprintf(vfp[v],"frame_rate = %u\n", c->frame_rate);
					fprintf(vfp[v],"bit_depth_luma = %u\n", c->bit_depth_luma);
					fprintf(vfp[v],"bit_depth_chroma = %u\n", c->bit_depth_chroma);
					fprintf(vfp[v],"error_count = %u\n", c->error_count);
					fprintf(vfp[v],"status = 0x%x\n", c->status);
					fprintf(vfp[v],"frame_count = %u\n", c->frame_count);
					fprintf(vfp[v],"error_frame_count = %u\n", c->error_frame_count);
					fprintf(vfp[v],"drop_frame_count = %u\n", c->drop_frame_count);
					fprintf(vfp[v],"total_data = 0x%llx\n", c->total_data);
					fprintf(vfp[v],"offset = 0x%x\n", c->offset);
					fprintf(vfp[v],"ratio_control = %u (0x%x)\n", c->ratio_control, c->ratio_control);
					fprintf(vfp[v],"vf_type = 0x%x\n", c->vf_type);
					fprintf(vfp[v],"signal_type = 0x%x\n", c->signal_type);
					fprintf(vfp[v],"pts = %u (0x%x)\n", c->pts, c->pts);
					fprintf(vfp[v],"pts_us64 = %llu (0x%llx)\n", c->pts_us64, c->pts_us64);
				}
			} // End for one vdec instance
		}
		usleep(wait_time*10*1000);
	}

    return 0;
}

