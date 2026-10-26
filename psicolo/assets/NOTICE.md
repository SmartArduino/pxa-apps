Psi Colo PXA port, 2026-10-10

Original game/code/images/sound scores: OBONO, MIT Copyright(c)2016-2026.
ArduboyWorks commit d4b1f041789dcd1d71907654e4025d613b4ab420, psicolo.
Original Arduboy2 provider: BSD-3-Clause, retained frozen common license.
Original Playtune1.0.4 API/scores: MIT, commit493d3c960a87d1b4716eb85fb297706230c8e937; owner-local silent software note/priority/millisecond interpreter, no physical speaker or AVR ISR/pitch proof.
Independent AVR15bit Park-Miller rand and reference source: BSD-3-Clause Regents of the University of California; original rand.c and full notice retained.
PXA glue/frontend: MIT. Frozen existing provider/SDK/Host unchanged.

Original40FPS(25ms),128x64bitmap,80frameLogo and modes retained. Original startup hardware utilities and physicalRGBLED output omitted; logicalLED state remains in completefield serialization.
Original16-bit AVR function address macros replaced by same typed original module function members; sound-pointer table likewise typed.
Original22byteAVRrecord bitfields explicitly encoded little-endian without host24bytepadding; original11LE16wordchecksum and896EEPROMbase retained, session1024FFRAM only.
Original unused-return drawing helper bool corrected to void after actual O2 SIGSEGV and UBSan missing-return failure; no caller used its return.
Original illegalLogo shifts guarded to0..31; no equivalent result claimed for undefined original inputs. Original260 sound literal explicitly truncated to originaluint8 value4.
MyArduboy/Playtune original construction uses aligned placement storage; original already-packedchannel count0 triggers originalconstructor once when SDKskips dynamicctors; no extra mutableguard, perNEW reconstruction, forcedseed, hiddenstate, or gameplay changes.
Four visibly labeled B+direction buttons generate simultaneous originalmasks needed to roll dice; original single controls/raster unchanged. OwnedSTATE/STATE2 diagnostic split keeps all19 originalexpressions/order and strictseq/gen/clock bindings under unchanged256bytelog API, fullpatch retained.

Native parity is adapted nativeC++ reference, not executedAVR/hardwarefirmware. Desktop and headless results are recorded only after actualtests. No flashing, publicdeployment, or persistentcrossprocesssave.
