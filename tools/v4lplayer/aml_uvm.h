#ifndef AMUVM_H
#define AMUVM_H

#include <stdint.h>

#define UVM_IMM_ALLOC	(1 << 0)
#define UVM_DELAY_ALLOC	(1 << 1)
#define META_DATA_SIZE 256
#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_ALLOC _IOWR((UVM_IOC_MAGIC), 0, \
			struct uvm_alloc_data)
#define UVM_IOC_FREE _IOWR(UVM_IOC_MAGIC, 1, \
			struct uvm_alloc_data)
#define UVM_IOC_SET_PID _IOWR(UVM_IOC_MAGIC, 2, \
			struct uvm_pid_data)
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, \
			struct uvm_fd_data)
#define UVM_IOC_GET_METADATA _IOWR(UVM_IOC_MAGIC, 4, \
			struct uvm_meta_data)

int32_t alloc_uvm_buffer(uint32_t width, uint32_t height, void** mapaddr, unsigned int i, int* fd);
int32_t free_uvm_buffers(void);
#endif

