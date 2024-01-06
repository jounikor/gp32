
** mlib v0.4  (c) 17-Jun-2005 Jouni 'Mr.Spiv/Scoopex' korhonen **


   This library & example program is my pathetic attempt to code a portable
   modplayer. Mostly because I wanted to proof myself I can do one without
   copy & paste and I also boasted to Mithris a long time to code one ;-)
   
   If and _when_ you find bugs feel free to report them and _send_ fixes!
   I really haven't worked on this code that many times and testing has been
   "light" at its best.


   How to compile?
        o Type 'make all' to build both the mlib and examples
        o Type 'make mlib' to build just the mlib
        o Type 'make mplayer' to build the example

  
   mlib compile time "options":

        o OUTSIDEIRQMIXING - selects an IRQ handler that runs player and
          mixer code in other mode than IRQ. Thus other IRQs may interrupt
          the player - good for precise timings and so on.
        o S8MIXER - selects signed 8 bits PCM output instead of signed
          16 bits PCM output (on hardware level).
        o ASMMIXER - selects handwritten ARM assembler versions of the
          mixer routines. You will lose some quality but I bet you won't
          hear the difference.

        Define appropriate defines for your needs. You need to modify the
        Makefile. The default setting is OUTSIDEIRQMIXING and ASMMIXER
        defined.

   Check the example program in the example directory how to use the player
   and play samples along the modules.
 
   Technical stuff:
   	o The library consists six source files
		o player.c - main player code (portable)
		o player.h - structures etc
		o sound.c  - GP32 specific direct hardware level DMA sound buffer
		o sound.h  - structures etc for the above
		o mixer.c  - example mixers (mostly portable)
		o mixer.h  - prototypes for the above
	
	o example player
		o main.c        - simple example player..
		o ModPlayer.cpp - C++ class for module player
		o ModPlayer     - C++ class header for module player

	o Up to 32 simultaneous channels (16 for mods, 16 for sound FXs)
	o Sound FX can either be any sample from a modfile or external 8bits
	  signed mono sample
	o Can be used as a sound ring buffer only (module playback is optional)
	o No SDK dependencies
	o No libc dependency (for gcc you should only need libgcc)
	o Uses only one IRQ (DMA.. no timer based polling)
	o Semi fast mixer (done with inline ARM asm)
	o Player code & mixer can be executed outside IRQ even if the sound
	  synching is done with IRQs -> very fast context switch time
	o Calculates GP32 PCLK etc dynamically based on the user provided
	  output rates
	o Callback functionality for synching your own code based on the module 
	o Total about 2000 lines of code (.c, .h and .s)
	
   The mixers (four of them are provided) are rather simple and far from
   correct ones (when it comes to proper signal processing). Sorry about 
   that.
   
   Excuse my not-too-good makefiles ;-)
   

   Version History:
   
    o v0.1 - initial release
    o v0.2 - added callbacks
    o v0.3 - fixed (hopefully) effects 3xx, 6xx and 7xx
    o v0.4 - more bug fixes and added support for 8bits
             DMA sound buffer 
           - new "outside IRQ" code.. and removed asmfuncs.s file
           - new Makefile 

   Contact:
    jouni.korhonen@iki.fi (http://www.deadcoderssociety.org)
