
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

#include "draw.h"
#include "view.h"

#ifndef CFL_SIZE
#define CFL_SIZE sizeof(complex float)
#endif

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

	const struct opt_s opts[] = {
		OPT_INT('x', &xdim, "xdim", "output xdim (default: 0) "),
		OPT_INT('y', &ydim, "ydim", "output ydim (default: 0). If both are zero, first two non-singleton dims are used."),
		OPT_FLOAT('l', &windowing[0], "l", "lower windowing value"),
		OPT_FLOAT('u', &windowing[1], "u", "upper windowing value"),
		OPT_FLOAT('z', &zoom, "z", "zoom factor (default: 2) "),
		OPT_SUBOPT('C', "cmap", "colormap. -Ch for help.", ARRAY_SIZE(modeopt), modeopt),
		OPT_SUBOPT('F', "flip", "flip. -Fh for help.", ARRAY_SIZE(flipopt), flipopt),
		OPT_INT('d', &debug_level, "level", "Debug level"),
	};
	cmdline(&argc, argv, 2, 1000, usage_str, help_str, ARRAY_SIZE(opts), opts);

	assert((windowing[0] >= 0.f) && (windowing[1] <= 1.f) && (windowing[0] < windowing[1]));

	const char* infile = argv[1];
	const char* output_prefix = argv[2];

	assert((0 <= xdim) && (xdim < DIMS));
	assert((0 <= ydim) && (ydim < DIMS));

	/*
	 * If the filename ends in ".hdr", ".cfl" or just "." (from
	 * tab-completion), just replace the "." with a \0-character.
	 */
	char* dot = strrchr(infile, '.');

	if ((NULL != dot) && (	 !strcmp(dot, ".cfl")
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

	export_images(output_prefix, xdim, ydim, windowing, zoom, mode, flip, dims, idata);


	unmap_cfl(DIMS, dims, idata);

	return 0;
}
