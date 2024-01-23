#include <stdbool.h>

struct view_s;



extern void ui_rgbbuffer_disconnect(struct view_s* v);
extern void ui_rgbbuffer_connect(struct view_s* v, int rgbw, int rgbh, int rgbstr, unsigned char* buf);

extern void ui_set_selected_dims(struct view_s* v, const bool* selected);

extern void ui_set_position(struct view_s* v, unsigned int dim, unsigned int p);

extern void ui_set_limits(struct view_s* v);
extern void ui_set_values(struct view_s* v);
extern void ui_set_windowing(struct view_s* v, double max, double inc, int digits);

extern void ui_set_mode(struct view_s* v);

extern void ui_set_msg(struct view_s* v, const char* msg);


extern void ui_window_new(struct view_s* v, int N, const long dims[N]);

extern void ui_main();
extern void ui_init(int* argc_p, char** argv_p[]);
extern void ui_loop_quit();


extern void ui_trigger_redraw(struct view_s* v);

extern void ui_pull_geom(struct view_s* v);
extern void ui_pull_window(struct view_s* v);


extern bool gtk_ui_save_png(struct view_s* v, const char* filename);
