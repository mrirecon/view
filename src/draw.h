
#include <complex.h>


enum mode_t { MAGN, MAGN_VIRIDS, CMPL, CMPL_MYGBM, PHSE, PHSE_MYGBM, REAL, FLOW };
enum flip_t { OO, XO, OY, XY };
enum interp_t { NLINEAR, NEAREST };

extern complex float sample(int N, const float pos[N], const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in);

extern void resample(int X, int Y, long str, complex float* buf,
	int N, const double pos[N], const double dx[N], const double dy[N], 
	const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in);

extern void draw(int X, int Y, int rgbstr, unsigned char* rgbbuf,
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf);

extern void update_buf(long xdim, long ydim, int N, const long dims[N],  const long strs[N], const long pos[N], enum flip_t flip, enum interp_t interpolation, double xzoom, double yzoom,
		long rgbw, long rgbh, const complex float* data, complex float* buf);

extern char* get_spec(int i);

