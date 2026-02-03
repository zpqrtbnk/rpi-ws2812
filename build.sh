#gcc -Wall -o bin/pixleds rpi_pixleds.c rpi_dma_utils.c
#gcc -Wall -o bin/dmatest rpi_dma_test.c
gcc -Wall -o bin/test test.c utils.c rpi_*.c
gcc -Wall -o bin/leds leds.c utils.c rpi_*.c

gcc -Wall -o bin/test_min test_min.c utils.c rpi_*.c
gcc -Wall -o bin/test_orig test_orig.c
