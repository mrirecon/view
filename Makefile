
all: view

viewer.inc: viewer.ui
	@echo "STRINGIFY(`cat viewer.ui`)" > viewer.inc

view:	view.c draw.[ch] viewer.inc
	gcc -g -Wall -O2 -std=c11 -fopenmp -Wl,-export-dynamic -o view -I../bart/src/ `pkg-config --cflags gtk+-3.0` view.c draw.c `pkg-config --libs gtk+-3.0` $(TOOLBOX_PATH)/lib/libmisc.a $(TOOLBOX_PATH)/lib/libnum.a -lm -lpng

