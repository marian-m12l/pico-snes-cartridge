#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>


// TODO From https://github.com/mrehkopf/sd2snes/blob/develop/cic/mangle.c

#define CARRY	if(a>0x0f) {\
			a &= 0xf;\
			carry=1;\
		} else carry=0;

#define CYCLES_COUNT_SKIP (84)
#define CYCLES_COUNT_NOSKIP (78)

uint32_t mangle(unsigned char* data) {
	unsigned char a,x,temp,i,offset,carry=0;
	a=data[0xf];
    // TODO Count cycles / iterations ?
    uint32_t cycles = 0;
    uint32_t iterations = 0;
    uint32_t iterations_skip = 0;
    uint32_t iterations_noskip = 0;
	do {
        iterations++;

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
            iterations_noskip++;
            cycles += CYCLES_COUNT_NOSKIP;
			temp=a; a=data[offset]; data[offset++]=temp&0xf;
		} else {
            // TODO Else "skip" cycles count (84)
            iterations_skip++;
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

    printf("mangle iterations: %d + %d = %d\n", iterations_skip, iterations_noskip, iterations);

    return cycles;
}


int main() {
    // CIC Emulation
    printf("Starting CIC emulation...\n");

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

    // Wait for CIC RST to raise
    printf("Waiting for CIC reset...\n");
    
    // Read seed nibble a.k.a. stream id (transmission bit order is 3-0-1-2. e.g. '1-0-1-1' => 0xe)
    keyseed[1] = 0x7;   //0x8;   //0xc;

    printf("CIC seed=0x%01x\n", keyseed[1]);

    // Output up to 15 keyseed bits in locksteps, mangling seed whenever we reach the last bit
    while (true) {
        // Output keyseed (starting at index 'restart') until last bit is reached
        unsigned char s = restart;
        while (restart < 16) {
            // FIXME DEBUG
            char output = keyseed[restart] & 1; // 1;
            printf("%01x ", output);
            restart++;
        }
        printf("\n");
        while (s < 16) {
            // FIXME DEBUG
            char output = lockseed[s] & 1; // 1;
            printf("%01x ", output);
            s++;
        }
        printf("\n");
        sleep(0.1f);
        
        // Mangle seeds and count expected cycles
        uint32_t mangle_cycles = mangle(lockseed);
        mangle_cycles += mangle(keyseed);
        mangle_cycles += mangle(lockseed);
        mangle_cycles += mangle(keyseed);
        mangle_cycles += mangle(lockseed);
        mangle_cycles += mangle(keyseed);
        printf("mangle cycles = %d\n", mangle_cycles);
        // 94 instructions between last output and mangle
        // 10 instructions overhead for each call to mangle (lock+key) --> 30 instructions
        // 15 (or 16) instructions overhead loop0
        // 2 + 8 instuctions start of loop0 untils gpio output
        printf("total cycles = %d\n", (mangle_cycles+94+10*3+15+2+8)*4);

        // seed 8: 21380 (21392) --> expected 21928 ==> +548 (137 instruction cycles !!!)
        
        restart = keyseed[7];
        swap = restart & 1;

        if (!restart) {
            restart = 1;
            // FIXME Add 2 cycles !!
        }
    }

    return 0;
}
