NightRaid: Copyright (c) 2018 Evan Barger, MIT. The official LICENSE, README.md
and complete original sketch, including inline plane/turret artwork, are kept
byte-for-byte in vendor/original at commit
36e7fa0048d39e93d0c093e8ad0a6a55020681fe.

Modified 2026-10-10 for PXA. platform.patch declares explicit AVR16 persistent
integer storage and EEPROM literal widths; portable integer Print replaces
Arduino String without changing decimal text. Original local nullMissile and
playerMissile are zero-initialized to eliminate undefined fields; scored is
actually read by the outgoing-missile renderer. Ordinary defined game rules,
targeting, capacities, frame timing, collision, explosions, score, difficulty,
city restoration, bonus plane and original resetGame quirks are retained.

Owner glue supplies Arduino sketch forward declarations and full cold global
initialization on NEW/RESTART. It models the original OLED invert(bool) command
in this owner's game.cpp: the original source drawing buffer is unchanged,
and the published physical display is complemented after an actual display()
while inversion is enabled. That process state is packed explicitly with all
original mutable globals, all 14 missiles, plane and eight cities. No game
state loader or replacement gameplay is present. Original artwork is MIT.
Arduboy2's separately pinned mixed BSD notices and PXA MIT glue notices are
included with the frozen common and assets licenses. No hardware/AVR firmware
or durable EEPROM is claimed. Original UP+A+B pause is tested natively; the
single-pointer desktop frontend cannot hold three independent buttons, and
the supported wrapper MENU/CONTINUE remains available.
