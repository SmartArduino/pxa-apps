/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MT_PLATFORM_H
#define MT_PLATFORM_H
#include <stdio.h>
FILE *mt_fopen(const char *name, const char *mode);
void mt_exit(int status);
#endif
