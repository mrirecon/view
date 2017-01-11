/* Copyright 2015. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Author:
 *	2015 Martin Uecker <martin.uecker@med.uni-goettingen.de>
 */

#include <math.h>
#include <complex.h>
#include <assert.h>

#include "misc/misc.h"

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
	return clamp(0., 1., (x - a) / (b - a));
}

static void trans_magnitude(double rgb[3], double a, double b, complex double value)
{
	double magn = window(a, b, cabs(value));

	rgb[0] *= magn;
	rgb[1] *= magn;
	rgb[2] *= magn;
}

static void trans_magnitude_viridis(double rgb[3], double a, double b, complex double value)
{
	double magn = window(a, b, cabs(value));
	// since window returns nonsense (NaN) if a == b, and since casting nonsense to int
	// and using it as an array subscript is bound to lead to segfaults,
	// we catch that case here
	if ( isfinite(magn) ) {
		int subscript = magn*255.;

		rgb[0] *= viridis[subscript][0];
		rgb[1] *= viridis[subscript][1];
		rgb[2] *= viridis[subscript][2];
	}
}

static void trans_real(double rgb[3], double a, double b, complex double value)
{
	rgb[0] *= window(a, b, +creal(value));
	rgb[1] *= window(a, b, -creal(value));
	rgb[2] *= 0.;
}

static void trans_phase(double rgb[3], double a, double b, complex double value)
{
	rgb[0] *= (1. + sin(carg(value) + 0. * 2. * M_PI / 3.)) / 2.;
	rgb[1] *= (1. + sin(carg(value) + 1. * 2. * M_PI / 3.)) / 2.;
	rgb[2] *= (1. + sin(carg(value) + 2. * 2. * M_PI / 3.)) / 2.;
}

static void trans_phase_MYGBM(double rgb[3], double a, double b, complex double value)
{
	double arg = carg(value);
	if (isfinite(arg)) {
		int subscript = (arg+M_PI)/2./M_PI*255.;
		assert( (0 <= subscript) && (subscript <=255) );
	
		rgb[0] *= cyclic_mygbm[subscript][0];
		rgb[1] *= cyclic_mygbm[subscript][1];
		rgb[2] *= cyclic_mygbm[subscript][2];
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


complex float sample(int N, const float pos[N], const long dims[N], const long strs[N], const complex float* in)
{
	float rem[N];
	int div[N];
	int D = 0;
	long strs2[N];

	// 0 1. [0 1] dims 2
	for (int i = 0; i < N; i++) {

		div[i] = 0;
		rem[i] = 0.;

		if (dims[i] > 1) {
	
			div[i] = truncf(pos[i]);
			float xrem = pos[i] - truncf(pos[i]);

			if (xrem != 0.) {

				strs2[D] = strs[i] / sizeof(complex float);
				rem[D++] = xrem;
			}

		//printf("%d\n", div[i]);

			if ((div[i] < 0) || (div[i] >= dims[i]) || ((div[i] >= dims[i] - 1) && (xrem > 0.)))
				return 0.;
		}
	}

	long off0 = 0;

	for (int i = 0; i < N; i++)
		off0 += div[i] * strs[i];

	return int_nlinear(D, rem, strs2, (const complex float*)(((char*)in) + off0));
}


extern void resample(int X, int Y, long str, complex float* buf,
	int N, const double pos[N], const double dx[N], const double dy[N], 
	const long dims[N], const long strs[N], const complex float* in)
{
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {

			float pos2[N];
			for (int i = 0; i < N; i++)
				pos2[i] = pos[i] + x * dx[i] + y * dy[i];

			buf[str * y + x] = sample(N, pos2, dims, strs, in);
		}
	}
}




extern void draw(int X, int Y, int rgbstr, unsigned char* rgbbuf,
	enum mode_t mode, float scale, float winlow, float winhigh, float phrot,
	long str, const complex float* buf)
{
	for (int x = 0; x < X; x++) {
		for (int y = 0; y < Y; y++) {

			complex float val = scale * buf[str * y + x];

			val *= cexpf(1.i * phrot);

			double rgb[3] = { 1., 1., 1. };

			switch (mode) {

			case MAGN: trans_magnitude(rgb, winlow, winhigh, val); break;
			case MAGN_VIRIDS: trans_magnitude_viridis(rgb, winlow, winhigh, val); break;
			case PHSE: trans_phase(rgb, winlow, winhigh, val); break;
			case PHSE_MYGBM: trans_phase_MYGBM(rgb, winlow, winhigh, val); break;
			case CMPL: trans_complex(rgb, winlow, winhigh, val); break;
			case CMPL_MYGBM: trans_complex_MYGBM(rgb, winlow, winhigh, val); break;
			case REAL: trans_real(rgb, winlow, winhigh, val); break;
			case FLOW: trans_flow(rgb, winlow, winhigh, val); break;
			default: assert(0);
			}

			rgbbuf[y * rgbstr + x * 4 + 0] = 255. * rgb[2];
			rgbbuf[y * rgbstr + x * 4 + 1] = 255. * rgb[1];
			rgbbuf[y * rgbstr + x * 4 + 2] = 255. * rgb[0];
			rgbbuf[y * rgbstr + x * 4 + 3] = 0.;
		}
	}
}



