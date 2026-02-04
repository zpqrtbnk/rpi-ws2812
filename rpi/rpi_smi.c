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

void *map_smi() {
    if (map_periph(&smi_regs, (void *)SMI_BASE, PAGE_SIZE) == 0)
        printf("error: failed to map smi registers\n");
    return smi_regs.virt;
}

void unmap_smi() {
    if (smi_regs.virt != 0)
        unmap_periph(&smi_regs);
}

// eof