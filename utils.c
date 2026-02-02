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
#include <ctype.h>

void strxcpy(char *src, char *dst, int len)
{
    // for alignment (?) reasons strcpy and strncpy bus-err on string access
    int i = 0;
    for (; i < len && *(src+i) != 0; i++) *(dst+i) = *(src+i);
    for (; i < len; i++) *(dst+i) = 0;
}

// Return hex digit value, -ve if not hex
int hexdig(char c)
{
    c = toupper(c);
    return((c>='0' && c<='9') ? c-'0' : (c>='A' && c<='F') ? c-'A'+10 : -1);
}

// Swap adjacent bytes in transmit data
void swap_bytes(void *data, int len)
{
    uint16_t *wp = (uint16_t *)data;

    len = (len + 1) / 2;
    while (len-- > 0)
    {
        *wp = __builtin_bswap16(*wp);
        wp++;
    }
}

// eof