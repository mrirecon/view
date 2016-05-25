/* Copyright 2015-2016. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Author:
 *	2015-2016 Martin Uecker <martin.uecker@med.uni-goettinge.de>
 */

#include <complex.h>
#include <string.h>

#include <gtk/gtk.h>

#undef MAX
#undef MIN

#include "misc/misc.h"
#include "misc/mmio.h"
#include "misc/opts.h"

#include "view.h"



static const char usage_str[] = "<image> ...";
static const char help_str[] = "View images.";


int main(int argc, char* argv[])
{
	gtk_init(&argc, &argv);

	const struct opt_s opts[] = {

	};

	cmdline(&argc, argv, 1, 100, usage_str, help_str, ARRAY_SIZE(opts), opts);

	struct view_s* v = NULL;

	for (int i = 1; i < argc; i++) {

		long dims[DIMS];

		/*
		 * If the filename ends in ".hdr", ".cfl" or just "." (from
		 * tab-completion), just replace the "." with a \0-character.
		 *
		 * Ignoring '.hdr' and '.cfl' is useful since now this viewer
		 * can be set as the default program to open these files from
		 * a file manager.
		 */

		char* dot = strrchr(argv[i], '.');

		if ((NULL != dot) && (	 !strcmp(dot, ".cfl") 
				      || !strcmp(dot, ".hdr") 
				      || !strcmp(dot, ".")))
			*dot = '\0';

		complex float* x = load_cfl(argv[i], DIMS, dims);



		// FIXME: we never delete them
		struct view_s* v2 = window_new(argv[i], NULL, dims, x);

		// If multiple files are passed on the commandline, add them to window
		// list. This enables sync of windowing and so on...

		if (NULL != v) {

			window_connect_sync(v, v2);

		} else {

			v = v2;
		}
	}

	gtk_main();

	return 0;
}
