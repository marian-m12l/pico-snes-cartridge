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
uint32_t addresses[BUFFER_SIZE];
uint8_t datas[BUFFER_SIZE];
uint32_t datas_out[BUFFER_SIZE];
#endif

#define SNES_ADDR_PINS_MASK  0x0000000000ffffff
#define SNES_DATA_PINS_MASK  0x00000000ff000000
#define SNES_DATA_PINS_SHIFT 24
#define SNES_RD_PIN_MASK     0x0000000100000000
#define SNES_CART_PIN_MASK   0x0000000200000000
#define SNES_CTRL_PINS_MASK  (SNES_RD_PIN_MASK | SNES_CART_PIN_MASK)

#define SNES_ALL_PINS_MASK (SNES_ADDR_PINS_MASK | SNES_DATA_PINS_MASK | SNES_CTRL_PINS_MASK)

uint32_t map_address_to_rom(uint32_t address) {
    // FIXME ADDRESS IS 24 BITS LONG --> HANDLE BANK ADDRESS CORRECTLY DEPENDING ON MEMORY MAPPER ??
    uint32_t bank = address >> 16;
    // FIXME LoROM / HiROM / ExHiROM ??? --> Check rom type at 0x7fd5 ??
    // TODO DON'T READ ROMTYPE EVERYTIME !!!
    uint8_t romtype = rom[0x7fd5] & 0x0f;  // 0: LoROM, 1: HiROM, 5: ExHiROM
    // FIXME (rom[0x7fd5] & 0x10) indicates SLOW or FAST ROM --> always patch to SLOW ??
    // FIXME address & 0x7fff or address & 0x6fff depending on (address & 0xf000) == 0xf000 ???
    if (romtype == 0) { // LoROM
        // LoROM: Up to 128 32KB-banks. Bank indexing starting from 0x80. Each bank starts at 0x8000
        return (bank & 0x7f) * 32768 + (address & 0x7fff);
    } else if (romtype == 1) {  // HiROM
        // HiROM: Up to 64 64KB-banks. Bank indexing starting from 0xc0. Each bank starts at 0x0000
        return (bank & 0x3f) * 65536 + address;
    } else {
        // TODO Add support for ExHiROM
        return (bank & 0x3f) * 65536 + address;
    }
}

int main() {
    stdio_init_all();

    // Overclock
    //vreg_set_voltage(VREG_VOLTAGE_1_20);
    //set_sys_clock_khz(284000, true);

    // Configure GPIOs
    gpio_set_dir_masked64(SNES_ALL_PINS_MASK, 0x0000000000000000);
    gpio_put_masked64(SNES_ALL_PINS_MASK, 0x0000000000000000);
    gpio_set_function_masked64(SNES_ALL_PINS_MASK, GPIO_FUNC_SIO);

    // FIXME Required ??
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+1, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+2, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+3, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+4, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+5, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+6, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+7, GPIO_SLEW_RATE_FAST);

    // FIXME Required ??
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+1, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+2, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+3, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+4, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+5, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+6, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_drive_strength(SNES_DATA_PINS_SHIFT+7, GPIO_DRIVE_STRENGTH_8MA);


    printf("Waiting for SNES to boot...\n");
    while((gpio_get_all64() & SNES_CTRL_PINS_MASK) == 0) {
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
        while((gpio_get_all64() & SNES_CTRL_PINS_MASK) != 0) {
             tight_loop_contents();
        }
        uint32_t address = (gpio_get_all64() & SNES_ADDR_PINS_MASK);
        //printf("Data requested. Address=%06x\n", address);

        uint32_t data_location_in_rom = map_address_to_rom(address);
        uint8_t data = rom[data_location_in_rom]; //(address & 0xf000) == 0xf000 ? rom[address & 0x7fff] : rom[address & 0x6fff];    // FIXME
        //printf("Data=%02x\n", data);

        uint64_t data_out = data << SNES_DATA_PINS_SHIFT;

        gpio_set_dir_out_masked64(SNES_DATA_PINS_MASK);
        gpio_put_masked64(SNES_DATA_PINS_MASK, data_out);

#ifdef DEBUG
        addresses[(counter-1)%BUFFER_SIZE] = address ;//FIXME & 0x7fff;
        datas[(counter-1)%BUFFER_SIZE] = data;
        datas_out[(counter-1)%BUFFER_SIZE] = data_out;
#endif

        // Wait for /RD to go HIGH
        while((gpio_get_all64() & SNES_RD_PIN_MASK) == 0) {
            tight_loop_contents();
        }

        // FIXME when to clear data bus ??

        // TODO set as high-impedance ?!
        gpio_set_dir_in_masked64(SNES_DATA_PINS_MASK);
        //gpio_clr_mask64(SNES_DATA_PINS_MASK);
    }

#ifdef DEBUG
    for (int i=0; i<BUFFER_SIZE; i++) {
        printf("#%02d: %06x -> rom[%06x] = %02x (%08x)\n", i, addresses[i], map_address_to_rom(addresses[i]), datas[i], datas_out[i]);
    }

    sleep_ms(5000);
#endif

    while(true) {
        tight_loop_contents();
    }
    return 0;
}
