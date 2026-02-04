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
#include "rpi_dma.h"

MEM_MAP dma_regs;

char *dma_regstrs[] = {"DMA CS", "CB_AD", "TI", "SRCE_AD", "DEST_AD",
    "TFR_LEN", "STRIDE", "NEXT_CB", "DEBUG", ""};

void *map_dma() {
    if (map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE) == 0)
        printf("error: failed to map dma registers\n");
    return dma_regs.virt;
}

void unmap_dma() {
    if (dma_regs.virt != 0)
        unmap_periph(&dma_regs);
}

// Enable and reset DMA
void enable_dma(int chan)
{
    debug("dma chan=%d enable", chan);
    *REG32(dma_regs, DMA_ENABLE) |= (1 << chan);
    *REG32(dma_regs, DMA_REG(chan, DMA_CS)) = 1 << 31;
    debug(" -> enabled\n");
}

// Start DMA, given first control block
void start_dma(MEM_MAP *mp, int chan, DMA_CB *cbp, uint32_t csval)
{
    debug("DMA %d %x %x %d %d %d", cbp->ti, cbp->srce_ad, cbp->dest_ad, cbp->tfr_len, cbp->stride, cbp->next_cb);

    debug("dma chan=%d start", chan);
    *REG32(dma_regs, DMA_REG(chan, DMA_CONBLK_AD)) = MEM_BUS_ADDR(mp, cbp);
    *REG32(dma_regs, DMA_REG(chan, DMA_CS)) = 2;        // Clear 'end' flag
    *REG32(dma_regs, DMA_REG(chan, DMA_DEBUG)) = 7;     // Clear error bits
    *REG32(dma_regs, DMA_REG(chan, DMA_CS)) = 1|csval;  // Start DMA
    debug(" -> started\n");
}

// Return remaining transfer length
uint32_t dma_transfer_len(int chan)
{
    uint32_t val = *REG32(dma_regs, DMA_REG(chan, DMA_TXFR_LEN));
    debug("dma chan=%d remain=%d\n", chan, val);
    return val;
}

// Check if DMA is active
uint32_t dma_active(int chan)
{
    uint32_t val = (*REG32(dma_regs, DMA_REG(chan, DMA_CS))) & 1;
    debug("dma chan=%d active=%d\n", chan, val);
    return val;
}

// Halt current DMA operation by resetting controller
void stop_dma(int chan)
{
    debug("dma chan=%d stop", chan);
    if (dma_regs.virt)
        *REG32(dma_regs, DMA_REG(chan, DMA_CS)) = 1 << 31;
    debug(" -> stopped\n");
}

// Display DMA registers
void disp_dma(int chan)
{
    volatile uint32_t *p = REG32(dma_regs, DMA_REG(chan, DMA_CS));
    int i=0;

    while (dma_regstrs[i][0])
    {
        printf("%-7s %08X ", dma_regstrs[i++], *p++);
        if (i%5==0 || dma_regstrs[i][0]==0)
            printf("\n");
    }
}

// eof