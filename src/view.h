
#ifndef DIMS
#define DIMS 16
#endif

struct view_s;

extern struct view_s* window_new(const char* name, long* pos, const long dims[DIMS], const complex float* x);
extern void window_connect_sync(struct view_s* a, struct view_s* b);

