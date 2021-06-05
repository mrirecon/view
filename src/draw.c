/* Copyright 2015-2016. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Author:
 *	2015-2016 Martin Uecker <martin.uecker@med.uni-goettingen.de>
 */

#include <math.h>
#include <complex.h>
#include <assert.h>

#include "misc/misc.h"

#include "geom/draw.h"

#include "draw.h"

#include "colormaps.inc"

// multind.h
#define MD_BIT(x) (1u << (x))
#define MD_IS_SET(x, y) ((x) & MD_BIT(y))
#define MD_CLEAR(x, y) ((x) & ~MD_BIT(y))
#define MD_SET(x, y)    ((x) | MD_BIT(y))

static double clamp(double a, double b, double x)
{
	return (x < a) ? a : ((x > b) ? b : x);
}

static double window(double a, double b, double x)
{
	if (a == b)
		return (0. == x) ? 0. : 1.;

	return clamp(0., 1., (x - a) / (b - a));
}

static void trans_magnitude(double rgb[3], double a, double b, complex double value)
{
	double magn = window(a, b, cabs(value));

	rgb[0] *= magn;
	rgb[1] *= magn;
	rgb[2] *= magn;
}

static void interpolate_cmap(double rgb[3], double x, const double cmap[256][3])
{
	int a = x * 255;
	int b = MIN(255, a + 1);
	double f = x * 255. - a;
	for (int i = 0; i < 3; ++i)
		rgb[i] *= cmap[a][i] + f * (cmap[b][i] - cmap[a][i]);
}

static void trans_magnitude_viridis(double rgb[3], double a, double b, complex double value)
{
	double magn = window(a, b, cabs(value));

	interpolate_cmap(rgb, magn, viridis);
}

static void trans_magnitude_turbo(double rgb[3], double a, double b, complex double value)
{
	double magn = window(a, b, cabs(value));

	interpolate_cmap(rgb, magn, turbo);
}

static void trans_real(double rgb[3], double a, double b, complex double value)
{
	rgb[0] *= window(a, b, +creal(value));
	rgb[1] *= window(a, b, -creal(value));
	rgb[2] *= 0.;
}

static void trans_phase(double rgb[3], double a, double b, complex double value)
{
	UNUSED(a);
	UNUSED(b);

	rgb[0] *= (1. + sin(carg(value) + 0. * 2. * M_PI / 3.)) / 2.;
	rgb[1] *= (1. + sin(carg(value) + 1. * 2. * M_PI / 3.)) / 2.;
	rgb[2] *= (1. + sin(carg(value) + 2. * 2. * M_PI / 3.)) / 2.;
}

static void trans_phase_MYGBM(double rgb[3], double a, double b, complex double value)
{
	UNUSED(a);
	UNUSED(b);

	double arg = carg(value);

	if (isfinite(arg)) {

		double val = (arg + M_PI) / 2. / M_PI;
		assert((0.0 <= val) && (val <= 1.0));
	
		interpolate_cmap(rgb, val, cyclic_mygbm);
	}
}

static void trans_complex(double rgb[3], double a, double b, complex double value)
{
	trans_magnitude(rgb, a, b, value);
	trans_phase(rgb, a, b, value);
}

static void trans_complex_MYGBM(double rgb[3], double a, double b, complex double value)
{
	trans_magnitude(rgb, a, b, value);
	trans_phase_MYGBM(rgb, a, b, value);
}

static void trans_flow(double rgb[3], double a, double b, complex double value)
{
	trans_magnitude(rgb, a, b, value);

	double pha = clamp(-1., 1., carg(value) / M_PI);

	rgb[0] *= (1. + pha) / 2.;
	rgb[1] *= (1. - fabs(pha)) / 2.;
	rgb[2] *= (1. - pha) / 2.;
}


// we could move in the multiplication with the factor
// as an extra argument which could save time
static complex float int_nlinear(int N, const float x[N], const long strs[N], const complex float* in)
{
	return (0 == N) ? in[0]
			: (  (1. - x[N - 1]) * int_nlinear(N - 1, x, strs, in + 0)
		           +       x[N - 1]  * int_nlinear(N - 1, x, strs, in + strs[N - 1]));
}


static complex float int_nearest(int N, const float x[N], const long strs[N], const complex float* in)
{
	size_t offs = 0;

	for (int i = 0; i < N; ++i)
		offs += round(x[i]) * strs[i];

	return *(in + offs);
}


static complex float lic_sample(int N, const float pos[N], const long dims[N], const long strs[N], const complex float* in);

complex float sample(int N, const float pos[N], const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in)
{
	if (LIINCO == interpolation)
		return lic_sample(N, pos, dims, strs, in);


	float rem[N];
	int div[N];
	int D = 0;
	long strs2[N];


	// 0 1. [0 1] dims 2
	for (int i = 0; i < N; i++) {

		div[i] = 0;
		rem[i] = 0.;

		if (dims[i] > 1) {
	
			float xrem = 0.;

			// values outside of valid range set to the edge values
			if (pos[i] < 0.) {

				div[i] = 0.;

			} else
			if (pos[i] > (dims[i] - 1)) {

				div[i] = dims[i] - 1;

			} else {

				div[i] = truncf(pos[i]);
				xrem = pos[i] - truncf(pos[i]);
			}

			if (xrem != 0.) {

				strs2[D] = strs[i] / sizeof(complex float);
				rem[D++] = xrem;
			}

			if ((div[i] < 0) || (div[i] >= dims[i]) || ((div[i] >= dims[i] - 1) && (xrem > 0.)))
				return 0.;
		}
	}

	long off0 = 0;

	for (int i = 0; i < N; i++)
		off0 += div[i] * strs[i];

	switch (interpolation) {

	case NLINEAR:
		return int_nlinear(D, rem, strs2, (const complex float*)(((char*)in) + off0));

	case NEAREST:
		return int_nearest(D, rem, strs2, (const complex float*)(((char*)in) + off0));

	default:
		assert(0);
	}
}


static complex float lic_hash(int p0, int p1)
{
	p0 += 12345;
	p0 *= 2654435761U;
	p0 += p1;
	p0 += 12345;
	p0 *= 2654435761U;
	p0 ^= p0 >> 13;
	p0 *= 2654435761U;
	p0 ^= p0 >> 17;
	p0 *= 2654435761U;

	return (1. * (p0 % 256) + 1.i * ((p0 / 256) % 256)) / 256.;
}

// line integral convolution

complex float lic_sample(int N, const float pos[N], const long dims[N], const long strs[N], const complex float* in)
{
	int L = 9;
	float os = 3.;

	assert(N >= 2);
	assert(1 < dims[0]);
	assert(1 < dims[1]);

	complex float out = 0.;

	float pos1[N];

	complex float val = sample(N, pos1, dims, strs, NLINEAR, in);

	for (int i = 0; i < N; i++)
		pos1[i] = pos[i];

	for (int i = 0; i < L; i++) {

		complex float a = sample(N, pos1, dims, strs, NLINEAR, in);

		a /= cabsf(a);

		pos1[0] += crealf(a) / os;
		pos1[1] += cimagf(a) / os;

		int p0 = (int)(os * pos1[0]);
		int p1 = (int)(os * pos1[1]);

		out += lic_hash(p0, p1);
	}

	for (int i = 0; i < N; i++)
		pos1[i] = pos[i];

	for (int i = 0; i < L; i++) {

		complex float a = sample(N, pos1, dims, strs, NLINEAR, in);

		a /= cabsf(a);

		pos1[0] -= crealf(a) / os;
		pos1[1] -= cimagf(a) / os;

		int p0 = (int)(os * pos1[0]);
		int p1 = (int)(os * pos1[1]);

		out += lic_hash(p0, p1);
	}

	return out * cabsf(val);
}


/* The idea is the following:
 * samples sit in the middle of their pixels, so for even zoom factors,
 * the original values are between adjacent pixels, with pixels outside
 * of the valid range (negative pos2 and pos2 greater than dim-1) set to the
 * corresponding values. Therefore we need to start the pos2 array at negative
 * positions.
 **/

extern void resample(int X, int Y, long str, complex float* buf,
	int N, const double pos[N], const double dx[N], const double dy[N], 
	const long dims[N], const long strs[N], enum interp_t interpolation, const complex float* in)
{
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {

			float pos2[N];

			for (int i = 0; i < N; i++) {

				/* start is only != 0 if dx or dy are != 0.
				 * Further, for negative dx/dy, it needs the same sign.
				 * ....0.......1....	d	(|d| - 1.) / 2.
				 * |---*---|---*---|	1.00 ->	-0.000
				 * |-*-|-*-|-*-|-*-|	0.50 ->	-0.250
				 * |*|*|*|*|*|*|*|*|	0.25 ->	-0.375
				 **/

				double start = 	- (dx[i] != 0.) * copysign((fabs(dx[i]) - 1.) / 2., dx[i])
						- (dy[i] != 0.) * copysign((fabs(dy[i]) - 1.) / 2., dy[i]);

				pos2[i] = pos[i] + start + x * dx[i] + y * dy[i];
			}

			buf[str * y + x] = sample(N, pos2, dims, strs, interpolation, in);
		}
	}
}






extern void draw(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4],
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf)
{
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {

			double rgb[3] = { 1., 1., 1. };

			complex float val = scale * buf[str * y + x];

			if (isfinite(crealf(val)) && isfinite(cimagf(val))) {

				val *= cexpf(1.i * phrot);

				switch (mode) {

				case MAGN: trans_magnitude(rgb, winlow, winhigh, val); break;
				case MAGN_VIRIDS: trans_magnitude_viridis(rgb, winlow, winhigh, val); break;
				case PHSE: trans_phase(rgb, winlow, winhigh, val); break;
				case PHSE_MYGBM: trans_phase_MYGBM(rgb, winlow, winhigh, val); break;
				case CMPL: trans_complex(rgb, winlow, winhigh, val); break;
				case CMPL_MYGBM: trans_complex_MYGBM(rgb, winlow, winhigh, val); break;
				case REAL: trans_real(rgb, winlow, winhigh, val); break;
				case MAGN_TURBO: trans_magnitude_turbo(rgb, winlow, winhigh, val); break;
				case FLOW: trans_flow(rgb, winlow, winhigh, val); break;
				default: assert(0);
				}

			} else {

				rgb[0] = 0.;
				rgb[1] = 0.;
				rgb[2] = 0.;
			}

			(*rgbbuf)[y][x][0] = 255. * rgb[2];
			(*rgbbuf)[y][x][1] = 255. * rgb[1];
			(*rgbbuf)[y][x][2] = 255. * rgb[0];
			(*rgbbuf)[y][x][3] = 255.;
		}
	}
}


void update_buf(long xdim, long ydim, int N, const long dims[N], const long strs[N], const long pos[N],
		enum flip_t flip, enum interp_t interpolation, double xzoom, double yzoom, bool plot,
		long rgbw, long rgbh, const complex float* data, complex float* buf)
{
	if (plot)
		rgbh = 1;

	double dpos[N];
	for (int i = 0; i < N; i++)
		dpos[i] = pos[i];

	dpos[xdim] = 0.;

	if (!plot)
		dpos[ydim] = 0.;

	double dx[N];
	for (int i = 0; i < N; i++)
		dx[i] = 0.;

	double dy[N];
	for (int i = 0; i < N; i++)
		dy[i] = 0.;

	dx[xdim] = 1.;

	if (!plot)
		dy[ydim] = 1.;


	if ((XY == flip) || (XO == flip)) {

		dpos[xdim] = dims[xdim] - 1;
		dx[xdim] *= -1.;
	}

	if ((XY == flip) || (OY == flip)) {

		if (!plot) {

			dpos[ydim] = dims[ydim] - 1;
			dy[ydim] *= -1.;
		}
	}

	dx[xdim] = dx[xdim] / xzoom;
	dy[ydim] = dy[ydim] / yzoom;

	resample(rgbw, rgbh, rgbw, buf,
		 N, dpos, dx, dy, dims, strs, interpolation, data);
}


const char color_white[3] = { 255, 255, 255 };
const char color_blue[3] = { 255, 0, 0 };
const char color_red[3] = { 0, 0, 255 };


extern void draw_line(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4], float x0, float y0, float x1, float y1, const char (*color)[3])
{
	unsigned char color2[4] = { (*color)[0], (*color)[1], (*color)[2], 1 };

	bresenham_rgba(Y, X, rgbbuf, &color2, y0, x0, y1, x1);
}


extern void draw_grid(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4], const float (*coord)[4][2], int divs, const char (*color)[3])
{
	for (int i = 0; i <= divs; i++) {
		for (int j = 0; j <= divs; j++) {

			float x0 = (*coord)[0][0] + i * ((*coord)[1][0] - (*coord)[0][0]) / divs;
			float y0 = (*coord)[0][1] + i * ((*coord)[1][1] - (*coord)[0][1]) / divs;

			float x1 = (*coord)[2][0] + i * ((*coord)[3][0] - (*coord)[2][0]) / divs;
			float y1 = (*coord)[2][1] + i * ((*coord)[3][1] - (*coord)[2][1]) / divs;

			draw_line(X, Y, rgbstr, rgbbuf, x0, y0, x1, y1, color);

			x0 = (*coord)[0][0] + i * ((*coord)[2][0] - (*coord)[0][0]) / divs;
			y0 = (*coord)[0][1] + i * ((*coord)[2][1] - (*coord)[0][1]) / divs;

			x1 = (*coord)[1][0] + i * ((*coord)[3][0] - (*coord)[1][0]) / divs;
			y1 = (*coord)[1][1] + i * ((*coord)[3][1] - (*coord)[1][1]) / divs;

			draw_line(X, Y, rgbstr, rgbbuf, x0, y0, x1, y1, color);
		}
	}
}



extern void draw_plot(int X, int Y, int rgbstr, unsigned char (*rgbbuf)[Y][rgbstr / 4][4],
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf)
{
	unsigned char bg[4] = { 255, 255, 255, 0 };
	unsigned char half[4] = { 163, 163, 163, 255 };

	assert(X == rgbstr / 4);
	assert(X == str);

	for (int i = 0; i < X; i++)
		for (int j = 0; j < Y; j++)
			for (int c = 0; c < 4; c++)
				(*rgbbuf)[j][i][c] = bg[c];


	for (int i = 1; i < 10; i++) {

		bresenham_rgba(Y, X, rgbbuf, &half, 0, i * (X / 10), Y - 1, i * (X / 10));
		bresenham_rgba(Y, X, rgbbuf, &half, i * (Y / 10), 0, i * (Y / 10), X - 1);
	}

	unsigned char colorr[4] = { 0, 0, 0, 255 };
	unsigned char colori[4] = { 255, 255, 0, 255 };

	for (int x = 0; x < X - 1; x++) {

		NESTED(double, trafo, (complex float val))
		{
			complex float v2 = scale * val * cexpf(1.i * phrot);

			return window(winlow, winhigh, crealf(v2))
				- window(winlow, winhigh, -crealf(v2));
		};

		float rx0 = (float)(x + 0);
		float ry0 = Y / 2 * (1. - trafo(buf[x + 0]));

		float rx1 = (float)(x + 1);
		float ry1 = Y / 2 * (1. - trafo(buf[x + 1]));

		xiaolin_wu_rgba(Y, X, rgbbuf, &colorr, ry0, rx0, ry1, rx1);

		float ix0 = (float)(x + 0);
		float iy0 = Y / 2 * (1. - trafo(1.i * buf[x + 0]));

		float ix1 = (float)(x + 1);
		float iy1 = Y / 2 * (1. - trafo(1.i * buf[x + 1]));

		xiaolin_wu_rgba(Y, X, rgbbuf, &colori, iy0, ix0, iy1, ix1);
	}
}

