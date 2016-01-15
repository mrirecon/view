
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

viewer.inc: viewer.ui
	@echo "STRINGIFY(`cat viewer.ui`)" > viewer.inc

view:	view.c draw.[ch] viewer.inc
	$(CC) $(CFLAGS) -export-dynamic -o view -I$(TOOLBOX_INC) `pkg-config --cflags gtk+-3.0` view.c draw.c `pkg-config --libs gtk+-3.0` $(TOOLBOX_LIB)/libmisc.a $(TOOLBOX_LIB)/libnum.a -lm -lpng


install:
	install view $(DESTDIR)/usr/lib/bart/commands/


clean:
	rm -f view viewer.inc
