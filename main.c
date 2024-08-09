#include <stdio.h>
#include "pico/stdlib.h"


int main() {
    stdio_init_all();

    while (true) {

        // TODO Wait for /RD and /CART LOW
        // FIXME Using PIO??
        // TODO Read address
        // TODO Get data from RAM array
        // FIXME From flash?? PSRAM??
        // TODO output data

        printf("Waiting for data query from SNES...\n");
        sleep_ms(1000);
    }
    return 0;
}
