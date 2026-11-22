# PXA Unciv asset attribution

This independent PXA implementation uses selected artwork and sounds distributed
by [Unciv](https://github.com/yairm210/Unciv), pinned at commit
`0f2eb314a17213da0c0ce3b9d9c4693e2ed6ef96`. The authoritative attribution list is
[Unciv Credits.md at that revision](https://github.com/yairm210/Unciv/blob/0f2eb314a17213da0c0ce3b9d9c4693e2ed6ef96/docs/Credits.md).
Exact upstream paths, authors, licenses and original SHA-256 values are recorded
in `sources.json`. No Kotlin/Java source, engine, upstream ruleset files
is included. Original application code is MIT; shared PXA pocket runtime is CC0.

## Artwork: CC BY 4.0

- FantasyHex terrain and improvements: CuddlyClover and Unciv contributors,
  [Fantasy Hex Tiles](https://cuddlyclover.itch.io/fantasy-hex-tiles).
  Files: Grassland, Plains, Desert, Hill, Mountain, Forest, Coast, Ocean,
  City center and Mine, from Unciv `android/Images.Tilesets/TileSets/FantasyHex/Tiles/`.
  The movement circle and attack crosshair are `Highlight.png` and `Crosshair.png`
  from the parent FantasyHex directory, reduced and tinted at runtime.
- AbsoluteUnits: [letstalkaboutdune](https://github.com/letstalkaboutdune/AbsoluteUnits).
  Files: Settler, Warrior, Scout, Worker, Archer, Swordsman, from Unciv
  `android/Images.AbsoluteUnits/TileSets/AbsoluteUnits/Units/`.

License: https://creativecommons.org/licenses/by/4.0/ (full text included).
Changes: unit transparent margins trimmed; sprites reduced to fit a 40-pixel
box, quantized to RGB565, and scaled at runtime. These changes do not imply
endorsement by Unciv or the asset authors. Artwork retains its CC BY 4.0 license.
Forest, Hill and Mountain sprites combine their original transparent feature
with the attributed Grassland base, bottom-aligned without stretching.

## Application logo: CC BY 4.0, including CC BY 3.0 components

`assets/icon.png` is the unmodified upstream `Unciv128.png`, by yairm210 and
u-ndefine (CC BY 4.0). The same logo is reduced to 64 pixels, quantized to
RGB565 and scaled for the main menu. Its included CC BY 3.0 components are:

- [Gear](https://thenounproject.com/term/gear/29368), Alex Bickov.
- [Apple](https://thenounproject.com/term/apple/1139396), Pedro Gonçalves.
- [Gold](https://thenounproject.com/term/gold/25438), Eliricon.
- [Beaker](https://thenounproject.com/term/beaker/621510), Delwar Hossain.
- [Music](https://thenounproject.com/term/music/5611/), Naomi Atkinson.
- [Smiley](https://thenounproject.com/term/smiley/1024026), Alexander Skowalsky.
- [City](https://thenounproject.com/icon/city-1765370/), Muhajir ila Robbi.

## Interface icons: CC BY 3.0 / Public Domain

The following icons are from the Noun Project, via Unciv. Upstream credits
identify the first five components as CC BY 3.0; for the remaining icons they
state CC BY 3.0 or Public Domain without selecting per file. We retain those
credits and provide attribution and the CC BY 3.0 terms for all of them.

| Upstream file | Creator | Original work |
| --- | --- | --- |
| StatIcons/Gold.png | Eliricon | [Gold](https://thenounproject.com/term/gold/25438) |
| StatIcons/Science.png | Delwar Hossain | [Beaker](https://thenounproject.com/term/beaker/621510) |
| StatIcons/Culture.png | Naomi Atkinson | [Music](https://thenounproject.com/term/music/5611/) |
| StatIcons/Food.png | Pedro Gonçalves | [Apple](https://thenounproject.com/term/apple/1139396) |
| StatIcons/Production.png | Alex Bickov | [Gear](https://thenounproject.com/term/gear/29368) |
| StatIcons/Population.png | Stéphanie Rusch | [User](https://thenounproject.com/term/user/38217) |
| OtherIcons/Options.png | Thengakola | [Options](https://thenounproject.com/icon/options-866090) |
| OtherIcons/New.png | Alice Design | [New](https://thenounproject.com/icon/new-1886943) |
| OtherIcons/Resume.png | Salvia Santos | [Go back](https://thenounproject.com/icon/go%20back-1901947) |
| BuildingIcons/Monument.png | Eliricon | [Obelisk](https://thenounproject.com/term/obelisk/749104/) |
| BuildingIcons/Granary.png | Pedro Santos | [Storehouse](https://thenounproject.com/term/storehouse/966786/) |
| BuildingIcons/Barracks.png | Creaticca Creative Agency | [Cross swords](https://thenounproject.com/term/cross-swords/965398/) |

License: https://creativecommons.org/licenses/by/3.0/ (full text included).
Changes: transparent margins trimmed, reduced to a 40-pixel box, RGB565
conversion, and runtime scaling; monochrome icons are tinted to match the UI.
The science icon also represents the library in this independent ruleset.

## Sound effects: CC0 1.0

Selected from Unciv `android/assets/sounds/`, with upstream credits:

| File | Original creator | Original work |
| --- | --- | --- |
| click.mp3 | EathanMarkson | https://freesound.org/people/EathanMarkson/sounds/388958/ |
| whoosh.mp3 | dave.des (credited dave.dev in upstream description) | https://freesound.org/people/dave.des/sounds/127196/ |
| chimes.mp3 | Bastianhallo | https://freesound.org/people/Bastianhallo/sounds/682633/ |
| paper.mp3 | stijn | https://freesound.org/people/stijn/sounds/43673/ |
| metalhit.mp3 | SlavicMagic | https://freesound.org/people/SlavicMagic/sounds/446016/ |
| arrow.mp3 | Twisted_Euphoria | https://freesound.org/people/Twisted_Euphoria/sounds/205938/ |

License: https://creativecommons.org/publicdomain/zero/1.0/ (full text included).
Changes: converted to 16 kHz mono PCM, trimmed to at most 1 second with a short tail fade, and normalized to a
quiet peak. Playback is optional; denied permission does not prevent gameplay.

## Background music: CC BY 4.0

"Thatched Villagers" by Kevin MacLeod (https://incompetech.com).
This is Unciv's default downloadable ambient track, credited in its pinned
[Credits.md](https://github.com/yairm210/Unciv/blob/0f2eb314a17213da0c0ce3b9d9c4693e2ed6ef96/docs/Credits.md#music).
Source: https://incompetech.com/music/royalty-free/mp3-royaltyfree/Thatched%20Villagers.mp3
License: https://creativecommons.org/licenses/by/4.0/ (full text included).
Changes: the complete track was converted to mono 16 kHz Vorbis at 24 kbps
for device storage and streaming; playback gain is reduced. Original and
converted SHA-256 values are in `sources.json`. No endorsement is implied.
