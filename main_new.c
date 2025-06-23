#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"

#include "bus.h"
#include "cic.h"
#include "pins.h"
#include "launcher.h"


#ifdef RESET_TO_LAUNCHER

void reset_gpio_callback(uint gpio, uint32_t events) {
    // Called when RESET is pressed (low)
    // Wait for RESET to be released (high) or for the long-press reset-to-launcher timeout
    uint32_t counter = 1000000;
    while (--counter && !gpio_get(SNES_RESET_PIN)) {
        tight_loop_contents();
    }
    if (counter == 0) {
        // Just reset RP2350 (into launcher)
        watchdog_reboot(0, 0, 0);
    }
}
#endif

#ifdef ENABLE_CIC
void core1_entry() {
    // Wait until core0 is done playing with cache pinning (this would make CIC crash when trying to read from flash)
    multicore_fifo_pop_blocking();

    while (true) {
        if (init_cic()) {
            loop_cic();
            deinit_cic();
        }
    }
}
#endif

int main() {
    // Overclock to 330MHz
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(330000, true);

#ifdef ENABLE_UART
    stdio_init_all();
#endif

#ifdef ENABLE_CIC
    // Start core1 with CIC emulator
    multicore_launch_core1(core1_entry);
#endif

#ifdef ENABLE_BUS
    // Configure GPIOs
    gpio_set_dir_masked64(SNES_ALL_PINS_MASK, 0x0000000000000000);
    gpio_put_masked64(SNES_ALL_PINS_MASK, 0x0000000000000000);
    gpio_set_function_masked64(SNES_ALL_PINS_MASK, GPIO_FUNC_SIO);

    // Disable hysteresis on /CART and /RD to shave off 2 cycles of latency
    gpio_set_input_hysteresis_enabled(SNES_CART_PIN, false);
    gpio_set_input_hysteresis_enabled(SNES_RD_PIN, false);

    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+1, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+2, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+3, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+4, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+5, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+6, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(SNES_DATA_PINS_SHIFT+7, GPIO_SLEW_RATE_FAST);

    // Hold console in reset until rom is loaded and loop is started
    gpio_put(SNES_RESET_PIN, 0);
    gpio_set_drive_strength(SNES_RESET_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(SNES_RESET_PIN, true);

#ifdef RESET_TO_LAUNCHER
    // Register RESET interrupt handler
    gpio_set_irq_callback(&reset_gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
#endif

    // Look for ROMs in flash memory
    find_rom_entries();

    // Load menu and loop
    uint8_t romtype = init_rom(launcher_rom, launcher_rom_size);

#ifdef ENABLE_CIC
    // Start CIC once we're done loading rom (and pinning xip cache)
    multicore_fifo_push_blocking(0xc1c0c1c0);
#endif

    // Release reset on console
    gpio_set_dir(SNES_RESET_PIN, false);

    loop_launcher();

    // Rom was selected, load and run loop
    uint8_t* selected = selected_rom();
    if (selected != 0) {
        // Hold console in reset until rom is loaded and loop is started
        gpio_set_dir(SNES_RESET_PIN, true);

        uint32_t size = *((uint32_t*) (selected + 16));
        uint8_t romtype = init_rom(selected + 32, size);

#ifdef RESET_TO_LAUNCHER
        gpio_set_irq_enabled(SNES_RESET_PIN, GPIO_IRQ_EDGE_FALL, true);
#endif
        
        // Release reset on console
        gpio_set_dir(SNES_RESET_PIN, false);

        if (romtype == 0) { // LoROM
            loop_lorom();
        } else if (romtype == 1) {  // HiROM
            loop_hirom();
        }
    }
#endif

    while(true) {
        tight_loop_contents();
    }
    return 0;
}
