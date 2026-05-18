#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include "aml_uvm.h"
#include "vcodec_utils.h"

#define AMUVM_DEV "/dev/uvm"
#define MAX_UVM_BUFF 32

struct uvmInfo {
	void* map_addr;
	int32_t shared_fd;
	int size;
};

/*
 * from uvm code, kernel 6.xx has "slot_id" member,
 * but kernel 5.xx doesn't have,
 * so v4lplayer has to use two struct
 */
struct uvm_alloc_data_kernel5 {
	int size;
	int align;
	unsigned int flags;
	int v4l2_fd;
	int fd;
	int byte_stride;
	uint32_t width;
	uint32_t height;
	int scalar;
	int scaled_buf_size;
};

struct uvm_alloc_data_kernel6 {
	int size;
	int align;
	unsigned int flags;
	int v4l2_fd;
	int fd;
	int byte_stride;
	uint32_t width;
	uint32_t height;
	int scalar;
	int scaled_buf_size;
	uint32_t slot_id;
};

struct uvm_meta_data {
	int fd;
	int type;
	int size;
	uint8_t data[META_DATA_SIZE];
};

static int32_t m_uvm_fd = -1;
static struct uvmInfo m_uvm_buff[MAX_UVM_BUFF];

static int amuvm_open()
{
	int uvm_fd = open(AMUVM_DEV, O_RDWR | O_NONBLOCK, 0);
	if (uvm_fd < 0) {
	    debug_print(DEBUG_ERROR, "open uvm dev fail");
	    return -1;
	}

	return uvm_fd;
}

static int amuvm_close(int uvmfd)
{
	if (uvmfd > 0)
	    close(uvmfd);

	return 0;
}

static int amuvm_allocate(int uvmfd, int size, uint32_t width, uint32_t height, unsigned int flag, int *sharefd)
{
	struct utsname sysinfo;
	struct uvm_alloc_data_kernel6 uad1;
	struct uvm_alloc_data_kernel5 uad2;
	int ret = 0;

	if (uname(&sysinfo) != 0) {
		debug_print(DEBUG_ERROR, "%s get uname fail\n", __func__);
		return -1;
	}

	debug_print(DEBUG_STATE, "system release: %s\n", sysinfo.release);
	if (strncmp(sysinfo.release, "6.", 2) == 0) {
		uad1.size = size;
		uad1.byte_stride = width;
		uad1.width = width;
		uad1.height = height;
		uad1.align = 0;
		uad1.flags = flag;
		uad1.scalar = 1;

		uad1.slot_id = 0;	/* valid on kernel 6.xx */
	} else if (strncmp(sysinfo.release, "5.", 2) == 0) {
		uad2.size = size;
		uad2.byte_stride = width;
		uad2.width = width;
		uad2.height = height;
		uad2.align = 0;
		uad2.flags = flag;
		uad2.scalar = 1;
	}

	if (uvmfd < 0) {
	    debug_print(DEBUG_ERROR, "need open uvm first\n");
	    return -1;
	}

	debug_print(DEBUG_STATE, "amuvm_allocate size:%d width:%d height:%d flag:%d\n", size, width, height, flag);
	if (strncmp(sysinfo.release, "6.", 2) == 0) {
		ret = ioctl(uvmfd, UVM_IOC_ALLOC_KERNEL6, &uad1);
	} else if (strncmp(sysinfo.release, "5.", 2) == 0) {
		ret = ioctl(uvmfd, UVM_IOC_ALLOC_KERNEL5, &uad2);
	}

	if (ret < 0) {
		debug_print(DEBUG_ERROR, "uvm alloc error, %s\n", strerror(errno));
		return -1;
	}
	if (strncmp(sysinfo.release, "6.", 2) == 0) {
		*sharefd = uad1.fd;
	} else if (strncmp(sysinfo.release, "5.", 2) == 0) {
		*sharefd = uad2.fd;
	}

	return 0;
}

static int amuvm_free(int sharefd)
{
	if (sharefd >= 0) {
	    close(sharefd);
	}

	return 0;
}

int32_t free_uvm_buffers(void)
{
	int32_t ret = 0;
	int i = 0;

	if (m_uvm_fd < 0) {
		return -1;
	}

	for (i = 0; i < MAX_UVM_BUFF; i ++) {
		if (m_uvm_buff[i].map_addr) {
			munmap(m_uvm_buff[i].map_addr, m_uvm_buff[i].size);
			m_uvm_buff[i].map_addr = NULL;
		}
		amuvm_free(m_uvm_buff[i].shared_fd);
		m_uvm_buff[i].shared_fd = -1;
	}

	ret = amuvm_close(m_uvm_fd);
	m_uvm_fd= -1;
	debug_print(DEBUG_STATE, "freeAllUvmBuffer\n");
	return ret;
}


int32_t alloc_uvm_buffer(uint32_t width, uint32_t height, void** mapaddr, unsigned int i, int* fd)
{
	int shared_fd = -1;
	int buffer_size = 0;
	int ret =  0;

	if (i >= MAX_UVM_BUFF) {
		debug_print(DEBUG_ERROR, "is over mOutputBufferNum\n");
		return -1;
	}

	if (m_uvm_fd <= 0) {
		m_uvm_fd = amuvm_open();
		if (m_uvm_fd < 0) {
			debug_print(DEBUG_ERROR, "open uvm device fail\n");
			return -1;
		}
	}

	//uvm alloc min size
	if (width < 720)
		width = 720;
	if (height < 576)
		height = 576;

	buffer_size = width * height * 3 / 2;
	ret = amuvm_allocate(m_uvm_fd, buffer_size, width, height, UVM_IMM_ALLOC, &shared_fd);
	if (ret < 0) {
		free_uvm_buffers();
		return -1;
	}

	void *cpu_ptr = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0);
	if (MAP_FAILED == cpu_ptr) {
		debug_print(DEBUG_ERROR, "mmap error!\n");
		free_uvm_buffers();
		return -1;
	}

	*mapaddr = cpu_ptr;
	if (fd)
		*fd = shared_fd;

	m_uvm_buff[i].map_addr = cpu_ptr;
	m_uvm_buff[i].shared_fd = shared_fd;
	m_uvm_buff[i].size = buffer_size;

	//printf("allocUvmBuffer shared_fd=%d, mDmaFd=%d, fd_ptr=%p, fd=%d\n", shared_fd, m_uvm_fd,cpu_ptr,shared_fd);

	return 0;
}

