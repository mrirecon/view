/* Copyright 2014-2017. The Regents of the University of California.
 * All rights reserved. Use of this source code is governed by
 * a BSD-style license which can be found in the LICENSE file.
 */

#include "misc/mri.h"

struct idx_s {
	unsigned int dim;
	unsigned int idx;
};

struct opt_idx_s {
	struct idx_s idxs[DIMS];
	unsigned int r;
};


extern _Bool opt_idx_init(struct opt_idx_s* iopts);

extern _Bool opt_idx(void* ptr, char c, const char* optarg);

extern void help_idx(void);

#include "misc/cppwrap.h"
