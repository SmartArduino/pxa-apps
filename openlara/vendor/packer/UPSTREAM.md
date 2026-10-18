# OpenLara fixed data converter

From `src/platform/gba/packer` at OpenLara commit
`3b268ca4bf986c380f44a26ffd9218bf6d376249` (BSD-2-Clause; ../LICENSE).

The portable entry point is `../../tools/packer.cpp`. PXA removes the unused
Windows process and image-quantizer dependencies, enumerates AD4 files using
C++17 filesystem, checks binary reads/writes, and initializes the output header.
The conversion retains all original rooms and sounds; it does not call the
upstream alpha's `cutData()` function. No game assets are included here.
