# OpenLara complete gameplay engine

Source: https://github.com/XProger/OpenLara, commit
`3b268ca4bf986c380f44a26ffd9218bf6d376249`, BSD-2-Clause (../LICENSE).
These are the original `src` engine, software renderer, translations, and
bundled decoding libraries. Original notices in the bundled libraries apply.
Game data is separate and is not included in this source directory.

PXA integration uses `__PXA_CLASSIC__` and `OL_CLASSIC_MEMORY`. It routes content
opens, settings, saves, music and input through the Core 2 services, retains
borrowed PHD arrays for the lifetime of their level, and transfers ownership
of static mesh buffers instead of copying them. The original enemy, object,
trigger, room-flip and campaign gameplay logic is retained.

The PC demo title (316138 bytes) uses the early palette placement, like the
demo's LEVEL2. The PXA loader corrects its identification. Missing Linux file
lookups no longer pass a null path to strcat. PXA does not use native threads
or native filesystem access. FMV playback is not yet connected to PXA.

The PXA controller defaults use A/B/X/Y for action/jump/weapon/roll and Start
for inventory; Select belongs to the platform pause menu. Native control
remapping survives level changes. Unavailable trigger/second-player bindings
and the desktop help-key tip are hidden. A separately loaded title palette
keeps the pinned title art independent of the PC model palette. The optional
O3 RGB565 span implementation in ../../classic-raster.cpp preserves the
original scanline interpolation and zero-index transparency.
