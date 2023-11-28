#ifndef VIEW_VIEW_H
#define VIEW_VIEW_H

#ifndef DIMS
#define DIMS 16
#endif

#include <stdbool.h>

enum mode_t { MAGN, MAGN_VIRIDS, CMPL, CMPL_MYGBM, PHSE, PHSE_MYGBM, REAL, MAGN_TURBO, FLOW, LIPARI_T1, NAVIA_T2 };
enum flip_t { OO, XO, OY, XY };
enum interp_t { NLINEAR, NLINEARMAG, NEAREST, LIINCO };

struct view_settings_s {

	long* pos; //[DIMS];

	int xdim;
	int ydim;

	double xzoom;
	double yzoom;

	enum flip_t flip;

	enum mode_t mode;

	bool cross_hair;
	bool plot;

	bool absolute_windowing;
	double winhigh;
	double winlow;

	double phrot;

	enum interp_t interpolation;
};

struct view_s {
	const char* name;
	struct view_settings_s settings;
	struct view_control_s* control;
	struct view_gtk_ui_s* ui;

	// change-management
	bool invalid;
	bool rgb_invalid;

	// multi-window
	struct view_s* next;
	struct view_s* prev;
	bool sync;
};


extern struct view_s* window_new(const char* name, const long pos[DIMS], const long dims[DIMS], const complex float* x, _Bool absolute_windowing);

extern void view_add_geometry(struct view_s* v, unsigned long flags, const float (*geom)[3][3]);
extern void window_connect_sync(struct view_s* a, struct view_s* b);

extern void view_refresh(struct view_s* v);
extern void view_setpos(struct view_s* v, unsigned int flags, const long pos[DIMS]);
extern struct view_s* view_clone(struct view_s* v, const long pos[DIMS]);

#endif
