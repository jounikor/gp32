#ifndef _sound_h_included_
#define _sound_h_included_

//////////////////////////////////////////////////////////////////////////////
//
// Module:
//  sound.h
//
// Description:
//  This module defines required structure and constants for the SDL like
//  sound ring buffer. This sound buffer representation is still host
//  independent.
//
// Author:
//  (c) 2005 Jouni 'Mr.Spiv' Korhonen (jouni.korhonen@iki.fi)
//
// Version:
//  - v0.1  xx-Feb-2005 Initial release
//
//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern"C" {
#endif


struct soundBufParams {
  void *callbackData;	// Fixed position to ease up
  int (*callback)( void *, void * );	// ASM IRQ dispatchers
  int frame;		// 08 - 0 == lower frame of the ring buffer
			//      1 == lower frame of the ring buffer
  char *buf[2];		// 12 - ptrs to ring buffer
  int *tmp;
  long calcFreq;	// 20 - precalculated frequency..
  int playing;		// 24 - 0 not playing, 1 playing
  int len;		// 28 - buffer size in bytes
  long playFreq;	// 32 - user wanted this freq
  long realFreq;	// 36 - and user got this freq
  int irq;		// 40 - -1 no irq installed, >= 0 irq installed
  int stereo;		// 44 - 1 = mono, 2 = stereo output
  int sampleSize;	// 48 - 1 = byte, 2 = short, etc
  int tickFreq;		// 52 - e.g. 50 or 60 times per second
  int clockConstant;	// 56 - as it says..
  int bpm;
  
  // some functions to control sound buffer
  
  void (*start)( struct soundBufParams * );	// start playing
  void (*stop)( struct soundBufParams * );	// stop playing
  void (*enterCriticalSection)( struct soundBufParams * );
  void (*leaveCriticalSection)( struct soundBufParams * );
  void (*volume)( int );
  
  // internal functions.. provided by the caller
  
  void (*installIRQ)( int irq, void (*func)( void ),
                      struct soundBufParams * );	//
  void (*removeIRQ)( int irq );			
  void *(*allocMem)( int size );		//
  void (*freeMem)( void * );			//
  
  // possible internal data..
  
  int __irq;
  long pclk;					// GP32
};

//
//
//

int initSoundBuffer( long playFreq, long pclk, struct soundBufParams *p,
                     void (*installIRQ)( int, void (*)(void),
                                         struct soundBufParams *  ),
                     void (*removeIRQ)( int ),
                     void *(*allocMem)( int ),
                     void (*freeMem)( void * ),
                     int (*callback)( void *, void * ),	// May be NULL
                     void *cbdata );				// May be NULL

void releaseSoundBuffer( struct soundBufParams * );
void playnextchunk(struct soundBufParams *sbuf );	// don't call..
int calcBufferSize( struct soundBufParams *p, int numBufs, int bpm );

#ifdef __cplusplus
}
#endif
#endif











