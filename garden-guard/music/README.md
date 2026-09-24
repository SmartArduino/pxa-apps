# Garden Guard music sources

The runtime does not decode MIDI files. `../tools/convert_music.py` quantizes
the selected tracks into `../garden_music.inc`, which the PXA synthesizer plays
as compact score events at 16 kHz.

Current mapping:

- `garden-main-menu.mid`: home, level list, and almanac
- `choose-your-seeds.mid`: plant selection
- `moongrains.mid`: night levels
- `loonboon.mid`: late day levels (3-1 and 3-3)
- `grasswalk.mid`: other day levels

Regenerate after changing a MIDI source or track mapping:

```sh
python3 -m pip install mido
python3 apps/pxa/garden-guard/tools/convert_music.py
```

The converter uses a 19-voice runtime limit, enough for the main-menu MIDI's
peak polyphony, and fails if any configured MIDI note would be omitted.
