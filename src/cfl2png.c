
#include <stdio.h>
#include <assert.h>
#include <complex.h>
#include <string.h>
#include <strings.h>

#undef MAX
#undef MIN

#include "num/multind.h"
#include "num/init.h"

#include "misc/misc.h"
#include "misc/debug.h"
#include "misc/mmio.h"
#include "misc/opts.h"
#include "misc/png.h"

#include "draw.h"
#include "view.h"

#ifndef CFL_SIZE
#define CFL_SIZE sizeof(complex float)
#endif

void export_images(const char* output_prefix, int xdim, int ydim, float windowing[2], float zoom, enum mode_t mode, enum flip_t flip, enum interp_t interpolation, const long dims[DIMS], const complex float* idata);

static const char usage_str[] = "<input> <output_prefix>";
static const char help_str[] = "Export images to png.";



int main(int argc, char* argv[])
{
	int xdim = 0;
	int ydim = 0;
	float windowing[2] = {0.f, 1.f};
	enum mode_t mode = MAGN;
	float zoom =2.f;
	enum flip_t flip = OO;
	enum interp_t interpolation = NLINEAR;

	struct opt_s modeopt[] = {
		OPT_SELECT('M', enum mode_t, &mode, MAGN, 		"magnitude gray (default) "),
		OPT_SELECT('V', enum mode_t, &mode, MAGN_VIRIDS, 	"magnitude viridis"),
		OPT_SELECT('C', enum mode_t, &mode, CMPL,	 	"complex"),
		OPT_SELECT('G', enum mode_t, &mode, CMPL_MYGBM,	 	"complex MYGBM"),
		OPT_SELECT('P', enum mode_t, &mode, PHSE, 		"phase"),
		OPT_SELECT('Y', enum mode_t, &mode, PHSE_MYGBM, 	"phase MYGBM"),
		OPT_SELECT('R', enum mode_t, &mode, REAL, 		"real"),
		OPT_SELECT('F', enum mode_t, &mode, FLOW, 		"flow"),
	};

	struct opt_s flipopt[] = {
		OPT_SELECT('O', enum flip_t, &flip, OO, 		"flip OO"),
		OPT_SELECT('X', enum flip_t, &flip, XO, 		"flip XO"),
		OPT_SELECT('Y', enum flip_t, &flip, OY, 		"flip OY"),
		OPT_SELECT('Z', enum flip_t, &flip, XY, 		"flip XY"),
	};

	struct opt_s interpopt[] = {
		OPT_SELECT('L', enum interp_t, &interpolation, NLINEAR, 		"n-linear interpolation"),
		OPT_SELECT('N', enum interp_t, &interpolation, NEAREST, 		"nearest neighbor interpolation"),
	};

	const struct opt_s opts[] = {
		OPT_INT('x', &xdim, "xdim", "output xdim (default: 0) "),
		OPT_INT('y', &ydim, "ydim", "output ydim (default: 0). If both are zero, first two non-singleton dims are used."),
		OPT_FLOAT('l', &windowing[0], "l", "lower windowing value"),
		OPT_FLOAT('u', &windowing[1], "u", "upper windowing value"),
		OPT_FLOAT('z', &zoom, "z", "zoom factor (default: 2) "),
		OPT_SUBOPT('C', "cmap", "colormap. -Ch for help.", ARRAY_SIZE(modeopt), modeopt),
		OPT_SUBOPT('F', "flip", "flip. -Fh for help.", ARRAY_SIZE(flipopt), flipopt),
		OPT_SUBOPT('I', "interp", "interp. -Ih for help.", ARRAY_SIZE(interpopt), interpopt),
		OPT_INT('d', &debug_level, "level", "Debug level"),
	};

	cmdline(&argc, argv, 2, 1000, usage_str, help_str, ARRAY_SIZE(opts), opts);

	assert(    (windowing[0] >= 0.f)
		&& (windowing[1] <= 1.f)
		&& (windowing[0] < windowing[1]));

	const char* infile = argv[1];
	const char* output_prefix = argv[2];

	assert((0 <= xdim) && (xdim < DIMS));
	assert((0 <= ydim) && (ydim < DIMS));

	/*
	 * If the filename ends in ".hdr", ".cfl" or just "." (from
	 * tab-completion), just replace the "." with a \0-character.
	 */
	char* dot = strrchr(infile, '.');

	if (   (NULL != dot)
            && (   !strcmp(dot, ".cfl")
		|| !strcmp(dot, ".hdr")
		|| !strcmp(dot, ".")))
		*dot = '\0';


	long dims[DIMS];
	complex float* idata = load_cfl(infile, DIMS, dims);

	char* ext = rindex(output_prefix, '.');

	if (NULL != ext) {

		if (0 != strcmp(ext, ".png"))
			error("Unknown file extension.");
		else
			*ext = '\0';
	}

	export_images(output_prefix, xdim, ydim, windowing, zoom, mode, flip, interpolation, dims, idata);


	unmap_cfl(DIMS, dims, idata);

	return 0;
}



/**
 * Convert flat index to pos
 *
 */
static void unravel_index(unsigned int D, long pos[D], const long dims[D], long index)
{
	for (unsigned int d = 0; d < D; ++d) {

		if (1 == dims[d])
			continue;

		pos[d] = index % dims[d];
		index /= dims[d];
	}
}


void export_images(const char* output_prefix, int xdim, int ydim, float windowing[2], float zoom, enum mode_t mode, enum flip_t flip, enum interp_t interpolation, const long dims[DIMS], const complex float* idata)
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
	for (long j = 0; j < md_calc_size(DIMS, dims); j++)
		if (max < cabsf(idata[j]))
			max = cabsf(idata[j]);

	if (0. == max)
		max = 1.;

	int rgbw = dims[xdim] * zoom;
	int rgbh = dims[ydim] * zoom;
	int rgbstr = 4 * rgbw;
	unsigned char* rgb = xmalloc(rgbh * rgbstr);

	complex float* buf = xmalloc(rgbh * rgbw * sizeof(complex float));

	// loop over all dims other than xdim and ydim
	long loopdims[DIMS];
	unsigned long loopflags = (MD_BIT(xdim)|MD_BIT(ydim));
	md_select_dims(DIMS, ~loopflags, loopdims, dims);

	debug_printf(DP_DEBUG3, "flags: %lu\nloopdims: ", loopflags);
	debug_print_dims(DP_DEBUG3, DIMS, loopdims);



	long pos[DIMS] = { [0 ... DIMS - 1] = 0  };
	long strs[DIMS];
	md_calc_strides(DIMS, strs, dims, sizeof(complex float));

	for (unsigned long d = 0l; d < md_calc_size(DIMS, loopdims); ++d){

		unravel_index(DIMS, pos, loopdims, d);

		debug_printf(DP_DEBUG3, "\ti: %lu\n\t", d);
		debug_print_dims(DP_DEBUG3, DIMS, pos);

		// Prepare output filename
		unsigned int bufsize = 255;
		char name[bufsize];
		char* cur = name;
		const char* end = name + bufsize;

		cur += snprintf(cur, end - cur, "%s", output_prefix);

		for (unsigned int i = 0; i < DIMS; i++) {

			if (1 != loopdims[i])
				cur += snprintf(cur, end - cur, "_%s_%04ld", get_spec(i), pos[i]);
		}

		cur += snprintf(cur, end - cur, ".png");

		debug_printf(DP_DEBUG2, "\t%s\n", name);

		update_buf(xdim, ydim, DIMS, dims, strs, pos, flip, interpolation, zoom, zoom,
			   rgbw, rgbh, idata, buf);

		draw(rgbw, rgbh, rgbstr, (unsigned char(*)[rgbw][rgbstr / 4][4])rgb,
			mode, 1. / max, windowing[0], windowing[1], 0,
			rgbw, buf);

		if (0 != png_write_bgr32(name, rgbw, rgbh, 0, rgb))
			error("Error: writing image file.\n");
	}

// 	free(source);
	free(buf);
	free(rgb);
}

