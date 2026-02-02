// Raspberry Pi DMA utilities; see https://iosoft.blog for details
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
#include "rpi_vm.h"

// Use mmap to obtain virtual address, given physical
void *map_periph(MEM_MAP *mp, void *phys, int size)
{
    mp->phys = phys;
    mp->size = PAGE_ROUNDUP(size);
    mp->bus = (void *)((size_t)phys - (size_t)PHYS_REG_BASE + (size_t)BUS_REG_BASE);
    mp->virt = map_segment(phys, mp->size);
    return mp->virt;
}

// Free mapped peripheral or memory
void unmap_periph(MEM_MAP *mp)
{
    unmap_segment(mp->virt, mp->size);
}

// EOF
