//////////////////////////////////////////////////////////////////////////////
//
// Module:
//  player.c
//
// Description:
//  This module implements a ProTracker compliant module player that can play
//  modules up to 16 channels. The player is also able to play up to 16 user
//  defined sound FXs. Thus the maximum number of supported simultaneous
//  channels is 32.
//  This player uses simple double buffered ring sample buffer. The approach
//  works fine with DMA based sound output, which is able to generate IRQ 
//  after one buffer has been played. No polling is required.
//  The FX system is able to play two kinds of samples:
//    1) sample stored in the modfile itself ( -> playNote() )
//    2) external signed 8 bits RAW sample   ( -> playFX()   )
//  This player does not depend on the libc or any other host system
//  dependant function.
//
// Note:
//  This was my first attempt to code a modplayer & mixer (since Amiga days
//  but that's slightly different). As you can see from the function names
//  the structure follows those old Amiga ASM players rather accurately.
//  I'm not really happy with the sound quality.. nor with the mixer speed.
//  However, I also reached the point where I either would have to study
//  what others have done or really learn sound related signal prosessing.
//  And lazyness won ;-)
//
// Author:
//  (c) 2005 Jouni 'Mr.Spiv' Korhonen (jouni.korhonen@iki.fi)
//
// Version:
//  - v0.1  xx-Feb-2005 Initial release (does not implement proper tempo)
//  - v0.2  06-Mar-2005 Added ARM ASM mixer and tempo. Hopefully the tempo
//          works corretly - I'm not really convinced about that ;)
//  - v0.3  20-Mar-2005 Slight clean up... giving up with this source :|
//          Some bugs still there -- I think.
//  - v0.4  5-Apr-2005 Added callbacks..
//  - v0.5  17-Apr-2005 Fixed effects 3xx, 6xx and 7xx.. and something else
//          too but forgot what.
//  - v0.6  14-Jun-2005 Added 8bits mixer support and moved mixer code to
//          a separate file.
//
//////////////////////////////////////////////////////////////////////////////

#include "player.h"
#include "mixer.h"

//

static unsigned char mt_funkTable[];
static unsigned char mt_vibratoTable[];
static short mt_periodTable[16][37];
static void mt_noNewNote( struct module * );
static void mt_getNewNote(  struct module * );
static void mt_arpeggio( struct module *mod, int n );
static void mt_portaUp( struct module *mod, int n, int fine );
static void mt_portaDown( struct module *mod, int n, int fine );
static void mt_tonePortamento( struct module *mod, int n );
static void mt_tonePortNoChange( struct module *mod, int n );
static void mt_vibrato( struct module *mod, int n );
static void mt_vibrato2( struct module *mod, int n );
static void mt_tonePlusVolSlide( struct module *mod, int n );
static void mt_vibratoPlusVolSlide( struct module *mod, int n );
static void mt_tremolo( struct module *mod, int n );
static void mt_sampleOffset( struct module *mod, int n );
static void mt_volumeSlide( struct module *mod, int n );
static void mt_positionJump( struct module *mod, int n );
static void mt_volumeChange( struct module *mod, int n );
static void mt_patternBreak( struct module *mod, int n );
static void mt_setSpeed( struct module *mod, int bpm );
static void mt_setGlissControl( struct module *mod, int n );
static void mt_filterOnOff( struct module *mod, int n );
static void mt_setVibratoControl( struct module *mod, int n );
static void mt_setFineTune( struct module *mod, int n );
static void mt_jumpLoop( struct module *mod, int n );
static void mt_setTremoloControl( struct module *mod, int n );
static void mt_retrigNote( struct module *mod, int n );
static void mt_volumeFineUp( struct module *mod, int n );
static void mt_volumeFineDown( struct module *mod, int n );
static void mt_noteCut( struct module *mod, int n );
static void mt_noteDelay( struct module *mod, int n );
static void mt_patternDelay( struct module *mod, int n );
static void mt_setTonePorta( struct module *mod, int n );
static int strncmp_( char *, char *, int );

//

int mt_init( char *data, struct soundBufParams *sbuf, struct module *mod ) {
	char *samples, *instr;
	int n, v, l;

	for (n = 0; n < sizeof(struct module); n++) {
		((char *)mod)[n] = 0;
	}

	mod->sbuf = sbuf;
	mod->moduleData = data;

	// check for a NULL module.. handy if one just wants to use sound FXs
	if (data == (char *)0) {
		mod->enable = 0;
		mod->sbuf->start( mod->sbuf );
		return 0;
	}

	// Parse module.. start with the name..
	mod->modName = data; data += 20;
	instr = data;
  
	// check module type and number of channels & samples
	mod->numInstruments = 31;
	mod->numCh = 4;
	if (!strncmp_(data+1080-20,"M.K.",4)) {
	} else if (!strncmp_(data+1080-20,"M!K!",4)) {
	} else if (!strncmp_(data+1080-20,"FLT4",4)) {
	} else if (!strncmp_(data+1080-20,"FLT8",4)) {
		mod->numCh   = 8;	// not really handled correctly :(
	} else if (!strncmp_(data+1080-20,"4CHN",4)) {
		mod->numCh   = 4;
	} else if (!strncmp_(data+1080-20,"6CHN",4)) {
		mod->numCh   = 6;
	} else if (!strncmp_(data+1080-20,"8CHN",4)) {
		mod->numCh   = 8;
	} else if (!strncmp_(data+1080-20,"10CH",4)) {
		mod->numCh   = 10;
	} else if (!strncmp_(data+1080-20,"12CH",4)) {
		mod->numCh   = 12;
	} else if (!strncmp_(data+1080-20,"14CH",4)) {
		mod->numCh   = 14;
	} else if (!strncmp_(data+1080-20,"16CH",4)) {
		mod->numCh   = 16;
	} else {
		// the module is MOST probably old 15 instrument & 4 channel mod.
		mod->numInstruments = 15;
	}
	if (mod->numCh  > MAX_MOD_CHANNELS) {
		return -1;
	}
	// Skip intrument infos
	data += mod->numInstruments * 30;
	// song length
	mod->songLen = *data++;
	// ciaa	
	mod->ciaa = *data++;
	// Song positions.. and find the size of pattern data..
	mod->songPositions = data;
  
	for (n = v = 0; n < mod->songLen; n++) {
		if (data[n] > v) { v = data[n]; } 
	}
	if (mod->numInstruments == 15) {
		data += 128;
	} else {
		data += 132;
	}
	mod->numPatterns = v + 1;
	mod->patterns   = (unsigned long *)data;
	mod->patternSize = 64 * mod->numCh;
	samples = data + mod->numCh * 256 * mod->numPatterns;	// pointer to the first sample..
  
	// Get instruments..
	for (n = 0; n < mod->numInstruments; n++) {
		mod->instruments[n].name = instr; instr += 22;
		mod->instruments[n].sampleLen = (((instr[0] & 0x7f) << 8)  | (instr[1] & 0xff)) << 1; instr += 2;
		mod->instruments[n].finetune = *instr++;
		mod->instruments[n].volume   = *instr++;
		mod->instruments[n].sampleStart = samples;
		samples[0] = 0; samples[1] = 0; samples[2] = 0; samples[3] = 0;
    
		v = (((instr[0] & 0xff) << 8)  | (instr[1] & 0xff)) << 1; instr += 2;	// repeat
		l = (((instr[0] & 0xff) << 8)  | (instr[1] & 0xff)) << 1; instr += 2;	// replen
    
		mod->instruments[n].loopStart = v;
		mod->instruments[n].replen = l;
		if (l <= 2) {
			mod->instruments[n].length = mod->instruments[n].sampleLen;
		} else {
			if (v+l > mod->instruments[n].sampleLen) {
				mod->instruments[n].length = mod->instruments[n].sampleLen;
				mod->instruments[n].loopStart = mod->instruments[n].length - l;
			} else {
				mod->instruments[n].length = v+l;
			}
			mod->instruments[n].looped = 1;
		}
		samples += mod->instruments[n].sampleLen;
	}
	// The rest..
  
	mod->songPos = 0;	//
	mod->speed = 6;
	mod->count = 6;
	mod->enable = 1;
	mt_setSpeed( mod, 125 );

	mod->sbuf->start( mod->sbuf );
	return 0;
}

void mt_end( struct module *m ) {
	m->sbuf->stop( m->sbuf );
}
void mt_enable( struct module *m ) {
	m->enable = 1;
}
void mt_disable( struct module *m ) {
	m->enable = 0;
}
void mt_masterVolume( struct module *m, int volume ) {
	if (volume < 0) { volume = 0; }
	if (volume > 31) { volume = 31; }
	m->sbuf->volume( volume );
}

void mt_setCallback( void (*cb)(int , int, void * ), void * data, struct module *m ) {
	m->userCallback = cb;
	m->userData     = data;
}

//

int mt_playNote( struct FXinfo *n, struct module *m ) {
	int ch = MAX_MOD_CHANNELS + n->channel;
	int sm = n->instrument;

	if (ch >= MAX_SUPPORTED_CHANNELS) { return -1; }

	m->channels[ch].volume      = n->volume;
	m->channels[ch].finalVolume = n->volume;
	m->channels[ch].period      = n->freq.period;
	m->channels[ch].finalPeriod = n->freq.period;
	m->channels[ch].start       = m->instruments[sm].sampleStart;
	m->channels[ch].loopstart   = m->instruments[sm].loopStart;
	m->channels[ch].length      = m->instruments[sm].length;
	m->channels[ch].looped      = m->instruments[sm].looped;
	m->channels[ch].pos         = m->instruments[sm].loopStart << PRECISION;
	m->playing |= (1 << ch);
	return 0;
}
int mt_playFX( signed char *smp, int len, struct FXinfo *n, struct module *m ) {
	int ch = MAX_MOD_CHANNELS + n->channel;

	if (ch >= MAX_SUPPORTED_CHANNELS) { return -1; }

	m->channels[ch].volume      = n->volume;
	m->channels[ch].finalVolume = n->volume;
	m->channels[ch].period      = m->sbuf->clockConstant / n->freq.playFreq;
	m->channels[ch].finalPeriod = m->sbuf->clockConstant / n->freq.playFreq;
	m->channels[ch].start       = smp;
	m->channels[ch].loopstart   = 0;
	m->channels[ch].length      = len;
	m->channels[ch].looped      = 0;
	m->channels[ch].pos         = 0;
	m->playing |= (1 << ch);
	return 0;
}
void mt_stopFX( int ch, struct module *m ) {
	m->playing &= ~(1 << ch);
	m->channels[ch].period = 0;
}

//

int mt_music( void *mod, void *magic ) {
	struct module *m = (struct module *)mod;
	if (m->enable) {
		if (++m->count < m->speed) {
			mt_noNewNote( m );
		} else {
			int d;
      
			m->count = 0;
      
			if (m->pattDelTime2) {
				mt_noNewNote( m );
			} else {
				mt_getNewNote( m );
			}
      
			m->patternPos  += m->numCh;			// 16
      
			if ((d = m->pattDelTime)) {
				m->pattDelTime2 = d;
				m->pattDelTime = 0;
			}
			if (m->pattDelTime2) {
				if (--m->pattDelTime2 > 0) {
					m->patternPos -= m->numCh;	// 16
				}
			}
			if (m->PBreakFlag) {
				d = m->PBreakPos;
				m->PBreakPos = 0;
				m->patternPos = d * m->numCh;	// << 4
			}
			if (m->patternPos >= m->patternSize) {	// 1024
				m->posJumpFlag = 1;	// forge mt_nextPosition
			}
		}
		if (m->posJumpFlag) {
			m->patternPos  = m->PBreakPos * m->numCh;
			m->PBreakPos   = 0;
			m->posJumpFlag = 0;
      
			if (++m->songPos >= m->songLen) {
				m->songPos = 0;
				m->patternPos = 0;
			}
		}
	} else {
		m->playing &= MOD_MASK;
	}

	// call the mixer and output the sound..
	mixer( m );

	// check callback..
	if (m->userCallback) {
		m->userCallback( m->songPos, m->patternPos, m->userData );
	}
	
	return 0;
}

static void mt_noNewNote( struct module *mod ) {
	int n;
	for (n = 0; n < mod->numCh; n++) {
		if (mod->channels[n].period == 0) {
			continue;
		}
		switch (mod->channels[n].effect) {
		case 0x00:	// mt_arpeggio
			if (mod->channels[n].params) {
				mt_arpeggio( mod, n );
			}
			break;
		case 0x01:	// mt_portaUp			//!
			mt_portaUp( mod, n, 0xff );
			break;
		case 0x02:	// mt_portaDown
			mt_portaDown( mod, n, 0xff );
			break;
		case 0x03:	// mt_TonePortamento
			mt_tonePortamento( mod, n );
			break;
		case 0x04:	// mt_Vibrato			//!
			mt_vibrato( mod, n );
			break;
		case 0x05:	// mt_TonePlusVolSlide		// done
			mt_tonePlusVolSlide( mod, n );
			break;
		case 0x06:	// mt_VibratoPlusVolSlide	// done
			mt_vibratoPlusVolSlide( mod, n );
			break;
		case 0x07:	// mt_Tremolo				// done
			mt_tremolo( mod, n );
			break;
		case 0x0a:	// mt_VolumeSlide		//!
			mt_volumeSlide( mod, n );
			break;
		case 0x0e:	// mt_E_Commands
			switch (mod->channels[n].params >> 4) {
			case 0x09:	// mt_RetrigNote		// 
				mt_retrigNote( mod, n );
				break;
			case 0x0c:	// mt_NoteCut
				mt_noteCut( mod, n );
				break;
			case 0x0d:	// mt_NoteDelay
				mt_noteDelay( mod, n );
				break;
			}
			break;
		default:
			return;
		}
	}
}

static void mt_getNewNote( struct module *m ) {
	unsigned char params, sample, effect;
	unsigned char *patt;
	short period;
	int n;
  
	patt = (unsigned char*)&m->patterns[(m->songPositions[m->songPos] *
			m->patternSize) + m->patternPos];
  
	m->playing &= MOD_MASK;
  
	for (n = 0; n < m->numCh; n++) {
		// Parse the Pattern Data Entry
		// Big Endian:    SSSSPPPP pppppppp ssssCCCC ccccnnnn 
    
		period  = *patt++ << 8;
		period |= *patt++;
		effect  = *patt++;
		params  = *patt++;
		sample  = ((period >> 8) & 0xf0) | (effect >> 4);
		period &= 0x0fff;
		effect &= 0x0f;
    
		m->channels[n].note   = period;
		m->channels[n].effect = effect;
		m->channels[n].params = params;
    
		// Check if we got a note to play..
    
		if (sample > 0) {
			m->channels[n].volume = m->instruments[sample-1].volume;
		}
		if (period > 0) {
			if (sample == 0 && effect == 0 && params == 0) {
				m->channels[n].pos = m->channels[n].loopstart << PRECISION;
			}
			if ((effect == 0x0e) && (params >= 0x50)) {
				m->channels[n].finetune = params & 0x0f;
			} else if ((effect == 0x05) || (effect == 0x03)) {
				mt_setTonePorta( m, n );
			} else {
				short *pt;
				int i;
        
				if (effect == 0x09) {
					mt_sampleOffset( m, n );
				}
				if (effect == 0x0e &&  params >= 0xd0) {	// reset sample and set period..
					// note delay
					if ((m->channels[n].params & 0x0f) != m->count) {
						m->playing &= ~(1 << n);
					}
				} else {
					m->playing |= 1 << n;
				}
				if (!(m->channels[n].wavecontrol & 0x04)) {
					m->channels[n].vibratopos = 0;
				}
				if (!(m->channels[n].wavecontrol & 0x40)) {
					m->channels[n].tremolopos = 0;
				}
				if (sample > 0) {
					m->channels[n].sample    = sample;
					m->channels[n].finetune  = m->instruments[sample-1].finetune;
					m->channels[n].start     = m->instruments[sample-1].sampleStart;
					m->channels[n].loopstart = m->instruments[sample-1].loopStart;
					m->channels[n].wavestart = m->instruments[sample-1].loopStart;
					m->channels[n].length    = m->instruments[sample-1].length;
					m->channels[n].looped    = m->instruments[sample-1].looped;
					m->channels[n].pos       = m->instruments[sample-1].loopStart << PRECISION;
				}
        
				pt = &mt_periodTable[0][0];
        
				for (i = 0; i < 36; i++) {
					if (period >= pt[i]) { break; }
				}
        
				m->channels[n].period = mt_periodTable[(int)m->channels[n].finetune][i];
			}
		}
		if (m->channels[n].period) {
			m->playing |= 1 << n;
		}

		// check non tick based effects..
    
		switch (effect) {
		case 0x09:	// mt_SampleOffset
			mt_sampleOffset( m, n );
			break;
		case 0x0b:	// mt_PositionJump
			mt_positionJump( m, n );
			break;
		case 0x0c:	// mt_VolumeChange		//!
			mt_volumeChange( m, n );
			break;
		case 0x0d:	// mt_PatternBreak
			mt_patternBreak( m, n );
			break;
		case 0x0f:	// mt_SetSpeed			//!
			mt_setSpeed( m, m->channels[n].params );
			break;
		case 0x0e:	// mt_E_Commands
			switch (params >> 4) {
			case 0x00:	// mt_FilterOnOff
				mt_filterOnOff( m, n );
				break;
			case 0x01:	// mt_portaUp			// t0
				mt_portaUp( m, n, 0x0f );
				break;
			case 0x02:	// mt_portaDown			// t0
				mt_portaDown( m, n, 0x0f );
				break;
			case 0x03:	// mt_SetGlissControl	// t0
				mt_setGlissControl( m, n );
				break;
			case 0x04:	// mt_SetVibratoControl	// t0
				mt_setVibratoControl( m, n );
				break;
			case 0x05:	// mt_SetFineTune		// t0
				mt_setFineTune( m, n );
				break;
			case 0x06:	// mt_JumpLoop			// t0
				mt_jumpLoop( m, n );
				break;
			case 0x07:	// mt_SetTremoloControl	// t0
				mt_setTremoloControl( m, n );
				break;
			case 0x0a:	// mt_VolumeFineUp		// t0
				mt_volumeFineUp( m, n );
				break;
			case 0x0b:	// mt_VolumeFineDown	// t0
				mt_volumeFineDown( m, n );
				break;
			case 0x0e:	// mt_PatternDelay		// t0
				mt_patternDelay( m, n );
				break;
			}
			break;
		default:
			break;
		}
		m->channels[n].finalVolume = m->channels[n].volume;
		m->channels[n].finalPeriod = m->channels[n].period;
	} // for channels
}

static void mt_sampleOffset( struct module *m, int n ) {
	int o;

	//*((long *)1) = 0xdeadbeef;

	o = m->channels[n].params << 8;
  
	if (m->channels[n].length > o) {
		m->channels[n].length -= o;
		m->channels[n].start  += o;
	} else {
		m->channels[n].length = 2;
	}
}

static void mt_tonePortamento( struct module *m, int n ) {
	if (m->channels[n].params) {
		m->channels[n].toneportspeed = m->channels[n].params;
	}
	mt_tonePortNoChange( m, n );
}

static void mt_tonePortNoChange( struct module *m, int n ) {
	int s, g;
  
	if (m->channels[n].wantedperiod == 0) { return; }
  
	s = m->channels[n].toneportspeed;
  
	if (!m->channels[n].toneportdirec) {
		// down
		if ((m->channels[n].period -= s) > m->channels[n].wantedperiod) {
			m->channels[n].period = m->channels[n].wantedperiod;
			m->channels[n].wantedperiod = 0;
		}
	} else {
		// up
		if ((m->channels[n].period += s) < m->channels[n].wantedperiod) {
			m->channels[n].period = m->channels[n].wantedperiod;
			m->channels[n].wantedperiod = 0;
		}
	}
  
	// tonePortaSetPer
  
	s = m->channels[n].period;

	if ((g = m->channels[n].glissfunk) & 0x0f) {
		int i;
		short *pt;
    
		pt = &mt_periodTable[0][0];
    
		for (i = 0; i < 36; i++) {
			if (s >= pt[i]) { break; }
		}
    
		s = mt_periodTable[(int)m->channels[n].finetune][i];	//pt[i];
	}
	m->channels[n].finalPeriod = s;
}
static void mt_noteDelay( struct module *m, int n ) {
	if ((m->channels[n].params & 0x0f) == m->count) {
		if (m->channels[n].period) {
			int sample = m->channels[n].sample;
			m->channels[n].loopstart = m->instruments[sample-1].loopStart;
			m->channels[n].wavestart = m->instruments[sample-1].loopStart;
			m->channels[n].length    = m->instruments[sample-1].length;
			m->channels[n].looped    = m->instruments[sample-1].looped;
			m->channels[n].pos       = m->instruments[sample-1].loopStart << PRECISION;
		}
	}
}
static void mt_positionJump( struct module *m, int n ) {
	m->songPos = m->channels[n].params + 1;
	m->PBreakPos   = 0;
	m->posJumpFlag = 1;
}
static void mt_volumeChange( struct module *m, int n ) {
	int v;
  
	if ((v = m->channels[n].params) > 64) { v = 64; }
  
	m->channels[n].volume = v;
	m->channels[n].finalVolume = v;
}
static void mt_patternBreak( struct module *m, int n ) {
	int p;
  
	p = ((m->channels[n].params >> 4) * 10) + (m->channels[n].params & 0x0f) ;
  
	if (p >= 64) {
		m->PBreakPos = 0;
		m->posJumpFlag = 1;
	} else {
		m->PBreakPos = p;
		m->posJumpFlag = 1;
  }
}
static void mt_setSpeed( struct module *m, int bpm ) {
	if (bpm) {
		if (bpm >= 32) {
			m->sbuf->len = calcBufferSize( m->sbuf, 1, bpm );
		} else {
			m->speed = bpm;
		}
	}
}
static void mt_arpeggio( struct module *m, int n ) {
	int x = 0, i, p;
	short *pt;
  
	switch (m->count % 3) {
	case 0:		// mt_arpeggio2
		m->channels[n].finalPeriod = m->channels[n].period;
		return;
	case 1:		// mt_arpeggio3
		x = m->channels[n].params >> 4;
		break;
	case 2:		// mt_arpeggio1
		x = m->channels[n].params & 0x0f;
		break;
	}
  
	// arpeggio4
	p = m->channels[n].period;
	pt = &mt_periodTable[0][0];
  
	for (i = 0; i < 36; i++) {
		if (p >= pt[i]) { break; }
	}
  
	m->channels[n].finalPeriod = mt_periodTable[(int)m->channels[n].finetune][x+i];	//pt[x+i];
}
static void mt_portaUp( struct module *m, int n, int fine ) {
	m->channels[n].period -= (m->channels[n].params & fine);
  
	if (m->channels[n].period < 113) {
		m->channels[n].period =  113;
	}
	if (fine == 0xff) {
		m->channels[n].finalPeriod = m->channels[n].period;
	}
}
static void mt_portaDown( struct module *m, int n, int fine ) {
	m->channels[n].period += (m->channels[n].params & fine);
  
	if (m->channels[n].period > 856) {
		m->channels[n].period =  856;
	}
	if (fine == 0xff) {
		m->channels[n].finalPeriod = m->channels[n].period;
	}
}
static void mt_setTonePorta( struct module *m, int n ) {
	int i, f;
	short *pt, note;
  
	note = m->channels[n].note;
	f = m->channels[n].finetune;
	pt = &mt_periodTable[0][0];
  
	for (i = 0; i < 36; i++) {
		if (note >= pt[i]) { break; }
	}
  
	if (f & 0x08) {
		note = mt_periodTable[f][i-1];
	} else {
		note = mt_periodTable[f][i];
	}
	if (m->channels[n].period == note) {
		m->channels[n].toneportdirec = 0;
		m->channels[n].wantedperiod = 0;
	} else if (m->channels[n].period < note) {
		m->channels[n].toneportdirec = 1;
		m->channels[n].wantedperiod = note;
	} else {
		m->channels[n].toneportdirec = 0;
		m->channels[n].wantedperiod = note;
	}
}
static void mt_vibrato( struct module *m, int n ) {
	int p;
  
	if ((p = m->channels[n].params)) {
		if (p & 0x0f) {
			m->channels[n].vibratoDepth = p & 0x0f;
		}
		if (p & 0xf0) {
			m->channels[n].vibratoSpeed = p >> 4;
		}
	}
  
	mt_vibrato2( m, n );
}
static void mt_vibrato2( struct module *m, int n ) {
	int p,y;

	p = m->channels[n].vibratopos;
	switch (m->channels[n].wavecontrol & 0x03) {
	case 0:
		if (p >= 32) {
			y = -mt_vibratoTable[p & 0x1f];
		} else {
			y = mt_vibratoTable[p & 0x1f];
		}
		break;
	case 1:
		if (p >= 32) {
				y = 255 - (p << 2);
		} else {
			y = p << 3;
		}
		break;
	default:
		y = 255;
	}
	// mt_vib_set
	y = (y * m->channels[n].vibratoDepth) >> 7;
  
	m->channels[n].finalPeriod = m->channels[n].period + y;
	m->channels[n].vibratopos += m->channels[n].vibratoSpeed;
	m->channels[n].vibratopos &= 0x3f;
}
static void mt_tonePlusVolSlide( struct module *m, int n ) {
	mt_tonePortNoChange( m, n );
	mt_volumeSlide( m, n );
}
static void mt_vibratoPlusVolSlide( struct module *m, int n ) {	// **FIX!
	mt_vibrato2( m, n );
	mt_volumeSlide( m, n );
}
static void mt_tremolo( struct module *m, int n ) {
	int v = 0, p;
  
	if (m->channels[n].params) {
			if (m->channels[n].params & 0xf0) {
				m->channels[n].tremoloSpeed = m->channels[n].params >> 4;
			}
			if (m->channels[n].params & 0x0f) {
				m->channels[n].tremoloDepth = m->channels[n].params & 0x0f;
			}
	}
  
	// mt_tremolo2
	p = m->channels[n].tremolopos;
	switch (m->channels[n].wavecontrol & 0x03) {
	case 0:
		if (p >= 32) {
			v = -mt_vibratoTable[p & 0x1f];
		} else {
			v = mt_vibratoTable[p & 0x1f];
		}
		break;
	case 1:
		if (v >= 32) {
				v = 255 - (p << 2);
		} else {
			v = p << 3;
		}
		break;
	default:
		v = 255;
	}
	// mt_vib_set
	v = (v * m->channels[n].tremoloDepth) >> 6;
	v = v + m->channels[n].volume;

  	if (v < 0) {
  		v = 0;
  	} else if (v > 64) {
  		v = 64;
  	}

  	m->channels[n].finalVolume = v;
	m->channels[n].tremolopos += m->channels[n].tremoloSpeed;
	m->channels[n].tremolopos &= 0x3f;
}
static void mt_volumeSlide( struct module *m, int n ) {
	if (m->channels[n].params & 0xf0) {	// slide up
		if ((m->channels[n].volume += (m->channels[n].params >> 4)) > 64) {
			m->channels[n].volume = 64;
		}
	} else {						// slide down
		if ((m->channels[n].volume -= (m->channels[n].params & 0x0f)) < 0) {
			m->channels[n].volume = 0;
		}
	}
  
	m->channels[n].finalVolume = m->channels[n].volume;
}
static void mt_filterOnOff( struct module *m, int n ) {
	m->filterOnOFF = m->channels[n].params & 0x01 ? 1 : 0;
}
static void mt_setGlissControl( struct module *m, int n ) {
	m->channels[n].glissfunk = (m->channels[n].glissfunk & 0xf0) | (m->channels[n].params & 0x0f);
}
static void mt_setVibratoControl( struct module *m, int n ) {
	m->channels[n].wavecontrol =
	    (m->channels[n].wavecontrol & 0xf0) | (m->channels[n].params & 0x0f);
}
static void mt_setFineTune( struct module *m, int n ) {
	m->channels[n].finetune = m->channels[n].params & 0x0f;
}
static void mt_jumpLoop( struct module *m, int n ) {
	int c;
	if ((c = m->channels[n].params & 0x0f)) {
		if (m->channels[n].loopcount) {
			if (--m->channels[n].loopcount == 0) {
				return;
			}
		} else {
			// mt_jumpcnt
			m->channels[n].loopcount = c;
		}
		// mt_jmploop
		m->PBreakPos = m->channels[n].pattpos;
		m->PBreakFlag = 1;
	} else {
		// mt_setLoop
		m->channels[n].pattpos = m->patternPos >> 4;
	}
}
static void mt_setTremoloControl( struct module *m, int n ) {
	m->channels[n].wavecontrol =
		(m->channels[n].wavecontrol & 0x0f) | (m->channels[n].params << 4);
}
static void mt_retrigNote( struct module *m, int n ) {
	int p, c;
  
	if ((p = m->channels[n].params & 0x0f)) {
		if ((c = m->count) == 0) {
			if (m->channels[n].period) {
				return;
			}
		}
		// mt_rtnskp
		if ((c % p) == 0) {
			int sample = m->channels[n].sample;
			m->channels[n].finetune  = m->instruments[sample-1].finetune;
			m->channels[n].start     = m->instruments[sample-1].sampleStart;

			m->channels[n].loopstart = m->instruments[sample-1].loopStart;
			m->channels[n].wavestart = m->instruments[sample-1].loopStart;
			m->channels[n].length    = m->instruments[sample-1].length;
			m->channels[n].looped    = m->instruments[sample-1].looped;
			m->channels[n].pos       = m->instruments[sample-1].loopStart << PRECISION;
		}
	}
}
static void mt_volumeFineUp( struct module *m, int n ) {
	if ((m->channels[n].volume += (m->channels[n].params & 0x0f)) > 64) {
		m->channels[n].volume = 64;
	}
}
static void mt_volumeFineDown( struct module *m, int n ) {
	if ((m->channels[n].volume -= (m->channels[n].params & 0x0f)) < 0) {
		m->channels[n].volume = 0;
	}
}
static void mt_noteCut( struct module *m, int n ) {
	m->channels[n].finalVolume = 0;
	m->channels[n].volume = 0;
}
static void mt_patternDelay( struct module *m, int n ) {
	if (m->pattDelTime2) { return; }
	m->pattDelTime = (m->channels[n].params & 0x0f) + 1;
}
int strncmp_( char *a, char *b, int len ) {
	int aa = 0, bb = 0, s = 0;
	while (s++ < len) {
		aa = *a++; bb = *b++;
		if (!aa && !bb) { break; }
		if (aa != bb) { break; }
	}
	return aa-bb;
}

//
// Tables..

static unsigned char mt_vibratoTable[] = {
	0, 24, 49, 74, 97,120,141,161,180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,180,161,141,120, 97, 74, 49, 24 };	// 32

static short mt_periodTable[16][37] = {
// Tuning 0, Normal
	{856,808,762,720,678,640,604,570,538,508,480,453,
	 428,404,381,360,339,320,302,285,269,254,240,226,
	 214,202,190,180,170,160,151,143,135,127,120,113,113},
// Tuning 1
	{850,802,757,715,674,637,601,567,535,505,477,450,
	 425,401,379,357,337,318,300,284,268,253,239,225,
	 213,201,189,179,169,159,150,142,134,126,119,113,113},
// Tuning 2
	{844,796,752,709,670,632,597,563,532,502,474,447,
	 422,398,376,355,335,316,298,282,266,251,237,224,
	 211,199,188,177,167,158,149,141,133,125,118,112,112},
// Tuning 3
	{838,791,746,704,665,628,592,559,528,498,470,444,
	 419,395,373,352,332,314,296,280,264,249,235,222,
	 209,198,187,176,166,157,148,140,132,125,118,111,111},
// Tuning 4
	{832,785,741,699,660,623,588,555,524,495,467,441,
	 416,392,370,350,330,312,294,278,262,247,233,220,
	 208,196,185,175,165,156,147,139,131,124,117,110,110},
// Tuning 5
	{826,779,736,694,655,619,584,551,520,491,463,437,
	 413,390,368,347,328,309,292,276,260,245,232,219,
	 206,195,184,174,164,155,146,138,130,123,116,109,109},
// Tuning 6
	{820,774,730,689,651,614,580,547,516,487,460,434,
	 410,387,365,345,325,307,290,274,258,244,230,217,
	 205,193,183,172,163,154,145,137,129,122,115,109,109},
// Tuning 7
	{814,768,725,684,646,610,575,543,513,484,457,431,
	 407,384,363,342,323,305,288,272,256,242,228,216,
	 204,192,181,171,161,152,144,136,128,121,114,108,108},
// Tuning -8
	{907,856,808,762,720,678,640,604,570,538,508,480,
	 453,428,404,381,360,339,320,302,285,269,254,240,
	 226,214,202,190,180,170,160,151,143,135,127,120,120},
// Tuning -7
	{900,850,802,757,715,675,636,601,567,535,505,477,
	 450,425,401,379,357,337,318,300,284,268,253,238,
	 225,212,200,189,179,169,159,150,142,134,126,119,119},
// Tuning -6
	{894,844,796,752,709,670,632,597,563,532,502,474,
	 447,422,398,376,355,335,316,298,282,266,251,237,
	 223,211,199,188,177,167,158,149,141,133,125,118,118},
// Tuning -5
	{887,838,791,746,704,665,628,592,559,528,498,470,
	 444,419,395,373,352,332,314,296,280,264,249,235,
	 222,209,198,187,176,166,157,148,140,132,125,118,118},
// Tuning -4
	{881,832,785,741,699,660,623,588,555,524,494,467,
	 441,416,392,370,350,330,312,294,278,262,247,233,
	 220,208,196,185,175,165,156,147,139,131,123,117,117},
// Tuning -3
	{875,826,779,736,694,655,619,584,551,520,491,463,
	 437,413,390,368,347,328,309,292,276,260,245,232,
 	 219,206,195,184,174,164,155,146,138,130,123,116,116},
// Tuning -2
	{868,820,774,730,689,651,614,580,547,516,487,460,
	 434,410,387,365,345,325,307,290,274,258,244,230,
	 217,205,193,183,172,163,154,145,137,129,122,115,115},
// Tuning -1
	{862,814,768,725,684,646,610,575,543,513,484,457,
	 431,407,384,363,342,323,305,288,272,256,242,228,
	 216,203,192,181,171,161,152,144,136,128,121,114,114}
};
