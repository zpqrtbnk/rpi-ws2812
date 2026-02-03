
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "utils.h"

#include "rpi_lib.h"
#include "rpi_gpio.h"
#include "rpi_smi.h"
#include "rpi_pwm.h"
#include "rpi_vc.h"
#include "rpi_vm.h"
#include "rpi_dma.h"

#define fail(x) {printf(x); terminate(0);}

void terminate(int sig);

int dma_test_mem_transfer(void);

#define DMA_CHAN 5
#define LED_PIN 21
#define PWM_FREQ 100000
#define PWM_RANGE 20000

// Size of uncached memory for DMA control blocks and data
#define DMA_MEM_SIZE PAGE_SIZE

MEM_MAP dma_mem;

int main(int argc, char *argv[])
{
    printf("configure\n");

    // cleanup if user hits ctrl-C
    signal(SIGINT, terminate);

    // map registers into user space virtual mem
    if (map_dma()  == 0) fail("oops\n");

    // enable dma
    enable_dma(DMA_CHAN);

    // get uncached memory for DMA decriptors and buffers
    if (map_uncached_mem(&dma_mem, DMA_MEM_SIZE) == 0) fail ("oops\n");

    // run tests
    printf("run tests\n");
    if (dma_test_mem_transfer() == 0) fail("oops\n");

    // over and out
    printf("terminate\n");
    terminate(0);
}

// DMA memory-to-memory test
int dma_test_mem_transfer(void)
{
    printf("test mem transfer\n");
    DMA_CB *cbp = dma_mem.virt;
    char *srce = (char *)(cbp+1);
    char *dest = srce + 0x100;

    strxcpy("memory transfer OK", srce, 20);
    memset(cbp, 0, sizeof(DMA_CB));
    cbp->ti = DMA_CB_SRCE_INC | DMA_CB_DEST_INC;
    cbp->srce_ad = MEM_BUS_ADDR(&dma_regs, srce);
    cbp->dest_ad = MEM_BUS_ADDR(&dma_regs, dest);
    cbp->tfr_len = strlen(srce) + 1;
    start_dma(&dma_regs, DMA_CHAN, cbp, 0);
    usleep(10);

    disp_dma(DMA_CHAN);

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

    stop_dma(DMA_CHAN);

    unmap_dma();
    unmap_mem(&dma_mem);

    exit(0);
}
