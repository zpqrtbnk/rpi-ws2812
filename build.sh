#gcc -Wall -o bin/pixleds rpi_pixleds.c rpi_dma_utils.c
gcc -Wall -o bin/dmatest rpi_dma_test.c
gcc -Wall -o bin/test test.c rpi_lib.c rpi_smi.c
