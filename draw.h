
#include <complex.h>


enum mode_t { MAGN, CMPL, PHSE, REAL, FLOW };

extern complex float sample(int N, const float pos[N], const long dims[N], const long strs[N], const complex float* in);

extern void resample(int X, int Y, long str, complex float* buf,
	int N, const double pos[N], const double dx[N], const double dy[N], 
	const long dims[N], const long strs[N], const complex float* in);

extern void draw(int X, int Y, int rgbstr, unsigned char* rgbbuf,
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf);


