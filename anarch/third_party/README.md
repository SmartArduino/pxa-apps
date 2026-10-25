# Anarch upstream source

The [anarch/](anarch/) directory contains unchanged source files from the official
[drummyfish/Anarch repository](https://gitlab.com/drummyfish/Anarch), pinned to
commit [`6f90562161200682459e772f1dacb747f23c5f95`](https://gitlab.com/drummyfish/Anarch/-/commit/6f90562161200682459e772f1dacb747f23c5f95)
(`Update make.sh`, committed 2023-09-08). Retrieved 2026-10-23 through the GitLab
HTTPS repository-files API using that full commit ID, rather than a moving branch.

This is the complete original game core and compiled-in game content: all ten
levels, textures, sprites, backgrounds, fonts, story and menu text, six sound
effects, and six procedural music tracks. No gameplay patches or content
reductions are applied. Desktop and device-specific frontends, optional mods,
prebuilt executables, media, and asset-conversion tools are omitted; none are
required to compile or run this core.

## License and authorship

Miloslav Ciz (drummyfish) created the code and original game assets. The upstream
[LICENSE](anarch/LICENSE) contains the complete CC0 1.0 Universal dedication. The
unchanged upstream [README](anarch/README.md#usage-rights) additionally preserves
the author's explicit waiver of all other intellectual-property rights, including
patents, and the original source notices are intact.

## Exact-file inventory

Git blob SHA-1 identifies the exact bytes, including original line endings. These
IDs are from the root repository tree at the pinned commit.

| File | Upstream Git blob SHA-1 |
| --- | --- |
| [constants.h](anarch/constants.h) | `956baa03b14dfb4fff52e40269c564d2299943c8` |
| [game.h](anarch/game.h) | `991058d7acb166e43039dfeae40244be7c68b5fe` |
| [images.h](anarch/images.h) | `8f88bcc9bb1d36f6e1d916ae6a184ce3e8c5d3cf` |
| [levels.h](anarch/levels.h) | `09164f896294a0234d65eac1b084ac67987ab1c0` |
| [palette.h](anarch/palette.h) | `ec66ee272a33f544952e013c0bdb605cf6fdf2c8` |
| [raycastlib.h](anarch/raycastlib.h) | `38a70ff716e9475293eee0f04531ed33c872a8a0` |
| [settings.h](anarch/settings.h) | `a0d8c1f53e22bc490f4f3eb24855f0aca9ee51f4` |
| [sounds.h](anarch/sounds.h) | `2e2b042c8a4600aef7a37dd4940db45fdc33598b` |
| [texts.h](anarch/texts.h) | `b7e53d4b999389b829fd151560917a2c6ca0d71a` |
| [LICENSE](anarch/LICENSE) | `0e259d42c996742e9e3cba14c677129b2c1b6311` |
| [README.md](anarch/README.md) | `5c9728e132ed5c478e1d0262ee87236ca6e71b84` |

To recompute a blob ID for one file, use `git hash-object <file>`. A source update
should deliberately change the pin and this inventory together.

## Frontend integration

Include [game.h](anarch/game.h) in one translation unit only; it contains function
and global definitions. It is C99 and also compiles as C++. Include
[sounds.h](anarch/sounds.h) after it if audio uses the original samples and
soundtrack. The core needs only standard fixed-width integer types, with no heap
allocation, filesystem, floating point, or desktop libraries.

Define platform settings before including the core, overriding defaults from
[settings.h](anarch/settings.h). Set `SFG_SCREEN_RESOLUTION_X`,
`SFG_SCREEN_RESOLUTION_Y`, `SFG_RESOLUTION_SCALEDOWN`, `SFG_FPS`, and `SFG_CAN_EXIT`
for the frontend. Upstream recommends `-O3`; use the PXA toolchain's C99 or C++
mode. Keep gameplay settings and cheats at their upstream defaults to preserve
the original game. Performance knobs include `SFG_RAYCASTING_SUBSAMPLE` (a divisor
of render width), screen scaledown, and ray traversal limits. Changing FPS changes
the integer simulation step, so choose a sustainable target in the upstream
recommended range of 20–100 FPS.

The frontend implements these declarations:

```c
int8_t SFG_keyPressed(uint8_t key);
void SFG_getMouseOffset(int16_t *x, int16_t *y);
uint32_t SFG_getTimeMs(void);
void SFG_sleepMs(uint16_t timeMs);
static inline void SFG_setPixel(uint16_t x, uint16_t y, uint8_t colorIndex);
void SFG_playSound(uint8_t soundIndex, uint8_t volume);
void SFG_setMusic(uint8_t value);
void SFG_processEvent(uint8_t event, uint8_t data);
void SFG_save(uint8_t data[SFG_SAVE_SIZE]);
uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE]);
```

Call `SFG_init()` once, then `SFG_mainLoopBody()` after polling platform events.
The latter returns zero when the player selects Exit. It performs all elapsed
fixed simulation steps and renders once; early calls may only request a sleep.
Track whether any pixel was written to decide whether to present a new surface.
Use app-relative monotonic milliseconds, starting near zero: initialization sets
`SFG_game.frameTime` to zero. Freeze app time during suspension so returning to the
game does not run a large backlog of simulation steps. Yield/event dispatch belongs
in the platform loop or sleep hook.

Mandatory keys are `UP`, `RIGHT`, `DOWN`, `LEFT`, `A` (fire/confirm), `B`
(cancel/strafe/look), and `C` (menu/jump/weapon combinations). Optional keys provide
dedicated jump, strafe, map, free look, weapon selection, and menu controls. Return
zero for unused keys; a frontend without a mouse can return zero offsets. Event
notifications can be ignored or used for vibration.

The pixel hook receives final screen coordinates even when scaledown is greater
than one: the core expands logical pixels itself. `paletteRGB565[colorIndex]`
provides the original palette directly. A fast framebuffer store avoids an import
or service call per pixel; present a complete frame through PXA after rendering.

`SFG_SAVE_SIZE` is 12 bytes. Save capability preserves unlocked levels, settings,
and a level-start checkpoint, rather than an arbitrary in-level snapshot. Return
one from `SFG_load` when saving is supported, including on first launch with no
record, leaving its default-initialized argument unchanged when no valid record
exists. Return zero only when saving is unavailable, which makes the core unlock
all levels. For asynchronous PXA Storage, preload and validate a cached record
before `SFG_init`, copy it in the synchronous load hook, and queue save operations
from a private copy of the 12-byte state.

## Original audio

[sounds.h](anarch/sounds.h) contains six 2048-sample effects, packed to four bits
per sample at 8000 Hz mono. `SFG_GET_SFX_SAMPLE(soundIndex, sampleIndex)` expands a
sample to unsigned eight-bit amplitude. `SFG_getNextMusicSample()` produces the
six original bytebeat tracks at 8000 Hz. Compile the translation unit containing
this header with `-fwrapv`: one original formula shifts a signed integer into its
sign bit and expects two's-complement wrapping. This flag preserves the intended
waveform and allows GCC/Clang undefined-behavior sanitizer checks without changing
the pinned source. Convert unsigned amplitudes to signed PCM using multiplication,
for example `(sample - 128) * 256`, and clamp mixed voices to the signed 16-bit range.

Music commands are `SFG_MUSIC_TURN_OFF`, `SFG_MUSIC_TURN_ON`, and `SFG_MUSIC_NEXT`.
The upstream `SFG_nextMusicTrack()` advances through the remainder of a complete
524288-sample track, which can stall an interpreted guest. A frontend can obtain
the identical next-track state by incrementing `SFG_MusicState.track` modulo
`SFG_TRACK_COUNT` and resetting its `t`, `t2`, and `n11t` fields to zero. This does
not require an upstream source patch.

PXA PCM accepts 8000 Hz mono signed 16-bit little-endian output with a maximum
20 ms write, or 160 samples at that rate. Respect accepted write counts and
backpressure so sample generation and voice positions advance only with buffered
or accepted output. Music tracks have significant DC components; remove DC in the
frontend before final gain and clipping. Audio processing and surface presentation
should both remain bounded between platform event polls.
