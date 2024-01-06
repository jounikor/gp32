////////////////////////////////////////////////////////////////////
//
// Direct hardware modplayer example..
// (c) 25-mar-2005 Jouni 'Mr.Spiv' Korhonen.
//
//
////////////////////////////////////////////////////////////////////
//
//
//

#include <stdlib.h>

#include "player.h"
#include "sound.h"
#include "gp32.h"

extern  char _binary_raw_echoing_mod_start[];
#define modData1 _binary_raw_echoing_mod_start
extern  char _binary_raw_shock_mod_start[];
#define modData0 _binary_raw_shock_mod_start

extern signed char     _binary_raw_ni_22khz_raw_start[];
#define sampleData     _binary_raw_ni_22khz_raw_start
extern int             _binary_raw_ni_22khz_raw_size;
#define sampleDataSize _binary_raw_ni_22khz_raw_size

//
//
//

void setCPUSpeed( long mclk, int fac, int div) {
	asm volatile(""
		"mov	r0,%0			\n"
		"mov	r1,%1			\n"
		"mov	r2,%2			\n"
		"stmdb	sp!,{r0,r1,r2,lr}	\n"
		"mov	r0,sp			\n"
		"swi	#0x0d			\n"
		"add	sp,sp,#12		\n"
		"ldmia	sp!,{lr}"
		: 
		: "r"(mclk),"r"(fac),"r"(div)
		: "r0","r1","r2");
}

void mmuChange( unsigned char *s, unsigned char *e, int flg ) {
	asm volatile(""
		"mov	r0,%0			\n"
		"mov	r1,%1			\n"
		"mov	r2,%2			\n"
		"stmdb	sp!,{lr}		\n"
		"swi	#0x02			\n"
		"ldmia	sp!,{lr}"
		: 
		: "r"(s),"r"(e),"r"(flg)
		: "r0","r1","r2");
}

void installIRQ( int num, void (*irq)(void) ) {
	asm volatile(""
		"stmdb	sp!,{lr}		\n"
		"mov	r0,%0			\n"
		"mov	r1,%1			\n"
		"swi	#0x09			\n"
		"ldmia	sp!,{lr}"
		: 
		: "r"(num),"r"(irq)
		: "r0","r1");
}

void removeIRQ( int num ) {
	asm volatile(""
		"stmdb	sp!,{lr}		\n"
		"mov	r0,%0			\n"
		"swi	#0x0a			\n"
		"ldmia	sp!,{lr}"
		:
		: "r"(num)
		: "r0");
}

void ARMDisableInterrupt( void ) {
	asm volatile(""
		"mrs	r0,CPSR			\n"
		"orr	r0,r0,#192		\n"
		"msr	CPSR_fsxc,r0	\n"
		:
		:
		: "r0");
}

void ARMEnableInterrupt( void ) {
	asm volatile(""
		"mrs	r0,CPSR			\n"
		"bic	r0,r0,#128		\n"
		"msr	CPSR_fsxc,r0	\n"
		:
		:
		: "r0");
}

void waitLine( int line ) {
	while ((rLCDCON1 >> 18) != line);
}

//
//
//
////////////////////////////////////////////////////////////////////
//
//
//
// A simple main program to demonstrate playing a sound.
//
//
////////////////////////////////////////////////////////////////////

static void *mymalloc( int len ) {
	long base;

	len += len & 4095 ? (4096 - (len & 4095)) : 0;
		
	void *p = malloc(len + 4096 + 4);
	
	if (p == (void*)0) { return (void*)0; }

	base = (long)p;
	p = (void *)(((long)p + 4 + 4095) & ~4095);
	((long *)p)[-1] = base;
	mmuChange( (unsigned char *)p, (unsigned char *)p + len - 1, 0xFF2 );

	return p;
}

static void myfree( void *p ) {
	if (p == (void*)0) { return; }
	free( (void *)((long *)p)[-1] );
}

void myinstallirq( int i, void (*f)(void), struct soundBufParams *p ) {
	installIRQ(i,f);
}

void myremoveirq( int i ) {
	removeIRQ(i);
}

//
//
//

void main( void *arg ) {
	long gpb, gpe;
	int m,n;
    int updownA, updownB;
	int biisi = 0;

	struct module mod;
	struct soundBufParams sbuf;
	struct FXinfo fx;

	
	// PCLK -> 66MHz

	setCPUSpeed (66000000, 0x3a012, 0);
	ARMEnableInterrupt();

	// Init soundbuffer with a fast context switch..
	// Output 16kHz rate, PCLK 66MHz..
	// It is generally better to set a high PCLK..
	initSoundBuffer(16000,66000000,&sbuf,myinstallirq,myremoveirq,mymalloc,myfree,mt_music,&mod);

	// Init only the mixing ring buffer - sound FX is possible but no module playback
	//initSoundBuffer(12000,66000000,&sbuf,myinstallirq,myremoveirq,mymalloc,myfree,0,0);

	// Init the module..
	mt_init(modData0,&sbuf,&mod);

	gpb = rPBDAT;   // 0x156
	gpe = rPEDAT;
	m = n = 0;
	updownA = 0;
	updownB = 0;

	rLCDCON1 |= 1;

	while ((gpb & (rKEY_L | rKEY_R)) != 0) {
		gpb = rPBDAT;   // 0x156
		gpe = rPEDAT;

		waitLine(100);
		*(volatile unsigned long*)0x14a007fc = 0xffff;
		waitLine(99);
		*(volatile unsigned long*)0x14a007fc = 0x0000;

		if ((gpb & rKEY_A) != 0)   {
			updownA = 0;
		}
		if ((gpb & rKEY_B) != 0)   {
			updownB = 0;
		}
		if (updownA == 0 && (gpb & rKEY_A) == 0)   {
			// Play a signed 8bits mono sample
			// on FX channel 0, volume 150 (normal volume is 0-64, max 255),
			// play on rate 22050Hz, sample size is 10223 bytes
			//
                  fx.channel = 0;
                  fx.instrument = 0;
                  fx.volume = 150; //64;
                  fx.freq.playFreq = 22050;
                  mt_playFX(sampleData,10223,&fx,&mod);
                  updownA = 1;
                  continue;
		}
		if (updownB == 0 && (gpb & rKEY_B) == 0)   {
			// Play instrument 0 from the modfile in FX channel 1
			// volume is 170 and "Amiga" period 300
			//
                  fx.channel = 1;
                  fx.instrument = 0;
                  fx.volume = 170;
                  fx.freq.period = 300;		// 113 <-> 856
                  mt_playNote(&fx,&mod);
                  updownB = 1;
                  continue;
		}
		if ((biisi == 0) && (gpe & rKEY_SELECT) == 0)   {
			mt_end(&mod);
			mt_init(modData1,&sbuf,&mod);
			biisi = 1;
			n = 0;
			continue;
		}
		if ((biisi == 1) && (gpe & rKEY_START) == 0)   {
			mt_end(&mod);
			mt_init(modData0,&sbuf,&mod);
			biisi = 0;
			n = 0;
			continue;
		}
		if (!(gpb & rKEY_UP))   { mt_enable(&mod); }  // left
		if (!(gpb & rKEY_DOWN))  { mt_disable(&mod); }  // right
		if (!(gpb & rKEY_LEFT))   { n = n + 1 < 31 ? n+1 : 31; }  // left
		if (!(gpb & rKEY_RIGHT))  { n = n - 1 >= 0 ? n-1 : 0; }  // right
		if (m!=n) {
			mt_masterVolume(&mod,n);
			m = n;
		}
	}
	asm volatile("swi #4");
}
