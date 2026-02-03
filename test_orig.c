// minimal DMA test

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define PHYS_REG_BASE    0x3F000000
#define BUS_REG_BASE     0x7E000000

#define DEBUG            1
#define USE_VC_CLOCK_SET 0

#define PAGE_SIZE        0x1000
#define PAGE_ROUNDUP(n)  ((n)%PAGE_SIZE==0 ? (n) : ((n)+PAGE_SIZE)&~(PAGE_SIZE-1))
#define DMA_MEM_SIZE     PAGE_SIZE

#if DEBUG
#define debug printf
#else
#define debug(...)
#endif

void *virt_dma_regs;
int mbox_fd, dma_mem_h;
void *bus_dma_mem;

#define BUS_PHYS_ADDR(a) ((void *)((size_t)(a) & ~0xC0000000))

typedef enum {
    MEM_FLAG_DISCARDABLE    = 1<<0, // can be resized to 0 at any time. Use for cached data
    MEM_FLAG_NORMAL         = 0<<2, // normal allocating alias. Don't use from ARM
    MEM_FLAG_DIRECT         = 1<<2, // 0xC alias uncached
    MEM_FLAG_COHERENT       = 2<<2, // 0x8 alias. Non-allocating in L2 but coherent
    MEM_FLAG_ZERO           = 1<<4, // initialise buffer to all zeros
    MEM_FLAG_NO_INIT        = 1<<5, // don't initialise (default is initialise to all ones)
    MEM_FLAG_HINT_PERMALOCK = 1<<6, // Likely to be locked for long periods of time
    MEM_FLAG_L1_NONALLOCATING=(MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) // Allocating in L2
} VC_ALLOC_FLAGS;

#define DMA_MEM_FLAGS (MEM_FLAG_DIRECT|MEM_FLAG_ZERO)

typedef struct {
    uint32_t len,   // Overall length (bytes)
        req,        // Zero for request, 1<<31 for response
        tag,        // Command number
        blen,       // Buffer length (bytes)
        dlen;       // Data length (bytes)
        uint32_t uints[32-5];   // Data (108 bytes maximum)
} VC_MSG __attribute__ ((aligned (16)));

#define DMA_CHAN        5
#define DMA_PWM_DREQ    5
#define DMA_BASE        (PHYS_REG_BASE + 0x007000)
#define DMA_CS          (DMA_CHAN*0x100)
#define DMA_CONBLK_AD   (DMA_CHAN*0x100 + 0x04)
#define DMA_TI          (DMA_CHAN*0x100 + 0x08)
#define DMA_SRCE_AD     (DMA_CHAN*0x100 + 0x0c)
#define DMA_DEST_AD     (DMA_CHAN*0x100 + 0x10)
#define DMA_TXFR_LEN    (DMA_CHAN*0x100 + 0x14)
#define DMA_STRIDE      (DMA_CHAN*0x100 + 0x18)
#define DMA_NEXTCONBK   (DMA_CHAN*0x100 + 0x1c)
#define DMA_DEBUG       (DMA_CHAN*0x100 + 0x20)
#define DMA_ENABLE      0xff0
#define VIRT_DMA_REG(a) ((volatile uint32_t *)((size_t)virt_dma_regs + (size_t)a))
char *dma_regstrs[] = {"DMA CS", "CB_AD", "TI", "SRCE_AD", "DEST_AD",
    "TFR_LEN", "STRIDE", "NEXT_CB", "DEBUG", ""};

// DMA control block (must be 32-byte aligned)
typedef struct {
    uint32_t ti,    // Transfer info
        srce_ad,    // Source address
        dest_ad,    // Destination address
        tfr_len,    // Transfer length
        stride,     // Transfer stride
        next_cb,    // Next control block
        debug,      // Debug register, zero in control block
        unused;
} DMA_CB __attribute__ ((aligned(32)));
#define DMA_CB_DEST_INC (1<<4)
#define DMA_CB_SRCE_INC (1<<8)

void *virt_dma_mem;

#define BUS_DMA_MEM(a)  ((size_t)a-(size_t)virt_dma_mem+(size_t)bus_dma_mem)

#define fail(x) {printf(x); terminate(0);}

void terminate(int sig);

int dma_test_mem_transfer(void);

int open_mbox(void);
void close_mbox(int fd);
uint32_t msg_mbox(int fd, VC_MSG *msgp);
void *map_segment(void *addr, int size);
void unmap_segment(void *addr, int size);
uint32_t alloc_vc_mem(int fd, uint32_t size, VC_ALLOC_FLAGS flags);
void *lock_vc_mem(int fd, int h);
uint32_t unlock_vc_mem(int fd, int h);
uint32_t free_vc_mem(int fd, int h);
uint32_t set_vc_clock(int fd, int id, uint32_t freq);
void disp_vc_msg(VC_MSG *msgp);
void enable_dma(void);
void start_dma(DMA_CB *cbp);
void stop_dma(void);
void disp_dma(void);

// Main program
int main(int argc, char *argv[])
{
    printf("configure\n");

    // cleanup if user hits ctrl-C
    signal(SIGINT, terminate);

    // map registers into user space virtual mem
    virt_dma_regs  = map_segment((void *)DMA_BASE, PAGE_SIZE);

    // enable dma
    enable_dma();

    // get uncached memory for DMA decriptors and buffers
    mbox_fd = open_mbox();
    if ((dma_mem_h = alloc_vc_mem(mbox_fd, DMA_MEM_SIZE, DMA_MEM_FLAGS)) <= 0 ||
        (bus_dma_mem = lock_vc_mem(mbox_fd, dma_mem_h)) == 0 ||
        (virt_dma_mem = map_segment(BUS_PHYS_ADDR(bus_dma_mem), DMA_MEM_SIZE)) == 0)
            fail("ERROR: can't allocate uncached memory\n");
    printf("vc mem handle=%u, phys=%p, virt=%p\n", dma_mem_h, bus_dma_mem, virt_dma_mem);

    // run tests
    printf("run tests\n");
    dma_test_mem_transfer();

    // over and out
    printf("terminate\n");
    terminate(0);
}

// DMA memory-to-memory test
int dma_test_mem_transfer(void)
{
    printf("test mem transfer\n");
    DMA_CB *cbp = virt_dma_mem;
    char *srce = (char *)(cbp+1);
    char *dest = srce + 0x100;

    strxcpy("memory transfer OK", srce, 20);
    memset(cbp, 0, sizeof(DMA_CB));
    cbp->ti = DMA_CB_SRCE_INC | DMA_CB_DEST_INC;
    cbp->srce_ad = BUS_DMA_MEM(srce);
    cbp->dest_ad = BUS_DMA_MEM(dest);
    cbp->tfr_len = strlen(srce) + 1;
    start_dma(cbp);
    usleep(10);

    disp_dma();

    printf("copy\n");
    char ddest[128];
    strxcpy(dest, ddest, 128);

    printf("result\n");
    printf("DMA test: %s\n", ddest[0] ? ddest : "failed");
    return(ddest[0] != 0);
}

// Free memory segments and exit
void terminate(int sig)
{
    printf("closing\n");

    stop_dma();
    unmap_segment(virt_dma_mem, DMA_MEM_SIZE);

    unlock_vc_mem(mbox_fd, dma_mem_h);
    free_vc_mem(mbox_fd, dma_mem_h);
    close_mbox(mbox_fd);
    unmap_segment(virt_dma_regs, PAGE_SIZE);
    exit(0);
}

// ----- VIDEOCORE MAILBOX -----

// Open mailbox interface, return file descriptor
int open_mbox(void)
{
   int fd;

   if ((fd = open("/dev/vcio", 0)) < 0)
       fail("error: can't open VC mailbox\n");
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

    return h ? (void*)((size_t)0 + msg_mbox(fd, &msg)) : 0;
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

// ----- VIRTUAL MEMORY -----

// Get virtual memory segment for peripheral regs or physical mem
void *map_segment(void *addr, int size)
{
    int fd;
    void *mem;

    size = PAGE_ROUNDUP(size);
    debug("mapping %d at %p\n", size, (void *)addr);

    if ((fd = open ("/dev/mem", O_RDWR|O_SYNC|O_CLOEXEC)) < 0)
        fail("error: can't open /dev/mem, run using sudo\n");

    mem = mmap(
        0, // any address in our space will do
        size, // block size
        PROT_WRITE|PROT_READ, // enable reading and writing
        MAP_SHARED, // shared with other processes
        fd, // file descriptor -> /dev/mem
        (size_t) addr // offset
    );

    close(fd);

    debug("  mapped %p -> %p\n", (void *)addr, mem);

    if (mem == MAP_FAILED)
        fail("error: can't map memory\n");

    return mem;
}

// Free mapped memory
void unmap_segment(void *mem, int size)
{
    if (mem)
        munmap(mem, PAGE_ROUNDUP(size));
}

// ----- DMA -----

// Enable and reset DMA
void enable_dma(void)
{
    debug("enable dma\n");
    *VIRT_DMA_REG(DMA_ENABLE) |= (1 << DMA_CHAN);
    *VIRT_DMA_REG(DMA_CS) = 1 << 31;
    debug("  enabled\n");
}

// Start DMA, given first control block
void start_dma(DMA_CB *cbp)
{
    debug("start dma\n");
    *VIRT_DMA_REG(DMA_CONBLK_AD) = BUS_DMA_MEM(cbp);
    *VIRT_DMA_REG(DMA_CS) = 2;       // Clear 'end' flag
    *VIRT_DMA_REG(DMA_DEBUG) = 7;    // Clear error bits
    *VIRT_DMA_REG(DMA_CS) = 1;       // Start DMA
    debug("  started\n");
}

// Halt current DMA operation by resetting controller
void stop_dma(void)
{
    debug("stop dma\n");
    if (virt_dma_regs)
        *VIRT_DMA_REG(DMA_CS) = 1 << 31;
}

// Display DMA registers
void disp_dma(void)
{
    size_t *p=(size_t *)VIRT_DMA_REG(DMA_CS);
    int i=0;

    while (dma_regstrs[i][0])
    {
        printf("%-7s %08lX ", dma_regstrs[i++], *p++);
        if (i%5==0 || dma_regstrs[i][0]==0)
            printf("\n");
    }

    printf(".\n");
}

// EOF
