/* Copyright 2017-2023. AG Uecker. University Medical Center Göttingen.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 */

#include <stdio.h>
#include <assert.h>
#include <complex.h>
#include <string.h>
#include <strings.h>

#include "num/multind.h"
#include "num/init.h"

#include "misc/misc.h"
#include "misc/debug.h"
#include "misc/mmio.h"
#include "misc/opts.h"
#include "misc/png.h"
#if 0
#include "misc/io.h"
#else
extern void io_reserve_input(const char* name);
extern void io_unregister(const char* name);
#endif

#include "draw.h"

#ifndef CFL_SIZE
#define CFL_SIZE sizeof(complex float)
#endif

#ifndef DIMS
#define DIMS 16
#endif


static void export_images(const char* output_prefix, int xdim, int ydim, float windowing[2], bool absolute_windowing, float zoom, enum mode_t mode, enum flip_t flip, enum interp_t interpolation, const long dims[DIMS], unsigned long loopflags, long pos[DIMS], const complex float* idata);


static const char help_str[] = "Export images to png.";



int main(int argc, char* argv[argc])
{
	const char* in_file;
	const char* out_prefix;

	struct arg_s args[] = {

		ARG_INFILE(true, &in_file, "input"),
		ARG_STRING(true, &out_prefix, "output_prefix"),
	};


	int xdim = 0;
	int ydim = 0;
	float windowing[2] = {0.f, 1.f};
	bool absolute_windowing = false;
	enum mode_t mode = MAGN;
	float zoom =2.f;
	enum flip_t flip = OO;
	enum interp_t interpolation = NLINEAR;
	unsigned long sliceflags = 0;
	
	long pos[DIMS] = { [0 ... DIMS - 1] = 0  };
	long pos_slc[DIMS] = { [0 ... DIMS - 1] = 0  };
	int pos_count = 0;

	struct opt_s modeopt[] = {
		OPT_SELECT('M', enum mode_t, &mode, MAGN, 		"magnitude gray (default) "),
		OPT_SELECT('V', enum mode_t, &mode, MAGN_VIRIDS, 	"magnitude viridis"),
		OPT_SELECT('C', enum mode_t, &mode, CMPL,	 	"complex"),
		OPT_SELECT('G', enum mode_t, &mode, CMPL_MYGBM,	 	"complex MYGBM"),
		OPT_SELECT('P', enum mode_t, &mode, PHSE, 		"phase"),
		OPT_SELECT('Y', enum mode_t, &mode, PHSE_MYGBM, 	"phase MYGBM"),
		OPT_SELECT('R', enum mode_t, &mode, REAL, 		"real"),
		OPT_SELECT('T', enum mode_t, &mode, MAGN_TURBO,		"magnitude turbo"),
		OPT_SELECT('F', enum mode_t, &mode, FLOW, 		"flow"),
		OPT_SELECT('1', enum mode_t, &mode, LIPARI_T1, 		"lipari (T1, R1)"),
		OPT_SELECT('2', enum mode_t, &mode, NAVIA_T2, 		"navia (T2, R2)"),
	};

	struct opt_s flipopt[] = {
		OPT_SELECT('O', enum flip_t, &flip, OO, 		"flip OO"),
		OPT_SELECT('X', enum flip_t, &flip, XO, 		"flip XO"),
		OPT_SELECT('Y', enum flip_t, &flip, OY, 		"flip OY"),
		OPT_SELECT('Z', enum flip_t, &flip, XY, 		"flip XY"),
	};

	struct opt_s interpopt[] = {
		OPT_SELECT('L', enum interp_t, &interpolation, NLINEAR, 		"n-linear interpolation"),
		OPT_SELECT('M', enum interp_t, &interpolation, NLINEARMAG, 		"n-linear interpolation on the magnitude"),
		OPT_SELECT('N', enum interp_t, &interpolation, NEAREST, 		"nearest neighbor interpolation"),
		OPT_SELECT('C', enum interp_t, &interpolation, LIINCO, 			"line-integral convolution"),
	};

	const struct opt_s opts[] = {
		OPT_INT('x', &xdim, "xdim", "output xdim (default: 0) "),
		OPT_INT('y', &ydim, "ydim", "output ydim (default: 0). If both are zero, first two non-singleton dims are used."),
		OPT_FLOAT('l', &windowing[0], "l", "lower windowing value"),
		OPT_FLOAT('u', &windowing[1], "u", "upper windowing value"),
		OPT_SET('A', &absolute_windowing, "use absolute windowing"),
		OPT_FLOAT('z', &zoom, "z", "zoom factor (default: 2) "),
		OPT_SUBOPT('C', "cmap", "colormap. -Ch for help.", ARRAY_SIZE(modeopt), modeopt),
		OPT_SUBOPT('F', "flip", "flip. -Fh for help.", ARRAY_SIZE(flipopt), flipopt),
		OPT_SUBOPT('I', "interp", "interp. -Ih for help.", ARRAY_SIZE(interpopt), interpopt),
		OPT_INT('d', &debug_level, "level", "Debug level"),
		OPT_ULONG('S', &sliceflags, "flags", "slice selected dims"),
#ifdef OPT_VECC
		OPT_VECC('P', &pos_count, pos_slc, "position for sliced dimensions"),
#endif
#ifdef OPT_VECN
		OPT_VECN('p', pos, "(provide all positions (useful if position is exported from view))"),
#endif
	};

	cmdline(&argc, argv, ARRAY_SIZE(args), args, help_str, ARRAY_SIZE(opts), opts);

	if (!absolute_windowing)
		assert(    (windowing[0] >= 0.f)
			&& (windowing[1] <= 1.f)
			&& (windowing[0] < windowing[1]));

	assert((0 <= xdim) && (xdim < DIMS));
	assert((0 <= ydim) && (ydim < DIMS));

	assert(0 == pos_count || pos_count == bitcount(sliceflags));
	
	if (0 < pos_count) {

		for (int i = 0, ip = 0; i < DIMS; i++) {

			if (MD_IS_SET(sliceflags, i))
				pos[i] = pos_slc[ip++];
		}
	}

	/*
	 * If the filename ends in ".hdr", ".cfl" or just "." (from
	 * tab-completion), just replace the "." with a \0-character.
	 */
	io_unregister(in_file);
	char* dot = strrchr(in_file, '.');

	if (   (NULL != dot)
            && (   !strcmp(dot, ".cfl")
		|| !strcmp(dot, ".hdr")
		|| !strcmp(dot, ".")))
		*dot = '\0';

	io_reserve_input(in_file);

	long dims[DIMS];
	complex float* idata = load_cfl(in_file, DIMS, dims);

	char* ext = rindex(out_prefix, '.');

	if (NULL != ext) {

		if (0 != strcmp(ext, ".png"))
			error("Unknown file extension.");
		else
			*ext = '\0';
	}

	export_images(out_prefix, xdim, ydim, windowing, absolute_windowing, zoom, mode, flip, interpolation, dims, ~sliceflags, pos, idata);


	unmap_cfl(DIMS, dims, idata);

	return 0;
}


/**
 * Convert flat index to pos
 *
 */
static void unravel_index(int D, long pos[D], unsigned long flags, const long dims[D], long index)
{
	long ind = index;
	for (int d = 0; d < D; ++d) {

		if (!MD_IS_SET(flags, d))
			continue;

		pos[d] = ind % dims[d];
		ind /= dims[d];
	}
}

void export_images(const char* output_prefix, int xdim, int ydim, float windowing[2], bool absolute_windowing, float zoom, enum mode_t mode, enum flip_t flip, enum interp_t interpolation, const long dims[DIMS], unsigned long loopflags, long _pos[DIMS], const complex float* idata)
{
	if (xdim == ydim) {

		long sq_dims[2] = { 0 };

		int l = 0;

		for (int i = 0; (i < DIMS) && (l < 2); i++)
			if (1 != dims[i])
				sq_dims[l++] = i;

		assert(2 == l);
		xdim = sq_dims[0];
		ydim = sq_dims[1];
	}

	double max = 0.;

	if (absolute_windowing) {

		max = 1.;
	} else {

		for (long j = 0; j < md_calc_size(DIMS, dims); j++)
			if (max < cabsf(idata[j]))
				max = cabsf(idata[j]);

		if (0. == max)
			max = 1.;
	}

	int rgbw = dims[xdim] * zoom;
	int rgbh = dims[ydim] * zoom;
	int rgbstr = 4 * rgbw;

	// loop over all dims other than xdim and ydim
	long loopdims[DIMS];
	loopflags &= ~(MD_BIT(xdim)|MD_BIT(ydim));
	md_select_dims(DIMS, loopflags, loopdims, dims);

	debug_printf(DP_DEBUG3, "imflags: %lu\nloopdims: ", (MD_BIT(xdim)|MD_BIT(ydim)));
	debug_print_dims(DP_DEBUG3, DIMS, loopdims);



	long strs[DIMS];
	md_calc_strides(DIMS, strs, dims, sizeof(complex float));


#pragma omp parallel for
	for (unsigned long d = 0l; d < md_calc_size(DIMS, loopdims); ++d) {

		long pos[DIMS];
		md_copy_dims(DIMS, pos, _pos);

		for (int i = 0; i < DIMS; i++)
			pos[i] = MIN(pos[i], dims[i] - 1);

		unravel_index(DIMS, pos, loopflags, loopdims, d);

		debug_printf(DP_DEBUG3, "\ti: %lu\n\t", d);
		debug_print_dims(DP_DEBUG3, DIMS, pos);

		// Prepare output filename
		char* name = construct_filename_view(DIMS, loopdims, pos, output_prefix, "png");

		debug_printf(DP_DEBUG2, "\t%s\n", name);

		complex float* buf = xmalloc(rgbh * rgbw * sizeof(complex float));

		update_buf(xdim, ydim, DIMS, dims, strs, pos,
			   flip, interpolation, zoom, zoom, false,
			   rgbw, rgbh, idata, buf);

		unsigned char* rgb = xmalloc(rgbh * rgbstr);

		draw(rgbw, rgbh, rgbstr, (unsigned char(*)[rgbw][rgbstr / 4][4])rgb,
			mode, 1. / max, windowing[0], windowing[1], 0,
			rgbw, buf);

		xfree(buf);

		if (0 != png_write_bgr32(name, rgbw, rgbh, 0, rgb))
			error("Error: writing image file.\n");

		xfree(rgb);
		xfree(name);
	}
}

