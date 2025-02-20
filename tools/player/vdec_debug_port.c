#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

#include <sys/mman.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include "vdec_debug_port.h"


int dev_status;

int debug_port_save_file_new(char *filename, char *buf, u32 size)
{
	int fd;
	int ret = 0;
	int retry = 0;

	if ((buf == NULL) || (size == 0))
		return 0;

	fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
	if (fd < 0) {
		printf("%s, create %s failed\n", __func__, filename);
		return -1;
	}

	do {
		ret += write(fd, buf + ret, size);
		if (++retry > ((size << 4) + 1))
			break;

	} while (ret < size);

	close(fd);

	if (ret < size) {
		printf("%s, write size less than required, %d < %d\n", __func__, ret, size);
	}
#ifdef DBG_PORT_DEBUG
	else if (ret == size) {
		printf("save to file %s success, write size 0x%x\n", filename, ret);
	}
#endif
	return 0;
}

int debug_port_save_file_append(int fd, char *buf, u32 size)
{
	int ret = 0;
	int retry = 0;

	lseek(fd, 0, SEEK_END);

	do {
		ret += write(fd, buf + ret, size);
		if (++retry > ((size << 4) + 1))
			break;

	} while (ret < size);

	if (ret < size) {
		printf("%s, write size less than required, %d < %d\n", __func__, ret, size);
	}

	return 0;
}

bool is_oversize(int w, int h)
{
	if (w <= 0 || h <= 0)
		return true;

	if (h != 0 && (w > (MAX_PIC_SIZE / h)))
		return true;

	return false;
}


void port_debug_config(int fd, int cmd, int vid, int pic_start, int pic_num, int mode)
{
	struct debug_config_param param;
	int ret;

	if (cmd & CMD_DUMP_YUV) {
		param.type		= TYPE_YUV;
		param.id		= vid;
		param.pic_start = pic_start;
		param.pic_num	= pic_num;
		ret = ioctl(fd, VDBG_IOC_PORT_CFG, &param);   //config yuv dump
		if (ret < 0) {
			printf("config dump yuv failed, err %d\n", errno);
		}
	}

	if (cmd & CMD_DUMP_CRC) {
		param.type     = TYPE_CRC;
		param.id       = vid;
		ret = ioctl(fd, VDBG_IOC_PORT_CFG, &param);
		if (ret < 0) {
			printf("config dump crc failed, err %d\n", errno);
		}
	}

	if (cmd & CMD_DUMP_ES) {
		param.type     = TYPE_ES;
		param.id       = vid;
		param.mode     = mode;
		ret = ioctl(fd, VDBG_IOC_PORT_CFG, &param);
		if (ret < 0) {
			printf("config dump es failed, err %d\n", errno);
		}
	}

	if (cmd & CMD_DUMP_AUX) {
		param.type     = TYPE_AUX;
		param.id       = vid;
		ret = ioctl(fd, VDBG_IOC_PORT_CFG, &param);
		if (ret < 0) {
			printf("config dump aux failed, err %d\n", errno);
		}
	}
}

static void signal_handler(int signum)
{
	dev_status = -1;
	printf("exit signal %d\n", signum);
}


int mm_debug_port_get_data(int dev, char *buf, u32 buf_size)
{
	int i, j;
	int dump_fd[MAX_INSTANCE_NUM][TYPE_MAX];
	int sum_pkt[MAX_INSTANCE_NUM][TYPE_MAX];

	char file_str[32] = "name-0-0";
	char last_str[32] = {0};
	char file_name[64] = {0};
	const char *file_ext[TYPE_MAX] = {"info", "yuv", "crc", "es", "aux"};

	u32 no_data_wait_time = 0;
	struct pollfd pfd;
	int ret;

	memset(dump_fd, 0xff, sizeof(dump_fd));
	memset(sum_pkt, 0, sizeof(sum_pkt));

	pfd.fd     = dev;
	pfd.events = POLLIN | POLLERR;

	do {
		if (dev_status < 0) {
			ret = dev_status;
			break;
		}
		ret = poll(&pfd, 1, VDBG_POLL_TIMEOUT);
		if (ret < 0) {
			break;
		} else if (ret == 0) {
			if (++no_data_wait_time > 10) {
				printf("vdec debug port waiting data ready ...\n");
				no_data_wait_time = 0;
				//break;
			}

			continue;
		}
		no_data_wait_time = 0;

		ret = read(dev, buf, buf_size);
		if (ret > 0) {
			struct port_data_packet pkt;

			memcpy(&pkt, buf, sizeof(pkt));

			if ((pkt.data_size > 0) &&
				((pkt.header != PACKET_HEADER) ||
				(pkt.id >= MAX_INSTANCE_NUM) ||
				(pkt.type >= TYPE_MAX))) {

				ret = ioctl(dev, VDBG_IOC_BUF_RESET);
				if (ret < 0)
					printf("VDBG_IOC_BUF_RESET failed\n");
				continue;
			}
			/*
			printf("pkt[%d][%d](%d): 0x%x, size 0x%x\n",
				pkt.id, pkt.type, sum_pkt[pkt.id][pkt.type], pkt.header, pkt.data_size);
			*/
			/* coverity[tainted_data:SUPPRESS] */
			printf("pkt[%d][%d](%d): size 0x%x\n",
				pkt.id, pkt.type, sum_pkt[pkt.id][pkt.type], pkt.data_size);

			/*
			 * if pkt.type > 4, buffer will be reset and
			 * continue to reread buf.
			 */
			/* coverity[OVERRUN:SUPPRESS] */
			sum_pkt[pkt.id][pkt.type]++;

			if (pkt.type == TYPE_INFO) {
				struct port_vdec_info info;

				memcpy(&info, buf + sizeof(pkt), sizeof(info));

				printf("info[%d]: \n", info.id);
				printf("format  : %d\n", info.format);
				printf("width   : %d\n", info.dw_w);
				printf("height  : %d\n", info.dw_h);

				memset(file_str, 0, sizeof(file_str));
				snprintf(file_str, sizeof(file_str), "%d_%d_%dx%d",
					pkt.id, info.format, info.dw_w, info.dw_h);
				continue;
			} else {
				int fd = dump_fd[pkt.id][pkt.type];

				if (fd > 0) {
#if 0
					//create a new file when file str changed ?
					if (strncmp(file_str, last_str, sizeof(file_str))) {
						printf("close old file %d\n", fd);
						close(fd);
						fd = -1;
						dump_fd[pkt.id][pkt.type] = -1;
					}
#endif
				} else if (fd <= 0) {
					memset(file_name, 0, sizeof(file_name));
					/* coverity[OVERRUN:SUPPRESS] */
					snprintf(file_name, sizeof(file_name), "%s/%s.%s",
						DUMP_FILE_PATH, file_str, file_ext[pkt.type]);
					memcpy(last_str, file_str, sizeof(file_str));

					fd = open(file_name, O_RDWR | O_CREAT | O_APPEND, 0644); //O_TRUNC
					if (fd < 0) {
						printf("create yuv %s failed, err %d\n", file_name, errno);
						continue;
					}
					/* coverity[OVERRUN:SUPPRESS] */
					dump_fd[pkt.id][pkt.type] = fd;

					printf("create file %s success\n", file_name);
				}

				/* coverity[tainted_data:SUPPRESS] */
				debug_port_save_file_append(fd, buf + sizeof(pkt), pkt.data_size);
			}
		}
	} while(1);

	for (i = 0; i < MAX_INSTANCE_NUM; i++) {
		for (j = 0; j < TYPE_MAX; j++) {
			if (dump_fd[i][j] > 0) {
				close(dump_fd[i][j]);
				dump_fd[i][j] = -1;
				printf("instance %d, type %d, total %d packets received\n", i, j, sum_pkt[i][j]);
			}
		}
	}

	return 0;
}


int main(int argc, char *argv[])
{
	char *dump_buf = NULL;
	int dev_fd = 0;

	dev_fd = open(DEC_DEBUG_PORT_DEV, O_RDWR);
	if (dev_fd < 0) {
		printf("open %s failed, error %d\n", DEC_DEBUG_PORT_DEV, errno);
		return 0;
	}

	dump_buf = (char *)malloc(MALLOC_BUF_SIZE);
	if (dump_buf == NULL) {
		printf("malloc failed\n");
		close(dev_fd);
		return 0;
	}

	dev_status = 0;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);

	mm_debug_port_get_data(dev_fd, dump_buf, MALLOC_BUF_SIZE);

	if (dump_buf)
		free(dump_buf);

	if (dev_fd > 0)
		close(dev_fd);

	printf("exited\n");

	return 0;
}

