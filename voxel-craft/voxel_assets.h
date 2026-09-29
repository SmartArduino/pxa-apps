#ifndef VOXEL_ASSETS_H
#define VOXEL_ASSETS_H
#include "pxa_assets.h"

/* Prepare a fresh context; no frames consume these bindings until READY.
 * One request/handle at a time keeps quotas and Guest workspace constant. */
int voxel_assets_begin(uint64_t context, uint32_t capabilities);
/* 0 unrelated; 1 consumed/pending; 2 ready; -1 terminal load/bind failure. */
int voxel_assets_on_event(const pxa_event_t *event);
void voxel_assets_cancel(void);
int voxel_assets_suspend(int suspended);
#endif
