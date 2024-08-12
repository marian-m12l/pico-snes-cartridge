#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "rom.h"

#define DEBUG 1

#ifdef DEBUG
#define COUNTER_THRESHOLD 10000000 // FIXME 140000
#define BUFFER_SIZE 20000
uint32_t counter = 0;
uint16_t addresses[BUFFER_SIZE];
uint8_t datas[BUFFER_SIZE];
uint32_t datas_out[BUFFER_SIZE];
#endif

int main() {
    stdio_init_all();

    // Overclock
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(284000, true);

    // Configure GPIOs
    gpio_init_mask(0x1800ffff | 0x047f0000);
    gpio_set_dir_in_masked(0x1800ffff);
    gpio_set_dir_out_masked(0x047f0000);

    gpio_set_slew_rate(16, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(17, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(18, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(19, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(20, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(21, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(22, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(26, GPIO_SLEW_RATE_FAST);

    gpio_set_drive_strength(16, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(17, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(18, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(19, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(20, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(21, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(22, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(26, GPIO_DRIVE_STRENGTH_8MA);

    gpio_set_dir_in_masked(0x047f0000);
    gpio_clr_mask(0x047f0000);    

    printf("Waiting for SNES to boot...\n");
    while(((sio_hw->gpio_in) & 0x18000000) == 0) {
        tight_loop_contents();
    }

#ifdef DEBUG
    //while (counter++ < BUFFER_SIZE) {
    while (counter++ < COUNTER_THRESHOLD) {
    //while (true) {
#else
    while (true) {
#endif

        // TODO Wait for /RD and /CART LOW
        // FIXME Using PIO??
        // TODO Read address
        // TODO Get data from RAM array
        // FIXME From flash?? PSRAM??
        // TODO output data

        //printf("Waiting for data query from SNES...\n");
        
        //bool nRD = ((sio_hw->gpio_in) & 0x010000) >> 16;
        //bool nCART = ((sio_hw->gpio_in) & 0x020000) >> 17;
        //while (nRD || nCART) {
        //uint8_t rd_cart = ((sio_hw->gpio_in) & 0x030000) >> 16;
        //while((((sio_hw->gpio_in) & 0x030000) >> 16) != 0) {
        while(((sio_hw->gpio_in) & 0x18000000) != 0) {
             tight_loop_contents();
        }
        uint32_t address = ((sio_hw->gpio_in) & 0xffff);    // FIXME 24 bit address with bank??
        //printf("Data requested. Address=%04x\n", address);

        uint8_t data = (address & 0xf000) == 0xf000 ? rom[address & 0x7fff] : rom[address & 0x6fff];    // FIXME
        //printf("Data=%02x\n", data);

        // DATA pins mask: 0x04000000 & 0x007f0000
        uint32_t data_out = ((data & 0x80) << 19) | ((data & 0x7f) << 16);

        gpio_set_dir_out_masked(0x047f0000);
        gpio_put_masked(0x047f0000, data_out);

#ifdef DEBUG
        addresses[(counter-1)%BUFFER_SIZE] = address ;//FIXME & 0x7fff;
        datas[(counter-1)%BUFFER_SIZE] = data;
        datas_out[(counter-1)%BUFFER_SIZE] = data_out;
#endif

        // Wait for /RD to go HIGH
        while(((sio_hw->gpio_in) & 0x08000000) == 0) {
            tight_loop_contents();
        }

        // FIXME when to clear data bus ??

        // TODO set as high-impedance ?!
        gpio_set_dir_in_masked(0x047f0000);
        //gpio_clr_mask(0x047f0000);
    }

#ifdef DEBUG
    for (int i=0; i<BUFFER_SIZE; i++) {
        printf("#%02d: %04x -> rom[%04x] = %02x (%08x)\n", i, addresses[i], addresses[i] & 0x7fff, datas[i], datas_out[i]);
    }

    sleep_ms(5000);
#endif

    while(true) {
        tight_loop_contents();
    }
    return 0;
}
