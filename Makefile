# Compiler and linker definitions.

APP	= uARM
CC	= gcc
LD	= gcc

# Logic for optional SDL=yes flag.

SDL ?= no

ifeq ($(SDL), yes)
	SDL_CC_OPTIONS := $(shell sdl-config --cflags) -DUSE_SDL
	SDL_LD_OPTIONS := $(shell sdl-config --libs)
endif

# Build type logic.

BUILD ?= debug

ifeq ($(BUILD), avr)

	CC_FLAGS        = -O2 -mmcu=atmega1284p -I/usr/lib/avr/include -DEMBEDDED -D_SIM -ffunction-sections -DAVR_ASM
	LD_FLAGS        = -O2 -mmcu=atmega1284p -Wl,--gc-sections
	CC              = avr-gcc
	LD              = avr-gcc
	EXTRA           = avr-size -Ax $(APP) && avr-objcopy -j .text -j .data -O ihex $(APP) $(APP).hex
	EXTRA_OBJS      = ./emulation-core/SD.o main_avr.o avr_asm.o
endif

ifeq ($(BUILD), debug)
	CC_FLAGS	= -O0 -g -ggdb -ggdb3 -D_FILE_OFFSET_BITS=64 -D__USE_LARGEFILE64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -DLCD_SUPPORT
	LD_FLAGS	= -O0 -g -ggdb -ggdb3 $(SDL_LD_OPTIONS)
	EXTRA_OBJS	= main_pc.o
endif

ifeq ($(BUILD), profile)
	CC_FLAGS	= -O3 -g -pg -fno-omit-frame-pointer -march=core2 -mpreferred-stack-boundary=4  -D_FILE_OFFSET_BITS=64 -D__USE_LARGEFILE64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
	LD_FLAGS	= -O3 -g -pg $(SDL_LD_OPTIONS)
	EXTRA_OBJS	= main_pc.o
endif

ifeq ($(BUILD), opt)
	CC_FLAGS	= -O3 -fomit-frame-pointer -march=core2 -mpreferred-stack-boundary=4 -momit-leaf-frame-pointer -D_FILE_OFFSET_BITS=64 -D__USE_LARGEFILE64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -flto
	LD_FLAGS	= $(CC_FLAGS) -flto -O3 $(SDL_LD_OPTIONS) 
	EXTRA_OBJS	= main_pc.o
endif

ifeq ($(BUILD), opt64)
	CC_FLAGS	= -m64 -O3 -fomit-frame-pointer -march=core2 -momit-leaf-frame-pointer -D_FILE_OFFSET_BITS=64 -D__USE_LARGEFILE64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
	LD_FLAGS	= -O3 $(SDL_LD_OPTIONS)
	EXTRA_OBJS	= main_pc.o
endif

LDFLAGS = $(LD_FLAGS) -Wall -Wextra
CCFLAGS = $(CC_FLAGS) -Wall -Wextra $(SDL_CC_OPTIONS)

OBJS	= $(EXTRA_OBJS) ./utilities/math64.o \
	./emulation-core/rt.o ./emulation-core/CPU.o \
	./emulation-core/MMU.o ./emulation-core/cp15.o \
	./emulation-core/mem.o ./emulation-core/RAM.o \
	./emulation-core/callout_RAM.o ./emulation-core/SoC.o ./emulation-core/icache.o \
	./emulation-core/pxa255_IC.o ./emulation-core/pxa255_TIMR.o \
	./emulation-core/pxa255_RTC.o ./emulation-core/pxa255_UART.o \
	./emulation-core/pxa255_PwrClk.o ./emulation-core/pxa255_GPIO.o \
	./emulation-core/pxa255_DMA.o ./emulation-core/pxa255_DSP.o \
	./emulation-core/pxa255_LCD.o

$(APP): $(OBJS)
	$(LD) -o $(APP) $(OBJS) $(LDFLAGS)
	$(EXTRA)

AVR:   $(APP)
	sudo avrdude -V -p ATmega1284p -c avrisp2 -P usb -U flash:w:$(APP).hex:i

# Non-standard (AVR or *nix) compilation make targets.

help:
	@echo "make clean | system6 | BUILD = (avr | debug | profile | opt | opt64) [SDL = no | yes]"

clean:
	rm -f $(APP) *.o
	
system7:
	# Assume that cross-compilation with Retro68 is different enough to require a whole
	# different target, because there's a lot of packaging steps afterwards.
