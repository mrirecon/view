
#ifndef DIMS
#define DIMS 16
#endif

#include "draw.h"
struct view_s {

	struct view_s* next;
	struct view_s* prev;
	bool sync;

	const char* name;

	// geometry
	long* pos; //[DIMS];
	int xdim;
	int ydim;
	double xzoom;
	double yzoom;
	enum flip_t { OO, XO, OY, XY } flip;
	bool transpose;

	// representation
	enum mode_t mode;
	double winhigh;
	double winlow;
	double phrot;
	double max;

	complex float* buf;

	cairo_surface_t* source;

	// rgb buffer
	int rgbh;
	int rgbw;
	int rgbstr;
	unsigned char* rgb;
	bool invalid;
	bool rgb_invalid;

	// data
	long dims[DIMS];
	long strs[DIMS];
	const complex float* data;

	// widgets
	GtkComboBox* gtk_mode;
	GtkComboBox* gtk_flip;
	GtkWidget* gtk_drawingarea;
	GtkWidget* gtk_viewport;
	GtkAdjustment* gtk_winlow;
	GtkAdjustment* gtk_winhigh;
	GtkAdjustment* gtk_zoom;
	GtkAdjustment* gtk_aniso;
	GtkEntry* gtk_entry;
	GtkToggleToolButton* gtk_transpose;

	GtkAdjustment* gtk_posall[DIMS];
	GtkCheckButton* gtk_checkall[DIMS];

	// windowing
	int lastx;
	int lasty;
};


extern struct view_s* window_new(const char* name, long* pos, const long dims[DIMS], const complex float* x);


