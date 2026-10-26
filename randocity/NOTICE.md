Rando City: Copyright (c) 2019 pmwasson, MIT. Complete pinned original game
source, artwork, screenshots, README and notices are kept byte for byte at
42e9b7e0fe0e17ee231f43577b222288306be238 in vendor/original (22 files).
ArduboyTones: Copyright (c) 2017 Scott Allen, MIT. All six pinned dependency
files are included at 972fe8117002da47073b6c1835117d654a03e16f.

Modified 2026-10-10 for PXA. platform.patch records every modified original
file. Serial.begin is omitted because the unused hardware debug port has no
PXA service. EEPROM.get/put is replaced by explicit original AVR13 field
encoding at480, without native structure padding. World::drawTile computes
its second-page mask only in the original branch that uses it, where its
shift is nonnegative; the original unconditional unused negative shift has
a preserved actual sanitizer failure. randomizeCourier multiplies block
coordinates by2048 rather than shifting negative signed coordinates; a
complete raw-sketch boundary fixture records the original UBSan failure.
The original exclusive random upper bounds and number of RNG draws remain.

Owner software Tones models pinned Timer3 CTC timing at F_CPU16MHz/8,
including retained counter phase, floor(duration*frequency/512), the extra
end-of-sequence interrupt, rest and muted busy state. Complete original
dependency code was compared using fake native registers in367 state
observations under O2 and ASan/UBSan. Sound output is silent; this is not
AVR hardware or instruction verification. The software phase is updated
on the existing cooperative1ms clock, without new Host imports.

NEW and RESTART reconstruct the sketch's declared cold globals, then call
its original setup; process-static Tones state and existing1024-byte session
EEPROM remain. Separate fresh Guests receive new FF EEPROM. The full
120-byte Game wire carries all mutable game/world/font/timer/record/tone
fields, with no object padding, function pointers or state loader.
The first actual Guest cold packet revealed unapplied dynamic initializers
for targets, Timer done defaults, Font buffer and Tones callback/END marker.
An owner-only first-boot construction reproduces these declared cold values
idempotently; it is never reapplied on NEW. The original failed package and
objects are preserved, and full native byte-equivalence is checked against
the pre-fallback oracle with ordinary inputs and elapsed time.

The approved owner frontend adds one DOWN+A touch button (mask24) and
splits its diagnostic state into bounded STATE header and STATE2 post lines.
Both lines remain within the official256-byte Log.Write message limit;
the reader requires exact repeated component/game/seq/gen/clock values
before HASH/BTN/MEM/END and retains strict complete legacy post support.
The actual H4 failure preserved a255-byte payload ending phase=pos.
The frozen256-byte logger is unchanged; worst-case source fields produce
246 bytes per line with complete phase markers and no new Host imports.
The frozen common frontend is kept unchanged and is excluded from this
owner's compilation; the owner file and exact patch are included. This
enables the original menu/pause input with a single pointer. Desktop key
ACKs are not presented as successful controller delivery.

Separately pinned Arduboy2 mixed BSD notices and PXA MIT glue notices are
included with the frozen common and the assets license files. Session
records do not persist to disk. No firmware flash or external deployment.
