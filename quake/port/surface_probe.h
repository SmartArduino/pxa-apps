/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef PXA_QUAKE_RENDER_PROBE
#include "render_profile.h"
#define PXA_SURFACE_BEGIN(stage) Pxa_QuakeRenderProfile(stage,1)
#define PXA_SURFACE_END(stage) Pxa_QuakeRenderProfile(stage,0)
#else
#define PXA_SURFACE_BEGIN(stage) ((void)0)
#define PXA_SURFACE_END(stage) ((void)0)
#endif
