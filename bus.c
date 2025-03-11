#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/xip_cache.h"

#include "bus.h"
#include "pins.h"
#include "rom.h"


#define CACHE_AS_SRAM_OFFSET 0x02000000

#ifdef LOAD_NO_BANKS
#define ROM_MAX_LENGTH (256*1024)
uint8_t sram_rom[ROM_MAX_LENGTH];
#endif

#ifdef LOAD_BANKS_16K
// 32 banks of 16 KiB (1 bank (16KiB) in pinned cache + 31 banks (496KiB) in main RAM)
#define BANK_LENGTH (16*1024)
#define MAX_BANKS_COUNT (32)
uint8_t* xip_bank31 = (uint8_t*) (XIP_BASE+CACHE_AS_SRAM_OFFSET);
uint8_t sram_banks[31][BANK_LENGTH];
uint8_t* banks[MAX_BANKS_COUNT]; // 524288 bytes of rom data across 32 banks
#endif

#ifdef LOAD_BANKS_4K
// 128 banks of 4 KiB (1 bank (4KiB) in USB RAM + 4 banks (16KiB) in pinned cache + 123 banks (492KiB) in main RAM)
#define BANK_LENGTH (4*1024)
#define MAX_BANKS_COUNT (128)
uint8_t* xip_banks = (uint8_t*) (XIP_BASE+CACHE_AS_SRAM_OFFSET);
uint8_t* usb_bank = (uint8_t*) (USBCTRL_DPRAM_BASE);
uint8_t sram_banks[123][BANK_LENGTH];
uint8_t* banks[MAX_BANKS_COUNT]; // 524288 bytes of rom data across 128 banks
#endif

uint32_t romsize;
uint8_t romtype;
uint8_t romspeed;


void pin_cache_lines() {
#ifdef ENABLE_UART
    printf("Pinning 16K of cache lines\n");
#endif
    // Pin cache lines for an additional 16 KiB of RAM
    xip_cache_pin_range(CACHE_AS_SRAM_OFFSET, 16*1024);
#ifdef ENABLE_UART
    printf("Pinned\n");
#endif
}

#ifdef LOAD_NO_BANKS
void load_no_banks() {
    int size = rom_size;
    if (size > ROM_MAX_LENGTH) {
#ifdef ENABLE_UART
        printf("Unsupported ROM size: %d > %d\n", size, ROM_MAX_LENGTH);
#endif
        size = ROM_MAX_LENGTH;
    }
#ifdef ENABLE_UART
    printf("Loading %d bytes ROM\n", size);
#endif
    memcpy(sram_rom, rom, size);
}
#endif

#ifdef LOAD_BANKS_16K
void load_banks_16k() {
    for (int i=0; i<31; i++) {
        banks[i] = sram_banks[i];
    }
    banks[31] = xip_bank31;
    // Bank 31 will NOT be zero-initialized by the BSS routine
    memset(xip_bank31, 0, BANK_LENGTH);
    // Load ROM into RAM
    int banks_count = rom_size / BANK_LENGTH;
#ifdef ENABLE_UART
    printf("ROM size: %d Banks count: %d\n", rom_size, banks_count);
#endif
    if (banks_count > MAX_BANKS_COUNT) {
#ifdef ENABLE_UART
        printf("Unsupported ROM size: %d banks > %d\n", banks_count, MAX_BANKS_COUNT);
#endif
        banks_count = MAX_BANKS_COUNT;
    }
#ifdef ENABLE_UART
    printf("Loading %d ROM banks\n", banks_count);
#endif
    for (int i=0; i<banks_count; i++) {
        memcpy(banks[i], rom + i*BANK_LENGTH, BANK_LENGTH);
    }
}
#endif

#ifdef LOAD_BANKS_4K
void load_banks_4k() {
    for (int i=0; i<123; i++) {
        banks[i] = sram_banks[i];
    }
    banks[123] = xip_banks;
    banks[124] = xip_banks + BANK_LENGTH;
    banks[125] = xip_banks + 2*BANK_LENGTH;
    banks[126] = xip_banks + 3*BANK_LENGTH;
    banks[127] = usb_bank;
    // Banks 123+ will NOT be zero-initialized by the BSS routine
    memset(xip_banks, 0, 4*BANK_LENGTH);
    memset(usb_bank, 0, BANK_LENGTH);
    // Load ROM into RAM
    int banks_count = rom_size / BANK_LENGTH;
#ifdef ENABLE_UART
    printf("ROM size: %d Banks count: %d\n", rom_size, banks_count);
#endif
    if (banks_count > MAX_BANKS_COUNT) {
#ifdef ENABLE_UART
        printf("Unsupported ROM size: %d banks > %d\n", banks_count, MAX_BANKS_COUNT);
#endif
        banks_count = MAX_BANKS_COUNT;
    }
#ifdef ENABLE_UART
    printf("Loading %d ROM banks\n", banks_count);
#endif
    for (int i=0; i<banks_count; i++) {
        memcpy(banks[i], rom + i*BANK_LENGTH, BANK_LENGTH);
    }
}
#endif

uint8_t init_rom() {
    // Pin cache lines if required
#if defined(LOAD_BANKS_16K) || defined(LOAD_BANKS_4K)
    pin_cache_lines();
#endif

    // Copy to RAM if required
#ifdef LOAD_NO_BANKS
    load_no_banks();
#endif

#ifdef LOAD_BANKS_16K
    load_banks_16k();
#endif

#ifdef LOAD_BANKS_4K
    load_banks_4k();
#endif

#ifdef ENABLE_UART
    printf("Loaded\n");
#endif

    // Read ROM header
    bool found_header = false;
    // First, try to read LoROM header @ 0x7fc0
    uint16_t complement = rom[0x7fdc] | (rom[0x7fdd] << 8); // TODO *((uint16_t*)rom[0x7fdc]) ??
    uint16_t checksum = rom[0x7fde] | (rom[0x7fdf] << 8);   // TODO *((uint16_t*)rom[0x7fde]) ??
    if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
        // Checksum matches
        uint8_t rom_speed_and_mapping = rom[0x7fd5];
        if ((rom_speed_and_mapping & ~0x10) == 0x20) {
            romtype = rom_speed_and_mapping & 0x0f; // 0 == LoROM
            romspeed = (rom_speed_and_mapping & 0x10) >> 4; // 0 == SlowROM, 1 == FastROM
            found_header = true;
        }
    }
    // Then, try to read HiROM header @ 0xffc0
    if (!found_header && rom_size >= 0x10000) {
        uint16_t complement = rom[0xffdc] | (rom[0xffdd] << 8); // TODO *((uint16_t*)rom[0xffdc]) ??
        uint16_t checksum = rom[0xffde] | (rom[0xffdf] << 8);   // TODO *((uint16_t*)rom[0xffde]) ??
        if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
            uint8_t rom_speed_and_mapping = rom[0xffd5];
            if ((rom_speed_and_mapping & ~0x10) == 0x21) {
                romtype = rom_speed_and_mapping & 0x0f; // 1 == HiROM
                romspeed = (rom_speed_and_mapping & 0x10) >> 4; // 0 == SlowROM, 1 == FastROM
                found_header = true;
            }
        }
    }
    romsize = rom_size;
    
#ifdef ENABLE_UART
    if (!found_header) {
        printf("Failed to read ROM header --> Defaulting to LoROM ?\n");
        romtype = rom[0x7fd5] & 0x0f;
        romspeed = (rom[0x7fd5] & 0x10) >> 4;
    }
    if (romtype == 0) { // LoROM
        printf("ROM type: LoROM\n");
    } else if (romtype == 1) {  // HiROM
        printf("ROM type: HiROM\n");
    }
    if (romspeed == 0) { // SlowROM
        printf("ROM speed: SlowROM\n");
    } else if (romspeed == 1) {  // FastROM
        printf("ROM speed: FastROM\n");
    }

    // TODO Also check
    //      ROM speed (rom[0x7fd5] & 0x10)
    //      Chipset (rom[0x7fd6] & 0x0f)
    //      Coprocessor (rom[0x7fd6] & 0xf0)
    //      ROM size (rom[0x7fd7])
    //      RAM size (rom[0x7fd8])
    //      Checksum ??
#endif

    return romtype;
}

void __not_in_flash_func(loop_lorom)() {
#ifdef ENABLE_UART
    printf("Waiting for SNES to boot...\n");
#endif

    while((gpio_get_all64() & SNES_CTRL_PINS_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & SNES_CTRL_PINS_MASK) != 0) {
             tight_loop_contents();
        }
        uint32_t address = (gpio_get_all64() & SNES_ADDR_PINS_MASK);
        uint32_t lorom_bank = address >> 16;
        uint32_t data_location_in_rom = (lorom_bank & 0x7f) * 32768 + (address & 0x7fff);
        uint8_t data = 0xff;
        if (data_location_in_rom < romsize) {
#ifdef NO_LOAD
            data = rom[data_location_in_rom];
#endif
#ifdef LOAD_NO_BANKS
            data = sram_rom[data_location_in_rom];
#endif
#ifdef LOAD_BANKS_16K
            int bank = (data_location_in_rom >> 14) & 0x1f;
            int addr = data_location_in_rom & 0x3fff;
            data = banks[bank][addr];
#endif
#ifdef LOAD_BANKS_4K
            int bank = (data_location_in_rom >> 12) & 0x7f;
            int addr = data_location_in_rom & 0x0fff;
            data = banks[bank][addr];
#endif
        }
        uint64_t data_out = data << SNES_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(SNES_DATA_PINS_MASK);
        gpio_put_masked64(SNES_DATA_PINS_MASK, data_out);
        while((gpio_get_all64() & SNES_RD_PIN_MASK) == 0) {
            tight_loop_contents();
        }
        gpio_set_dir_in_masked64(SNES_DATA_PINS_MASK);
        //gpio_clr_mask64(SNES_DATA_PINS_MASK);
    }
}

void __not_in_flash_func(loop_hirom)() {
#ifdef ENABLE_UART
    printf("Waiting for SNES to boot...\n");
#endif

    while((gpio_get_all64() & SNES_CTRL_PINS_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & SNES_CTRL_PINS_MASK) != 0) {
             tight_loop_contents();
        }
        uint32_t address = (gpio_get_all64() & SNES_ADDR_PINS_MASK);
        // TODO Handle ROM mirror ? banks 0x00-3F + 0x80-BF @ 0x8000-0xFFFF
        uint32_t hirom_bank = address >> 16;
        uint32_t data_location_in_rom = (hirom_bank & 0x3f) * 65536 + (address & 0xffff);
        uint8_t data = 0xff;
        if (data_location_in_rom < romsize) {
#ifdef NO_LOAD
            data = rom[data_location_in_rom];
#endif
#ifdef LOAD_NO_BANKS
            data = sram_rom[data_location_in_rom];
#endif
#ifdef LOAD_BANKS_16K
            int bank = (data_location_in_rom >> 14) & 0x1f;
            int addr = data_location_in_rom & 0x3fff;
            data = banks[bank][addr];
#endif
#ifdef LOAD_BANKS_4K
            int bank = (data_location_in_rom >> 12) & 0x7f;
            int addr = data_location_in_rom & 0x0fff;
            data = banks[bank][addr];
#endif
        }
        uint64_t data_out = data << SNES_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(SNES_DATA_PINS_MASK);
        gpio_put_masked64(SNES_DATA_PINS_MASK, data_out);
        while((gpio_get_all64() & SNES_RD_PIN_MASK) == 0) {
            tight_loop_contents();
        }
        gpio_set_dir_in_masked64(SNES_DATA_PINS_MASK);
        //gpio_clr_mask64(SNES_DATA_PINS_MASK);
    }
}
