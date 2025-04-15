#include <snes.h>
#include "../../shared/menu.h"

extern char tilfont, palfont;

// Rom entries are read on cartridge at 0xf000/0x7000
// Rom load is triggered by reading at an offset (rom index) from 0xf400/0x7400

//#define DEBUG 1
#ifdef DEBUG
roms_t my_roms = {
    22,
    {
        { "ROM 1 9876543210987", (void*) 0x10001000 },
        { "ROM 2              ", (void*) 0x10002000 },
        { "ROM 3              ", (void*) 0x10003000 },
        { "ROM 4              ", (void*) 0x10004000 },
        { "ROM 5              ", (void*) 0x10005000 },
        { "ROM 6              ", (void*) 0x10006000 },
        { "ROM 7              ", (void*) 0x10007000 },
        { "ROM 8              ", (void*) 0x10008000 },
        { "ROM 9              ", (void*) 0x10009000 },
        { "ROM 10             ", (void*) 0x1000a000 },
        { "ROM 11             ", (void*) 0x1000b000 },
        { "ROM 12             ", (void*) 0x1000c000 },
        { "ROM 13             ", (void*) 0x1000d000 },
        { "ROM 14             ", (void*) 0x1000e000 },
        { "ROM 15             ", (void*) 0x1000f000 },
        { "ROM 16             ", (void*) 0x10010000 },
        { "ROM 17             ", (void*) 0x10011000 },
        { "ROM 18             ", (void*) 0x10012000 },
        { "ROM 19             ", (void*) 0x10013000 },
        { "ROM 20             ", (void*) 0x10014000 },
        { "ROM 21             ", (void*) 0x10015000 },
        { "ROM 22             ", (void*) 0x10016000 }
    }
};

roms_t* roms = &my_roms;
#else
roms_t* roms = 0xf000;
#endif

char* trigger = 0xf400;


int main(void)
{
    // Initialize text console with our font
    consoleSetTextMapPtr(0x6800);
    consoleSetTextGfxPtr(0x3000);
    consoleSetTextOffset(0x0100);
    consoleInitText(0, 16 * 2, &tilfont, &palfont);

    // Init background
    bgSetGfxPtr(0, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    // Now Put in 16 color mode and disable Bgs except current
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    consoleDrawText(4, 2, "~ pico-snes-cartridge ~");
    consoleDrawText(4, 4, ">");

    int i;
    for (i=0; i<roms->count; i++) {
        consoleDrawText(6, i+4, roms->entries[i].name);
    }

    setScreenOn();


    char cursorPos = 0;
    char keydownpressed = 0;
    char keyuppressed = 0;
    char keyapressed = 0;
    while (1)
    {
        WaitForVBlank();

        // Navigate with cursor
        if (padsCurrent(0) & KEY_DOWN) {
            if (keydownpressed == 0) {
                keydownpressed = 1;
                consoleDrawText(4, cursorPos+4, " ");
                cursorPos++;
                if (cursorPos >= roms->count) {
                    cursorPos = 0;
                }
                consoleDrawText(4, cursorPos+4, ">");
            }
        } else {
            keydownpressed = 0;
        }
        if (padsCurrent(0) & KEY_UP) {
            if (keyuppressed == 0) {
                keyuppressed = 1;
                consoleDrawText(4, cursorPos+4, " ");
                cursorPos--;
                if (cursorPos < 0) {
                    cursorPos = roms->count - 1;
                }
                consoleDrawText(4, cursorPos+4, ">");
            }
        } else {
            keyuppressed = 0;
        }

        // A button loads the selected ROM
        if (padsCurrent(0) & KEY_A) {
            if (keyapressed == 0) {
                keyapressed = 1;

                // Reading this address triggers the ROM load and console reset
                volatile char status = *(trigger + cursorPos);
                //consoleDrawText(4, 1, "STATUS=0x%02x POS=%d", status, cursorPos);
                //consoleDrawText(4, 10, "TRIGGER=0x%02x ADDR=0x%04x", trigger, trigger+cursorPos);
            }
        } else {
            keyapressed = 0;
        }
        
    }
    return 0;
}