/* Copyright 2015-2023. Martin Uecker.
 * Copyright 2023-2024. TU Graz. Institute of Biomedical Imaging.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 */

#include "num/multind.h"
#include <complex.h>
#include <string.h>

#if defined __has_include
#if __has_include ("misc/stream.h")
#define HAS_BART_STREAM
#endif
#endif

#include "misc/misc.h"
#include "misc/mmio.h"
#include "misc/opts.h"

#if 0
#include "misc/io.h"
#else
extern void io_reserve_input(const char* name);
extern void io_unregister(const char* name);
#endif

#include "view.h"
#include "gtk_ui.h"



static const char help_str[] = "View images.";


int main(int argc, char* argv[argc])
{
#ifdef UNUSED
	long count;
#else
	int count;
#endif
	const char** in_files;
	int realtime = -1;

	struct arg_s args[] = {

		ARG_TUPLE(true, &count, 1, { OPT_INFILE, sizeof(char*), &in_files, "image" }),
	};

	bool absolute_windowing = false;
	enum color_t ctab = NONE;;

	const struct opt_s opts[] = {

		OPT_SET('a', &absolute_windowing, "Use absolute windowing"),
		OPT_SELECT('V', enum color_t, &ctab, VIRIDIS, "viridis"),
		OPT_SELECT('Y', enum color_t, &ctab, MYGBM, "MYGBM"),
		OPT_SELECT('T', enum color_t, &ctab, TURBO, "turbo"),
		OPT_SELECT('L', enum color_t, &ctab, LIPARI, "lipari"),
		OPT_SELECT('N', enum color_t, &ctab, NAVIA, "navia"),

#ifdef HAS_BART_STREAM
		OPTL_INT(0, "real-time", &realtime, "n", "Realtime Input along axis n"),
#endif
	};

	cmdline(&argc, argv, ARRAY_SIZE(args), args, help_str, ARRAY_SIZE(opts), opts);

	// We initialize the UI after cmdline(), so that we can run '-h' without needing a display
	ui_init(&argc, &argv);

	struct view_s* v = NULL;

	for (int i = 0; i < count; i++) {

		long dims[DIMS];

		/*
		 * If the filename ends in ".hdr", ".cfl" or just "." (from
		 * tab-completion), just replace the "." with a \0-character.
		 *
		 * Ignoring '.hdr' and '.cfl' is useful since now this viewer
		 * can be set as the default program to open these files from
		 * a file manager.
		 */

		io_unregister(in_files[i]);

		char* dot = strrchr(in_files[i], '.');

		if ((NULL != dot) && (	 !strcmp(dot, ".cfl") 
				      || !strcmp(dot, ".hdr") 
				      || !strcmp(dot, ".")))
			*dot = '\0';


		io_reserve_input(in_files[i]);

#ifdef HAS_BART_STREAM
		complex float* x = ((0 <= realtime) ? load_async_cfl : load_cfl)(in_files[i], DIMS, dims);
#else
		complex float* x = load_cfl(in_files[i], DIMS, dims);
#endif

		long pos[DIMS] = { 0 };
		for (int i = 0; i < 3; i++)
			pos[i] = dims[i] / 2;

#ifdef HAS_BART_STREAM
		// absolute windowing for realtime. avoids access to 'unsynced'
		// memory when calculating the windowing
		if (0 <= realtime) {

			absolute_windowing = true;

			// don't set position for streamed dimension.
			md_select_strides(DIMS, ~MD_BIT(realtime), pos, pos);
		}
#endif
		// FIXME: we never delete them
		struct view_s* v2 = window_new(in_files[i], pos, dims, x, absolute_windowing, ctab, realtime);

		// If multiple files are passed on the commandline, add them to window
		// list. This enables sync of windowing and so on...

		if (NULL != v) {

			window_connect_sync(v, v2);

		} else {

			v = v2;
		}
	}

	ui_main();

	return 0;
}

