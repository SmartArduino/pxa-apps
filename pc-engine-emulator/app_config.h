/* PXA console profile. GPL-2.0-or-later. */
#pragma once
#define EMU_APP_ID "pxa-pc-engine"
#define EMU_SYSTEM_TAG "PCE"
#define EMU_SAVE_TAG "pce"
#define EMU_EXTENSIONS "pce"
#define EMU_ROM_LIMIT (4u*1024u*1024u+512u)
#define EMU_LINEAR_MEMORY 6291456
#define EMU_MAX_SOURCE_WIDTH 512u
#define EMU_MAX_SOURCE_HEIGHT 512u
#define EMU_CONTROLLER_STYLE 2
#define EMU_ROM_PADDING 0u
#define EMU_ROM_CAPACITY(size) ((size)+EMU_ROM_PADDING)

/* Keep cartridge allocation from consuming the core initialization budget. */
#define EMU_ROM_WORKSPACE_RESERVE 1048576u
