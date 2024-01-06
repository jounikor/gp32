#ifndef _player_h_included
#define _player_h_included
//////////////////////////////////////////////////////////////////////////////
//
// Module:
//  player.h
//
// Description:
//  This module defines required structure and constants for the module
//  player. Note that the upper maximum for the MAX_MOD_CHANNELS is 16
//  and the maximum for the MAX_FX_CHANNELS is 16. Thus the maximum number
//  of supported channels is 32.
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

#include "sound.h"


//
#define MAX_MOD_CHANNELS        16
#define MOD_MASK                ~((1 << MAX_MOD_CHANNELS) - 1)
#define MAX_FX_CHANNELS		4
#define MAX_SUPPORTED_CHANNELS	(MAX_MOD_CHANNELS+MAX_FX_CHANNELS)
#define PRECISION     		8	//12
#define PRECMASK		((1 << PRECISION) - 1)
#define VOLUMESHIFT             0   // 0,1 or 2
//
struct module {
  struct soundBufParams *sbuf;

  // Callback stuff
  
  void (*userCallback)( int, int, void * );
  void *userData;

  // module info
	
  char *moduleData;
  char *modName;
  int songLen;
  int ciaa;
  int numPatterns;
  int patternSize;
  unsigned long *patterns;
  unsigned char *songPositions;
  
  char numInstruments;
  char numCh;
  char speed;
  char count;
  char posJumpFlag;
  char PBreakFlag;
  char enable;
  
  int songPos;
  int PBreakPos;
  int pattDelTime;
  int pattDelTime2;
  int patternPos;
  int filterOnOFF;
  
  struct _instruments {
    char *name;
    char *sampleStart;
    int sampleLen;		// in bytes
    char finetune;
    char volume;
    char looped;
    
    int loopStart;
    int length;
    int replen;
  } instruments[31];
  
  // channels & patters
  
  unsigned long playing;
  
  struct _channels {	// DO NOT CHANGE THE ORDER OF THESE MEMBERS!!!!!!!
    int looped;     // 0
    int loopstart;  // 4
    short period;   // 8
    short note;

    int finalVolume;	// 12
    int length;		// 16
    char *start;	// 20
    int pos;		// 24
    int finalPeriod;	// 28

    unsigned char sample;			// sample number
    unsigned char effect;
    unsigned char params;
    
    // cached instrument info
    
    int volume;
    int wavestart;
    char finetune;		// finetune adjustement
    
    char toneportdirec;
    char toneportspeed;
    unsigned char vibratoSpeed;
    unsigned char vibratoDepth;
    unsigned char vibratopos;
    unsigned char tremoloSpeed;
    unsigned char tremoloDepth;
    unsigned char tremolopos;
    unsigned char wavecontrol;
    unsigned char glissfunk;
    char loopcount;
    
    short wantedperiod;
    int pattpos;
    int funkoffset;
    int reallength;
  } channels[MAX_SUPPORTED_CHANNELS];
};
struct FXinfo {
  char channel;
  char instrument;
  char volume;
  union {
    short period;
    int playFreq;
  } freq;
};
//
int mt_init( char *data, struct soundBufParams *sbuf, struct module *mod );
int mt_music( void *mod, void *magic );
int mt_musicFastSwitch( void *m, void *magic );
void mt_end( struct module *mod );
void mt_enable( struct module *mod );
void mt_disable( struct module *mod );
void mt_masterVolume( struct module *mod, int volume );
int mt_playFX( signed char *smp, int len, struct FXinfo *nfo, struct module *mod );
int mt_playNote( struct FXinfo *nfo, struct module *mod );
void mt_stopFX( int ch, struct module *mod );
void mt_setCallback( void (*cb)(int , int, void * ), void * data, struct module *m );

#ifdef __cplusplus
}
#endif
#endif
