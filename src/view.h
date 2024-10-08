#ifndef VIEW_VIEW_H
#define VIEW_VIEW_H

#ifndef DIMS
#define DIMS 16
#endif

#include <stdbool.h>


enum mode_t { MAGN, CMPLX, PHASE, REAL, FLOW };
enum flip_t { OO, XO, OY, XY };
enum interp_t { NLINEAR, NLINEARMAG, NEAREST, LIINCO };
enum color_t { NONE, VIRIDIS, MYGBM, TURBO, LIPARI, NAVIA };


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
	enum color_t colortable;
};

struct view_ui_params_s {

	bool* selected;

	double zoom;

	int windowing_digits;
	double windowing_max;
	double windowing_inc;
};


struct view_s {

	const char* name;

	struct view_control_s* control;
	struct view_gtk_ui_s* ui;

	struct view_settings_s settings;
	struct view_ui_params_s ui_params;

	// multi-window
	struct view_s* next;
	struct view_s* prev;
	bool sync;
};


// setup etc
extern struct view_s* window_new(const char* name, const long pos[DIMS], const long dims[DIMS], const _Complex float* x, _Bool absolute_windowing, enum color_t ctab, int realtime);

extern void window_connect_sync(struct view_s* a, struct view_s* b);


// usually callbacks:
extern void view_fit(struct view_s* v, int width, int height);
extern void view_geom(struct view_s* v, const bool* selected, const long* pos, double zoom, double aniso, _Bool transpose, enum flip_t flip, enum interp_t interp);
extern void view_refresh(struct view_s* v);
extern void view_window(struct view_s* v, enum mode_t mode, double winlow, double winhigh);

extern void view_draw(struct view_s* v);

extern bool view_save_png(struct view_s* v, const char *filename);
extern bool view_save_pngmovie(struct view_s* v, const char *folder);

extern void view_motion(struct view_s* v, int x, int y, double inc_low, double inc_high, int button);

extern void view_click(struct view_s* v, int x, int y, int button);

extern void view_toggle_plot(struct view_s* v);
extern void view_toggle_absolute_windowing(struct view_s* v, _Bool state);

extern void view_add_geometry(struct view_s* v, unsigned long flags, const float (*geom)[3][3]);

extern struct view_s* view_window_clone(struct view_s* v);

extern void view_window_close(struct view_s* v);


//
extern _Bool view_acquire(struct view_s* v, _Bool wait);
extern void view_release(struct view_s* v);

// helpers
extern char *construct_filename_view2(struct view_s* v);

#endif

