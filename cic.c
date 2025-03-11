#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include "cic.h"
#include "pins.h"
#include "counter.pio.h"

#define CIC_EXTRA_CYCLES_LEFT_RIGHT 8

volatile uint32_t cic_clock_count = 0xffffffff;
uint32_t wait_next;
unsigned char restart, swap;
uint8_t seed;
unsigned char lockseed[16];
unsigned char keyseed[16];

const unsigned char lockseed_init[16] = {
    0x0,    // dummy
    0xb,0x1,0x4,0xf,
    0x4,0xb,0x5,0x7,
    0xf,0xd,0x6,0x1,
    0xe,0x9,0x8
};
const unsigned char keyseed_init[16] = {
    0x0,    // dummy
    0x0,    // fill-in value sent by lock on reset in bit order 3-0-1-2
#ifdef CIC_REGION_PAL
    0x6,0xa,0x1,    // PAL
#else
    0x9,0xa,0x1,    // NTSC
#endif
    0x8,0x5,0xf,0x1,
    0x1,0xe,0x1,0x0,
    0xd,0xe,0xc
};

// PIO counter
PIO pio;
uint offset;
uint sm;

// DMA counter
int dma_chan, dma_chan2;


// From https://github.com/mrehkopf/sd2snes/blob/develop/cic/mangle.c
#define CARRY	if(a>0x0f) {\
			a &= 0xf;\
			carry=1;\
		} else carry=0;

#define CYCLES_COUNT_SKIP (84)
#define CYCLES_COUNT_NOSKIP (78)

uint __not_in_flash_func(mangle)(unsigned char* data) {
	unsigned char a,x,temp,i,offset,carry=0;
	a=data[0xf];
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
            // "no-skip" cycles count (78)
            cycles += CYCLES_COUNT_NOSKIP;
			temp=a; a=data[offset]; data[offset++]=temp&0xf;
		} else {
            // Else "skip" cycles count (84)
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

static inline void __not_in_flash_func(wait_until_clock_pulses)(uint32_t until) {
    // TODO Handle counter overflow/underflow?
    while(~cic_clock_count < until) {
        tight_loop_contents();
    }
}

// FIXME Deprecate?
static inline void __not_in_flash_func(wait_clock_pulses)(uint32_t count) {
    wait_until_clock_pulses(~cic_clock_count + count);
}

bool init_cic() {
    // CIC Emulation
#ifdef ENABLE_UART
    printf("Starting CIC emulation...\n");
#endif

#ifdef CIC_DEBUG
    gpio_set_dir_masked64(DEBUG_PINS_MASK, DEBUG_PINS_MASK);
    gpio_put_masked64(DEBUG_PINS_MASK, 0x0000000000000000);
    gpio_put(DEBUG_PIN, 1);
#endif

    // Setup clock pulses counter (CIC clock may be 4MHz, 3.58MHz, 3.54MHz, or 3.072MHz)
    cic_clock_count = 0xffffffff;
    seed = 0;
    restart = 1;
    swap = 0;

    memcpy(lockseed, lockseed_init, sizeof(lockseed_init));
    memcpy(keyseed, keyseed_init, sizeof(keyseed_init));

    // Setup PIO counter
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&counter_program, &pio, &sm, &offset, SNES_CIC_CLK_PIN, 1, true);
    if (!success) {
#ifdef ENABLE_UART
        printf("Failed to initialize PIO\n");
#endif
        return false;
    }
    counter_program_init(pio, sm, offset, SNES_CIC_CLK_PIN);

    // Setup counter DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_chan2 = dma_claim_unused_channel(true);

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

    // GPIO setup: initial configuration: P1 is key output, P2 is key input (Pin 55 (P2) is **GPIO 0** in PIC CIC clone code)
    // Lock sends seed over P2 / D2 / Pin55
    gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, SNES_CIC_P1_PIN_MASK);
    gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);

    // Wait for CIC RST to raise
#ifdef ENABLE_UART
    printf("Waiting for CIC reset...\n");
#endif
    while((gpio_get_all64() & SNES_CIC_RST_PIN_MASK) == 0) {
        tight_loop_contents();
    }
    // Start counting clock pulses on RST falling edge
    while((gpio_get_all64() & SNES_CIC_RST_PIN_MASK) != 0) {
        tight_loop_contents();
    }
    pio_sm_set_enabled(pio, sm, true);

#ifdef CIC_DEBUG
    //printf("CIC start\n");
    gpio_put(DEBUG_PIN, 0);
#endif

    // Read seed nibble a.k.a. stream id (transmission bit order is 3-0-1-2. e.g. '1-0-1-1' => 0xe)
    wait_next = 632 * 4;    // FIXME 635 * 4;
    wait_until_clock_pulses(wait_next);
#ifdef CIC_DEBUG
    gpio_put(DEBUG_PIN, 1);
#endif
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
#ifdef CIC_DEBUG
    gpio_put(DEBUG_PIN, 0);
#endif

    //printf("CIC seed=0x%01x\n", keyseed[1]);

    seed = keyseed[1];

    // Output the first values on D2
    gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, SNES_CIC_P2_PIN_MASK);
    gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);

    wait_next += 130 * 4;   // FIXME 134 * 4;

    return true;
}

void __not_in_flash_func(loop_cic)() {
    // Output up to 15 keyseed bits in locksteps, mangling seed whenever we reach the last bit
    while (true) {
        wait_next += 2 * 4;
        // Output keyseed (starting at index 'restart') until last bit is reached
        bool first = true;
        bool adjusted = false;
        bool die = false;
        while (restart < 16) {
            unsigned char output = keyseed[restart] & 1;
            unsigned char input = lockseed[restart] & 1;
            wait_until_clock_pulses(wait_next);
            wait_next += CIC_EXTRA_CYCLES_LEFT_RIGHT * 4;
            // Output keyseed bit to P2 or P1
#ifdef CIC_DEBUG
            gpio_put(DEBUG_PIN, 1);
#endif
            gpio_put(swap ? SNES_CIC_P1_PIN : SNES_CIC_P2_PIN, output);
            restart++;

            // If we expect to receive a '1' from the lock, measure our drift and adjust
            if (/*!adjusted &&*/ input != 0) {
                uint32_t now = ~cic_clock_count;
                // Timeout and die in case we never read the expected '1'
                uint32_t timeout = now + ((CIC_EXTRA_CYCLES_LEFT_RIGHT*2 + 4)*4);    // We output the value for 20 instruction cycles (80 cic pulses). No need to wait longer, we're already cooked...
                while((gpio_get_all64() & (swap ? SNES_CIC_P2_PIN_MASK : SNES_CIC_P1_PIN_MASK)) == 0 && (~cic_clock_count) < timeout) {
                    tight_loop_contents();
                }
                if ((~cic_clock_count) >= timeout) {
#ifdef ENABLE_UART
                    printf("CIC dying because we did not receive the expected high pulse from lock\n");
#endif
                    die = true;
                    break;
                }
                uint32_t delay_cycles = (~cic_clock_count) - now;
                if (delay_cycles < (CIC_EXTRA_CYCLES_LEFT_RIGHT*4)-4) {
                    // We are drifting late, let's shorten our delay by 3*4=12 cycles
                    wait_next -= 1 * 4;
                } else if (delay_cycles > (CIC_EXTRA_CYCLES_LEFT_RIGHT*4)+4) {
                    // We are drifting early, let's lengthen our delay by 3*4=12 cycles
                    wait_next += 1 * 4;
                }
                adjusted = true;
            }

            wait_next += 4 * 4; // FIXME 3 * 4;
            wait_next += CIC_EXTRA_CYCLES_LEFT_RIGHT * 4; // FIXME
            wait_until_clock_pulses(wait_next);
            // Clear output
            gpio_put(swap ? SNES_CIC_P1_PIN : SNES_CIC_P2_PIN, 0);
#ifdef CIC_DEBUG
            gpio_put(DEBUG_PIN, 0);
#endif

            wait_clock_pulses(72-CIC_EXTRA_CYCLES_LEFT_RIGHT);
            // Both pins must be low when no bit transfer takes place (if not, restart cic emulation)
            // TODO This is where we would switch region?
            if ((gpio_get_all64() & SNES_CIC_IO_PINS_MASK) != 0) {
#ifdef ENABLE_UART
                printf("CIC dying because data lines are not low between pulses\n");
#endif
                die = true;
                break;
            }
            
            wait_next += (81-CIC_EXTRA_CYCLES_LEFT_RIGHT) * 4;    // FIXME 82 * 4;
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

        // Account for mangle function calls overhead (10 cycles for each key-lock pair --> 30 cycles)
        wait_next += 10 * 3 * 4; // FIXME 18 * 4;

        restart = keyseed[7];
        swap = restart & 1;

        // Wait before swapping pins
        wait_until_clock_pulses(wait_next);
        // Swap GPIO input/output
        gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, swap ? SNES_CIC_P1_PIN_MASK : SNES_CIC_P2_PIN_MASK);
        gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);

        // Account for end of 'loop0'
        wait_next += 15 * 4; // end of "loop0"
        
        if (!restart) {
            restart = 1;
            // Account for 'loop' overhead
            wait_next += 2 * 4; // FIXME 1 * 4;
        }
    }
}

void deinit_cic() {
#ifdef ENABLE_UART
        printf("CIC died...\n");
        printf("seed=0x%01x\n", seed);
#endif

        // TODO Switch region PAL/NTSC ??

        gpio_set_dir_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);
        gpio_put_masked64(SNES_CIC_IO_PINS_MASK, 0x0000000000000000);
#ifdef CIC_DEBUG
        gpio_put(DEBUG_PIN, 1);
#endif

        // disable both dma channels
        // abort both dma channels
        dma_channel_abort(dma_chan);
        dma_channel_abort(dma_chan2);
        // cleanup both dma channels
        dma_channel_cleanup(dma_chan);
        dma_channel_cleanup(dma_chan2);
        // unclaim/free both dma channels
        dma_channel_unclaim(dma_chan);
        dma_channel_unclaim(dma_chan2);

        // stop pio program
        pio_sm_set_enabled(pio, sm, false);
        // unclaim/free pio
        pio_remove_program_and_unclaim_sm(&counter_program, pio, sm, offset);

#ifdef CIC_DEBUG
        gpio_put(DEBUG_PIN, 0);
#endif
}
