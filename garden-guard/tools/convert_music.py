#!/usr/bin/env python3
"""Convert Garden Guard MIDI sources into compact PXA scores."""

from __future__ import annotations

import hashlib
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

try:
    import mido
except ImportError as error:
    raise SystemExit("mido is required: python3 -m pip install mido") from error


SAMPLE_RATE = 16000
MAX_VOICES = 19


@dataclass(frozen=True)
class Track:
    index: int
    priority: int
    gain_at_velocity_64: int
    timbre: str
    channel: int | None = None
    minimum_gain: int = 1


@dataclass(frozen=True)
class Song:
    symbol: str
    filename: str
    tracks: tuple[Track, ...]


@dataclass
class Note:
    start: int
    end: int
    midi_note: int
    priority: int
    gain: int
    timbre: str
    source: int


PIANO = "PXA_MUSIC_TIMBRE_PIANO"
BASS = "PXA_MUSIC_TIMBRE_BASS"
SAX = "PXA_MUSIC_TIMBRE_SAX"
STRINGS = "PXA_MUSIC_TIMBRE_STRINGS"
PIZZICATO = "PXA_MUSIC_TIMBRE_PIZZICATO"
SINE = "PXA_MUSIC_TIMBRE_SINE"
LEAD = "PXA_MUSIC_TIMBRE_LEAD"
DRUM = "DRUM"

SONGS = (
    Song("garden_main_menu", "garden-main-menu.mid", (
        Track(2, 120, 64, PIANO, minimum_gain=35),
        Track(3, 125, 92, LEAD, minimum_gain=28),
        Track(4, 95, 95, PIZZICATO, minimum_gain=24),
        Track(5, 105, 55, SINE, minimum_gain=24),
        Track(6, 85, 60, STRINGS, minimum_gain=30),
        Track(7, 60, 42, DRUM), Track(8, 70, 48, DRUM),
        Track(9, 110, 52, BASS, minimum_gain=32),
        Track(10, 75, 42, DRUM), Track(11, 75, 42, DRUM),
        Track(12, 75, 42, DRUM), Track(13, 115, 64, DRUM),
        Track(14, 100, 55, SAX, minimum_gain=30),
        Track(15, 120, 72, DRUM),
    )),
    Song("garden_grasswalk", "grasswalk.mid", (
        Track(0, 80, 62, PIANO, 0), Track(0, 120, 106, PIANO, 1),
        Track(0, 105, 72, BASS, 2), Track(0, 110, 84, SAX, 3),
        Track(0, 70, 42, STRINGS, 4), Track(0, 90, 58, PIZZICATO, 5),
        Track(0, 85, 56, STRINGS, 6), Track(0, 60, 38, DRUM, 7),
        Track(0, 60, 38, DRUM, 8), Track(0, 115, 62, DRUM, 9),
        Track(0, 65, 42, DRUM, 10),
    )),
    Song("garden_choose_seeds", "choose-your-seeds.mid", (
        Track(1, 120, 75, PIANO), Track(2, 90, 46, STRINGS),
    )),
    Song("garden_loonboon", "loonboon.mid", (
        Track(1, 120, 83, PIANO), Track(2, 100, 62, BASS),
        Track(3, 75, 44, STRINGS), Track(4, 90, 56, PIZZICATO),
        Track(5, 95, 58, SINE), Track(6, 70, 42, SAX),
        Track(7, 105, 64, BASS), Track(8, 110, 58, DRUM),
        Track(9, 115, 66, DRUM),
    )),
    Song("garden_moongrains", "moongrains.mid", (
        Track(1, 100, 55, STRINGS), Track(2, 120, 75, SINE),
    )),
)


def drum_voice(midi_note: int) -> tuple[int, int, str]:
    if midi_note in (35, 36, 41, 43, 45):
        return 420, 82, "PXA_MUSIC_TIMBRE_KICK"
    if midi_note in (38, 40, 47, 48, 50):
        return 1800, 64, "PXA_MUSIC_TIMBRE_SNARE"
    return 3200, 44, "PXA_MUSIC_TIMBRE_CLICK"


def paired_notes(track, settings: Track, ticks_per_unit: float,
                 source_index: int = 0):
    active = defaultdict(list)
    absolute_tick = 0
    for message in track:
        absolute_tick += message.time
        if (settings.channel is not None and hasattr(message, "channel") and
                message.channel != settings.channel):
            continue
        key = (getattr(message, "channel", 0), getattr(message, "note", -1))
        if message.type == "note_on" and message.velocity > 0:
            active[key].append((absolute_tick, message.velocity))
        elif message.type == "note_off" or (
            message.type == "note_on" and message.velocity == 0
        ):
            if not active[key]:
                continue
            start_tick, velocity = active[key].pop(0)
            start = round(start_tick / ticks_per_unit)
            end = max(start + 1, round(absolute_tick / ticks_per_unit))
            gain = max(settings.minimum_gain, min(127, round(
                settings.gain_at_velocity_64 * velocity / 64.0
            )))
            timbre = settings.timbre
            midi_note = message.note
            if timbre == DRUM:
                midi_note, gain, timbre = drum_voice(message.note)
                gain = max(1, min(127, round(gain * velocity / 64.0)))
                end = start + 1
            yield Note(start, end, midi_note, settings.priority, gain, timbre,
                       source_index)


def limit_polyphony(notes: list[Note], required_sources: set[int]) -> list[Note]:
    by_start = defaultdict(list)
    for note in notes:
        by_start[note.start].append(note)

    selected = []
    active = []
    for start in sorted(by_start):
        active = [note for note in active if note.end > start]
        candidates = sorted(
            by_start[start],
            key=lambda note: (-note.priority, note.midi_note, note.source),
        )
        for note in candidates[: max(0, MAX_VOICES - len(active))]:
            selected.append(note)
            active.append(note)

    represented = {note.source for note in selected}
    if represented != required_sources:
        missing = ", ".join(str(index) for index in
                            sorted(required_sources - represented))
        raise ValueError(f"polyphony selection removed MIDI sources: {missing}")
    if len(selected) != len(notes):
        raise ValueError(
            f"voice limit removed {len(notes) - len(selected)} MIDI notes; "
            "increase MAX_VOICES"
        )
    return sorted(selected, key=lambda note: (note.start, -note.priority,
                                               note.midi_note))


def phase_step(midi_note: int) -> int:
    if midi_note > 127:
        return midi_note
    frequency = 440.0 * math.pow(2.0, (midi_note - 69) / 12.0)
    return round(65536.0 * frequency / SAMPLE_RATE)


def first_bpm(midi) -> int:
    for track in midi.tracks:
        for message in track:
            if message.type == "set_tempo":
                return round(mido.tempo2bpm(message.tempo))
    return 120


def midi_length_ticks(midi, ticks_per_unit: float) -> int:
    return max(round(sum(message.time for message in track) / ticks_per_unit)
               for track in midi.tracks)


def render_song(source: Path, song: Song) -> list[str]:
    midi = mido.MidiFile(source)
    ticks_per_unit = midi.ticks_per_beat / 4.0
    notes = []
    required_sources = set()
    for source_index, settings in enumerate(song.tracks):
        if settings.index >= len(midi.tracks):
            raise ValueError(f"{source.name}: missing track {settings.index}")
        track_notes = list(paired_notes(midi.tracks[settings.index], settings,
                                        ticks_per_unit, source_index))
        if not track_notes:
            raise ValueError(
                f"{source.name}: track {settings.index}, channel "
                f"{settings.channel} has no notes"
            )
        notes.extend(track_notes)
        required_sources.add(source_index)
    notes = limit_polyphony(notes, required_sources)
    score_ticks = max(max(note.end for note in notes),
                      midi_length_ticks(midi, ticks_per_unit))
    if any(note.end - note.start > 255 for note in notes):
        raise ValueError(f"{source.name}: a note exceeds the event duration field")
    digest = hashlib.sha256(source.read_bytes()).hexdigest()

    lines = [
        f"/* {source.name}: SHA-256 {digest}",
        f" * {len(song.tracks)} MIDI sources, {len(notes)} sixteenth-note events, "
        f"voice capacity {MAX_VOICES}. */",
        f"static const pxa_game_music_event_t {song.symbol}_score[] = {{",
    ]
    for note in notes:
        lines.append(
            f"    PXA_MUSIC_EVENT({note.start}, {phase_step(note.midi_note)}, "
            f"{note.end - note.start}, {note.gain}, {note.timbre}),"
        )
    lines.extend([
        "};",
        "",
        f"static const pxa_game_music_song_t {song.symbol}_song = {{",
        f"    NULL, 0, {first_bpm(midi) * 2}, 0, 0, 0,",
        "    {PXA_MUSIC_REST, PXA_MUSIC_REST, PXA_MUSIC_REST, PXA_MUSIC_REST,",
        "     PXA_MUSIC_REST, PXA_MUSIC_REST, PXA_MUSIC_REST, PXA_MUSIC_REST},",
        f"    {song.symbol}_score, PXA_MUSIC_LENGTH({song.symbol}_score),",
        f"    {score_ticks},",
        "};",
        "",
    ])
    return lines


def main() -> None:
    app_dir = Path(__file__).resolve().parent.parent
    music_dir = app_dir / "music"
    lines = [
        "/* Generated by tools/convert_music.py. Do not edit manually. */",
        f"#if PXA_GAME_SFX_SCORE_VOICES < {MAX_VOICES}",
        '#error "Garden Guard music requires more score voices"',
        "#endif",
        "",
    ]
    for song in SONGS:
        lines.extend(render_song(music_dir / song.filename, song))
    (app_dir / "garden_music.inc").write_text(
        "\n".join(lines), encoding="ascii"
    )


if __name__ == "__main__":
    main()
