/*
 * Copyright (C) 2013 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

/*---------------------------------------------------------------------------*/

int  hmd_stat() { return 0; }

void hmd_init() { }
void hmd_free() { }
void hmd_step() { }
void hmd_swap() { }

void hmd_prep_left()  { }
void hmd_prep_right() { }

/* Match the declared ABI even when HMD is disabled. */
void hmd_persp(float fov, float aspect) { (void)fov; (void)aspect; }
void hmd_ortho() { }

/*---------------------------------------------------------------------------*/
