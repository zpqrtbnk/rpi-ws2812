//

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "rpi_lib.h"
#include "rpi_vc.h"

// Open mailbox interface, return file descriptor
int open_mbox(void)
{
   int fd;

   if ((fd = open("/dev/vcio", 0)) < 0)
   {
       printf("error: can't open VC mailbox\n");
   }
   return fd;
}

// Close mailbox interface
void close_mbox(int fd)
{
    if (fd >= 0)
        close(fd);
}

// Send message to mailbox, return first response int, 0 if error
uint32_t msg_mbox(int fd, VC_MSG *msgp)
{
    uint32_t ret=0, i;

    for (i = msgp->dlen / 4; i <= msgp->blen / 4; i += 4)
        msgp->uints[i++] = 0;
    msgp->len = (msgp->blen + 6) * 4;
    msgp->req = 0;
    if (ioctl(fd, _IOWR(100, 0, void *), msgp) < 0)
        printf("warn: VC IOCTL failed\n");
    else if ((msgp->req&0x80000000) == 0)
        printf("warn: VC IOCTL error\n");
    else if (msgp->req == 0x80000001)
        printf("warn: VC IOCTL partial error\n");
    else
        ret = msgp->uints[0];
		
#if DEBUG
    printf("msg mbox: ");
    disp_vc_msg(msgp);
#endif

    return ret;
}

// Allocate memory on PAGE_SIZE boundary, return handle
uint32_t alloc_vc_mem(int fd, uint32_t size, VC_ALLOC_FLAGS flags)
{
    debug("alloc vc mem\n");
    VC_MSG msg = {
        .tag  = 0x3000c,
        .blen = 12,
        .dlen = 12,
        .uints = {
            PAGE_ROUNDUP(size),
            PAGE_SIZE,
            flags
        }
    };
    return msg_mbox(fd, &msg);
}

// Lock allocated memory, return bus address
void *lock_vc_mem(int fd, int h)
{
    debug("lock vc mem\n");
    VC_MSG msg = {
		.tag  = 0x3000d,
        .blen = 4,
        .dlen = 4,
        .uints = {
            h
        }
    };

    // msg_box returns uint32_t that we cast into a (void *) which is 64 bits
    // because msg_box returns a bus address which is 32 bits even on 64 bits
    // this below... gets rid of the warning... even though it's not pretty

    // FIXME get rid of this
    void *r = h ? (void*)((size_t)0 + msg_mbox(fd, &msg)) : 0;
    debug(" -> %p", r);
    return r;
}

// Unlock allocated memory
uint32_t unlock_vc_mem(int fd, int h)
{
    debug("unlock vc mem\n");
    VC_MSG msg = {
        .tag  = 0x3000e,
        .blen = 4,
        .dlen = 4,
        .uints = {
            h
        }
    };
    return h ? msg_mbox(fd, &msg) : 0;
}

// Free memory
uint32_t free_vc_mem(int fd, int h)
{
    debug("free vc mem\n");
    VC_MSG msg = {
        .tag  = 0x3000f,
        .blen = 4,
        .dlen = 4,
        .uints = {
            h
        }
    };
    return h ? msg_mbox(fd, &msg) : 0;
}

// Set VC clock
uint32_t set_vc_clock(int fd, int id, uint32_t freq)
{
    VC_MSG msg1 = {
        .tag  = 0x38001,
        .blen = 8,
        .dlen = 8,
        .uints = {
            id,
            1
        }
    };
    msg_mbox(fd, &msg1);

    VC_MSG msg2 = {
        .tag  = 0x38002,
        .blen = 12,
        .dlen = 12,
        .uints = {
            id,
            freq,
            0
        }
    };
    msg_mbox(fd, &msg2);

    return 0;
}

// Display mailbox message
void disp_vc_msg(VC_MSG *msgp)
{
    int i;

    printf("VC msg len=%X, req=%X, tag=%X, blen=%x, dlen=%x, data ",
        msgp->len, msgp->req, msgp->tag, msgp->blen, msgp->dlen);
    for (i=0; i<msgp->blen/4; i++)
        printf("%08X ", msgp->uints[i]);
    printf("\n");
}

// Allocate uncached memory, get bus & phys addresses
void *map_uncached_mem(MEM_MAP *mp, int size)
{
    void *ret;
    mp->size = PAGE_ROUNDUP(size);
    mp->fd = open_mbox();
    ret = (mp->h = alloc_vc_mem(mp->fd, mp->size, DMA_MEM_FLAGS)) > 0 &&
        (mp->bus = lock_vc_mem(mp->fd, mp->h)) != 0 &&
        (mp->virt = map_segment(BUS_PHYS_ADDR(mp->bus), mp->size)) != 0
        ? mp->virt : 0;
    printf("VC mem handle %u, phys %p, virt %p\n", mp->h, mp->bus, mp->virt);
    return ret;
}

void unmap_mem(MEM_MAP *mp)
{
    unmap_segment(mp->virt, mp->size);
    unlock_vc_mem(mp->fd, mp->h);
    free_vc_mem(mp->fd, mp->h);
    close_mbox(mp->fd);
}

// eof