#include <stdbool.h>

struct view_s;
struct view_ui_params_s;
struct view_settings_s;


extern void ui_rgbbuffer_disconnect(struct view_s* v);
extern void ui_rgbbuffer_connect(struct view_s* v, int rgbw, int rgbh, int rgbstr, unsigned char* buf);

void ui_set_params(struct view_s* v, struct view_ui_params_s params, struct view_settings_s img_params);

extern void ui_set_msg(struct view_s* v, const char* msg);

extern void ui_window_new(struct view_s* v, int N, const long dims[N], const struct view_settings_s settings);

extern void ui_main();
extern void ui_init(int* argc_p, char** argv_p[]);
extern void ui_loop_quit();


extern void ui_trigger_redraw(struct view_s* v);


extern bool gtk_ui_save_png(struct view_s* v, const char* filename);
