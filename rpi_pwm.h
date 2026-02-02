//

extern MEM_MAP pwm_regs;

// PWM controller registers
#define PWM_BASE        (PHYS_REG_BASE + 0x20C000)
#define PWM_CTL         0x00   // Control
#define PWM_STA         0x04   // Status
#define PWM_DMAC        0x08   // DMA control
#define PWM_RNG1        0x10   // Channel 1 range
#define PWM_DAT1        0x14   // Channel 1 data
#define PWM_FIF1        0x18   // Channel 1 fifo
#define PWM_RNG2        0x20   // Channel 2 range
#define PWM_DAT2        0x24   // Channel 2 data

// PWM register values
#define PWM_CTL_RPTL1   (1<<2)  // Chan 1: repeat last data when FIFO empty
#define PWM_CTL_USEF1   (1<<5)  // Chan 1: use FIFO
#define PWM_DMAC_ENAB   (1<<31) // Start PWM DMA
#define PWM_ENAB        1       // Enable PWM
#define PWM_PIN         12      // GPIO pin for PWM output, 12 or 18

void *map_pwm();
void unmap_pwm();

void init_pwm(int freq, int range, int val);
void start_pwm(void);
void stop_pwm(void);

// eof