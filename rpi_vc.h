//

#include "rpi_vm.h"

int open_mbox(void);
void close_mbox(int fd);
uint32_t msg_mbox(int fd, VC_MSG *msgp);
uint32_t alloc_vc_mem(int fd, uint32_t size, VC_ALLOC_FLAGS flags);
void *lock_vc_mem(int fd, int h);
uint32_t unlock_vc_mem(int fd, int h);
uint32_t free_vc_mem(int fd, int h);
uint32_t set_vc_clock(int fd, int id, uint32_t freq);
void disp_vc_msg(VC_MSG *msgp);
void *map_uncached_mem(MEM_MAP *mp, int size);
void unmap_mem(MEM_MAP *mp);

// eof