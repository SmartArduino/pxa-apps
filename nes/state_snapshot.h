/* GPL-2.0-or-later. Pause-only, versioned snapshots; no frame-loop hooks. */
#pragma once
#include <stddef.h>
__attribute__((noinline,cold)) size_t nes_state_size(void);
__attribute__((noinline,cold)) const char *nes_state_save(void *data,size_t size);
__attribute__((noinline,cold)) const char *nes_state_load(const void *data,size_t size);
