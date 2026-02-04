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
#include "rpi_smi.h"

MEM_MAP smi_regs;

// Pointers to SMI registers
volatile SMI_CS_REG  *smi_cs;
volatile SMI_L_REG   *smi_l;
volatile SMI_A_REG   *smi_a;
volatile SMI_D_REG   *smi_d;
volatile SMI_DMC_REG *smi_dmc;
volatile SMI_DSR_REG *smi_dsr;
volatile SMI_DSW_REG *smi_dsw;
volatile SMI_DCS_REG *smi_dcs;
volatile SMI_DCA_REG *smi_dca;
volatile SMI_DCD_REG *smi_dcd;

void *map_smi() {
    if (map_periph(&smi_regs, (void *)SMI_BASE, PAGE_SIZE) == 0)
        printf("error: failed to map smi registers\n");
    return smi_regs.virt;
}

void unmap_smi() {
    if (smi_regs.virt != 0)
        unmap_periph(&smi_regs);
}

// Initialise SMI, given data width, time step, and setup/hold/strobe counts
// Step value is in nanoseconds: even numbers, 2 to 30
void init_smi(int width, int ns, int setup, int strobe, int hold)
{
    int i, divi = ns / 2;

    smi_cs  = (SMI_CS_REG *) REG32(smi_regs, SMI_CS);
    smi_l   = (SMI_L_REG *)  REG32(smi_regs, SMI_L);
    smi_a   = (SMI_A_REG *)  REG32(smi_regs, SMI_A);
    smi_d   = (SMI_D_REG *)  REG32(smi_regs, SMI_D);
    smi_dmc = (SMI_DMC_REG *)REG32(smi_regs, SMI_DMC);
    smi_dsr = (SMI_DSR_REG *)REG32(smi_regs, SMI_DSR0);
    smi_dsw = (SMI_DSW_REG *)REG32(smi_regs, SMI_DSW0);
    smi_dcs = (SMI_DCS_REG *)REG32(smi_regs, SMI_DCS);
    smi_dca = (SMI_DCA_REG *)REG32(smi_regs, SMI_DCA);
    smi_dcd = (SMI_DCD_REG *)REG32(smi_regs, SMI_DCD);
    smi_cs->value = smi_l->value = smi_a->value = 0;
    smi_dsr->value = smi_dsw->value = smi_dcs->value = smi_dca->value = 0;
    if (*REG32(clk_regs, CLK_SMI_DIV) != divi << 12)
    {
        *REG32(clk_regs, CLK_SMI_CTL) = CLK_PASSWD | (1 << 5);
        usleep(10);
        while (*REG32(clk_regs, CLK_SMI_CTL) & (1 << 7)) ;
        usleep(10);
        *REG32(clk_regs, CLK_SMI_DIV) = CLK_PASSWD | (divi << 12);
        usleep(10);
        *REG32(clk_regs, CLK_SMI_CTL) = CLK_PASSWD | 6 | (1 << 4);
        usleep(10);
        while ((*REG32(clk_regs, CLK_SMI_CTL) & (1 << 7)) == 0) ;
        usleep(100);
    }
    if (smi_cs->seterr)
        smi_cs->seterr = 1;
    smi_dsr->rsetup = smi_dsw->wsetup = setup;
    smi_dsr->rstrobe = smi_dsw->wstrobe = strobe;
    smi_dsr->rhold = smi_dsw->whold = hold;
    smi_dmc->panicr = smi_dmc->panicw = 8;
    smi_dmc->reqr = smi_dmc->reqw = REQUEST_THRESH;
    smi_dsr->rwidth = smi_dsw->wwidth = width;
    for (i=0; i<LED_NCHANS; i++)
        gpio_mode(LED_D0_PIN+i, GPIO_ALT1);
}

// Set up SMI transfers using DMA
void setup_smi_dma(MEM_MAP *mp, int chan, int nsamp)
{
    DMA_CB *cbs=mp->virt;

    txdata = (TXDATA_T *)(cbs+1);
    smi_dmc->dmaen = 1;
    smi_cs->enable = 1;
    smi_cs->clear = 1;
    smi_cs->pxldat = 1;
    smi_l->len = nsamp * sizeof(TXDATA_T);
    smi_cs->write = 1;
    enable_dma(chan);
    cbs[0].ti = DMA_DEST_DREQ | (DMA_SMI_DREQ << 16) | DMA_CB_SRCE_INC | DMA_WAIT_RESP;
    cbs[0].tfr_len = nsamp * sizeof(TXDATA_T);
    cbs[0].srce_ad = MEM_BUS_ADDR(mp, txdata);
    cbs[0].dest_ad = REG_BUS_ADDR(smi_regs, SMI_D);
}

// Start SMI DMA transfers
void start_smi(MEM_MAP *mp, int chan)
{
    DMA_CB *cbs=mp->virt;

    start_dma(mp, chan, &cbs[0], 0);
    smi_cs->start = 1;
}

// eof