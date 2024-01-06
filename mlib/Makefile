#
# (c) 2001-2005 Jouni 'Mr.Spiv' Korhonen / Dead Coders Society & Scoopex
#
#

.PHONY: clean all dep dist bins cbins
.SUFFIXES:
.SUFFIXES: .c .o .h .s .c
.DEFAULT:
	make all
#
#
#
#

INCLUDE = include
SOURCE  = src
EXAMPLE = example
RAW     = raw
DEPEND  = .dep

DIRS    = $(INCLUDE) $(SOURCE) $(EXAMPLE) $(RAW) 

MLIB   = $(CURDIR)/lib/libmlib.a
TARGET = mplayer.gxb

MIXER = -DASMMIXER -DOUTSIDEIRQMIXING
#MIXER = -DASMMIXER -DS8MIXER

LOCAL_LIBDIR = -L$(CURDIR)/lib
LOCAL_LIBS   = -lmlib
MLIB         = $(CURDIR)/lib/libmlib.a

CC = arm-elf-gcc
AS = arm-elf-as
AR = arm-elf-ar
OC = arm-elf-objcopy
B2 = b2fxec

LOCAL_CFLAGS =  -I$(CURDIR)/$(INCLUDE) -mtune=arm920t -O2 -fomit-frame-pointer -finline-functions \
	-fno-exceptions -fno-common -fno-builtin -ffreestanding -mstructure-size-boundary=32 \
	-mno-thumb-interwork -Wall $(MIXER)
LOCAL_AFLAGS =
LOCAL_LDFLAGS = -T $(EXAMPLE)/lnkscript -Wl,-Map,$(TARGET).map  -nostartfiles -lgcc -lc

#
#
#
#

MLIBSRCS    = $(wildcard $(SOURCE)/*.c) $(wildcard $(SOURCE)/*.s)
EXAMPLESRCS = $(wildcard $(EXAMPLE)/*.c) $(wildcard $(EXAMPLE)/*.cpp) $(wildcard $(SOURCE)/*.s)
HDRS        = $(wildcard $(INCLUDE)/*.h) 

MLIBOBJS    = $(addsuffix .o, $(basename $(wildcard $(SOURCE)/*.s))) \
	$(addsuffix .o, $(basename $(wildcard $(SOURCE)/*.c)))
EXAMPLEOBJS = $(EXAMPLE)/crt0.o $(addsuffix .o, $(basename $(wildcard $(EXAMPLE)/*.c)))
RAWOBJS     = $(addsuffix .o, $(basename $(wildcard $(RAW)/*.mod))) $(addsuffix .o, $(basename $(wildcard $(RAW)/*.raw)))

SRCS = $(MLIBSRCS) $(EXAMPLESRCS)

#
#
#
# rules

all: $(DEPEND) $(MLIB) $(TARGET)

mlib: $(MLIB)

$(TARGET): $(MLIB)


ifeq ($(DEPEND),$(wildcard $(DEPEND)))
include $(DEPEND)
endif

$(DEPEND): Makefile
	$(CC) -I$(CURDIR)/$(INCLUDE) -MM $(SRCS) > $(DEPEND)
	@echo "Dependencies done"

%.o: %.mod
	$(OC) -I binary -O elf32-littlearm  -B armv4t --rename-section .data=data $< $@

%.o: %.raw
	$(OC) -I binary -O elf32-littlearm  -B armv4t --rename-section .data=data $< $@


%.o: %.c
	$(CC) $(LOCAL_CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(LOCAL_AFLAGS) $< -o $@

$(MLIB): $(MLIBOBJS)
	$(AR) crs $(MLIB) $(MLIBOBJS)

$(TARGET): $(EXAMPLEOBJS) $(RAWOBJS)
	$(CC) -o $(TARGET).elf  $(EXAMPLEOBJS) $(RAWOBJS) $(LOCAL_LDFLAGS) $(LOCAL_LIBDIR) $(LOCAL_LIBS)
	$(OC) -O binary $(TARGET).elf $(TARGET)
	-$(B2) -gf $(TARGET) C_$(TARGET)

clean:
	-rm $(DEPEND) $(MLIB) $(EXAMPLEOBJS) $(RAWOBJS) $(MLIBOBJS) $(TARGET) C_$(TARGET) $(TARGET).*
	-find . -name "*~" -exec rm {} \;
	-rm *.map

dist:
	#tar cvfz mlib.tgz readme.txt Makefile $(INCLUDE) $(SRCS) $(RAW)
	tar cvfz mlib.tgz ../mlib




