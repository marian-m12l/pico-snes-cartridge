#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/xip_cache.h"

#include "bus.h"
#include "pins.h"
#include "rom.h"

#include "shared/menu.h"


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

roms_t my_roms;

uint8_t* __uninitialized_ram(selected_rom_addr);


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
void load_no_banks(const uint8_t* romdata, uint32_t size) {
    if (size > ROM_MAX_LENGTH) {
#ifdef ENABLE_UART
        printf("Unsupported ROM size: %d > %d\n", size, ROM_MAX_LENGTH);
#endif
        size = ROM_MAX_LENGTH;
    }
#ifdef ENABLE_UART
    printf("Loading %d bytes ROM\n", size);
#endif
    memcpy(sram_rom, romdata, size);
    // Verify ROM
    if (memcmp(sram_rom, romdata, size) != 0) {
#ifdef ENABLE_UART
        printf("ROM mismatch\n");
#endif
    }
}
#endif

#ifdef LOAD_BANKS_16K
void load_banks_16k(const uint8_t* romdata, uint32_t size) {
    for (int i=0; i<31; i++) {
        banks[i] = sram_banks[i];
    }
    banks[31] = xip_bank31;
    // Bank 31 will NOT be zero-initialized by the BSS routine
    memset(xip_bank31, 0, BANK_LENGTH);
    // Load ROM into RAM
    int banks_count = size / BANK_LENGTH;
#ifdef ENABLE_UART
    printf("ROM size: %d Banks count: %d\n", size, banks_count);
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
        memcpy(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH);
    }
    // Verify ROM
    for (int i=0; i<banks_count; i++) {
        if (memcmp(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH) != 0) {
#ifdef ENABLE_UART
            printf("ROM bank mismatch: %d\n", i);
#endif
        }
    }
#ifdef ENABLE_UART
    printf("ROM banks verified\n");
#endif
}
#endif

#ifdef LOAD_BANKS_4K
void load_banks_4k(const uint8_t* romdata, uint32_t size) {
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
    int banks_count = size / BANK_LENGTH;
#ifdef ENABLE_UART
    printf("ROM size: %d Banks count: %d\n", size, banks_count);
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
        memcpy(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH);
    }
    // Verify ROM
    for (int i=0; i<banks_count; i++) {
        if (memcmp(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH) != 0) {
#ifdef ENABLE_UART
            printf("ROM bank mismatch: %d\n", i);
#endif
        }
    }
#ifdef ENABLE_UART
    printf("ROM banks verified\n");
#endif

    // TODO Output some data from ram banks ??
#ifdef ENABLE_UART
    for (int i=0; i<banks_count; i++) {
        printf("Bank 0x%02x @ 0x%08x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    i, banks[i],
                    banks[i][0], banks[i][1], banks[i][2], banks[i][3], banks[i][4], banks[i][5], banks[i][6], banks[i][7],
                    banks[i][8], banks[i][9], banks[i][10], banks[i][11], banks[i][12], banks[i][13], banks[i][14], banks[i][15]
        );
    }
#endif
}
#endif

uint8_t init_rom(const uint8_t* romdata, uint32_t size) {
    // Pin cache lines if required
#if defined(LOAD_BANKS_16K) || defined(LOAD_BANKS_4K)
    pin_cache_lines();
#endif

    // Copy to RAM if required
#ifdef LOAD_NO_BANKS
    load_no_banks(romdata, size);
#endif

#ifdef LOAD_BANKS_16K
    load_banks_16k(romdata, size);
#endif

#ifdef LOAD_BANKS_4K
    load_banks_4k(romdata, size);
#endif

#ifdef ENABLE_UART
    printf("Loaded ROM at 0x%p\n", romdata);
#endif

    // Read ROM header
    bool found_header = false;
    // First, try to read LoROM header @ 0x7fc0
    uint16_t complement = romdata[0x7fdc] | (romdata[0x7fdd] << 8); // TODO *((uint16_t*)rom[0x7fdc]) ??
    uint16_t checksum = romdata[0x7fde] | (romdata[0x7fdf] << 8);   // TODO *((uint16_t*)rom[0x7fde]) ??
    if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
        // Checksum matches
        uint8_t rom_speed_and_mapping = romdata[0x7fd5];
        if ((rom_speed_and_mapping & ~0x10) == 0x20) {
            romtype = rom_speed_and_mapping & 0x0f; // 0 == LoROM
            romspeed = (rom_speed_and_mapping & 0x10) >> 4; // 0 == SlowROM, 1 == FastROM
            found_header = true;
        }
    }
    // Then, try to read HiROM header @ 0xffc0
    if (!found_header && size >= 0x10000) {
        uint16_t complement = romdata[0xffdc] | (romdata[0xffdd] << 8); // TODO *((uint16_t*)rom[0xffdc]) ??
        uint16_t checksum = romdata[0xffde] | (romdata[0xffdf] << 8);   // TODO *((uint16_t*)rom[0xffde]) ??
        if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
            uint8_t rom_speed_and_mapping = romdata[0xffd5];
            if ((rom_speed_and_mapping & ~0x10) == 0x21) {
                romtype = rom_speed_and_mapping & 0x0f; // 1 == HiROM
                romspeed = (rom_speed_and_mapping & 0x10) >> 4; // 0 == SlowROM, 1 == FastROM
                found_header = true;
            }
        }
    }

    // TODO Read ROM size from header ?!
    romsize = size;
    
#ifdef ENABLE_UART
    if (!found_header) {
        printf("Failed to read ROM header --> Defaulting to LoROM ?\n");
        romtype = romdata[0x7fd5] & 0x0f;
        romspeed = (romdata[0x7fd5] & 0x10) >> 4;
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

void __not_in_flash_func(loop_menu)() {
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
        if (data_location_in_rom >= 0x7000 && data_location_in_rom < 0x7400) {
            // Rom entries
            uint16_t offset = data_location_in_rom - 0x7000;
            data = *((uint8_t*)(&my_roms) + offset);
        } else if (data_location_in_rom >= 0x7400 && data_location_in_rom < 0x7400 + my_roms.count) {
            // Trigger trigger rom load by reading an offset (rom index) in 0x7400
            uint16_t offset = data_location_in_rom - 0x7400;
            if (offset >= 0 && offset < my_roms.count) {
                selected_rom_addr = my_roms.entries[offset].address;

                // Hold console in reset until rom is loaded and lopp is started
                gpio_set_dir(SNES_RESET_PIN, true);
                gpio_set_drive_strength(SNES_RESET_PIN, GPIO_DRIVE_STRENGTH_12MA);
                gpio_put(SNES_RESET_PIN, 0);
                break;
            }
        } else if (data_location_in_rom < romsize) {
#ifdef NO_LOAD
            data = menu_rom[data_location_in_rom];
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
        
        while((gpio_get_all64() & SNES_RD_PIN_MASK) != 0) {
            tight_loop_contents();
        }
    }
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

uint8_t* selected_rom() {
    return selected_rom_addr;
}

const char* magic = "pico-snes-rom   ";
uint16_t bsd_checksum(uint8_t* addr, uint32_t size) {
    uint8_t ch;
    uint16_t checksum = 0;

    for (int i=0; i<size; i++) {
        ch = *(addr + 32 + i);
        checksum = (checksum >> 1) + ((checksum & 1) << 15);
        checksum += ch;
        checksum &= 0xffff;
    }
    return checksum;
}
void find_rom_entries() {
    printf("find_rom_entries\n");
    int romIndex = 0;
    for (int i=1; i<16; i++) {
        uint8_t* addr = (uint8_t*) 0x10000000 + i * 0x100000;
        printf("addr=0x%08x\n", addr);
        if (memcmp(addr, magic, 16) == 0) {
            // Found magic bytes
            printf("found magic\n");
            uint32_t size = *((uint32_t*) (addr + 16));
            printf("size=%d\n", size);
            uint16_t checksum = *((uint16_t*) (addr + 30));
            printf("checksum=0x%04x\n", checksum);
            if (checksum == bsd_checksum(addr, size)) {
                // Checksum matches
                printf("checksum matches\n");
                // Read name from rom header
                uint8_t* name;
                bool found_header = false;
                uint8_t* romdata = addr + 32;
                // First, try to read LoROM header @ 0x7fc0
                uint16_t complement = romdata[0x7fdc] | (romdata[0x7fdd] << 8); // TODO *((uint16_t*)rom[0x7fdc]) ??
                uint16_t checksum = romdata[0x7fde] | (romdata[0x7fdf] << 8);   // TODO *((uint16_t*)rom[0x7fde]) ??
                if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
                    // Checksum matches
                    name = &romdata[0x7fc0];
                    found_header = true;
                }
                // Then, try to read HiROM header @ 0xffc0
                if (!found_header && size >= 0x10000) {
                    uint16_t complement = romdata[0xffdc] | (romdata[0xffdd] << 8); // TODO *((uint16_t*)rom[0xffdc]) ??
                    uint16_t checksum = romdata[0xffde] | (romdata[0xffdf] << 8);   // TODO *((uint16_t*)rom[0xffde]) ??
                    if ((checksum + complement) == 0xffff && (checksum != 0) && (complement != 0)) {
                        name = &romdata[0xffc0];
                        found_header = true;
                    }
                }
                if (found_header) {
                    strcpy(my_roms.entries[romIndex].name, name);
                } else {
                    strcpy(my_roms.entries[romIndex].name, "ROM ###");
                }
                my_roms.entries[romIndex].address = addr;
                romIndex++;
            }
        }
    }
    printf("find_rom_entries: %d\n", romIndex);
    my_roms.count = romIndex;
}
