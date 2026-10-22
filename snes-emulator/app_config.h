/* PXA console profile. GPL-2.0-or-later. */
#pragma once
#define EMU_APP_ID "pxa-snes"
#define EMU_SYSTEM_TAG "SNES"
#define EMU_SAVE_TAG "snes"
#define EMU_EXTENSIONS "sfc|smc|fig"
#define EMU_ROM_LIMIT (8u*1024u*1024u+512u)
#define EMU_LINEAR_MEMORY 10485760
#define EMU_MAX_SOURCE_WIDTH 512u
#define EMU_MAX_SOURCE_HEIGHT 512u
#define EMU_CONTROLLER_STYLE 4
#define EMU_ROM_PADDING 66048u
#define EMU_ROM_CAPACITY(size) ((size)+EMU_ROM_PADDING)

/* Keep cartridge allocation from consuming the core initialization budget. */
#define EMU_ROM_WORKSPACE_RESERVE 3145728u
