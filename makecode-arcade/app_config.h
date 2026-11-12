/* PXA frontend configuration. GPL-2.0-or-later. */
#pragma once
#define EMU_APP_ID "pxa-makecode-arcade"
#define EMU_SYSTEM_TAG "ARCADE"
#define EMU_SAVE_TAG "arcade"
#define EMU_EXTENSIONS "pxt64"
#define EMU_ROM_LIMIT (512u*1024u)
#define EMU_ROM_MIN_SIZE 64u
#define EMU_LINEAR_MEMORY 4194304
#define EMU_MAX_SOURCE_WIDTH 160u
#define EMU_MAX_SOURCE_HEIGHT 120u
#define EMU_CONTROLLER_STYLE 2
#define EMU_ROM_PADDING 16u
#define EMU_ROM_CAPACITY(size) ((size)+EMU_ROM_PADDING)
#define EMU_ROM_WORKSPACE_RESERVE (2u*1024u*1024u)
