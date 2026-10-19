/* Optional platform-owned render timing. GPL-2.0-or-later. */
#pragma once
enum {
  PXA_QUAKE_RENDER_TOTAL, PXA_QUAKE_RENDER_WORLD, PXA_QUAKE_RENDER_BRUSH,
  PXA_QUAKE_RENDER_SCAN, PXA_QUAKE_RENDER_SURFACES, PXA_QUAKE_RENDER_ENTITIES,
  PXA_QUAKE_RENDER_VIEWMODEL, PXA_QUAKE_RENDER_PARTICLES, PXA_QUAKE_RENDER_STAGES
};
void Pxa_QuakeRenderProfile(unsigned stage,int begin);
#define PXA_RENDER_BEGIN(stage) Pxa_QuakeRenderProfile(stage,1)
#define PXA_RENDER_END(stage) Pxa_QuakeRenderProfile(stage,0)
