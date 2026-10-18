# OpenLara

Source: https://github.com/XProger/OpenLara

Pinned commit: 3b268ca4bf986c380f44a26ffd9218bf6d376249

BSD-2-Clause; see LICENSE. Vendored fixed engine and portable GBA C++ rasterizer and sound mixer.
PXA changes: platform definitions, 320x240 byte-indexed framebuffer, portable integer formatting, no hardware render hooks; portable input mapping; deferred, incremental level transitions; optional ordinary-RAM raster access and per-polygon texture-pointer snapshots. The PXA adapter retains a 30 Hz simulation independently of presentation.
Additional PXA changes: bounds-checked save serialization and asynchronous restore, signed PCM mixing, validated music-bank ranges, action/jump swap. Platform persistence uses checksummed private files and asynchronous atomic replacement.
Game data is separate and is not covered by the engine license.

The C++ transformRoom fallback shifted an already normalized 0..31 lightmap row right by eight again. PXA now stores that row directly, matching the official GBA ARM transform and official ROM visual reference. Underwater lighting retains its separate 8-bit fractional conversion.
