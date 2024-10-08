

-include Makefile.local

CUDA?=0
CUDA_BASE ?= /usr/local/cuda
CUDA_LIB ?= lib
DEBUG?=0
OMP?=1
PKG_CONFIG?=pkg-config

BUILDTYPE = Linux
UNAME = $(shell uname -s)

ifeq ($(UNAME),Darwin)
    BUILDTYPE = MacOSX
endif


ifeq ($(BART_TOOLBOX_PATH),)
TOOLBOX_INC=/usr/include/bart/
TOOLBOX_LIB=/usr/lib/bart/
else
TOOLBOX_INC=$(BART_TOOLBOX_PATH)/src/
TOOLBOX_LIB=$(BART_TOOLBOX_PATH)/lib/
endif


ifeq ($(origin CC), default)
	CC = gcc
endif

CFLAGS ?= -Wall -Wextra

ifeq ($(DEBUG),1)
	CFLAGS += -Og -g
else
	CFLAGS += -O2
endif

ifeq ($(BUILDTYPE), MacOSX)
	CFLAGS += -std=gnu11 -Xpreprocessor
else
	CFLAGS += -std=gnu11
endif

ifeq ($(OMP),1)
	CFLAGS += -fopenmp
else
	CFLAGS += -Wno-unknown-pragmas
endif


# clang

ifeq ($(findstring clang, $(CC)), clang)
	CFLAGS += -fblocks
	LDFLAGS += -lBlocksRuntime
endif


ifeq ($(CUDA),1)
    CUDA_L := -L$(CUDA_BASE)/$(CUDA_LIB) -lcufft -lcudart -lcublas -lblas
else
    CUDA_L :=
endif


EXPDYN = -rdynamic


ifeq ($(BUILDTYPE), MacOSX)
	LDFLAGS += -L/opt/local/lib -lm -lpng -lomp -lrt
else
	LDFLAGS += -lm -lpng -lrt
endif



all: view cfl2png

src/viewer.inc: src/viewer.ui
	@echo "STRINGIFY(`cat src/viewer.ui`)" > src/viewer.inc

view:	src/main.c src/view.[ch] src/draw.[ch] src/gtk_ui.[ch] src/viewer.inc
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXPDYN) -o view -I$(TOOLBOX_INC) `$(PKG_CONFIG) --cflags gtk+-3.0` src/main.c src/view.c src/gtk_ui.c src/draw.c `$(PKG_CONFIG) --libs gtk+-3.0` $(TOOLBOX_LIB)/libmisc.a $(TOOLBOX_LIB)/libgeom.a $(TOOLBOX_LIB)/libnum.a $(TOOLBOX_LIB)/libmisc.a $(CUDA_L) $(LDFLAGS)

cfl2png:	src/cfl2png.c src/view.[ch] src/draw.[ch] src/viewer.inc
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXPDYN) -o cfl2png -I$(TOOLBOX_INC) src/cfl2png.c src/draw.c $(TOOLBOX_LIB)/libmisc.a  $(TOOLBOX_LIB)/libgeom.a $(TOOLBOX_LIB)/libnum.a $(TOOLBOX_LIB)/libmisc.a $(CUDA_L) $(LDFLAGS)

install:
	install -D view $(DESTDIR)/usr/lib/bart/commands/view
	install cfl2png $(DESTDIR)/usr/lib/bart/commands/


clean:
	rm -f view cfl2png src/viewer.inc

