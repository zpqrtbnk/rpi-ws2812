// Raspberry Pi WS2812 LED driver using SMI
// For detailed description, see https://iosoft.blog
//
// Copyright (c) 2020 Jeremy P Bentham
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// v0.01 JPB 16/7/20 Adapted from rpi_smi_adc_test v0.06
// v0.02 JPB 15/9/20 Addded RGB to GRB conversion
// v0.03 JPB 15/9/20 Added red-green flashing
// v0.04 JPB 16/9/20 Added test mode
// v0.05 JPB 19/9/20 Changed test mode colours
// v0.06 JPB 20/9/20 Outlined command-line data input
// v0.07 JPB 25/9/20 Command-line data input if not in test mode
// v0.08 JPB 26/9/20 Changed from 4 to 3 pulses per LED bit
//                   Added 4-bit zero preamble
//                   Added raw Tx data test
// v0.09 JPB 27/9/20 Added 16-channel option
// v0.10 JPB 28/9/20 Corrected Pi Zero caching problem
// v0.11 JPB 29/9/20 Added enable_dma before transfer (in case still active)
//                   Corrected DMA nsamp value (was byte count)
// v0.12 JPB 26/5/21 Corrected transfer length for 16-bit mode

#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "utils.h"

#include "rpi/rpi_lib.h"
#include "rpi/rpi_gpio.h"
#include "rpi/rpi_smi.h"
#include "rpi/rpi_pwm.h"
#include "rpi/rpi_vc.h"
#include "rpi/rpi_vm.h"
#include "rpi/rpi_dma.h"

// 

// get a 400ns cycle time
#if WHAT_PI == PI_Z2
// assume 1.5 GHz (what fq is that?)
#define SMI_TIMING 10, 15, 30, 15
#else
// assume 1 GHz
//#define SMI_TIMING 10, 10, 20, 10
#error meh
#endif

// FIXME adjust to ACTUAL pi model!
// #if PHYS_REG_BASE==PI_4_REG_BASE        // Timings for RPi v4 (1.5 GHz)
// #define SMI_TIMING       10, 15, 30, 15    // 400 ns cycle time
// #else                                   // Timings for RPi v0-3 (1 GHz)
// #define SMI_TIMING       10, 10, 20, 10   // 400 ns cycle time
// #endif

#define TX_TEST         0   // If non-zero, use dummy Tx data
#define LED_D0_PIN      8   // GPIO pin for D0 output
#define LED_NCHANS      8   // Number of LED channels (8 or 16)
#define LED_NBITS       24  // Number of data bits per LED
#define LED_PREBITS     4   // Number of zero bits before LED data
#define LED_POSTBITS    4   // Number of zero bits after LED data
#define BIT_NPULSES     3   // Number of O/P pulses per LED bit
#define CHAN_MAXLEDS    50  // Maximum number of LEDs per channel
#define CHASE_MSEC      100 // Delay time for chaser light test
#define REQUEST_THRESH  2   // DMA request threshold
#define DMA_CHAN        10  // DMA channel to use

// Length of data for 1 row (1 LED on each channel)
#define LED_DLEN        (LED_NBITS * BIT_NPULSES)

// Transmit data type, 8 or 16 bits
#if LED_NCHANS > 8
#define TXDATA_T        uint16_t
#else
#define TXDATA_T        uint8_t
#endif

MEM_MAP vc_mem;

// Ofset into Tx data buffer, given LED number in chan
#define LED_TX_OSET(n)      (LED_PREBITS + (LED_DLEN * (n)))

// Size of data buffers & NV memory, given number of LEDs per chan
#define TX_BUFF_LEN(n)      (LED_TX_OSET(n) + LED_POSTBITS)
#define TX_BUFF_SIZE(n)     (TX_BUFF_LEN(n) * sizeof(TXDATA_T))
#define VC_MEM_SIZE         (PAGE_SIZE + TX_BUFF_SIZE(CHAN_MAXLEDS))

// RGB values for test mode (1 value for each of 16 channels)
int on_rgbs[16] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff,
                  0xff4040, 0x40ff40, 0x4040ff, 0x404040,
                  0xff0000, 0x00ff00, 0x0000ff, 0xffffff,
                  0xff4040, 0x40ff40, 0x4040ff, 0x404040};
int off_rgbs[16];

#if TX_TEST
// Data for simple transmission test
TXDATA_T tx_test_data[] = {1, 2, 3, 4, 5, 6, 7, 0};
#endif

TXDATA_T *txdata;                       // Pointer to uncached Tx data buffer
TXDATA_T tx_buffer[TX_BUFF_LEN(CHAN_MAXLEDS)];  // Tx buffer for assembling data

int testmode, chan_ledcount=1;          // Command-line parameters
int rgb_data[CHAN_MAXLEDS][LED_NCHANS]; // RGB data
int chan_num;                           // Current channel for data I/P

#define fail(x) {printf(x); terminate(0);}

void terminate(int sig);

void rgb_txdata(int *rgbs, TXDATA_T *txd);
int str_rgb(char *s, int rgbs[][LED_NCHANS], int chan);
void map_devices(void);
void init_smi(int width, int ns, int setup, int hold, int strobe);
void setup_smi_dma(MEM_MAP *mp, int chan, int nsamp);
void start_smi(MEM_MAP *mp, int chan);

int main(int argc, char *argv[])
{
    int args=0, n, oset=0;

    while (argc > ++args)               // Process command-line args
    {
        if (argv[args][0] == '-')
        {
            switch (toupper(argv[args][1]))
            {
            case 'N':                   // -N: number of LEDs per channel
                if (args >= argc-1)
                    fprintf(stderr, "Error: no numeric value\n");
                else
                    chan_ledcount = atoi(argv[++args]);
                break;
            case 'T':                   // -T: test mode
                testmode = 1;
                break;
            default:                    // Otherwise error
                printf("Unrecognised option '%c'\n", argv[args][1]);
                printf("Options:\n"
                       "  -n num    number of LEDs per channel\n"\
                       "  -t        Test mode (flash LEDs)\n"\
                      );
                return(1);
            }
        }
        else if (chan_num<LED_NCHANS && hexdig(argv[args][0])>=0 &&
                 (n=str_rgb(argv[args], rgb_data, chan_num))>0)
        {
            chan_ledcount = n > chan_ledcount ? n : chan_ledcount;
            chan_num++;
        }
    }

    signal(SIGINT, terminate);

    if (map_periph(&gpio_regs, (void *)GPIO_BASE, PAGE_SIZE) == 0) fail("oops\n");
    if (map_periph(&dma_regs,  (void *)DMA_BASE, PAGE_SIZE) == 0) fail("oops\n");
    if (map_periph(&clk_regs,  (void *)CLK_BASE, PAGE_SIZE) == 0) fail("oops\n");
    if (map_periph(&smi_regs,  (void *)SMI_BASE, PAGE_SIZE) == 0) fail("oops\n");

    init_smi(LED_NCHANS>8 ? SMI_16_BITS : SMI_8_BITS, SMI_TIMING);
    map_uncached_mem(&vc_mem, VC_MEM_SIZE);

#if TX_TEST
    oset = oset;
    setup_smi_dma(&vc_mem, DMA_CHAN, sizeof(tx_test_data)/sizeof(TXDATA_T));
#if LED_NCHANS <= 8
    swap_bytes(tx_test_data, sizeof(tx_test_data));
#endif
    memcpy(txdata, tx_test_data, sizeof(tx_test_data));
    start_smi(&vc_mem, DMA_CHAN);
    usleep(10);
    while (dma_active(DMA_CHAN))
        usleep(10);
#else
    setup_smi_dma(&vc_mem, DMA_CHAN, TX_BUFF_LEN(chan_ledcount));
    printf("%s %u LED%s per channel, %u channels\n", testmode ? "Testing" : "Setting",
           chan_ledcount, chan_ledcount==1 ? "" : "s", LED_NCHANS);
    if (testmode)
    {
        while (1)
        {
            if (chan_ledcount < 2)
                rgb_txdata(oset&1 ? off_rgbs : on_rgbs, tx_buffer);
            else
            {
                for (n=0; n<chan_ledcount; n++)
                {
                    rgb_txdata(n==oset%chan_ledcount ? on_rgbs : off_rgbs,
                               &tx_buffer[LED_TX_OSET(n)]);
                }
            }
            oset++;
#if LED_NCHANS <= 8
            swap_bytes(tx_buffer, TX_BUFF_SIZE(chan_ledcount));
#endif
            memcpy(txdata, tx_buffer, TX_BUFF_SIZE(chan_ledcount));
            start_smi(&vc_mem, DMA_CHAN);
            usleep(CHASE_MSEC * 1000);
        }
    }
    else
    {
        for (n=0; n<chan_ledcount; n++)
            rgb_txdata(rgb_data[n], &tx_buffer[LED_TX_OSET(n)]);
#if LED_NCHANS <= 8
        swap_bytes(tx_buffer, TX_BUFF_SIZE(chan_ledcount));
#endif
        memcpy(txdata, tx_buffer, TX_BUFF_SIZE(chan_ledcount));
        enable_dma(DMA_CHAN);
        start_smi(&vc_mem, DMA_CHAN);
        usleep(10);
        while (dma_active(DMA_CHAN))
            usleep(10);
    }
#endif
    terminate(0);
    return(0);
}

// Convert RGB text string into integer data, for given channel
// Return number of data points for this channel
int str_rgb(char *s, int rgbs[][LED_NCHANS], int chan)
{
    int i=0;
    char *p;

    while (chan<LED_NCHANS && i<CHAN_MAXLEDS && hexdig(*s)>=0)
    {
        rgbs[i++][chan] = strtoul(s, &p, 16);
        s = *p ? p+1 : p;
    }
    return(i);
}

// Set Tx data for 8 or 16 chans, 1 LED per chan, given 1 RGB val per chan
// Logic 1 is 0.8us high, 0.4 us low, logic 0 is 0.4us high, 0.8us low
void rgb_txdata(int *rgbs, TXDATA_T *txd)
{
    int i, n, msk;

    // For each bit of the 24-bit RGB values..
    for (n=0; n<LED_NBITS; n++)
    {
        // Mask to convert RGB to GRB, M.S bit first
        msk = n==0 ? 0x8000 : n==8 ? 0x800000 : n==16 ? 0x80 : msk>>1;
        // 1st byte or word is a high pulse on all lines
        txd[0] = (TXDATA_T)0xffff;
        // 2nd has high or low bits from data
        // 3rd is a low pulse
        txd[1] = txd[2] = 0;
        for (i=0; i<LED_NCHANS; i++)
        {
            if (rgbs[i] & msk)
                txd[1] |= (1 << i);
        }
        txd += BIT_NPULSES;
    }
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

// Free memory segments and exit
void terminate(int sig)
{
    int i;

    printf("closing\n");
    if (gpio_regs.virt)
    {
        for (i=0; i<LED_NCHANS; i++)
            gpio_mode(LED_D0_PIN+i, GPIO_IN);
    }
    if (smi_regs.virt)
        *REG32(smi_regs, SMI_CS) = 0;
    stop_dma(DMA_CHAN);

    unmap_mem(&vc_mem);

    unmap_periph(&smi_regs);
    unmap_periph(&dma_regs);
    unmap_periph(&clk_regs);
    unmap_periph(&gpio_regs);

    exit(0);
}

// EOF
