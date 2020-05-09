

CUDA?=0
CUDA_BASE ?= /usr/local/cuda

BUILDTYPE = Linux
UNAME = $(shell uname -s)

ifeq ($(UNAME),Darwin)
    BUILDTYPE = MacOSX
endif


ifeq ($(TOOLBOX_PATH),)
TOOLBOX_INC=/usr/include/bart/
TOOLBOX_LIB=/usr/lib/bart/
else
TOOLBOX_INC=$(TOOLBOX_PATH)/src/
TOOLBOX_LIB=$(TOOLBOX_PATH)/lib/
endif


CC ?= gcc
CFLAGS ?= -Wall -O2

ifeq ($(BUILDTYPE), MacOSX)
	CFLAGS += -std=c11 -Xpreprocessor -fopenmp
else
	CFLAGS += -std=c11 -fopenmp
endif


ifeq ($(CUDA),1)
    CUDA_L := -L$(CUDA_BASE)/lib64 -lcufft -lcudart -lcublas
else
    CUDA_L :=
endif


ifeq ($(BUILDTYPE), MacOSX)
    EXPDYN = -Wl,-export_dynamic
else
    EXPDYN = -export-dynamic
endif

ifeq ($(BUILDTYPE), MacOSX)
	LIBFLAGS = -L/opt/local/lib -lm -lpng -lomp
else
	LIBFLAGS = -lm -lpng
endif

-include Makefile.local


all: view cfl2png

src/viewer.inc: src/viewer.ui
	@echo "STRINGIFY(`cat src/viewer.ui`)" > src/viewer.inc

view:	src/main.c src/view.[ch] src/draw.[ch] src/viewer.inc
	$(CC) $(CFLAGS) $(EXPDYN) -o view -I$(TOOLBOX_INC) `pkg-config --cflags gtk+-3.0` src/main.c src/view.c src/draw.c `pkg-config --libs gtk+-3.0` $(TOOLBOX_LIB)/libmisc.a $(TOOLBOX_LIB)/libnum.a $(LIBFLAGS)

cfl2png:	src/cfl2png.c src/view.[ch] src/draw.[ch] src/viewer.inc
	$(CC) $(CFLAGS) $(EXPDYN) -o cfl2png -I$(TOOLBOX_INC) src/cfl2png.c src/draw.c $(TOOLBOX_LIB)/libmisc.a $(TOOLBOX_LIB)/libnum.a $(LIBFLAGS)

install:
	install view $(DESTDIR)/usr/lib/bart/commands/


clean:
	rm -f view cfl2png viewer.inc
