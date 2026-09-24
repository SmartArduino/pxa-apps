# Gameplay Image Sources

The gameplay PNG files in this directory use the following sources:

- `icon.png` is rendered from `apps/pxa/common/art/garden-icon.svg` at its
  final `96x96` application-icon size.
- `pea-shooter.png`, `sunflower.png`, `wall-nut.png`, `cherry-bomb.png`,
  `repeater.png`, `ice-shooter.png`, `zombie.png`, `cone-zombie.png`,
  `bucket-zombie.png`, and `mower.png` are original artwork rendered from
  `apps/pxa/common/art/garden-actors.svg`. Each file is pre-cropped with a
  transparent safety margin at its final in-game size.
- `puff-shroom.png`, `sun-shroom.png`, `spikeweed.png`, `fume-shroom.png`,
  `doom-shroom.png`, `magnet-shroom.png`, and `gloom-shroom.png` are rendered
  at `32x32` from
  `apps/pxa/common/art/garden-extra-actors.svg`.
- `pole-zombie.png` and `gargantuar.png` are rendered at `40x48` and `48x48`
  from `apps/pxa/common/art/garden-extra-zombies.svg`.
- `actors.png` places the original ten actors and shovel in its first row,
  seven extra plants in its second row, and the two tall zombies in its third
  row. The game draws the `352x112` atlas at 1:1 scale and clips whole sprites
  without resizing sprite pixels.
- `shovel.png` is rendered from `apps/pxa/common/art/garden-shovel.svg` at its
  final `24x24` button-icon size.
- `sun.png` is rendered from `apps/pxa/common/art/garden-sun.svg` at its final
  `22x22` size. Falling suns and the resource counter reuse this same PNG at
  1:1 scale.

The lawn is rendered with Canvas colors and grid lines, so `actors.png` and
`sun.png` are the only gameplay PNG files decoded by this app. Keeping the
shovel in the actor atlas avoids exceeding the runtime's two decoded-image
slots during a canvas frame.

Run `apps/pxa/garden-guard/render_assets.sh` after editing either extra actor
SVG to regenerate the individual PNG files and the runtime atlas.
