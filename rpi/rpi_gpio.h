//

extern MEM_MAP gpio_regs;

// GPIO register definitions
#define GPIO_BASE       (PHYS_REG_BASE + 0x200000)
#define GPIO_MODE0      0x00
#define GPIO_SET0       0x1c
#define GPIO_CLR0       0x28
#define GPIO_LEV0       0x34
#define GPIO_GPPUD      0x94
#define GPIO_GPPUDCLK0  0x98

// GPIO I/O definitions
#define GPIO_IN         0
#define GPIO_OUT        1
#define GPIO_ALT0       4
#define GPIO_ALT1       5
#define GPIO_ALT2       6
#define GPIO_ALT3       7
#define GPIO_ALT4       3
#define GPIO_ALT5       2
#define GPIO_MODE_STRS  "IN","OUT","ALT5","ALT4","ALT0","ALT1","ALT2","ALT3"
#define GPIO_NOPULL     0
#define GPIO_PULLDN     1
#define GPIO_PULLUP     2

void *map_gpio();
void unmap_gpio();

void gpio_set(int pin, int mode, int pull);
void gpio_pull(int pin, int pull);
void gpio_mode(int pin, int mode);
void gpio_out(int pin, int val);
uint8_t gpio_in(int pin);
void disp_mode_vals(uint32_t mode);

//