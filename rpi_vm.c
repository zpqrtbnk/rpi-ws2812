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
#include "rpi_vm.h"

// Get virtual memory segment for peripheral regs or physical mem
void *map_segment(void *addr, int size)
{
    int fd;
    void *mem;

    size = PAGE_ROUNDUP(size);
    debug("mapping %d at %p", size, (void *)addr);

    if ((fd = open ("/dev/mem", O_RDWR|O_SYNC|O_CLOEXEC)) < 0)
    {
        debug("\n");
        printf("error: can't open /dev/mem, are you root?\n");
        return 0;
    }

    mem = mmap(
        0, // any address in our space will do
        size, // block size
        PROT_WRITE|PROT_READ, // enable reading and writing
        MAP_SHARED, // shared with other processes
        fd, // file descriptor -> /dev/mem
        (size_t) addr // offset
    );

    close(fd);

    if (mem == MAP_FAILED)
    {
        debug("\n");
        printf("error: failed to map memory\n");
        return 0;
    }

    debug(" -> %p\n", mem);

    return mem;
}

// Free mapped memory
void unmap_segment(void *mem, int size)
{
    if (mem)
        munmap(mem, PAGE_ROUNDUP(size));
}

// eof