@****************************************************
@*    gp32 crt0.S v1.0 by Jeff F                    *
@*    gp32 crt0.S v2.0 by Mr.Spiv                   *
@****************************************************

@ v1.0 - Original release
@ v1.1 - Removed support for user_init.o by default
@        to keep from having to link to a usually empty file
@ v2.0 - adds 32M/64M support (assumes a patched libc
@        though) and also assumes that your BIOS handles
@        32M/64M RAM setup..
@
@ This file is released into the public domain for commercial
@ or non-commercial usage with no restrictions placed upon it.

    .TEXT

@ Note: Normally it is the job of crt0.S to clear the BSS
@ (Zero Initialized) section to 0x00, but in the case of
@ the gp32 we do not have to do this because it is done
@ by the gp32 bios after it loads the app.

@ The official sdt dev kit uses 'Main ()' as the entry
@ point. If you would rather use 'main ()' instead then
@ comment out the next line.
@
@ You have to use 'main ()' at some point in your program
@ if you want to do c++ code. GCC will do a call to constructor
@ setup before executing 'main ()'. Using 'main ()' also increases
@ your program size by ~5500 bytes.

@ .equ __OfficialEntry, 1

@ The official sdt dev kit initializes various things in init.o
@ Crt0.S performs similar tasks for compatibility. If you don't
@ wish to use the official libs, or those that are compatible,
@ then you need to comment out the next line to prevent link errors.

@ .equ __OfficialInits, 1

@ The official sdt dev kit allows the user to initializes various
@ things in by calling user_init.o  Since most don't need this
@ feature it is disabled by default to keep from having to always
@ link to a usually empty file. Uncomment the nect line to enable
@ that feature.

@ .equ __OfficialUserInits, 1

@
@ This switch enables memory checking code that searcher
@ for the additional 32M/64M RAM and patches libc variables
@ acording to it.
@

@.equ __ExtraRAMCheck, 1



    .GLOBAL     _start
	@.GLOBAL		sp
_start:
        .ALIGN
        .CODE 32

    @ Start Vector

        b       _GpInit

        .word   __text_start    @ Start of text (Read Only) section
_roe:   .word   __ro_end        @ End "
_rws:   .word   __data_start    @ Start of data (Read/Write) section
        .word   __bss_end       @ End of bss (this is the way sdt does it for some reason)
_zis:   .word   __bss_start     @ Start of bss (Zero Initialized) section
_zie:   .word   __bss_end       @ End "

        .word   0x44450011
        .word   0x44450011

        .word   0x01234567
        .word   0x12345678
        .word   0x23456789
        .word   0x34567890
        .word   0x45678901
        .word   0x56789012
        .word   0x23456789
        .word   0x34567890
        .word   0x45678901
        .word   0x56789012
        .word   0x23456789
        .word   0x34567890
        .word   0x45678901
        .word   0x56789012

_GpInit:
        mrs r0,CPSR
        orr r0,r0,#0xc0
        msr CPSR_fsxc,r0

.ifdef __OfficialInits

@ Get pointer to GpSurfaceSet routine
        mov r0,#0
        swi 0xb
        ldr r1,=GpSurfaceSet
        ldr r2,=GpSurfaceFlip
        str r0,[r1]
        str r0,[r2]

@ Get time passed
        mov r0,#6
        swi 0xb
        ldr r1,=_timepassed
        str r0,[r1]

@ Get button stuff
        mov r0,#0
        swi 0x10
        ldr r2,=_reg_io_key_a
        ldr r3,=_reg_io_key_b
        str r0,[r2]
        str r1,[r3]

@ Set heap start location
        ldr r0,_zie
        ldr r1,=HEAPSTART
        str r0,[r1]

@ Set heap end location
        mov r0,#5
        swi 0xb
        ldr r1,=HEAPEND
        sub r0,r0,#255
        bic r0,r0,#3
        str r0,[r1]

@ Set App Argument
        swi 0x15
		mov	r10,r0
		mov	r11,r1
		
@ Set SDK compatibility
		ldr	r1,=fake_allow_sdk
		mov	r0,#1
		str	r0,[r1]

@ Check available RAM..
.ifdef __ExtraRAMCheck
		bl	checkRAM
.endif

        mrs r0,CPSR
        bic r0,r0,#192
        orr r0,r0,#64
        msr CPSR_fsxc,r0

        mov r0,r10
        mov r1,r11       @ possibly not needed but left in anyway
		@

.else


@ Check available RAM..
.ifdef __ExtraRAMCheck
		bl	checkRAM
.endif
        mov r0,#5
        swi 0xb
        sub r0,r0,#255
        bic sp,r0,#3
.endif

@ Jump to Main ()

.ifdef __OfficialEntry
        ldr     r3,=Main
.else
        ldr     r3,=main
.endif
        bx  r3           @ Init.o uses 'mov pc,r3' but
                         @ 'bx r3' is used here instead. This way
                         @ the main function can be ARM or Thumb.

		swi		#4

@
@
@
@
.ifdef __ExtraRAMCheck

checkRAM:
		stmdb   sp!, {r4-r12,lr}

		@ Disable MMU & DCache & ICache
		mrc	p15,0,r0,c1,c0,0
		bic	r0,r0,#0x1000
		bic	r0,r0,#0x0005
		mcr	p15,0,r0,c1,c0,0

		@ Drain WB
		mov	r3,#0
		mcr	p15,0,r3,c7,c10,4

		@ Flush DCache
		mov		r1,#0
0:		mov		r2,#0
1:		mov		r0,r2,lsl #5
		orr		r0,r0,r1,lsl #26
		mcr		p15,0,r0,c7,c14,2
		add		r2,r2,#1
		cmp		r2,#8
		bne		1b
		add		r1,r1,#1
		cmp		r1,#64
		bne		0b

		@ Flush ICache
		mcr	p15,0,r3,c7,c5,0

		@
		@ Check RAM..
		mov     r0, #0x14000000
		@ get memregs
		ldmia   r0!,{r1-r12,r14}
		@ patch BANK7 to same as BANK6
		mov             r9,r8
		@ write memregs back..
		stmdb   r0!,{r1-r12,lr}
		@ check mem
		mov		r0,#0x0c800000
		mov		r1,#7
		mov		r2,#0x01
		mov		r4,r0
		@
loop:
		str		r2,[r0]
		nop
		nop
		ldr		r3,[r0]
		cmp		r2,r3
		bne		done
		add		r0,r0,#0x00800000
		add		r2,r2,#0x11
		subs    r1,r1,#1
		bne		loop
done:
		cmp		r0,r4
		beq		skip
		ldr		r1,=fake_heap_end
		str		r4,[r1]
		ldr		r1,=fake_stack_ptr
		sub		r0,r0,#256
		str		r0,[r1]

		@ Enable MMU & DCache & ICache
skip:
		mrc	p15,0,r0,c1,c0,0
		orr	r0,r0,#0x1000
		orr	r0,r0,#0x0005
		mcr	p15,0,r0,c1,c0,0

		@
		ldmia   sp!, {r4-r12, pc}
.endif

    .ALIGN
    .POOL


    .END

