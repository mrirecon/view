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

struct view_ui_params_s {
	bool* selected;

	double zoom;
	double aniso;

	bool transpose;
};


struct view_s {
	const char* name;
	struct view_settings_s settings;
	struct view_ui_params_s ui_params;
	struct view_control_s* control;
	struct view_gtk_ui_s* ui;

	// change-management
	bool invalid;
	bool rgb_invalid;

	// multi-window
	struct view_s* next;
	struct view_s* prev;
	bool sync;

	// shared state (owned by controller)
	double win_high_max;
	double win_low_max;
};


// setup etc
extern struct view_s* window_new(const char* name, const long pos[DIMS], const long dims[DIMS], const _Complex float* x, _Bool absolute_windowing);

extern void window_connect_sync(struct view_s* a, struct view_s* b);



// usually callbacks:
extern void view_fit(struct view_s* v, int width, int height);
extern void view_geom(struct view_s* v);
extern void view_refresh(struct view_s* v);
extern void view_window(struct view_s* v);

extern bool view_save_png(struct view_s* v, const char *filename);
extern bool view_save_pngmovie(struct view_s* v, const char *folder);

extern void view_motion(struct view_s* v, int x, int y, double inc_low, double inc_high, int button);

extern struct view_s* view_window_clone(struct view_s* v, const long pos[DIMS]);

extern void view_draw(struct view_s* v);

extern void view_toggle_plot(struct view_s* v);

extern void view_add_geometry(struct view_s* v, unsigned long flags, const float (*geom)[3][3]);

extern void view_click(struct view_s* v, int x, int y, int button);


extern void view_toggle_absolute_windowing(struct view_s* v);

extern void view_window_close(struct view_s* v);


//




// helpers
extern char *construct_filename_view2(struct view_s* v);

#endif
