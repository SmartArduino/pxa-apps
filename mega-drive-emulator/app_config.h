/* PXA console profile. GPL-2.0-or-later. */
#pragma once
#define EMU_APP_ID "pxa-mega-drive"
#define EMU_SYSTEM_TAG "MD"
#define EMU_SAVE_TAG "md"
#define EMU_EXTENSIONS "md|gen|bin|smd"
#define EMU_ROM_LIMIT (16u*1024u*1024u+512u)
#define EMU_LINEAR_MEMORY 8388608
#define EMU_MAX_SOURCE_WIDTH 512u
#define EMU_MAX_SOURCE_HEIGHT 512u
#define EMU_CONTROLLER_STYLE 6
#define EMU_ROM_PADDING 0u
static inline size_t emu_md_rom_capacity(size_t size){
    if(size%0x4000==512)size-=512;
    size_t n=0x80000;while(n<size)n*=2;return n+0x80004;
}
#define EMU_ROM_CAPACITY(size) emu_md_rom_capacity(size)

/* Keep cartridge allocation from consuming the core initialization budget. */
#define EMU_ROM_WORKSPACE_RESERVE 524288u
