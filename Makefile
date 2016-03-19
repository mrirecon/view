
ifeq ($(TOOLBOX_PATH),)
TOOLBOX_INC=/usr/include/bart/
TOOLBOX_LIB=/usr/lib/bart/
else
TOOLBOX_INC=$(TOOLBOX_PATH)/src/
TOOLBOX_LIB=$(TOOLBOX_PATH)/lib/
endif

CFLAGS ?= -Wall -O2
CFLAGS += -std=c11 -fopenmp


all: view

src/viewer.inc: src/viewer.ui
	@echo "STRINGIFY(`cat src/viewer.ui`)" > src/viewer.inc

view:	src/main.c src/view.[ch] src/draw.[ch] src/viewer.inc
	$(CC) $(CFLAGS) -export-dynamic -o view -I$(TOOLBOX_INC) `pkg-config --cflags gtk+-3.0` src/main.c src/view.c src/draw.c `pkg-config --libs gtk+-3.0` $(TOOLBOX_LIB)/libmisc.a $(TOOLBOX_LIB)/libnum.a -lm -lpng


install:
	install view $(DESTDIR)/usr/lib/bart/commands/


