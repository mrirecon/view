
#include <complex.h>
#include <stdbool.h>

#include "view.h"

extern complex float sample(int N, const float pos[N], const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in);

extern void resample(int X, int Y, long str, complex float* buf,
	int N, const double pos[N], const double dx[N], const double dy[N], 
	const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in);

extern void draw(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4],
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf);

extern void draw_plot(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4],
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf);

extern void update_buf(long xdim, long ydim, int N, const long dims[N],  const long strs[N], const long pos[N],
		enum flip_t flip, enum interp_t interpolation, double xzoom, double yzoom, bool plot,
		long rgbw, long rgbh, const complex float* data, complex float* buf);

extern void draw_line(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4], float x0, float y0, float x1, float y1, const char (*color)[3]);
extern void draw_grid(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4], const float (*coord)[4][2], int divs, const char (*color)[3]);

extern const char color_white[3];
extern const char color_red[3];
extern const char color_blue[3];

extern char* construct_filename_view(unsigned int D, const long loopdims[D], const long pos[D], const char* prefix, const char* ext);
