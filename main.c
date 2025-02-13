#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "rom.h"
#include "counter.pio.h"

//#define DEBUG 1

#ifdef DEBUG
//#define DEBUG_KEEPALIVE 1
#define COUNTER_THRESHOLD 40000
#define BUFFER_SIZE 40000
uint32_t counter = 0;
uint32_t addresses[BUFFER_SIZE];
uint8_t datas[BUFFER_SIZE];
uint32_t datas_out[BUFFER_SIZE];
uint16_t repetition[BUFFER_SIZE];
#endif

#define SNES_ADDR_PINS_MASK  0x0000000000ffffff
#define SNES_DATA_PINS_MASK  0x00000000ff000000
#define SNES_DATA_PINS_SHIFT 24
#define SNES_RD_PIN_MASK     0x0000000200000000
#define SNES_CART_PIN_MASK   0x0000000100000000
#define SNES_CTRL_PINS_MASK  (SNES_RD_PIN_MASK | SNES_CART_PIN_MASK)
// TODO Output to unused pins (41 and 42?) for debug ??

#define SNES_CIC_P1_PIN      37
#define SNES_CIC_P2_PIN      38
#define SNES_CIC_CLK_PIN     39
#define SNES_CIC_P1_PIN_MASK 0x0000002000000000
#define SNES_CIC_P2_PIN_MASK 0x0000004000000000
#define SNES_CIC_CLK_PIN_MASK 0x0000008000000000
#define SNES_CIC_RST_PIN_MASK 0x0000010000000000
#define SNES_CIC_IO_PINS_MASK  (SNES_CIC_P1_PIN_MASK | SNES_CIC_P2_PIN_MASK)
#define SNES_CIC_PINS_MASK  (SNES_CIC_IO_PINS_MASK | SNES_CIC_CLK_PIN_MASK | SNES_CIC_RST_PIN_MASK)

#define DEBUG_PIN            41
#define DEBUG_PINS_MASK      0x0000020000000000

#define SNES_ALL_PINS_MASK (SNES_ADDR_PINS_MASK | SNES_DATA_PINS_MASK | SNES_CTRL_PINS_MASK | SNES_CIC_PINS_MASK | DEBUG_PINS_MASK)

uint8_t romtype;
volatile uint32_t cic_clock_count = 0xffffffff;

uint32_t map_address_to_rom(uint32_t address) {
    uint32_t bank = address >> 16;
    // FIXME address & 0x7fff or address & 0x6fff depending on (address & 0xf000) == 0xf000 ???
    if (romtype == 0) { // LoROM
        // LoROM: Up to 128 32KB-banks. Bank indexing starting from 0x80. Each bank starts at 0x8000
        return (bank & 0x7f) * 32768 + (address & 0x7fff);
        /*if ((address & 0xf000) == 0x8000 || (address & 0xf000) == 0x9000) {
            return (bank & 0x7f) * 32768 + (address & 0x6fff);
        } else {
            return (bank & 0x7f) * 32768 + (address & 0x7fff);
        }*/
    } else if (romtype == 1) {  // HiROM
        // HiROM: Up to 64 64KB-banks. Bank indexing starting from 0xc0. Each bank starts at 0x0000
        return (bank & 0x3f) * 65536 + address;
    } else {
        // TODO Add support for ExHiROM
        return (bank & 0x3f) * 65536 + address;
    }
}

/*
inline void gpio_set_ie(bool enabled) {
    // FIXME which pins need IE disabled ??
    gpio_set_input_enabled(12, enabled);
    //gpio_set_input_enabled(16, enabled);
    //gpio_set_input_enabled(18, enabled);
    //gpio_set_input_enabled(20, enabled);
    //gpio_set_input_enabled(22, enabled);
}

inline void gpio_disable_ie() {
    gpio_set_ie(false);
}

inline void gpio_enable_ie() {
    gpio_set_ie(true);
}
*/


// TODO From https://github.com/mrehkopf/sd2snes/blob/develop/cic/mangle.c

#define CARRY	if(a>0x0f) {\
			a &= 0xf;\
			carry=1;\
		} else carry=0;

#define CYCLES_COUNT_SKIP (84)
#define CYCLES_COUNT_NOSKIP (78)

uint mangle(unsigned char* data) {
	unsigned char a,x,temp,i,offset,carry=0;
	a=data[0xf];
    // TODO Count cycles / iterations ?
    uint cycles = 0;
	do {
		x=a;
		offset=1;
		carry=1;
		a+=data[offset]+carry;
		data[offset]=a&0xf;
		a=data[offset++];
		a+=data[offset]+carry;
		a=(~a)&0xf;
		temp=a; a=data[offset]; data[offset++]=temp&0xf;
		a+=data[offset]+carry;
		if(a<0x10) {
            // TODO "no-skip" cycles count (78)
            cycles += CYCLES_COUNT_NOSKIP;
			temp=a; a=data[offset]; data[offset++]=temp&0xf;
		} else {
            // TODO Else "skip" cycles count (84)
            cycles += CYCLES_COUNT_SKIP;
        }
		a+=data[offset];
		data[offset]=a&0xf;
		a=data[offset++];
		carry=0;
		a+=data[offset]+carry;
		temp=a; a=data[offset]; data[offset++]=temp&0xf;
		a+=8;
		if(a<0x10) {
			a+=data[offset]+carry;
		}
		temp=a; a=data[offset]; data[offset++]=temp&0xf;

		while(offset<0x10) {
			a++;
			a+=data[offset]+carry;
			data[offset]=a&0xf;
			a=data[offset++];
		}
		offset &= 0xf;
		a=x;
		a+=0xf;
		CARRY;
	} while(carry);

    return cycles;
}


uint32_t mangles[1024];
uint32_t delays[1024];
uint32_t timings[1024];
uint32_t timings_end[1024];


static inline void wait_until_clock_pulses(uint32_t until) {
    // TODO Handle counter overflow/underflow?
    while(~cic_clock_count < until) {
        tight_loop_contents();
    }
}

static inline void wait_clock_pulses(uint32_t count) {
    wait_until_clock_pulses(~cic_clock_count + count);
}

// code for core1
void core1_entry() {

    // TODO One-time initialization ??

    gpio_set_dir_masked64(DEBUG_PINS_MASK, DEBUG_PINS_MASK);
    gpio_put_masked64(DEBUG_PINS_MASK, 0x0000000000000000);
    
    while (true) {
        // CIC Emulation
        printf("Starting CIC emulation...\n");

        gpio_put(DEBUG_PIN, 1);

        // TODO Use IRQ to reset CIC when RST goes up? (e.g. when resetting the console manually?)

        // Setup clock pulses counter (CIC clock may be 4MHz, 3.58MHz, 3.54MHz, or 3.072MHz)
        cic_clock_count = 0xffffffff;
        uint32_t wait_next;

        // Setup PIO counter
        PIO pio;
        uint offset;
        uint sm;
        bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&counter_program, &pio, &sm, &offset, SNES_CIC_CLK_PIN, 1, true);
        if (!success) {
            printf("Failed to initialize PIO\n");
            goto cic_die;
        }
        counter_program_init(pio, sm, offset, SNES_CIC_CLK_PIN);

        // Setup counter DMA
        int dma_chan = dma_claim_unused_channel(true);
        int dma_chan2 = dma_claim_unused_channel(true);

        // Channel 1, this starts and than hands over to the second channel when it is done
        // Channel 2 then hands back to channel 1, so we get a continous DMA stream to a single target variable
        dma_channel_config dc = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_read_increment(&dc, false);
        channel_config_set_write_increment(&dc, false);
        channel_config_set_chain_to(&dc, dma_chan2);
        channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, false));
        dma_channel_configure(dma_chan, &dc,
            &cic_clock_count,
            &pio->rxf[sm],
            0xffffffff,
            true
        );

        // Channel 2 as above
        dma_channel_config dc2 = dma_channel_get_default_config(dma_chan2);
        channel_config_set_transfer_data_size(&dc2, DMA_SIZE_32);
        channel_config_set_read_increment(&dc2, false);
        channel_config_set_write_increment(&dc2, false);
        channel_config_set_dreq(&dc2, pio_get_dreq(pio, sm, false));
        channel_config_set_chain_to(&dc2, dma_chan);
        dma_channel_configure(dma_chan2, &dc2,
            &cic_clock_count,
            &pio->rxf[sm],
            0xffffffff,
            false
        );

        unsigned char lockseed[16]={0x0, // dummy
                    0xb,0x1,0x4,0xf,
                    0x4,0xb,0x5,0x7,
                    0xf,0xd,0x6,0x1,
                    0xe,0x9,0x8};
        unsigned char keyseed[16]={0x0, // dummy
                    0x0, // fill-in value sent by lock on reset in bit order 3-0-1-2
                    0x6,0xa,0x1,   // PAL
                    //0x9,0xa,0x1,  // TODO Add support for NTSC
                    0x8,0x5,0xf,0x1,
                    0x1,0xe,0x1,0x0,
                    0xd,0xe,0xc};
        unsigned char restart = 1, swap = 0;

        // GPIO setup: initial configuration: P1 is key output, P2 is key input (Pin 55 (P2) is **GPIO 0** in PIC CIC clone code)
        // Lock sends seed over P2 / D2 / Pin55
        gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, SNES_CIC_P1_PIN_MASK);
        gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);
        
        // Wait for CIC RST to raise
        printf("Waiting for CIC reset...\n");
        while((gpio_get_all64() & SNES_CIC_RST_PIN_MASK) == 0) {
            tight_loop_contents();
        }
        // Start counting clock pulses on RST falling edge
        while((gpio_get_all64() & SNES_CIC_RST_PIN_MASK) != 0) {
            tight_loop_contents();
        }
        pio_sm_set_enabled(pio, sm, true);

        //printf("CIC start\n");
        gpio_put(DEBUG_PIN, 0);

        // Read seed nibble a.k.a. stream id (transmission bit order is 3-0-1-2. e.g. '1-0-1-1' => 0xe)
        wait_next = 632 * 4;    // FIXME 635 * 4;
        wait_until_clock_pulses(wait_next);
        gpio_put(DEBUG_PIN, 1);
        keyseed[1] |= gpio_get(SNES_CIC_P2_PIN) << 3;
        wait_next += 15 * 4;
        wait_until_clock_pulses(wait_next);
        keyseed[1] |= gpio_get(SNES_CIC_P2_PIN);
        wait_next += 15 * 4;
        wait_until_clock_pulses(wait_next);
        keyseed[1] |= gpio_get(SNES_CIC_P2_PIN) << 1;
        wait_next += 15 * 4;
        wait_until_clock_pulses(wait_next);
        keyseed[1] |= gpio_get(SNES_CIC_P2_PIN) << 2;
        gpio_put(DEBUG_PIN, 0);

        //printf("CIC seed=0x%01x\n", keyseed[1]);

        // wait (instruction) cycles until main loop: 133

        // wait (instruction) cycles in main loop:
        //  - (a) 1 cycle (loop overhead)
        //  - (b) 2 cycles (loop0 overhead)
        //  - (c) 8 cycles until GPIO out within loop1
        //  - 3 cycles output
        //  - 82 cycles until loop1 loopback to (c)
        //  - OR 94 cycles until calls to mangle (if looping in loop1)
        //      - mangle cycles vary (use return value)
        //      - 15 cycles until loop0 loopback to (b)
        //      - OR 16 cycles until loop0 loopback to (a)

        // wait (instruction) cycles before first loop1 output: 11 cycles (--> 44 cic cycles)
        // wait (instruction) cycles between loop1 outputs: 3 + 82 + 8 = 93 instruction cycles (--> 372 cic cycles)
        // wait (instruction) cycles between last loop1 output and next loop0 output: 3 + 94 + mangle (anywhere between 468 and 504 PER MANGLE LOOP (+ mangle call overhead ~18 ???)) + ((15 + 2) OR (16 + 3 if offset=0)) + 2
        // TODO Between ~10k and ~18k cic pulses in logic analyzer captures

        // FIXME Count again?

        // FIXME Should output the first values on D2 ???
        gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, SNES_CIC_P2_PIN_MASK);
        gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);

        wait_next += 130 * 4;   // FIXME 134 * 4;
        
        // Output up to 15 keyseed bits in locksteps, mangling seed whenever we reach the last bit
        uint16_t idx = 0;
        while (true) {
            wait_next += 2 * 4;
            // Output keyseed (starting at index 'restart') until last bit is reached
            bool first = true;
            bool adjusted = false;
            bool die = false;
            while (restart < 16) {
                // FIXME DEBUG
                unsigned char output = keyseed[restart] & 1;
                unsigned char input = lockseed[restart] & 1;
                // FIXME wait_next += 8 * 4;
                wait_until_clock_pulses(wait_next);
                wait_next += 8 * 4; // FIXME
                // Output keyseed bit to P2 or P1
                if (first) {
                    delays[idx] = wait_next;
                    timings[idx] = ~cic_clock_count + 8*4;
                    first = false;
                }
                gpio_put(DEBUG_PIN, 1);
                gpio_put(swap ? SNES_CIC_P1_PIN : SNES_CIC_P2_PIN, output);
                restart++;

                // TODO If we expect to receive a '1' from the lock, measure our drift and adjust
                if (!adjusted && input != 0) {
                    uint32_t now = ~cic_clock_count;
                    // FIXME Should add a timeout and die in case we never read the expected '1'
                    uint32_t timeout = now + 80;    // We output the value for 20 instruction cycles (80 cic pulses). No need to wait longer, we're already cooked...
                    while((gpio_get_all64() & (swap ? SNES_CIC_P2_PIN_MASK : SNES_CIC_P1_PIN_MASK)) == 0 && (~cic_clock_count) < timeout) {
                        tight_loop_contents();
                    }
                    if ((~cic_clock_count) >= timeout) {
                        // Didn't read the expected '1'
                        // Die ? Or should we go on ?
                        //goto cic_die;
                        printf("CIC dying because we did not receives the expected high pulse from lock\n");
                        die = true;
                        break;
                    }
                    uint32_t delay_cycles = (~cic_clock_count) - now;
                    if (delay_cycles < 20) {
                        // TODO We are drifting late, let's shorten our delay by 3*4=12 cycles
                        wait_next -= 3 * 4;
                    } else if (delay_cycles > 44) {
                        // TODO We are drifting early, let's lengthen our delay by 3*4=12 cycles
                        wait_next += 3 * 4;
                    }
                    adjusted = true;

                    // FIXME KO after 725ms ?? ~ 130 data exchanges ?! --> CIC died
                    // ==> expected input '1', start first output and wait for rising egde on input --> never happens, infinite loop...
                    // ==> bad cic calculation ?? e.g. 1.txt line 394
                    // FIXME Wrong mangling cycles ? cycles.c gives 18500 cic pulses, actual wait is 17500 cic pulses ?!
                }

                // TODO Just time the GPIO output with cpu instructions as it may be too short to actually count cic cycles here ?
                wait_next += 4 * 4; // FIXME 3 * 4;
                wait_next += 8 * 4; // FIXME
                wait_until_clock_pulses(wait_next);
                // Clear output
                gpio_put(swap ? SNES_CIC_P1_PIN : SNES_CIC_P2_PIN, 0);
                gpio_put(DEBUG_PIN, 0);
                if (restart == 16) {
                    timings_end[idx++] = ~cic_clock_count - 8*4;
                }

                wait_clock_pulses(72-8/*FIXME*/);  // TODO how long to wait before checking ??
                // TODO Both pins must be low when no bit transfer takes place
                // If not -> restart cic emulation
                if ((gpio_get_all64() & SNES_CIC_IO_PINS_MASK) != 0) {
                    // TODO Replace goto with break and a flag ??
                    //goto cic_die;
                    printf("CIC dying because data lines are not low between pulses\n");
                    die = true;
                    break;
                }
                
                wait_next += (81-8/*FIXME*/) * 4;    // FIXME 82 * 4;
            }
            if (die) {
                break;
            }

            // TODO already added 82 cycles --> only add diff: 12 cycles ???
            wait_next += 12 * 4;    //11 * 4 - 1;    // FIXME 12 * 4;    // FIXME 94 * 4;
            
            // Mangle seeds and count expected cycles
            uint mangle_cycles = mangle(lockseed);
            mangle_cycles += mangle(keyseed);
            mangle_cycles += mangle(lockseed);
            mangle_cycles += mangle(keyseed);
            mangle_cycles += mangle(lockseed);
            mangle_cycles += mangle(keyseed);
            wait_next += mangle_cycles * 4;
            printf("mangle cycles = %d\n", mangle_cycles);
            mangles[idx] = mangle_cycles;
            //printf("mangle cycles = %d --> %d vs %d / 0x%08x\n", mangle_cycles, wait_next, ~cic_clock_count, cic_clock_count);

            // FIXME With seed 0xc --> 55 * 4 cycles too fast between first and second iteration (61.625 us)
            //wait_next += 55 * 4;    // FIXME
            // Account for mangle function calls overhead (10 cycles for each key-lock pair --> 30 cycles)
            wait_next += 10 * 3 * 4; // FIXME 18 * 4; // FIXME Call overhead for mangle (3 * 6 cycles) ??

            restart = keyseed[7];
            swap = restart & 1;

            // FIXME Need to wait before swapping pins?
            wait_until_clock_pulses(wait_next);
            // Swap GPIO input/output
            gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, swap ? SNES_CIC_P1_PIN_MASK : SNES_CIC_P2_PIN_MASK);
            gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);

            // TODO ???
            // Account for end of 'loop0'
            wait_next += 15 * 4; // end of "loop0"
            
            if (!restart) {
                restart = 1;
                // Account for 'loop' overhead
                wait_next += 2 * 4; // FIXME 1 * 4; // "loop" overhead cycles
            }
        }

    cic_die:
        printf("CIC died...\n");


        for (uint16_t i=0; i<=idx; i++) {
            uint32_t diff = timings[i];
            if (i > 0) {
                diff -= timings_end[i-1];
            }
            printf("#%d -> %d %d / %d / %d %d %d\n", i, mangles[i], mangles[i]*4, delays[i], timings[i], timings_end[i], diff);
        }

        // TODO Switch region PAL/NTSC ??

        gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);
        gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);
        gpio_put(DEBUG_PIN, 1);

        // TODO Uninit DMA and PIO + Loop back to CIC init

        // disable both dma channels
        // abort both dma channels
        dma_channel_abort(dma_chan);
        dma_channel_abort(dma_chan2);
        // TODO cleanup ?
        dma_channel_cleanup(dma_chan);
        dma_channel_cleanup(dma_chan2);
        // unclaim/free both dma channels
        dma_channel_unclaim(dma_chan);
        dma_channel_unclaim(dma_chan2);

        // stop pio program
        pio_sm_set_enabled(pio, sm, false);
        // unclaim/free pio
        pio_remove_program_and_unclaim_sm(&counter_program, pio, sm, offset);

        gpio_put(DEBUG_PIN, 0);
    }
}


int main() {
    // Overclock
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(330000, true);

    stdio_init_all();

    // Start core1 with CIC emulator
    multicore_launch_core1(core1_entry);

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

    // Erratum E9: disable IE
    /*for (int pin = 0; pin < 34; pin++) {
        gpio_set_input_enabled(pin, false);
    }*/
//    gpio_disable_ie();

    romtype = rom[0x7fd5] & 0x0f;  // 0: LoROM, 1: HiROM, 5: ExHiROM
    if (romtype == 0) { // LoROM
        printf("ROM type: LoROM\n");
    } else if (romtype == 1) {  // HiROM
        printf("ROM type: HiROM\n");
    } else {
        printf("ROM type: ExHiROM\n");
    }

    printf("Waiting for SNES to boot...\n");

    // Erratum E9: enable IE just before reading, disable right after reading
    /*
    uint64_t input = 0;
    do {
        gpio_set_input_enabled(32, true);
        gpio_set_input_enabled(33, true);
        input = gpio_get_all64();
        gpio_set_input_enabled(32, false);
        gpio_set_input_enabled(33, false);
    } while ((input & SNES_CTRL_PINS_MASK) == 0);
    */
    while((gpio_get_all64() & SNES_CTRL_PINS_MASK) == 0) {
        tight_loop_contents();
    }


#ifdef DEBUG
    //while (counter++ < BUFFER_SIZE) {
    #ifdef DEBUG_KEEPALIVE
        while (true) {
    #else
        while (counter++ < COUNTER_THRESHOLD) {
    #endif
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

        // Erratum E9: enable IE just before reading, disable right after reading
        /*input = 0;
        do {
            gpio_set_input_enabled(32, true);
            gpio_set_input_enabled(33, true);
            input = gpio_get_all64();
            gpio_set_input_enabled(32, false);
            gpio_set_input_enabled(33, false);
        } while ((input & SNES_CTRL_PINS_MASK) != 0);
        */
        
        while((gpio_get_all64() & SNES_CTRL_PINS_MASK) != 0) {
             tight_loop_contents();
        }
        
        
        // Erratum E9: enable IE just before reading, disable right after reading        
        /*input = 0;
        for (int pin = 0; pin < 24; pin++) {
            gpio_set_input_enabled(pin, true);
        }*/
        //gpio_set_input_enabled(12, true);
//        gpio_enable_ie();
        uint32_t address = (gpio_get_all64() & SNES_ADDR_PINS_MASK);
        /*for (int pin = 0; pin < 24; pin++) {
            gpio_set_input_enabled(pin, false);
        }*/
        //gpio_set_input_enabled(23, false);
        //printf("Data requested. Address=%06x\n", address);

        uint32_t data_location_in_rom = map_address_to_rom(address);
        uint8_t data = 0xff;
        if (data_location_in_rom > rom_size) {
            // TODO out of bounds!!!
            printf("Out of bound! Address=%06x LocationInRom=%04x\n", address, data_location_in_rom);

    #ifdef DEBUG
            addresses[(counter-1)%BUFFER_SIZE] = address;
            datas[(counter-1)%BUFFER_SIZE] = data;
            datas_out[(counter-1)%BUFFER_SIZE] = data << SNES_DATA_PINS_SHIFT;
    #endif

            break;
        } else {
            data = rom[data_location_in_rom];
        }
        //uint8_t data = rom[data_location_in_rom]; //(address & 0xf000) == 0xf000 ? rom[address & 0x7fff] : rom[address & 0x6fff];    // FIXME
        //printf("Data=%02x\n", data);

        uint64_t data_out = data << SNES_DATA_PINS_SHIFT;

        gpio_set_dir_out_masked64(SNES_DATA_PINS_MASK);
        gpio_put_masked64(SNES_DATA_PINS_MASK, data_out);


        //gpio_set_input_enabled(12, false);
//        gpio_disable_ie();

#ifdef DEBUG
        if (counter > 1 && addresses[(counter-2)%BUFFER_SIZE] == address && datas[(counter-2)%BUFFER_SIZE] == data && datas_out[(counter-2)%BUFFER_SIZE] == data_out) {
            // Count repetitions
            counter--;
            repetition[(counter-1)%BUFFER_SIZE]++;
        } else {
            addresses[(counter-1)%BUFFER_SIZE] = address;
            datas[(counter-1)%BUFFER_SIZE] = data;
            datas_out[(counter-1)%BUFFER_SIZE] = data_out;
        }
#endif

        // Wait for /RD to go HIGH
        // Erratum E9: enable IE just before reading, disable right after reading
        /*input = 0;
        do {
            gpio_set_input_enabled(32, true);
            gpio_set_input_enabled(33, true);
            input = gpio_get_all64();
            gpio_set_input_enabled(32, false);
            gpio_set_input_enabled(33, false);
        } while ((input & SNES_RD_PIN_MASK) == 0);
        */
        while((gpio_get_all64() & SNES_RD_PIN_MASK) == 0) {
            tight_loop_contents();
        }
        

        // FIXME when to clear data bus ??

        // TODO set as high-impedance ?!
        gpio_set_dir_in_masked64(SNES_DATA_PINS_MASK);
        //gpio_clr_mask64(SNES_DATA_PINS_MASK);
    }

#ifdef DEBUG
    for (int i=0; i<BUFFER_SIZE && i<COUNTER_THRESHOLD && i<counter; i++) {
        if (repetition[i] > 0) {
            printf("#%05d: %06x -> rom[%06x] = %02x (%08x) [repeated %d times]\n", i, addresses[i], map_address_to_rom(addresses[i]), datas[i], datas_out[i], repetition[i]+1);
        } else {
            printf("#%05d: %06x -> rom[%06x] = %02x (%08x)\n", i, addresses[i], map_address_to_rom(addresses[i]), datas[i], datas_out[i]);
        }
    }

    sleep_ms(5000);
#endif

    while(true) {
        tight_loop_contents();
    }
    return 0;
}
