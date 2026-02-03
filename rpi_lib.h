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

// Get virtual 8 and 32-bit pointers to register
#define REG8(m, x)  ((volatile uint8_t *) ((size_t)(m.virt) + (size_t)(x)))
#define REG32(m, x) ((volatile uint32_t *)((size_t)(m.virt) + (size_t)(x)))

// Get bus address of register
#define REG_BUS_ADDR(m, x)  ((size_t)(m.bus) + (size_t)(x))
// Convert uncached memory virtual address to bus address
#define MEM_BUS_ADDR(mp, a) ((size_t)a - (size_t)(mp)->virt + (size_t)(mp)->bus)
// Convert bus address to physical address (for mmap)
#define BUS_PHYS_ADDR(a)    ((void *)((size_t)(a) & ~0xC0000000))

void *map_periph(MEM_MAP *mp, void *phys, int size);
void unmap_periph(MEM_MAP *mp);

// eof
