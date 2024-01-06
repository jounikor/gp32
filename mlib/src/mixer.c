//////////////////////////////////////////////////////////////////////////////
//
// Module:
//  mixer.c
//
// Description:
//  This module implements several example mixers.  Included mixer are:
//   1) signed 16 bits mixer (both C and ASM versions)
//   2) signed 8 bits mixer (both C and ASM versions)
//  All mixers are mono, that outputs the same sample on both channels.
//  C versions do simple interpolation between samples. To be honest these
//  mixers are far from correct ones in terms of proper signal processing.
//
//  These mixers do not depend on the libc or any other host system
//  dependant function.
//
// Note:
//
// Author:
//  (c) 2005 Jouni 'Mr.Spiv' Korhonen (jouni.korhonen@iki.fi)
//
// Version:
//  - v0.1  19-Jun-2005 Initial release
//
//////////////////////////////////////////////////////////////////////////////

#include "player.h"
#include "mixer.h"

//
// Defines used to select proper mixer code:
//  ASMMIXER - selects ARM assembly version of the mixers. ASM versions have
//             lower sound quality that C versions.
//  S8MIXER  - selects signed 8 bits PCM output instead of signed 16 bits
//             PCM output. 8 bits versions have lower sound quality than
//             16 bits versions.
//

#ifdef ASMMIXER

//
// Mix first into a 32bits buffer and when done convert into a 16bits
// output buffer..
//
// This assumes L+R stereo output..
//
// This mixer is in no means near to a corret one.. It has adequate output
// quality but cuts corners and thus introduces aliasing etc. 
//
// Be careful with this routine. It expects certain structure from the
// module structure defined in the player.h.
//
// PS: I really couldn't be arsed to get rid of the rest of the C code
//     in this function -- sowwy.
//

#ifndef S8MIXER

void mixer( struct module *m ) {
	int len, ch;
	short *d16; 
	int   *d32; 
  
	len = m->sbuf->len;
	d32 = m->sbuf->tmp;
	d16 = (short *)m->sbuf->buf[m->sbuf->frame];

	if (m->playing == 0) {
		asm volatile(""
		"	mov		r0,#0					\n"
		"	add		r1,%[_d],%[_l],lsl #1	\n"
		"loop%=:							\n"
		"	str		r0,[r1,#-4]!			\n"
		"	cmp		r1,%[_d]				\n"
		"	bgt		loop%=					\n"
		:
		: [_l]"r"(len),[_d]"r"(d16)
		: "r0","r1");
		return;
	}

	asm volatile(""
	"	mov		r0,#0					\n"
	"	add		r1,%[_d],%[_l],lsl #2	\n"
	"loop%=:							\n"
	"	str		r0,[r1,#-4]!			\n"
	"	str		r0,[r1,#-4]!			\n"
	"	cmp		r1,%[_d]				\n"
	"	bgt		loop%=					\n"
	:
	: [_l]"r"(len),[_d]"r"(d32)
	: "r0","r1");

	for (ch = 0; ch < MAX_SUPPORTED_CHANNELS; ch++) {
		int dx;

		if (!(m->playing & (1 << ch))) { continue; }

		asm volatile(""
		"	rsb	%[_b],%[_b],#0					\n"
		"	cmn	%[_a],%[_b],lsl #8				\n"
		"	mov	%[_b],%[_b],lsl #15				\n"
		"	movlo	%[_a],%[_a],lsl #7			\n"
		"	blo	skip%=							\n"

		"	adds	%[_a],%[_b],%[_a]			\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"skip%=:								\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		
		"	adc	%[_a],%[_a],%[_a]				\n"
		"	mov	%[_b],%[_a],lsr #16				\n"
		"	bic	%[_dx],%[_a],%[_b],lsl #16		\n"
		: [_dx]"=r"(dx)
		: [_a]"r"(m->sbuf->calcFreq << PRECISION),[_b]"r"(m->channels[ch].finalPeriod)
		);

		asm volatile(""
		"	add		r5,%[_m],#12		@ start			\n"
		"	ldmia	r5,{r5,r6,r7,r8}					\n"
		"	@ r5 = finalVolume							\n"
		"	@ r6 = length								\n"
		"	@ r7 = start								\n"
		"	@ r8 = pos									\n"
		"	mov		r1,%[_d]			@ r1 = d32		\n"
		"	mov		r4,%[_l],lsr #1		@ r4 = len		\n"
#if VOLUMESHIFT > 0
		"	mov		r5,r5,lsl %[VOL]					\n"
#endif
		"	mov		r6,r6,lsl %[PREC]					\n"
		"loop%=:										\n"
		"	add		r2,r7,r8,lsr %[PREC]				\n"
		"	ldrsb	r2,[r2]				@ r2 = smp		\n"
		"	add		r8,r8,%[_dx]		@ pos += dx		\n"
		"	cmp		r8,r6				@				\n"
		"	ldr		r0,[r1]				@ r0 = d32[n]	\n"
		"	blo		skip%=				@				\n"
		"	ldr		r3,[%[_m],#0]		@ get looped	\n"
		"	cmp		r3,#0				@				\n"
		"	streqh	r3,[%[_m],#8]		@ period = 0	\n"
		"	ldrne	r3,[%[_m],#4]		@				\n"
		"	moveq	r4,#0				@ len =0 -> break\n"
		"	movne	r8,r3,lsl %[PREC]	@ pos = ..		\n"
		"skip%=:										\n"
		"	mla		r3,r5,r2,r0			@ r3 = vol*...	\n"
		"	subs	r4,r4,#1			@				\n"
		"	str		r3,[r1],#4			@				\n"
		"	bgt		loop%=				@				\n"
		"	str		r8,[%[_m],#24]		@ store pos		\n"
		:
		: [_dx]"r"(dx),[_d]"r"(d32),[_m]"r"(&m->channels[ch]),[_l]"r"(len),
			[PREC]"i"(PRECISION),[VOL]"i"(VOLUMESHIFT)
		: "r0","r1","r2","r3","r4","r5","r6","r7","r8");

		if (m->channels[ch].period == 0) {
			m->playing &= ~(1 << ch);
		}
	}

	asm volatile(""
	"	mov		r1,%[_d16]							\n"
	"	mov		r2,%[_d32]							\n"
	"	mvn		r4,#0								\n"
	"loop%=:										\n"
	"	ldr		r3,[%[_d32]],#4						\n"
	"	cmp		r3,r4,lsr #17	@ r3 > 0x00007fff	\n"
     "	movgt	r3,r4,lsr #17	@ r3 = 0x00007fff	\n"
	"	cmplt	r3,r4,lsl #15	@ r3 < 0xffff8000	\n"
	"	movlt	r3,r4,lsl #15	@ r3 = 0xffff8000	\n"
	"	strh	r3,[%[_d16]],#2						\n"
	"	subs	%[_l],%[_l],#2						\n"
	"	strh	r3,[%[_d16]],#2						\n"
	"	bgt		loop%=								\n"
	: 
	: [_d16]"r"(d16),[_d32]"r"(d32),[_l]"r"(len)
	: "r1","r2","r3","r4");
}

#else	// S8MIXER

//
// Mix first into a 32bits buffer and when done convert into a 8bits
// output buffer..
//
// This assumes L+R stereo output..
//
// This mixer is in no means near to a corret one.. It has adequate output
// quality but cuts corners and thus introduces aliasing etc. 
//
// Be careful with this routine. It expects certain structure from the
// module structure defined in the player.h.
//
// PS: I really couldn't be arsed to get rid of the rest of the C code
//     in this function -- sowwy.
//

void mixer( struct module *m ) {
	int len, ch;
	signed char *d8; 
	int   *d32; 
  
	len = m->sbuf->len;
	d32 = m->sbuf->tmp;
	d8 = m->sbuf->buf[m->sbuf->frame];

	if (m->playing == 0) {
		asm volatile(""
		"	mov		r0,#0					\n"
		"	add		r1,%[_d],%[_l],lsl #1	\n"
		"loop%=:							\n"
		"	str		r0,[r1,#-4]!			\n"
		"	cmp		r1,%[_d]				\n"
		"	bgt		loop%=					\n"
		:
		: [_l]"r"(len),[_d]"r"(d8)
		: "r0","r1");
		return;
	}

	asm volatile(""
	"	mov		r0,#0					\n"
	"	add		r1,%[_d],%[_l],lsl #2	\n"
	"loop%=:							\n"
	"	str		r0,[r1,#-4]!			\n"
	"	str		r0,[r1,#-4]!			\n"
	"	cmp		r1,%[_d]				\n"
	"	bgt		loop%=					\n"
	:
	: [_l]"r"(len),[_d]"r"(d32)
	: "r0","r1");

	for (ch = 0; ch < MAX_SUPPORTED_CHANNELS; ch++) {
		int dx;

		if (!(m->playing & (1 << ch))) { continue; }

		asm volatile(""
		"	rsb	%[_b],%[_b],#0					\n"
		"	cmn	%[_a],%[_b],lsl #8				\n"
		"	mov	%[_b],%[_b],lsl #15				\n"
		"	movlo	%[_a],%[_a],lsl #7			\n"
		"	blo	skip%=							\n"

		"	adds	%[_a],%[_b],%[_a]			\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"skip%=:								\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		"	adcs	%[_a],%[_b],%[_a],lsl #1	\n"
		"	sublo	%[_a],%[_a],%[_b]			\n"
		
		"	adc	%[_a],%[_a],%[_a]				\n"
		"	mov	%[_b],%[_a],lsr #16				\n"
		"	bic	%[_dx],%[_a],%[_b],lsl #16		\n"
		: [_dx]"=r"(dx)
		: [_a]"r"(m->sbuf->calcFreq << PRECISION),[_b]"r"(m->channels[ch].finalPeriod)
		);

		asm volatile(""
		"	add		r5,%[_m],#12		@ start			\n"
		"	ldmia	r5,{r5,r6,r7,r8}					\n"
		"	@ r5 = finalVolume							\n"
		"	@ r6 = length								\n"
		"	@ r7 = start								\n"
		"	@ r8 = pos									\n"
		"	mov		r1,%[_d]			@ r1 = d32		\n"
		"	mov		r4,%[_l],lsr #1		@ r4 = len		\n"
		"	mov		r6,r6,lsl %[PREC]					\n"
		"loop%=:										\n"
		"	add		r2,r7,r8,lsr %[PREC]				\n"
		"	ldrsb	r2,[r2]				@ r2 = smp		\n"
		"	add		r8,r8,%[_dx]		@ pos += dx		\n"
		"	cmp		r8,r6				@				\n"
		"	ldr		r0,[r1]				@ r0 = d32[n]	\n"
		"	blo		skip%=				@				\n"
		"	ldr		r3,[%[_m],#0]		@ get looped	\n"
		"	cmp		r3,#0				@				\n"
		"	streqh	r3,[%[_m],#8]		@ period = 0	\n"
		"	ldrne	r3,[%[_m],#4]		@				\n"
		"	moveq	r4,#0				@ len =0 -> break\n"
		"	movne	r8,r3,lsl %[PREC]	@ pos = ..		\n"
		"skip%=:										\n"
		"	mla		r3,r5,r2,r0			@ r3 = vol*...	\n"
		"	subs	r4,r4,#1			@				\n"
		"	str		r3,[r1],#4			@				\n"
		"	bgt		loop%=				@				\n"
		"	str		r8,[%[_m],#24]		@ store pos		\n"
		:
		: [_dx]"r"(dx),[_d]"r"(d32),[_m]"r"(&m->channels[ch]),[_l]"r"(len),
			[PREC]"i"(PRECISION)
		: "r0","r1","r2","r3","r4","r5","r6","r7","r8");

		if (m->channels[ch].period == 0) {
			m->playing &= ~(1 << ch);
		}
	}

	asm volatile(""
	"	mov		r1,%[_d8]							\n"
	"	mov		r2,%[_d32]							\n"
	"	mvn		r4,#0								\n"
	"loop%=:										\n"
	"	ldr		r3,[%[_d32]],#4						\n"
	"	mov		r3,r3,asr #6						\n"
	"	cmp		r3,r4,lsr #25	@ r3 > 0x0000007f	\n"
    "	movgt	r3,r4,lsr #25	@ r3 = 0x0000007f	\n"
	"	cmplt	r3,r4,lsl #7	@ r3 < 0xffffff80	\n"
	"	movlt	r3,r4,lsl #7	@ r3 = 0xffffff80	\n"
	"	strb	r3,[%[_d8]],#1						\n"
	"	subs	%[_l],%[_l],#2						\n"
	"	strb	r3,[%[_d8]],#1						\n"
	"	bgt		loop%=								\n"
	: 
	: [_d8]"r"(d8),[_d32]"r"(d32),[_l]"r"(len)
	: "r1","r2","r3","r4");
}

#endif	// S8MIXER


//////////////////////////////////////////////////////////////////////////////
#else	// ASMMIXER
//////////////////////////////////////////////////////////////////////////////

//
//
//
// This mixer does some simple interpolation on samples while
// mixing and assumes L+R stereo output..
//
// This mixer is in no means near to a corret one.. It has an adequate output
// quality and has a lame attempt of embedding kind of a low-pass filter to
// better the sound quality and reduce aliasing. However, the multirate
// mixing has not been implemented corretly. Thus it is somekind of half way
// thing skipping proper sample interpolation + low-pass filter + decimation
// with non integer stepping sample interpolation etc bla blaa bla.
//

#ifndef S8MIXER

void mixer( struct module *m ) {
	int pos, len, ch;
	short *d16; 
	int *d32;
	int n;

	len = m->sbuf->len >> 1;
	d16 = (short *)m->sbuf->buf[m->sbuf->frame];
	d32 = m->sbuf->tmp;

	if (m->playing == 0) {
		for (n = 0; n < len; n++) {
			*d16++ = 0;
			*d16++ = 0;
		}
		return;
	} else {
		for (n = 0; n < len; n++) {
			d32[n] = 0;
		}
	}

	for (ch = 0; ch < MAX_SUPPORTED_CHANNELS; ch++) {
		int dx, vol, end;
		signed char *sta;
    
		if (!(m->playing & (1 << ch))) { continue; }
 
		pos = m->channels[ch].pos;
		vol = m->channels[ch].finalVolume << VOLUMESHIFT;
		sta = m->channels[ch].start;
		end = m->channels[ch].length << PRECISION;
		dx = (m->sbuf->calcFreq << PRECISION) / m->channels[ch].finalPeriod;
    
		for (n = 0; n < len; n++) {
			int smp, f, x;

			x = pos >> PRECISION;
			f = pos & PRECMASK;
			smp = 	((((int)sta[x] * (PRECMASK + 1 - f)) + 
					((int)sta[x+1] * f)) * vol) >> PRECISION;
					//((int)sta[x+1] * f)) >> PRECISION) * vol;

			pos += dx;
			d32[n] += smp;
      
			if (pos >= end) {
				if (!m->channels[ch].looped) {
					m->channels[ch].period = 0;
					break;
				} else {
					pos = m->channels[ch].loopstart << PRECISION;
				}
			}
		}
		if (m->channels[ch].period == 0) {
			m->playing &= ~(1 << ch);
		}
		m->channels[ch].pos = pos;
	}
  
	for (n = 0; n < len; n++) {
		int smp = *d32++;
		if (smp >= 32768) {
			smp = 32767;
		} else if (smp < -32768) {
			smp = -32768;
		}
		*d16++ = smp;
		*d16++ = smp;
	}  
}

#else  // S8MIXER

void mixer( struct module *m ) {
	int pos, len, ch;
	signed char *d8; 
	int *d32;
	int n;

	len = m->sbuf->len >> 1;
	d8 = (signed char *)m->sbuf->buf[m->sbuf->frame];
	d32 = m->sbuf->tmp;

	if (m->playing == 0) {
		for (n = 0; n < len; n++) {
			*d8++ = 0;
			*d8++ = 0;
		}
		return;
	} else {
		for (n = 0; n < len; n++) {
			d32[n] = 0;
		}
	}

	for (ch = 0; ch < MAX_SUPPORTED_CHANNELS; ch++) {
		int dx, vol, end;
		signed char *sta;
    
		if (!(m->playing & (1 << ch))) { continue; }
 
		pos = m->channels[ch].pos;
		vol = m->channels[ch].finalVolume << VOLUMESHIFT;
		sta = m->channels[ch].start;
		end = m->channels[ch].length << PRECISION;
		dx = (m->sbuf->calcFreq << PRECISION) / m->channels[ch].finalPeriod;
    
		for (n = 0; n < len; n++) {
			int smp, f, x;

			x = pos >> PRECISION;
			f = pos & PRECMASK;
			smp = 	((((int)sta[x] * (PRECMASK + 1 - f)) + 
					((int)sta[x+1] * f)) * vol) >> (PRECISION+6);

			pos += dx;
			d32[n] += smp;
      
			if (pos >= end) {
				if (!m->channels[ch].looped) {
					m->channels[ch].period = 0;
					break;
				} else {
					pos = m->channels[ch].loopstart << PRECISION;
				}
			}
		}
		if (m->channels[ch].period == 0) {
			m->playing &= ~(1 << ch);
		}
		m->channels[ch].pos = pos;
	}
  
	for (n = 0; n < len; n++) {
		int smp = *d32++;
		if (smp >= 128) {
			smp = 127;
		} else if (smp < -128) {
			smp = -128;
		}
		*d8++ = smp;
		*d8++ = smp;
	}  
}

#endif  // S8MIXER
#endif  // ASMMIXER

