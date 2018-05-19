/* Copyright 2015-2017. The Regents of the University of California.
 * Copyright 2015-2016. Martin Uecker.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Authors:
 * 2018 Sebastian Rosenzweig <sebastian.rosenzweig@med.uni-goettingen.de>
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <complex.h>
#include <stdio.h>
#include <math.h>

#include "misc/mri.h"
#include "misc/utils.h"
#include "misc/opts.h"
#include "misc/debug.h"

#include "iopts.h"


#define CFL_SIZE sizeof(complex float)


void help_idx(void)
{
	printf( "Choose indicex of dimension to show\n"
			"-S <DIM>:<IDX>\n"
	      );
}




bool opt_idx(void* ptr, char c, const char* optarg)
{
	struct opt_idx_s* p = ptr;
	struct idx_s* idxs = p->idxs;
	const int r = p->r;

	int ret = sscanf(optarg, "%d:%d", &idxs[r].dim, &idxs[r].idx);
	assert(2 == ret);

	assert(r < DIMS);

	p->r++;
	return false;
}

bool opt_idx_init(struct opt_idx_s* iopts)
{
	iopts->r = 0;
	return false;
}

