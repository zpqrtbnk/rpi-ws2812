// Raspberry Pi DMA utility definitions; see https://iosoft.blog for details
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

#define PI_Z  0
#define PI_Z2 100
#define PI_1  1
#define PI_2  2
#define PI_3  3
#define PI_4  4
#define PI_5  5

// PI model
#define WHAT_PI PI_Z2

// If non-zero, print debug information
#define DEBUG 1

// If non-zero, enable PWM hardware output
#define PWM_OUT         0

// Location of peripheral registers in physical memory
// Clock frequency -- FIXME what is clock meh?
#if WHAT_PI == PI_Z
#define PHYS_REG_BASE 0x20000000
#define CLOCK_HZ      400000000 // 400... for ZERO! what about 1?
#elif WHAT_PI == PI_1
#define PHYS_REG_BASE 0x20000000
#define CLOCK_HZ      400000000 // 400... for ZERO! what about 1?
#elif WHAT_PI == PI_2
#define PHYS_REG_BASE 0x3F000000
#define CLOCK_HZ      250000000
#elif WHAT_PI == PI_3
#define PHYS_REG_BASE 0x3F000000
#define CLOCK_HZ      250000000
#elif WHAT_PI == PI_Z2
#define PHYS_REG_BASE 0x3F000000
#define CLOCK_HZ      250000000
#elif WHAT_PI == PI_4 
#define PHYS_REG_BASE 0xFE000000
#define CLOCK_HZ      250000000
#else
#error "What PI?"
#endif

// Debug statements
#if DEBUG
#define debug printf
#else
#define debug(...)
#endif

// Location of peripheral registers in bus memory
#define BUS_REG_BASE 0x7E000000

// If non-zero, set PWM clock using VideoCore mailbox
#define USE_VC_CLOCK_SET 0

// Size of memory page
#define PAGE_SIZE 0x1000

// Round up to nearest page
#define PAGE_ROUNDUP(n) ((n)%PAGE_SIZE==0 ? (n) : ((n)+PAGE_SIZE)&~(PAGE_SIZE-1))

// Structure for mapped peripheral or memory
typedef struct {
    int  fd;      // File descriptor
    int  h;       // Memory handle
    int  size;    // Memory size
    void *bus;    // Bus address
    void *virt;   // Virtual address
    void *phys;   // Physical address
} MEM_MAP;

// Virtual memory pointers to acceess GPIO, DMA and PWM from user space
extern MEM_MAP clk_regs;

// Get virtual 8 and 32-bit pointers to register
#define REG8(m, x)  ((volatile uint8_t *) ((size_t)(m.virt) + (size_t)(x)))
#define REG32(m, x) ((volatile uint32_t *)((size_t)(m.virt) + (size_t)(x)))

// Get bus address of register
#define REG_BUS_ADDR(m, x)  ((size_t)(m.bus)  + (size_t)(x))
// Convert uncached memory virtual address to bus address
#define MEM_BUS_ADDR(mp, a) ((size_t)a - (size_t)(mp)->virt + (size_t)(mp)->bus)
// Convert bus address to physical address (for mmap)
#define BUS_PHYS_ADDR(a)    ((void *)((size_t)(a) & ~0xC0000000))

// Videocore mailbox memory allocation flags
// see: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
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

// VC flags for uncached DMA memory
#define DMA_MEM_FLAGS (MEM_FLAG_DIRECT|MEM_FLAG_ZERO)

// Mailbox command/response structure
typedef struct {
    uint32_t len,   // Overall length (bytes)
        req,        // Zero for request, 1<<31 for response
        tag,        // Command number
        blen,       // Buffer length (bytes)
        dlen;       // Data length (bytes)
        uint32_t uints[32-5];   // Data (108 bytes maximum)
} VC_MSG __attribute__ ((aligned (16)));




// Clock registers and values
#define CLK_BASE        (PHYS_REG_BASE + 0x101000)
#define CLK_PWM_CTL     0xa0
#define CLK_PWM_DIV     0xa4
#define CLK_PASSWD      0x5a000000
#define PWM_CLOCK_ID    0xa

void *map_periph(MEM_MAP *mp, void *phys, int size);
void unmap_periph(MEM_MAP *mp);

// eof
