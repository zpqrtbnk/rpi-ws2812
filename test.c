
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
void dma_test_led_flash(int pin);
void dma_test_pwm_trigger(int pin);

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
    if (map_gpio() == 0) fail("oops\n");
    if (map_pwm()  == 0) fail("oops\n");
    if (map_dma()  == 0) fail("oops\n");
    if (map_clk()  == 0) fail("oops\n");
    if (map_smi()  == 0) fail("oops\n");

    // enable dma
    enable_dma(DMA_CHAN);

    // set LED pin as output, pull high
    gpio_set(LED_PIN, GPIO_OUT, 1);

    // light LED once
    gpio_out(LED_PIN, 1);
    sleep(2);
    gpio_out(LED_PIN, 0);

    // get uncached memory for DMA decriptors and buffers
    if (map_uncached_mem(&dma_mem, DMA_MEM_SIZE) == 0) fail ("oops\n");

    // run tests
    printf("run tests\n");
    if (dma_test_mem_transfer() == 0) fail("oops\n"); // FIXME that fails
    dma_test_led_flash(LED_PIN);
    dma_test_pwm_trigger(LED_PIN);

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

// DMA memory-to-GPIO test: flash LED
void dma_test_led_flash(int pin)
{
    printf("test led flash\n");

    DMA_CB *cbp= dma_mem.virt;
    size_t *data = (size_t *)(cbp+1), n;

    printf("DMA test: flashing LED on GPIO pin %u\n", pin);
    memset(cbp, 0, sizeof(DMA_CB));
    *data = 1 << pin;
    cbp->tfr_len = 4;
    cbp->srce_ad = MEM_BUS_ADDR((&dma_regs), data);
    for (n=0; n<16; n++)
    {
        usleep(200000);
        cbp->dest_ad = REG_BUS_ADDR(gpio_regs, n&1 ? GPIO_CLR0 : GPIO_SET0);
        start_dma(&dma_regs, DMA_CHAN, cbp, 0);
    }
}

// DMA trigger test: fLash LED using PWM trigger
void dma_test_pwm_trigger(int pin)
{
    printf("test pwm trigger\n");

    DMA_CB *cbs= dma_mem.virt;
    size_t n, *pindata=(size_t *)(cbs+4), *pwmdata=pindata+1;

    printf("DMA test: PWM trigger, ctrl-C to exit\n");
    memset(cbs, 0, sizeof(DMA_CB)*4);
    // Transfers are triggered by PWM request
    cbs[0].ti = cbs[1].ti = cbs[2].ti = cbs[3].ti = (1 << 6) | (DMA_PWM_DREQ << 16);
    // Control block 0 and 2: clear & set LED pin, 4-byte transfer
    cbs[0].srce_ad = cbs[2].srce_ad = MEM_BUS_ADDR(&dma_regs, pindata);
    cbs[0].dest_ad = REG_BUS_ADDR(gpio_regs, GPIO_CLR0);
    cbs[2].dest_ad = REG_BUS_ADDR(gpio_regs, GPIO_SET0);
    cbs[0].tfr_len = cbs[2].tfr_len = 4;
    *pindata = 1 << pin;
    // Control block 1 and 3: update PWM FIFO (to clear DMA request)
    cbs[1].srce_ad = cbs[3].srce_ad = MEM_BUS_ADDR(&dma_regs, pwmdata);
    cbs[1].dest_ad = cbs[3].dest_ad = REG_BUS_ADDR(pwm_regs, PWM_FIF1);
    cbs[1].tfr_len = cbs[3].tfr_len = 4;
    *pwmdata = PWM_RANGE / 2;
    // Link control blocks 0 to 3 in endless loop
    for (n=0; n<4; n++)
        cbs[n].next_cb = MEM_BUS_ADDR(&dma_regs, &cbs[(n+1)%4]);
    // Enable PWM with data threshold 1, and DMA
    init_pwm(PWM_FREQ, PWM_RANGE, PWM_RANGE/2);
    *REG32(pwm_regs, PWM_DMAC) = PWM_DMAC_ENAB|1;
    start_pwm();
    start_dma(&dma_regs, DMA_CHAN, &cbs[0], 0);
    // Nothing to do while LED is flashing
    sleep(4);
}

// Free memory segments and exit
void terminate(int sig)
{
    printf("closing\n");

    stop_pwm();
    stop_dma(DMA_CHAN);

    unmap_dma();
    unmap_clk();
    unmap_pwm();
    unmap_gpio();
    unmap_smi();

    unmap_mem(&dma_mem);

    exit(0);
}
