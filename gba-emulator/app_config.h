/* GPL-2.0-or-later. */
#pragma once
/* CPU-heavy scenes gain little from excessive skipping. Retain a 70 ms
 * render budget instead of the shared 90 ms budget under sustained load. */
#define EMU_PACER_VISIBLE_BUDGET_US 70000u
#define EMU_APP_ID "pxa-gba"
#define EMU_SYSTEM_TAG "GBA"
#define EMU_SAVE_TAG "gba"
#define EMU_EXTENSIONS "gba|agb"
#define EMU_ROM_LIMIT (32u*1024u*1024u)
#define EMU_ROM_MIN_SIZE 0xc0u
#define EMU_LINEAR_MEMORY 8388608
#define EMU_MAX_SOURCE_WIDTH 240u
#define EMU_MAX_SOURCE_HEIGHT 160u
#define EMU_CONTROLLER_STYLE 3
#define EMU_ROM_PADDING 0u
#define EMU_ROM_CAPACITY(size) (size)
#define EMU_ROM_WORKSPACE_RESERVE (1536u*1024u)
/* Let gpSP own one cache instead of holding a second complete cartridge. */
#define EMU_ROM_DIRECT_LIMIT 0u
#define EMU_ROM_CACHE_BUDGET (2u*1024u*1024u)
