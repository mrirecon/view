
#ifndef DIMS
#define DIMS 16
#endif

struct view_s;

extern struct view_s* window_new(const char* name, long* pos, const long dims[DIMS], const complex float* x);


