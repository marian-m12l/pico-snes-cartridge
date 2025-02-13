    #include <p12f629.inc>
processor p12f629

; ---------------------------------------------------------------------
;   SNES CIC clone for PIC Microcontroller (key mode only)
;
;   Copyright (C) 2010 by Maximilian Rehkopf (ikari_01) <otakon@gmx.net>
;   This software is part of the sd2snes project.
;
;   Last Modified: Oct. 2015 by Peter Bartmann <borti4938@gmx.de>
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation; version 2 of the License only.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;
; ---------------------------------------------------------------------
;
;   pin configuration: (cartridge pin) [key CIC pin]
;
;                       ,---_---.
;      +5V (27,58) [16] |1     8| GND (5,36) [8]
;      CIC clk (56) [6] |2     7| CIC data i/o 0 (55) [2]
;            status out |3     6| CIC data i/o 1 (24) [1]
;                    nc |4     5| CIC slave reset (25) [7]
;                       `-------'
;
;
;   Status out can be connected to a LED. It indicates:
;
;   state                   | output
;  -------------------------+--------------------
;   OK (normal operation)   | high
;   error (unlock failed)   | alternating @~2.5Hz
;   no CIC (modded SNES)    | low
;
;   In case lockout fails, the region is switched automatically and
;   will be used after the next reset.
;
;   memory usage:
;
;   0x20		buffer for seed calc and transfer
;   0x21 - 0x2f		seed area (lock seed)
;   0x30		buffer for seed calc
;   0x31 - 0x3f		seed area (key seed; 0x31 filled in by lock)
;   0x40 - 0x41		buffer for seed calc
;   0x4d		buffer for eeprom access
;   0x4e		loop variable for longwait
;   0x4f		loop variable for wait
;
; ---------------------------------------------------------------------


; -----------------------------------------------------------------------
    __CONFIG _EC_OSC & _WDT_OFF & _PWRTE_OFF & _MCLRE_OFF & _CP_OFF & _CPD_OFF

; -----------------------------------------------------------------------



; clock from CIC (4MHz, 3.58MHz, 3.54MHz, 3.072MHz)
; 1 instruction cycle = 4 clock cycles



; code memory

	org	0x0000      ; RESET vector
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	goto	init    ;[2]

isr
    ; Interrupt called on FALLING EDGE of GP2 (CIC RESET)
    ; Interrupt latency: [3-4] instruction cycles
	org	0x0004      ; INTERRUPT vector
	bcf	INTCON, 1	;[1]    ; clear interrupt cause
	bcf	GPIO, 0     ;[1]
	bcf	GPIO, 1     ;[1]
	bsf	GPIO, 4		;[1]    ; LED on
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	bsf	INTCON, 7	;[1]    ; re-enable interrupts (ISR will continue as main)
	goto	main    ;[2]
    ;(17)
    
init
	org 0x0010
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	clrf	GPIO    ;[1]
	movlw	0x07	;[1]    ; GPIO2..0 are digital I/O (not connected to comparator)
	movwf	CMCON   ;[1]
	movlw	0x90	;[1]    ; global enable interrupts + enable external interrupt
	movwf	INTCON  ;[1]
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	movlw	0x2d	;[1]    ; in out in in out in
	movwf	TRISIO  ;[1]
	movlw	0x24	;[1]    ; pullups for reset+clk to avoid errors when no CIC in host 
	movwf	WPU     ;[1]
	movlw	0x00	;[1]    ; 0x80 for global pullup disable
	movwf	OPTION_REG  ;[1]
	
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	bcf 	GPIO, 4	;[1]    ; LED off
idle
	goto	idle	;[2]    ; wait for interrupt from lock

main
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	bsf	TRISIO, 0   ;[1]
	bcf	TRISIO, 1   ;[1]
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
; --------INIT LOCK SEED (what the lock sends)--------
	movlw	0xb     ;[1]
	movwf	0x21    ;[1]
	movlw	0x1     ;[1]
	movwf	0x22    ;[1]
	movlw	0x4     ;[1]
	movwf	0x23    ;[1]
	movlw	0xf     ;[1]
	movwf	0x24    ;[1]
	movlw	0x4     ;[1]
	movwf 	0x25    ;[1]
	movlw	0xb     ;[1]
	movwf 	0x26    ;[1]
	movlw	0x5     ;[1]
	movwf 	0x27    ;[1]
	movlw	0x7     ;[1]
	movwf 	0x28    ;[1]
	movlw	0xf     ;[1]
	movwf 	0x29    ;[1]
	movlw	0xd     ;[1]
	movwf 	0x2a    ;[1]
	movlw	0x6     ;[1]
	movwf 	0x2b    ;[1]
	movlw	0x1     ;[1]
	movwf 	0x2c    ;[1]
	movlw	0xe     ;[1]
	movwf 	0x2d    ;[1]
	movlw	0x9     ;[1]
	movwf 	0x2e    ;[1]
	movlw	0x8     ;[1]
	movwf 	0x2f    ;[1]
    ;(53)
	
; --------INIT KEY SEED (what we must send)--------
	bsf     STATUS, RP0 ;[1]    ; D/F411 and D/F413
	nop                 ;[1]
	clrf	EEADR		;[1]    ; differ in 2nd seed nibble
	bsf 	EECON1, RD	;[1]    ; of key stream,
	movf	EEDAT, w	;[1]    ; restore saved nibble from EEPROM
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	movwf	0x32    ;[1]
	movlw	0xa     ;[1]
	movwf	0x33    ;[1]
	movlw	0x1     ;[1]
	movwf	0x34    ;[1]
	movlw	0x8     ;[1]
	movwf 	0x35    ;[1]
	movlw	0x5     ;[1]
	movwf 	0x36    ;[1]
	movlw	0xf     ;[1]
	movwf 	0x37    ;[1]
	movlw	0x1     ;[1]
	movwf 	0x38    ;[1]
	movwf 	0x39    ;[1]
	movlw	0xe     ;[1]
	movwf 	0x3a    ;[1]
	movlw	0x1     ;[1]
	movwf 	0x3b    ;[1]
	movlw	0x0     ;[1]
	movwf 	0x3c    ;[1]
	movlw	0xd     ;[1]
	movwf 	0x3d    ;[1]
	movlw	0xe     ;[1]
	movwf 	0x3e    ;[1]
	movlw	0xc     ;[1]
	movwf 	0x3f    ;[1]
    ;(86)

; wait = 3*(W-1)+7 cycles
	
; --------wait for stream ID--------
	movlw	0xb5    ;[1]    ; 0xb5 = 181
	call	wait    ;[2]    ; 3*180+7 = 547 instruction cycles
	clrf	0x31	;[1]    ; clear lock stream ID
    ;(635)

; --------lock sends stream ID. 15 cycles per bit--------
;	bsf	GPIO, 0		;[1]    ; (debug marker)
;	bcf	GPIO, 0		;[1]    ; 
	btfsc	GPIO, 0 ;[1|2]	; check stream ID bit
	bsf	0x31, 3		;[1]    ; copy to lock seed
	movlw	0x2		;[1]    ; wait=3*W+5
	call	wait	;[2]	; burn 11 cycles    --> 3*1+7 = 10 instruction cycles
	nop             ;[1]
	nop             ;[1]
    ;(650)

;	bsf	GPIO, 0     ;[1]
;	bcf	GPIO, 0     ;[1]
	btfsc	GPIO, 0	;[1|2]	; check stream ID bit
	bsf	0x31, 0		;[1]    ; copy to lock seed
	movlw	0x2		;[1]
	call	wait	;[2]	; burn 11 cycles
	nop             ;[1]
	nop             ;[1]
    ;(665)

;	bsf	GPIO, 0     ;[1]
;	bcf	GPIO, 0     ;[1]
	btfsc	GPIO, 0	;[1|2]	; check stream ID bit
	bsf	0x31, 1		;[1]    ; copy to lock seed
	movlw	0x2		;[1]
	call	wait	;[2]	; burn 11 cycles
	nop             ;[1]
	nop             ;[1]
    ;(680)

;	bsf	GPIO, 0     ;[1]
;	bcf	GPIO, 0     ;[1]
	btfsc	GPIO, 0	;[1|2]	; check stream ID bit
	bsf	0x31, 2		;[1]    ; copy to lock seed
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	bcf	TRISIO, 0   ;[1]
	bsf	TRISIO, 1   ;[1]
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	nop             ;[1]
	movlw	0x27	;[1]	; "wait" 1  --> 0x27 = 39
	call	wait	;[2]	; wait 121  --> 3*38+7 = 121
	nop             ;[1]
	nop             ;[1]
    ;(813)

; --------main loop--------
loop	
	movlw	0x1     ;[1]
loop0
	addlw	0x30	;[1]    ; key stream
	movwf	FSR	    ;[1]    ; store in index reg
loop1
	movf	INDF, w ;[1]    ; load seed value
	movwf	0x20    ;[1]
	bcf	0x20, 1	    ;[1]    ; clear bit 1 
	btfsc	0x20, 0 ;[1|2]  ; copy from bit 0
	bsf	0x20, 1     ;[1]    ; (if set)
	bsf	0x20, 4     ;[1]    ; LED on
	movf	0x20, w ;[1]
	movwf	GPIO    ;[1]    ; GPIO output (pin stays high (or low) for 3 instruction cycles)
	nop             ;[1]
	movlw	0x10    ;[1]
	movwf	GPIO	;[1]    ; reset GPIO
	movlw	0x16    ;[1]    ; 0x16 = 22
	call	wait    ;[2]    ; 3*21+7 = 70
	nop             ;[1]
	btfsc	GPIO, 0 ;[1|2]  ; both pins must be low...
	goto	die     ;[2]
	btfsc	GPIO, 1 ;[1|2]  ; ...when no bit transfer takes place
	goto	die	    ;[2]    ; if not -> lock cic error state -> die
	incf	FSR, f	;[1]    ; next one
	movlw	0xf     ;[1]
	andwf	FSR, w  ;[1]
	btfss	STATUS, Z   ;[1|2]
	goto	loop1   ;[2]    ; loop as long as FSR points to <= 0x3f
	movlw	0x2	    ;[1]    ; wait 10
	call	wait	;[2]    ; 3*1+7 = 10
	nop             ;[1]
	nop             ;[1]
	call	mangle  ;[2]    ; TODO cycles ? 2 + 4 + x*(78|84) + y*(78|84)
	call	mangle  ;[2]    ; TODO cycles ? 2 + 4 + x*(78|84) + y*(78|84)
	call	mangle  ;[2]    ; TODO cycles ? 2 + 4 + x*(78|84) + y*(78|84)
	btfsc	0x37, 0 ;[1|2]
	goto	swap    ;[2]
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	bcf	TRISIO, 0   ;[1]
	bsf	TRISIO, 1   ;[1]
	goto	swapskip    ;[2]
swap
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	bsf	TRISIO, 0   ;[1]
	bcf	TRISIO, 1   ;[1]
	nop             ;[1]
swapskip
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	movf	0x37, w ;[1]
	andlw	0xf     ;[1]
	btfss	STATUS, Z   ;[1|2]
	goto	loop0   ;[2]    ; loop back to 0x30+w
	goto	loop    ;[2]    ; if w is 0, set to 1 and loop back to 0x30+w

; --------calculate new seeds--------
; had to be unrolled because PIC has an inefficient way of handling
; indirect access, no post increment, etc.
mangle
	call	mangle_lock ;[2]
	nop             ;[1]
	nop             ;[1]
mangle_key
	movf	0x2f, w ;[1]
	movwf	0x20	;[1]
mangle_key_loop
	addlw	0x1     ;[1]
	addwf	0x21, f ;[1]
	movf	0x22, w ;[1]
	movwf	0x40    ;[1]
	movf	0x21, w ;[1]
	addwf	0x22, f ;[1]
	incf	0x22, f ;[1]
	comf	0x22, f ;[1]
	movf	0x23, w ;[1]
	movwf	0x41	;[1]    ; store 23 to 41
	movlw	0xf     ;[1]
	andwf	0x23, f ;[1]
	movf	0x40, w ;[1]    ; add 40(22 old)+23+#1 and skip if carry
	andlw	0xf     ;[1]
	addwf	0x23, f ;[1]
	incf	0x23, f ;[1]
	btfsc	0x23, 4 ;[1|2]
	goto	mangle_key_withskip ;[2]
mangle_key_withoutskip
	movf	0x41, w ;[1]    ; restore 23
	addwf	0x24, f ;[1]    ; add to 24
	movf	0x25, w ;[1]
	movwf	0x40	;[1]    ; save 25 to 40
	movf	0x24, w ;[1]
	addwf	0x25, f ;[1]
	movf	0x26, w ;[1]
	movwf	0x41	;[1]    ; save 26 to 41
	movf	0x40, w ;[1]    ; restore 25
	andlw	0xf	    ;[1]    ; mask nibble
	addlw	0x8	    ;[1]    ; add #8 to HIGH nibble
	movwf	0x40    ;[1]
	btfss	0x40, 4 ;[1|2]  ; skip if carry to 5th bit
	addwf	0x26, w ;[1]
	movwf	0x26    ;[1]

	movf	0x41, w ;[1]    ; restore 26
	addlw	0x1	    ;[1]    ; inc
	addwf	0x27, f	;[1]    ; add to 27

	movf	0x27, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x28, f ;[1]    ; add to 28

	movf	0x28, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x29, f ;[1]    ; add to 29

	movf	0x29, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2a, f ;[1]    ; add to 2a

	movf	0x2a, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2b, f ;[1]    ; add to 2b

	movf	0x2b, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2c, f ;[1]    ; add to 2c

	movf	0x2c, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2d, f ;[1]    ; add to 2d

	movf	0x2d, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2e, f ;[1]    ; add to 2e

	movf	0x2e, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2f, f ;[1]    ; add to 2f

	movf	0x20, w ;[1]    ; restore original 0xf
	andlw	0xf     ;[1]
	addlw	0xf     ;[1]
	movwf	0x20    ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	btfss	0x20, 4 ;[1|2]  ; skip if half-byte carry
	goto mangle_return ;[2] ; +2 cycles in return
	nop             ;[1]
	goto mangle_key_loop    ;[2]
; 69 when goto, 69 when return
; CIC has 78 -> 9 nops

mangle_key_withskip
	movf	0x41, w ;[1]    ; restore 23
	addwf	0x23, f ;[1]    ; add to 23
	movf	0x24, w ;[1]
	movwf	0x40	;[1]    ; save 24 to 40
	movf	0x23, w ;[1]
	addwf	0x24, f ;[1]
	movf	0x25, w ;[1]
	movwf	0x41	;[1]    ; save 25 to 41
	movf	0x40, w ;[1]    ; restore 24
	andlw	0xf	    ;[1]    ; mask nibble
	addlw	0x8	    ;[1]    ; add #8 to HIGH nibble
	movwf	0x40    ;[1]
	btfss	0x40, 4 ;[1|2]  ; skip if carry to 5th bit
	addwf	0x25, w ;[1]
	movwf	0x25    ;[1]

	movf	0x41, w ;[1]    ; restore 25
	addlw	0x1	    ;[1]    ; inc
	addwf	0x26, f	;[1]    ; add to 26

	movf	0x26, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x27, f ;[1]    ; add to 27

	movf	0x27, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x28, f ;[1]    ; add to 28

	movf	0x28, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x29, f ;[1]    ; add to 29

	movf	0x29, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2a, f ;[1]    ; add to 2a

	movf	0x2a, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2b, f ;[1]    ; add to 2b

	movf	0x2b, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2c, f ;[1]    ; add to 2c

	movf	0x2c, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2d, f ;[1]    ; add to 2d

	movf	0x2d, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2e, f ;[1]    ; add to 2e

	movf	0x2e, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x2f, f ;[1]    ; add to 2f

	movf	0x20, w ;[1]    ; restore original 0xf
	andlw	0xf     ;[1]
	addlw	0xf     ;[1]
	movwf	0x20    ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	btfss	0x20, 4 ;[1|2]  ; skip if half-byte carry
	goto mangle_return ;[2] ; +2 cycles in return
	nop             ;[1]
	goto mangle_key_loop ;[2]
mangle_return
	return          ;[2]
; 73 when goto, 73 when return
; CIC has 84 -> 11 nops

mangle_lock
	movf	0x3f, w ;[1]
	movwf	0x30	;[1]
mangle_lock_loop
	addlw	0x1     ;[1]
	addwf	0x31, f ;[1]
	movf	0x32, w ;[1]
	movwf	0x40    ;[1]
	movf	0x31, w ;[1]
	addwf	0x32, f ;[1]
	incf	0x32, f ;[1]
	comf	0x32, f ;[1]
	movf	0x33, w ;[1]
	movwf	0x41	;[1]    ; store 33 to 41
	movlw	0xf     ;[1]
	andwf	0x33, f ;[1]
	movf	0x40, w ;[1]    ; add 40(32 old)+33+#1 and skip if carry
	andlw	0xf     ;[1]
	addwf	0x33, f ;[1]
	incf	0x33, f ;[1]
	btfsc	0x33, 4 ;[1|2]
	goto	mangle_lock_withskip    ;[2]
mangle_lock_withoutskip
	movf	0x41, w ;[1]    ; restore 33
	addwf	0x34, f ;[1]    ; add to 34
	movf	0x35, w ;[1]
	movwf	0x40	;[1]    ; save 35 to 40
	movf	0x34, w ;[1]
	addwf	0x35, f ;[1]
	movf	0x36, w ;[1]
	movwf	0x41	;[1]    ; save 36 to 41
	movf	0x40, w ;[1]    ; restore 35
	andlw	0xf	    ;[1]    ; mask nibble
	addlw	0x8	    ;[1]    ; add #8 to HIGH nibble
	movwf	0x40    ;[1]
	btfss	0x40, 4 ;[1|2]  ; skip if carry to 5th bit
	addwf	0x36, w ;[1]
	movwf	0x36    ;[1]

	movf	0x41, w ;[1]    ; restore 36
	addlw	0x1	    ;[1]    ; inc
	addwf	0x37, f	;[1]    ; add to 37

	movf	0x37, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x38, f ;[1]    ; add to 38

	movf	0x38, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x39, f ;[1]    ; add to 39

	movf	0x39, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3a, f ;[1]    ; add to 3a

	movf	0x3a, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3b, f ;[1]    ; add to 3b

	movf	0x3b, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3c, f ;[1]    ; add to 3c

	movf	0x3c, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3d, f ;[1]    ; add to 3d

	movf	0x3d, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3e, f ;[1]    ; add to 3e

	movf	0x3e, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3f, f ;[1]    ; add to 3f

	movf	0x30, w ;[1]    ; restore original 0xf
	andlw	0xf     ;[1]
	addlw	0xf     ;[1]
	movwf	0x30    ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	btfss	0x30, 4 ;[1|2]  ; skip if half-byte carry
	goto mangle_return  ;[2]
	nop             ;[1]
	goto mangle_lock_loop   ;[2]
; 69 when goto, 69 when return
; CIC has 78 -> 9 nops
	
mangle_lock_withskip
	movf	0x41, w ;[1]    ; restore 33
	addwf	0x33, f ;[1]    ; add to 33
	movf	0x34, w ;[1]
	movwf	0x40	;[1]    ; save 34 to 40
	movf	0x33, w ;[1]
	addwf	0x34, f ;[1]
	movf	0x35, w ;[1]
	movwf	0x41	;[1]    ; save 35 to 41
	movf	0x40, w ;[1]    ; restore 34
	andlw	0xf	    ;[1]    ; mask nibble
	addlw	0x8	    ;[1]    ; add #8 to HIGH nibble
	movwf	0x40    ;[1]
	btfss	0x40, 4 ;[1|2]  ; skip if carry to 5th bit
	addwf	0x35, w ;[1]
	movwf	0x35    ;[1]

	movf	0x41, w ;[1]    ; restore 35
	addlw	0x1	    ;[1]    ; inc
	addwf	0x36, f	;[1]    ; add to 36

	movf	0x36, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x37, f ;[1]    ; add to 37

	movf	0x37, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x38, f ;[1]    ; add to 38

	movf	0x38, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x39, f ;[1]    ; add to 39

	movf	0x39, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3a, f ;[1]    ; add to 3a

	movf	0x3a, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3b, f ;[1]    ; add to 3b

	movf	0x3b, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3c, f ;[1]    ; add to 3c

	movf	0x3c, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3d, f ;[1]    ; add to 3d

	movf	0x3d, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3e, f ;[1]    ; add to 3e

	movf	0x3e, w ;[1]
	addlw	0x1	    ;[1]    ; inc
	addwf	0x3f, f ;[1]    ; add to 3f

	movf	0x30, w ;[1]    ; restore original 0xf
	andlw	0xf     ;[1]
	addlw	0xf     ;[1]
	movwf	0x30    ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	nop             ;[1]
	btfss	0x30, 4 ;[1|2]  ; skip if half-byte carry
	goto mangle_return  ;[2]
	nop             ;[1]
	goto mangle_lock_loop   ;[2]
; 73 when goto, 73 when return
; CIC has 84 -> 11 nops

; --------wait: 3*(W-1)+7 cycles (including call+return). W=0 -> 256!--------
wait	
	movwf	0x4f    ;[1]
wait0	decfsz	0x4f, f ;[1|2]
	goto	wait0   ;[2]
	return	        ;[2]

; --------wait long: 8+(3*(w-1))+(772*w). W=0 -> 256!--------
longwait
	movwf	0x4e    ;[1]
	clrw            ;[1]
longwait0
	call	wait    ;[2]
	decfsz	0x4e, f ;[1|2]
	goto	longwait0   ;[2]
	return          ;[2]

; --------change region in eeprom and die--------
die
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	clrw            ;[1]
	movwf	EEADR   ;[1]
	bsf	EECON1, RD  ;[1]
	movf	EEDAT, w    ;[1]
	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
	movwf	0x4d    ;[1]
	btfsc	0x4d, 0 ;[1|2]
	goto	die_reg_6   ;[2]
die_reg_9
	movlw	0x9	    ;[1]    ; died with PAL, fall back to NTSC
	goto	die_reg_cont    ;[2]
die_reg_6
	movlw	0x6	    ;[1]    ; died with NTSC, fall back to PAL
die_reg_cont
	bsf     STATUS, RP0 ;[1]
	nop             ;[1]
	movwf	EEDAT   ;[1]
	bsf	EECON1, WREN    ;[1]

die_intloop
	bcf	INTCON, GIE ;[1]
	btfsc	INTCON, GIE ;[1|2]
	goto	die_intloop ;[2]
	
	movlw	0x55    ;[1]
	movwf	EECON2  ;[1]
	movlw	0xaa    ;[1]
	movwf	EECON2  ;[1]
	bsf	EECON1, WR  ;[1]
	bsf	INTCON, GIE ;[1]

	bcf     STATUS, RP0 ;[1]
	nop             ;[1]
; --------forever: blink status pin--------
die_blink	
	clrw            ;[1]
	call	longwait    ;[2]
	bcf	GPIO, 4     ;[1]
	call	longwait    ;[2]
	bsf	GPIO, 4     ;[1]
	goto	die_blink   ;[2]
; -----------------------------------------------------------------------
; eeprom memory
DEEPROM	CODE
	de      0x09	; D411 (NTSC)
end