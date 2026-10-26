//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001-2005 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#ifndef __STD_H__
#define __STD_H__

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

// Almost everything is C99-compliant nowadays; MSC might be the only
// exception, so just in case -
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
typedef enum {
	false,
	true
} bool;
#endif

// Integer equivalents of fmin()/fmax():
static inline int imin(int x, int y)
{
	if (x < y) {
		return x;
	} else {
		return y;
	}
}

static inline int imax(int x, int y)
{
	if (x > y) {
		return x;
	} else {
		return y;
	}
}

// These often read a lot clearer when imin/imax are being used to restrict
// a value to a particular range:
#define clamp_min                    imax
#define clamp_max                    imin
#define clamp_range(min, value, max) clamp_min(min, clamp_max(value, max))

static inline bool in_range(int min, int value, int max)
{
	// Note: This should probably be 'value < max' but '<=' is used here
	// to be consistent with clamp_range() above.
	return value >= min && value <= max;
}

#endif
