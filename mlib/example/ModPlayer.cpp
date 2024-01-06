//////////////////////////////////////////////////////////////////////////////
//
// GP32 specific C++ wrapper for the mlib.
// (c) 2005 Jouni 'Mr.Spiv' Korhonen
//
//
//
//////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include "ModPlayer"

//
//
//

extern"C" static long mygetPCLK();	
extern"C" static void mymmuChange( unsigned char *s, unsigned char *e, int flg );
extern"C" static void myinstallIRQ( int num, void (*irq)(void), struct soundBufParams *p );
extern"C" static void myremoveIRQ( int num );
extern"C" static void *mymalloc( int len );
extern"C" static void myfree( void * );

//
//
//
///////////////////////////////////////////////////////////////////////////////
//
// Description:
//   Constructor..
//
// Parameters:
//   mod        - [in] ptr to module data
//   freq       - [in] desired output frequency
//   fastSwitch - [in] if true mixing & module parsing will be done outside
//                IRQs. If false everything will be made within the IRQ handler
//
// Returns:
//   none
//
// Changes:
//   none.
//
///////////////////////////////////////////////////////////////////////////////

ModPlayer::ModPlayer( char* mod, int freq, bool fastSwitch ) {
	int (*func)( void *, void * );
	long pclk = mygetPCLK();
	
	::initSoundBuffer(freq,pclk,&_sbuf,myinstallIRQ, myremoveIRQ,
			mymalloc, myfree, mt_music, &mod);

	if (::mt_init(mod, &_sbuf, &_mod) < 0) {
		// error..
	}
}

ModPlayer::ModPlayer( char* mod, void (*cb)(int,int, void *), void *data,
					int freq, bool fastSwitch ) {
	int (*func)( void *, void * );
	long pclk = mygetPCLK();

	::initSoundBuffer(freq,pclk,&_sbuf,myinstallIRQ, myremoveIRQ,
			mymalloc, myfree, mt_music, &mod);

	if (::mt_init(mod, &_sbuf, &_mod) < 0) {
		// error..
	}

	::mt_setCallback(cb,data,&_mod);
}

//

ModPlayer::~ModPlayer() {
	::mt_end(&_mod);
	::releaseSoundBuffer(&_sbuf);
}

void ModPlayer::enable() {
	::mt_enable(&_mod);
}

void ModPlayer::disable() {
	::mt_disable(&_mod);
}

int ModPlayer::masterVolume( int vol ) {
	::mt_masterVolume(&_mod,vol);
}

///////////////////////////////////////////////////////////////////////////////
//
// Description:
//   Plays any signed 8bits raw mono sample at the requested frequency.
//
// Parameters:
//   smp  - [in] ptr to the sample data
//   len  - [in] length of the sample in bytes
//   ch   - [in] channel to play the sample (from 0 to MAX_FX_CHANNELS-1)
//   vol  - [in] desired output volume (from 0 to 255)
//   freq - [in] desired output frequency
//
// Returns:
//   0 if ok, -1 if channel number is out of bounds
//
// Changes:
//   none.
//
///////////////////////////////////////////////////////////////////////////////

int ModPlayer::playFX( signed char* smp, int len, int ch, int vol, int freq ) {
	struct FXinfo fx;

	// Do some sanity checking..

	if (vol > 255) { vol = 255; }
	if (vol < 0) { vol = 0; }
	if (ch >= MAX_FX_CHANNELS) { ch = MAX_FX_CHANNELS-1; }
	if (ch < 0) { ch = 0; }


	fx.channel = ch;
	fx.instrument = 0;
	fx.volume = vol;
	fx.freq.playFreq = freq;
	return mt_playFX(smp,len,&fx,&_mod);
}

///////////////////////////////////////////////////////////////////////////////
//
// Description:
//   Plays any instrument within the module data at the requested period.
//
// Parameters:
//   ch     - [in] channel to play the sample (from 0 to MAX_FX_CHANNELS-1)
//   vol    - [in] desired output volume (from 0 to 255)
//   inst   - [in] instrument number toplay
//   period - [in] desired output period
//
// Returns:
//   0 if ok, -1 if channel number is out of bounds
//
// Changes:
//   none.
//
///////////////////////////////////////////////////////////////////////////////

int ModPlayer::playNote( int ch, int vol, int inst, int period ) {
	struct FXinfo fx;

	// Do some sanity checking..

	if (vol > 255) { vol = 255; }
	if (vol < 0) { vol = 0; }
	if (ch >= MAX_FX_CHANNELS) { ch = MAX_FX_CHANNELS-1; }
	if (ch < 0) { ch = 0; }
	if (inst > 31) { inst = 31; }
	if (inst < 0) { inst = 0; }
	if (period > 856) { period = 856; }
	if (period < 113) { period = 113; }

	fx.channel = ch;
	fx.instrument = inst;
	fx.volume = vol;
	fx.freq.period = period;
	return ::mt_playNote( &fx, &_mod );
}

///////////////////////////////////////////////////////////////////////////////
//
// Description:
//   Stops playing any FX channel (if there is some output).
//
// Parameters:
//   ch - [in] channel to turn off
//
// Returns:
//   none
//
// Changes:
//   none.
//
///////////////////////////////////////////////////////////////////////////////

void ModPlayer::stopFX( int ch ) {
	if (ch >= MAX_FX_CHANNELS) { ch = MAX_FX_CHANNELS-1; }
	if (ch < 0) { ch = 0; }

	::mt_stopFX( ch, &_mod );
}

///////////////////////////////////////////////////////////////////////////////
//
// Description:
//   Installs/removes callback function..User defined callback
//   gets called every time a next sample buffer gets mixed.
//
// Parameters:
//   cb   - [in] function pointer to your callback function. If NULL then
//          callback functionality gets disableb/removed.
//   data - [in] ptr to user data (optional), which get passed to callback
//
// Returns:
//   none
//
// Note about the callback: every time the callback gets called the callback
// function gets passed three variables:
//   1) current song position in mod
//   2) current pattern position in mod
//   3) a ptr to userdata 
//
///////////////////////////////////////////////////////////////////////////////

void ModPlayer::setCallback( void (*cb)(int songpos, int pattpos, void *userdata), void *data ) {
	::mt_setCallback(cb,data,&_mod);
}



//
//
//

static long mygetPCLK() {
	long pclk;

	asm volatile(""
		"stmdb	sp!,{lr}	\n"
		"mov	r0,#4		\n"
		"swi	#0x0b		\n"
		"ldr	r0,[r0,#8]	\n"
		"mov	%[output],r0	\n"
		"ldmia	sp!,{lr}"
		: [output] "=r"(pclk) 
		:
		: "r0","r1");

	return pclk;
}


static void mymmuChange( unsigned char *s, unsigned char *e, int flg ) {
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

static void myinstallIRQ( int num, void (*irq)(void), struct soundBufParams *p ) {
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

static void myremoveIRQ( int num ) {
	asm volatile(""
		"stmdb	sp!,{lr}		\n"
		"mov	r0,%0			\n"
		"swi	#0x0a			\n"
		"ldmia	sp!,{lr}"
		:
		: "r"(num)
		: "r0");
}

static void *mymalloc( int len ) {
	long base;
	long *pl;

	len += len & 4095 ? (4096 - (len & 4095)) : 0;
		
	void *p = malloc(len + 4096 + 4);
	
	if (p == NULL) { return NULL; }

	base = (long)p;
	p = (void *)(((long)p + 4 + 4095) & ~4095);
	((long *)p)[-1] = base;
	mymmuChange( (unsigned char *)p, (unsigned char *)p + len - 1, 0xFF2 );

	return p;
}

static void myfree( void *p ) {
	if (p == NULL) { return; }
	free( (void *)((long *)p)[-1] );
}

