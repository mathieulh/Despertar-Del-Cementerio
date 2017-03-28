TARGET = unbricker_install
OBJS = crt0.o main.o pspPSAR.o pspDecrypt.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division 
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

LIBDIR = ../lib
LDFLAGS = -mno-crt0 -nostdlib -nodefaultlibs
LIBS = -lpspsystemctrl_user -lpspkubridge -lpsppower -lpsprtc -lvlfgui -lvlfgu -lvlfutils -lvlflibc -lpspreg 

PSP_FW_VERSION = 271

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Despertar del Cementerio v7.77
PSP_EBOOT_ICON = DC.PNG

BUILD_PRX = 1
PSP_LARGE_MEMORY = 1

PSPSDK=$(shell psp-config --pspsdk-path)
include ./build.mak

