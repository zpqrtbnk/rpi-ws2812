
#include "rpi_log.h"

int verbose;

void log_verbose(int v)
{
    verbose = v;
}

int is_verbose()
{
    return verbose;
}
