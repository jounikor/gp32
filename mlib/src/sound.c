//////////////////////////////////////////////////////////////////////////////
//
// Module:
//  sound.c
//
// Description:
//  This module implements a GP32 specific SDL like sound ring buffer,
//  which outputs 16bits signed stereo PCM at the selected frequency.
//  This sound buffer implementation is DMA IRQ driven, which means
//  output PCM buffers get swapped when an DMA completed IRQ takes
//  place. The sound buffer gets never polled or does not require
//  timer IRQ support. Output rates are calculated correctly for a
//  given PCLK values and fs modes, thus the actual output rate might
//  differ slightly from what the user requested.
//
// Note:
//  This code uses gcc specific features for IRQ handlers.
//
// Author:
//  (c) 2005 Jouni 'Mr.Spiv/Scoopex' Korhonen (jouni.korhonen@iki.fi)
//
// Version:
//  - v0.1  xx-Feb-2005 Initial release (does not implement proper tempo)
//  - v0.2  05-Apr-2005 Tweaked the ISR routine
//  - v0.3  13-Jun-2005 Added native 8bits support and a new IRQ handler
//
//////////////////////////////////////////////////////////////////////////////

#include "gp32.h"
#include "sound.h"

//

static int abs_(int);
void myDMA2_ISR(void) __attribute__ ((naked));
static int initIIS( long pclk, long rate );
static void disableIIS( void );
static void PcmPlayRAW( unsigned char *raw, int len, int rep, int irq );
static void WrL3Addr(unsigned char data);
static void WrL3Data(unsigned char data,int flag);
static long calcRate( long pclk_, long rate_, long *realRate );
static void setSysfreq( int fsmode, int busmode );
static void setVolume( int vol);
static void startSound( struct soundBufParams * );
static void stopSound( struct soundBufParams * );
static void initSoundModule( int fsmode, int busmode, int vol );
static void enterCriticalSection( struct soundBufParams * );
static void leaveCriticalSection( struct soundBufParams * );

struct soundBufParams *g_sbuf;

// System frequency & bus modes...

enum _fsmode  { fs512=0, fs384, fs256 };
enum _busmode { iisbus=0, msbbus=0x4 };

// GP32 specific data

static long preScaler;			// prescaler.. - GP32 specific
static int fsMode;			// 256 or 384  - GP32 specific

// Other stuff

#define L3_CLK_MASK			0x200	//bit 9
#define L3_MODE_MASK		0x400	//bit 10
#define L3_DATA_MASK		0x800	//bit 11
#define L3_MASK				0xe00
#define UDA_T_DELAY			8

//

//
//
//

#define FSMASK 0x7fffffff
#define FS384  0x80000000
#define nISR_DMA2       0x13

#define TICKFREQ	50
#define SSIZE16BITS	2
#define SSIZE8BITS	1
#define STEREOSIZE      2
#define MONOSIZE        1

#define PALCLOCK	3546895
#define NTSCCLOCK	3579545

////////////////////////////////////////////////////////////////////
//
// UDA part...
//

static void setVolume( int vol) {
	vol &= 0x3f;
	//data type transfer // data value - volume zero
	WrL3Addr(0x14 + 0);
	WrL3Data(vol & 0x3f, 1);	
}

static void WrL3Addr(unsigned char data) {	
	int i,j;

	rPEDAT = rPEDAT & ~L3_MASK;		//L3D=L/L3M=L(in address mode)/L3C=L
	rPEDAT |= L3_CLK_MASK;			//L3C=H
  
	for(j=0;j<UDA_T_DELAY;j++);		//tsu(L3) > 190ns
  
		//PD[8:6]=L3D:L3C:L3M
	for(i=0;i<8;i++) {	//LSB first 
		if(data&0x1) {	//if data's LSB is 'H'
			rPEDAT &= ~L3_CLK_MASK;	//L3C=L
			rPEDAT |= L3_DATA_MASK;	//L3D=H		    
			for(j=0;j<UDA_T_DELAY;j++);		//tcy(L3) > 500ns
				rPEDAT |= L3_CLK_MASK;	//L3C=H
			rPEDAT |= L3_DATA_MASK;	//L3D=H
			for(j=0;j<UDA_T_DELAY;j++);		//tcy(L3) > 500ns
		} else {		//if data's LSB is 'L'
			rPEDAT &= ~L3_CLK_MASK;		//L3C=L
			rPEDAT &= ~L3_DATA_MASK;	//L3D=L
			for(j=0;j<UDA_T_DELAY;j++);			//tcy(L3) > 500ns
				rPEDAT |= L3_CLK_MASK;		//L3C=H
			rPEDAT &= ~L3_DATA_MASK;	//L3D=L
			for(j=0;j<UDA_T_DELAY;j++);			//tcy(L3) > 500ns
		}
	data >>=1;
	}
	rPEDAT = (rPEDAT & ~L3_MASK) | (L3_CLK_MASK|L3_MODE_MASK);	//L3M=H,L3C=H
}

static void WrL3Data(unsigned char data,int flag) {
	int i,j;
  
	if(flag) {
		rPEDAT |= L3_CLK_MASK;	//L3C=H(while tstp, L3 interface halt condition)
		for(j=0;j<UDA_T_DELAY;j++); //tstp(L3) > 190ns
		rPEDAT &= (~L3_MODE_MASK);
		for (j=0;j<UDA_T_DELAY;j++);
		rPEDAT |= L3_MODE_MASK;
	}
  
	rPEDAT = (rPEDAT & ~L3_MASK) | (L3_CLK_MASK|L3_MODE_MASK);	    //L3M=H(in data transfer mode)	
	for(j=0;j<4;j++);	    //tsu(L3)D > 190ns
  
		//PD[8:6]=L3D:L3C:L3M
	for(i=0;i<8;i++) {
		if(data&0x1) {	//if data's LSB is 'H'
			rPEDAT &= ~L3_CLK_MASK;	//L3C=L
			rPEDAT |= L3_DATA_MASK;	//L3D=H
			for(j=0;j<UDA_T_DELAY;j++);	//tcy(L3) > 500ns
			rPEDAT |= (L3_CLK_MASK|L3_DATA_MASK);//L3C=H,L3D=H
			for(j=0;j<UDA_T_DELAY;j++);	//tcy(L3) > 500ns
		} else {		//if data's LSB is 'L'
			rPEDAT &= ~L3_CLK_MASK;	//L3C=L
			rPEDAT &= ~L3_DATA_MASK;	//L3D=L
			for(j=0;j<UDA_T_DELAY;j++);	//tcy(L3) > 500ns
			rPEDAT |= L3_CLK_MASK;	//L3C=H
			rPEDAT &= ~L3_DATA_MASK;	//L3D=L
			for(j=0;j<UDA_T_DELAY;j++);	//tcy(L3) > 500ns
		}
		data>>=1;	//for check next bit
	}
	rPEDAT = (rPEDAT & ~L3_MASK) | L3_CLK_MASK;
	for (j=0;j<UDA_T_DELAY;j++);
	rPEDAT |= L3_MODE_MASK;
}

static void setSysfreq( int fsmode, int busmode ) {
	//status type transfer (0x14--> UDA1330 Address, 2 --> status identifier)
	WrL3Addr(0x14+2);				
	//status value : 
	//		b[7:6] mode 1, b[5:4] fs, b[3:1] input format is MSB
	//WrL3Data((fsmode<<4) + (0x4<<1), 0);

	WrL3Data((fsmode<<4) + (busmode<<1), 0);
}

static void initSoundModule(int fsmode, int busmode, int vol ) {
	rPGCON |= 0xa0;
		
	/*port reconfiguration : 
	    PORTE 9,10,11 --> output
		pull-up disable
		L3_MODE, L3_CLK High
	*/
	rPEDAT = (rPEDAT & ~0xe00) | (L3_MODE_MASK|L3_CLK_MASK);
	rPEUP |= 0xe00;		
	rPECON = (rPECON & (~(0x3f << 18))) | (0x15<<18);
  
	setSysfreq( fsmode, busmode );	// 512/384/256fs, msb/iis setting
	setVolume( vol /*VALUE_UDA_VOLUME*/);
  
	//
	//data type transfer // data value - no de-emphasis, no muting
	WrL3Addr(0x14 + 0);
	WrL3Data(0x80,1);	
}

////////////////////////////////////////////////////////////////////
//
// DMA PCM part...
//
////////////////////////////////////////////////////////////////////
//
// Description:
//  Calculates prescalers and system fs rates based on the desired
//  sampling frequency and the currect PCLK rate.
//
// Parameters:
//  pclk_ - [in] a long containing current PCLK rate.
//  rate_ - [in] a long containing the desired output sampling rate.
//
// Returns:
//  Prescaler value. If the bit 31 is 0 then 256fs system fs rate
//  must be selected. If the bit 31 is 1 then 384 system fs rate
//  must be selected.
//
// Note:
//  Depending on the given PCLK & sampling rate the calculated
//  prescaler values & system fs rate might not produce exactly the
//  desired output sampling rate.
//
////////////////////////////////////////////////////////////////////

static long calcRate( long pclk_, long rate_, long *realrate ) {
	long fs256 = pclk_ / 256;
	long fs384 = pclk_ / 384;
	long fs256min = 0xffff;
	long fs384min = 0xffff;
	long t;
	int n;
	int ps256 = -1;
	int ps384 = -1;
	int ret;

	//

	for (n = 0; n < 32; n++) {
		t = fs256 / (n + 1);		// prescaler+1
    
		if (abs_((rate_ - t)) < fs256min) {
			fs256min = abs_((rate_ - t));
			ps256 = n;
		}
	}	
  
	for (n = 0; n < 32; n++) {
		t = fs384 / (n + 1);
		if (abs_((rate_ - t)) < fs384min) {
			fs384min = abs_((rate_ - t));
			ps384 = n;
		}
	}
  
	if (fs256min > fs384min) {
		if (realrate) *realrate = fs384 / (ps384 + 1);
		ret = ps384 | FS384;
	} else {
		if (realrate) *realrate = fs256 / (ps256 + 1);
		ret = ps256;
	}
  
	return ret;
}

static int abs_( int x ) {
	x = x < 0 ? -x : x;
	return x;
}

////////////////////////////////////////////////////////////////////
//
// Description:
//
// Parameters:
//  pclk - [in] a long telling the current PCLK seeting.
//  rate - [in] a long telling the desired Hz for the sample.
//
// Returns:
//  fs256 if 256fs system fs, fs384 if 384 system fs.
//
// Note:
//
////////////////////////////////////////////////////////////////////

static int initIIS( long pclk, long rate ) {
	IISPSR iispr;
	IISCON iiscon;
	IISMOD iismod;
	IISSFIFCON iissf;
	int sysfs;
	unsigned long prescale;
  
	//
  
	iispr.all = 0;
	iiscon.all = 0;
	iismod.all = 0;
	iissf.all = 0;
  
	//
  
	prescale = calcRate( pclk, rate, (long*)0 );
  
	if (prescale & FS384) {
		sysfs = fs384;	// 384fs
		prescale &= FSMASK;
	} else {
	  sysfs = fs256;	// 256fs
	}
  
	//
  
	iispr.reg.PSA = prescale;
	iispr.reg.PSB = prescale;
  
	iiscon.reg.TXDMAENA = 1;
	iiscon.reg.RXDMAENA = 0;
	iiscon.reg.TXCHIDLE = 0;
	iiscon.reg.RXCHIDLE = 1;
	iiscon.reg.IISPSENA = 1;
	iiscon.reg.IISIFENA = 0;	// transmit start
  
	iismod.reg.MODE = 0;		// master mode
	iismod.reg.TXRXMODE = 2;	// transmit mode
	iismod.reg.ACTLEVCH = 0;	// low byte = left channel
	iismod.reg.SIFMT = 0;		// IIS compatible!!!!
#ifdef S8MIXER					// 8 bits mono..
	iismod.reg.SDBITS = 0;		// 8 bits
#else
	iismod.reg.SDBITS = 1;		// 16 bits
#endif
	iismod.reg.MCLKFS = sysfs;	// sys fs
	iismod.reg.SBCLKFS = 1;		// 32fs serial bit clock speed
  
	iissf.reg.TXFIFOMODE = 1;	// DMA
	iissf.reg.RXFIFOMODE = 0;	// normal mode
	iissf.reg.TXFIFOENA = 1;	// transmit fifo enable
	iissf.reg.RXFIFOENA = 0;	// receive fifo disable
  
	//
  
	rIISPSR = iispr.all;
	rIISCON = iiscon.all;
	rIISMOD = iismod.all;
	rIISFIFCON = iissf.all;
	rIISCON |=0x1;  /* iis enable */
  
	return sysfs;
}

static void disableIIS( void ) {
	rIISCON &= ~0x1;  /* iis enable */
}

////////////////////////////////////////////////////////////////////
//
// Description:
//  This function plays a raw 16 bits PCM sample using currect
//  UDA & IIS settings. The playing is done using DMA2 and assumes
//  that appropriate IRQ handler has already been set up.
//
// Parameters:
//  raw - [in] a ptr to a raw 16bits PCM sample.
//  len - [in] the length of the sample in half words.
//  rep - [in] if 1 repeat sample, if 0 play once.
//  irq - [in] if 1 enable DMA finished IRQ.
//
// Returns:
//  none.
//
// Note:
//
////////////////////////////////////////////////////////////////////

void PcmPlayRAW( unsigned char *raw, int len, int rep, int irq ) {
	rDMASKTRIG2 |=(1<<2)+(0<<1)+0;

	rDIDST2=(1<<30)+       // destination on peripherial bus */
	  (1<<29)+             // fixed address
	  ((int)IISFIF);       // IIS FIFO txd
	rDISRC2=(0<<30)+       // source from system bus
	  (0<<29)+             // auto-increment
	  (int)(raw);          // buffer address
	rDCON2 =(1<<30)+       // handshake mode
	  (0<<29)+             // DREQ and DACK are synchronized to PCLK (APB clock)
	  (irq<<28)+           // generate irq when transfer is done
	  (0<<27)+             // unit transfer
	  (0<<26)+             // single service
	  (0<<24)+             // dma-req.source=I2SSDO
	  (1<<23)+             // (H/W request mode)
	  (rep<<22)+           // auto reload on/off
#ifdef S8MIXER		       // 8 bits mono..
	  (0<<20)+             // data size byte
#else
	  (1<<20)+             // data size hword
#endif
	  (len);    	       // transfer size (hwords)

	rDMASKTRIG2 = (0<<2)+(1<<1)+0;  // no-stop, DMA2 channel on, no-sw trigger
}

////////////////////////////////////////////////////////////////////
//
// Description:
//  This function play the next block of sample data ans calls
//  the DMA2 set up function. This style of playing can be used
//  to implement a ringbuffer for an "infinite" sample..
//
// Parameters:
//  none.
//
// Returns:
//  none.
//
// Note:
//
////////////////////////////////////////////////////////////////////

void playnextchunk( struct soundBufParams *sbuf ) {
	//PcmPlayRAW( sbuf->buf[sbuf->frame],sbuf->len * sbuf->sampleSize, 0, 1 );
	PcmPlayRAW( sbuf->buf[sbuf->frame],sbuf->len, 0, 1 );
	sbuf->frame = (sbuf->frame + 1) & 1;
}

////////////////////////////////////////////////////////////////////
//
// Description:
//  DMA2 finished IRQ serving routine.
//
// Parameters:
//  none.
//
// Returns:
//  none.
//
// Note:
//   If OUTSIDEIRQMIXING is defined then the actual player code and
//   mixing is done outside IRQ mode. The CPU is switched to SYSTEM
//   mode thus other IRQs may take place duirng mixing.
//
////////////////////////////////////////////////////////////////////


void myDMA2_ISR(void) {
#ifdef OUTSIDEIRQMIXING
	asm volatile (""
		"	sub		lr,lr,#4			\n"
		"	stmdb	r13!,{r0-r5,r12,lr}	\n"
		"	mrs		r4,spsr				\n"
		"	ldr		r5,=g_sbuf			\n"
		"	str		r4,[r13,#-4]!		\n"
		"	ldr		r0,[r5]				\n"
		"	bl		playnextchunk		\n"
		"	mrs		r2,cpsr				\n"
		"	and		r4,r4,#0x1f			\n"  // get previous mode
		"	bic		r2,r2,#0x9f			\n"  // Enable IRQs
		"	orr		r2,r2,r4			\n"  // Switch to previous non IRQ mode
		"	ldr		r4,[r5]				\n"  // r4 = ptr to struct soundBufParams
		"	msr		CPSR_fsxc,r2		\n"  // ...
		"	ldr		r2,[r4,#4]			\n"  // r0 = ptr to struct module
		"	ldr		r0,[r4]				\n"  // r2 = ptr to player function
		"	cmp		r2,#0				\n"
		"	beq		skip				\n"
		"	mov		lr,pc				\n"
		"	mov		pc,r2				\n"  // call the player
		"skip:							\n"
		//"	mov		r0,#0x14400000		\n"  // Optional on GP32 when BIOS is in
		//"	mov		r1,#0x13			\n"  // control of IRQ handling..
		//"	str		r1,[r0]				\n"  // But when not use these to clear
		//"	str		r1,[r0,#0x10]		\n"  // pending DMA2 IRQ bits..
		//"	ldr		r1,[r0,#0x10]		\n"  //
		"	mrs		r2,cpsr				\n"
		"	bic		r2,r2,#0x1f			\n"
		"	orr		r2,r2,#0x92			\n"  // Disable IRQs and switch to IRQ mode
		"	msr		cpsr_fsxc,r2		\n"  // ..
		"	ldr		r4,[r13],#4			\n"
		"	msr		spsr_cxsf,r4		\n"
		"	ldmia	r13!,{r0-r5,r12,pc}^	\n"
		"	.pool							"
	);
#else
	asm volatile (""
		"	stmdb	r13!,{r0-r12,lr}	\n"
		"	ldr		r4,=g_sbuf			\n"
		"	ldr		r0,[r4]				\n"
		"	bl		playnextchunk		\n"
		"	ldr		r4,[r4]				\n"  // r4 = ptr to struct soundBufParams
		"	ldr		r2,[r4,#4]			\n"  // r0 = ptr to struct module
		"	ldr		r0,[r4]				\n"  // r2 = ptr to player function
		"	cmp		r2,#0				\n"
		"	sub		r1,r1,#4			\n"
		"	beq		skip				\n"
		"	mov		lr,pc				\n"
		"	mov		pc,r2				\n"
		"skip:							\n"
		//"	mov		r0,#0x14400000		\n"	// Optional on GP32 when BIOS is in
		//"	mov		r1,#0x13			\n"	// control of IRQ handling..
		//"	str		r1,[r0]				\n"	// But when not use these to clear
		//"	str		r1,[r0,#0x10]		\n"	// pending DMA2 IRQ bits..
		//"	ldr		r1,[r0,#0x10]		\n"	//
		"	ldmia	r13!,{r0-r12,lr}	\n"
		"	subs	pc,lr,#4			\n"
		"	.pool							"
	);
#endif
}

int calcBufferSize( struct soundBufParams *p, int numBufs, int bpm ) {
	int len = (p->realFreq / p->tickFreq) * 125 / bpm;
	len = len & 1 ? len + 1 : len;
	return numBufs * p->stereo * len;
}
int initSoundBuffer( long playFreq, long pclk, struct soundBufParams *p,
					 void (*installIRQ)( int, void (*)(void), struct soundBufParams * ),
					 void (*removeIRQ)( int ),
					 void *(*allocMem)( int ),
					 void (*freeMem)( void * ),
					 int (*callback)( void *, void * ),
					 void *cbdata ) {
	char *buffer;
	long freq;
	int n, len;
  
	//memset(p,0,sizeof(struct soundBufParams));
	for (n = 0; n < sizeof(struct soundBufParams); n++) {
		((char *)p)[n] = 0;
	}
	p->installIRQ = installIRQ;
	p->removeIRQ  = removeIRQ;
	p->allocMem   = allocMem;
	p->freeMem    = freeMem;
	p->callback   = callback;
	p->callbackData = cbdata;
	p->playFreq   = playFreq;
	p->stereo     = STEREOSIZE;
#ifdef S8MIXER						// 8 bits mono..
	p->sampleSize = SSIZE8BITS;		// 8bits sample only supported
#else
	p->sampleSize = SSIZE16BITS;	// 16bits sample only supported
#endif
	p->tickFreq   = TICKFREQ;		// 50 ticks per second supported
	p->clockConstant = PALCLOCK;	// Amiga PAL clock constant
	p->irq        = -1;				// no irq installed
	p->frame = 0;
	p->bpm = 125;
  
	//
  
	p->start  = startSound;
	p->stop   = stopSound;
	p->volume = setVolume;
	p->enterCriticalSection = enterCriticalSection;
	p->leaveCriticalSection = leaveCriticalSection;
  
  
	// GP32 specific vars..
  
	freq = calcRate( pclk, playFreq, &p->realFreq );
	preScaler = freq & ~FSMASK;
	fsMode    = freq & FS384 ? fs384 : fs256;
	p->pclk   = pclk;
  
	//
  
	len = calcBufferSize( p, 1, 32 );
	p->len = calcBufferSize( p, 1, 125 );;
  
	if ((buffer = allocMem(2 * len * p->sampleSize +
		sizeof(int) * len))) {
		p->buf[0] = buffer;
		p->buf[1] = buffer + len * p->sampleSize;
		p->tmp    = (int *)(buffer + 2 * len * p->sampleSize);
	} else {
		return -1;
	}
	if (installIRQ) {
		p->__irq = 1;
	}
	// Ok.. rock'n'roll
  
	g_sbuf = (void*)0;
	return 0;
}

void releaseSoundBuffer( struct soundBufParams *p ) {
	if (p == (void *)0) { return; }
	if (p->playing) {
		p->stop(p);
		p->playing = 0;
	}
	if (p->__irq) {
		if (p->irq >= 0) {
			p->removeIRQ(p->irq);
			p->irq = -1;
		}
	}
	if (p->buf[0]) {
		p->freeMem(p->buf[0]);
		p->buf[0] = (void *)0;
	}
}

static void startSound( struct soundBufParams *p ) {
	int n, m;
  
	m = (p->buf[1] - p->buf[0]) * 2;
  
	for (n = 0; n < m; n++) {
		(p->buf[0])[n] = 0;
	}
  
	// Setup IIS etc..

	rDMASKTRIG2=(1<<2)+(0<<1)+0;
	n = initIIS( p->pclk, p->realFreq );
	initSoundModule(n,iisbus,0);
	p->calcFreq = p->clockConstant / p->realFreq;
  
	g_sbuf = p;
	if (p->__irq) {
		p->irq = nISR_DMA2;
		p->installIRQ( p->irq, myDMA2_ISR, p );
	}
	// and trigger DMA..
  
	playnextchunk( p );
}

static void stopSound( struct soundBufParams *p ) {
	// should also stop the DMA..
	
	rDMASKTRIG2=(1<<2)+(0<<1)+0;
	disableIIS();
	
	p->removeIRQ( p->irq );
	p->irq = -1;
	g_sbuf = (void *)0;
}

static void enterCriticalSection( struct soundBufParams *p ) {
}
static void leaveCriticalSection( struct soundBufParams *p ) {
}
