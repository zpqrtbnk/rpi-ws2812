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
#include "rpi_gpio.h"

MEM_MAP gpio_regs;

char *gpio_mode_strs[] = {GPIO_MODE_STRS};

void *map_gpio() {
    if (map_periph(&gpio_regs, (void *)GPIO_BASE, PAGE_SIZE) == 0)
        printf("error: failed to map gpio registers\n");
    return gpio_regs.virt;
}

void unmap_gpio() {
    if (gpio_regs.virt != 0)
        unmap_periph(&gpio_regs);
}

// Set input or output with pullups
void gpio_set(int pin, int mode, int pull)
{
    gpio_mode(pin, mode);
    gpio_pull(pin, pull);
}

// Set ping pullup or pulldown
void gpio_pull(int pin, int pull)
{
    volatile uint32_t *reg = REG32(gpio_regs, GPIO_GPPUDCLK0) + pin / 32;
    debug("set gpio pin %d pull %d (%p)\n", pin, pull, reg);

    *REG32(gpio_regs, GPIO_GPPUD) = pull;
    usleep(2);
    *reg = 1 << (pin % 32);
    usleep(2);
    *REG32(gpio_regs, GPIO_GPPUD) = 0;
    *reg = 0;
}

// Set input or output
void gpio_mode(int pin, int mode)
{
    volatile uint32_t *reg = REG32(gpio_regs, GPIO_MODE0) + pin / 10;
    debug("set gpio pin %d mode %d (%p)\n", pin, mode, reg);

    uint32_t shift = (pin % 10) * 3;
    *reg = (*reg & ~(7 << shift)) | (mode << shift);
}

// Set an O/P pin
void gpio_out(int pin, int val)
{
    volatile uint32_t *reg = REG32(gpio_regs, val ? GPIO_SET0 : GPIO_CLR0) + pin/32;
    debug("set gpio pin %d value %d (%p)\n", pin, val, reg);

    *reg = 1 << (pin % 32);
}

// Get an I/P pin value
uint8_t gpio_in(int pin)
{
    volatile uint32_t *reg = REG32(gpio_regs, GPIO_LEV0) + pin/32;
    uint8_t val = (((*reg) >> (pin % 32)) & 1);
    debug("get gpio pin %d value %d (%p)\n", pin, val, reg);
    
    return val;
}

// Display the values in a GPIO mode register
void disp_mode_vals(uint32_t mode)
{
    int i;

    for (i=0; i<10; i++)
        printf("%u:%-4s ", i, gpio_mode_strs[(mode>>(i*3)) & 7]);
    printf("\n");
}

// eof