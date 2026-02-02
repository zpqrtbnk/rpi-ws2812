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
#include "rpi_pwm.h"

MEM_MAP pwm_regs;

void *map_pwm() {
    if (map_periph(&pwm_regs, (void *)PWM_BASE, PAGE_SIZE) == 0)
        printf("error: failed to map pwm registers\n");
    return pwm_regs.virt;
}

void unmap_pwm() {
    if (pwm_regs.virt != 0)
        unmap_periph(&pwm_regs);
}

// Initialise PWM
void init_pwm(int freq, int range, int val)
{
    stop_pwm();
    if (*REG32(pwm_regs, PWM_STA) & 0x100)
    {
        printf("PWM bus error\n");
        *REG32(pwm_regs, PWM_STA) = 0x100;
    }
#if USE_VC_CLOCK_SET
    set_vc_clock(mbox_fd, PWM_CLOCK_ID, freq);
#else
    int divi=CLOCK_HZ / freq;
    *REG32(clk_regs, CLK_PWM_CTL) = CLK_PASSWD | (1 << 5);
    while (*REG32(clk_regs, CLK_PWM_CTL) & (1 << 7)) ;
    *REG32(clk_regs, CLK_PWM_DIV) = CLK_PASSWD | (divi << 12);
    *REG32(clk_regs, CLK_PWM_CTL) = CLK_PASSWD | 6 | (1 << 4);
    while ((*REG32(clk_regs, CLK_PWM_CTL) & (1 << 7)) == 0) ;
#endif
    usleep(100);
    *REG32(pwm_regs, PWM_RNG1) = range;
    *REG32(pwm_regs, PWM_FIF1) = val;
#if PWM_OUT
    gpio_mode(PWM_PIN, PWM_PIN==12 ? GPIO_ALT0 : GPIO_ALT5);
#endif
}

// Start PWM operation
void start_pwm(void)
{
    *REG32(pwm_regs, PWM_CTL) = PWM_CTL_USEF1 | PWM_ENAB;
}

// Stop PWM operation
void stop_pwm(void)
{
    if (pwm_regs.virt)
    {
        *REG32(pwm_regs, PWM_CTL) = 0;
        usleep(100);
    }
}

// eof