#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise both Freedoom phases in the real PXA product simulator."""
import argparse
from array import array
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import time
import traceback

APP = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('doom_sim', APP.parent / 'doom/tests/simulator.py')
simtools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(simtools)
require = simtools.require
simtools.FAULTS = re.compile(simtools.FAULTS.pattern + r'|Freedoom MUSIC malformed', re.IGNORECASE)


def music_case(args, root, report):
    """Check actual Host PCM with SFX muted; exercise the real settings UI."""
    state, sock = root / 'music-state', root / 'music.sock'
    media = simtools.discover(args, state, sock, 'music-discover', report)
    (media / 'freedoom2.wad').write_bytes((args.wads / 'freedoom2.wad').read_bytes())

    def tail_peak(name):
        time.sleep(1.2)  # Let the last menu sound and queued PCM drain.
        pcm = array('h', (args.output / f'{name}-audio.s16').read_bytes()[-32768:])
        require(len(pcm) > 4096, 'Host did not write enough PCM')
        return max(map(abs, pcm), default=0)

    with simtools.Simulator(args, state, sock, 'music', report) as sim:
        sim.log_match(r'Freedoom MUSIC song=dm2ttl format=MIDI playing=1')
        # Use looping level music for slider checks; the title cue ends at 9.7s.
        sim.key('ENTER'); sim.key('Z'); sim.key('UP'); sim.key('UP'); sim.key('Z')
        sim.log_match(r'Freedoom MUSIC song=runnin format=MIDI playing=1 loop=1')
        sim.stats(state=0, menu=0)  # Wait through the level transition/wipe.
        sim.key('ENTER'); sim.key('DOWN'); sim.key('Z')  # Options
        sim.shot('options')
        sim.key('UP'); sim.key('Z')  # Sound volume, wrapped from first option
        sim.shot('sound-options')
        for _ in range(16):
            sim.key('LEFT', hold=0.09, settle=0.03)  # SFX = 0
        sim.key('DOWN')  # Music slider
        for _ in range(16):
            sim.key('LEFT', hold=0.09, settle=0.03)
        sim.shot('muted-sliders')
        sim.log_match(r'Freedoom MUSIC .*volume=0')
        silent = tail_peak('music') if args.capture_audio else None
        if args.capture_audio:
            require(silent == 0, f'Music/SFX mute left audible samples: {silent}')
        for _ in range(5):
            sim.key('RIGHT', hold=0.09, settle=0.03)
        audible = tail_peak('music') if args.capture_audio else None
        if args.capture_audio:
            require(audible > 64, f'Music alone produced no audible PCM: {audible}')
        sim.shot('music-volume')
        checkpoint = len(sim.logs())
        sim.key('X')  # B maps to Escape, which closes all menus.
        sim.log_match(r'DOOM saved /data/freedoom2-default\.cfg', after=checkpoint)
        configs = list((state / 'private-files').glob('*-pxa-freedoom/data/freedoom2-default.cfg'))
        require(len(configs) == 1, 'Settings file missing')
        config = configs[0].read_text()
        require(re.search(r'music_volume\s+5\b', config) and re.search(r'sfx_volume\s+0\b', config),
                'Music and SFX sliders did not persist independently')
    with simtools.Simulator(args, state, sock, 'music-reload', report) as sim:
        sim.log_match(r'Freedoom MUSIC .*volume=5')
        if args.capture_audio:
            require(tail_peak('music-reload') > 64, 'Music did not resume after relaunch')
        sim.key('ENTER'); sim.key('Z'); sim.key('UP'); sim.key('UP'); sim.key('Z')
        sim.log_match(r'Freedoom MUSIC song=runnin format=MIDI playing=1')
        sim.stats(state=0, menu=0)
        sim.shot('level-music')
    report['assertions']['music'] = {'muted_peak': silent, 'music_only_peak': audible,
        'persisted_volume': 5, 'sfx_volume': 0, 'title_to_level': True, 'relaunch': True}


def episode4_case(args, root, report):
    state, sock = root / 'episode4-state', root / 'episode4.sock'
    media = simtools.discover(args, state, sock, 'episode4-discover', report)
    (media / 'freedoom1.wad').write_bytes((args.wads / 'freedoom1.wad').read_bytes())
    with simtools.Simulator(args, state, sock, 'episode4', report) as sim:
        sim.log_match(r'DOOM ready IWAD=freedoom1\.wad')
        sim.key('ENTER'); sim.key('Z')
        for _ in range(3):
            sim.key('DOWN')
        sim.key('Z'); sim.key('UP'); sim.key('UP'); sim.key('Z')
        sim.log_match(r'Freedoom MUSIC song=D_E4M1 format=MIDI playing=1 loop=1')
        sim.stats(state=0, menu=0)
        sim.shot('original-episode4-music')
    report['assertions']['episode4_original_music'] = True


def gameplay(args, root, report, phase):
    state, sock = root / 'shared-state', root / 'game.sock'
    media = simtools.discover(args, state, sock, f'phase{phase}-discover', report)
    for name in ('freedoom1.wad', 'freedoom2.wad'):
        (media / name).unlink(missing_ok=True)
    wad = args.wads / f'freedoom{phase}.wad'
    (media / wad.name).write_bytes(wad.read_bytes())
    expected_title, _ = simtools.wad_title(wad, 64 * 1024 * 1024, b'E1M1' if phase == 1 else b'MAP01')
    with simtools.Simulator(args, state, sock, f'phase{phase}', report) as sim:
        sim.log_match(r'DOOM ready IWAD=' + re.escape(wad.name))
        time.sleep(0.3)
        title = sim.shot('title')
        report['assertions'][f'phase{phase}-title'] = simtools.title_assertion(title, expected_title)
        sim.key('ENTER')
        sim.key('Z')  # New Game
        if phase == 1:
            sim.key('Z')  # Episode 1
        sim.key('UP'); sim.key('UP')
        sim.key('Z', settle=0.8)
        first = sim.stats(state=0, menu=0)
        require(first['map'] == 1 and first['hp'] > 0, 'First map did not start')
        start = sim.shot('level')
        sim.key('UP', hold=0.5)
        moved = sim.stats(after=first['log_end'], state=0, menu=0)
        require((first['x'], first['y']) != (moved['x'], moved['y']), 'Player did not move')
        sim.shot('forward')
        sim.key('S', hold=0.3)  # Controller Y turns left
        turned = sim.shot('turn')
        simtools.changed(report, f'phase{phase}-turn', start, turned, (40, 40, 270, 180), 0.08)
        idle = sim.shot('before-fire')
        frames = []
        with sim.held('W'):  # Controller R fires
            for i in range(4):
                time.sleep(0.18)
                frames.append(sim.shot(f'fire-{i}'))
        require(max(simtools.difference(idle, f, (110, 100, 220, 200))['changed_fraction'] for f in frames) > 0.02,
                'Weapon did not animate')
        sim.key('Q')  # Controller L uses a door/switch
        sim.key('ENTER')
        checkpoint = len(sim.logs())
        paused = sim.stats(after=checkpoint, state=0, menu=1)
        later = sim.stats(after=paused['log_end'], state=0, menu=1)
        require((paused['x'], paused['y']) == (later['x'], later['y']), 'Menu did not pause world')
        sim.button('MENU')  # First touch only reveals the overlay
        sim.stats(after=later['log_end'], state=0, menu=1)
        sim.shot('touch-menu')
        # Main menu remembers New Game. Down three times selects Save Game.
        for _ in range(3):
            sim.button('DOWN')
        sim.button('OK'); sim.button('OK')
        checkpoint = len(sim.logs())
        sim.button('OK')
        sim.log_match(r'DOOM saved /data/freedoom' + str(phase) + r'-doomsav0\.dsg', after=checkpoint)
        saved_state = sim.stats(after=checkpoint, state=0, menu=0)
        sim.shot('saved')
        private = list((state / 'private-files').glob('*-pxa-freedoom/data'))
        require(len(private) == 1, 'Unexpected private storage binding')
        saved = private[0] / f'freedoom{phase}-doomsav0.dsg'
        data = saved.read_bytes()
        require(len(data) > 1024 and data[:24].split(b'\0', 1)[0] == b'PXA SAVE 1', 'Invalid save header')
        digest = hashlib.sha256(data).hexdigest()
    if args.capture_audio:
        pcm = array('h', (args.output / f'phase{phase}-audio.s16').read_bytes())
        nonzero = sum(v != 0 for v in pcm)
        require(nonzero > 512 and max(map(abs, pcm), default=0) > 64, 'No audible Host output')
        report['assertions'][f'phase{phase}-audio'] = {'nonzero_samples': nonzero}
    with simtools.Simulator(args, state, sock, f'phase{phase}-reload', report) as sim:
        sim.log_match(r'DOOM ready IWAD=' + re.escape(wad.name))
        sim.key('ENTER'); sim.key('DOWN'); sim.key('DOWN'); sim.key('Z')
        checkpoint = len(sim.logs())
        sim.key('Z', settle=0.8)
        loaded = sim.stats(after=checkpoint, state=0, menu=0)
        require(loaded['map'] == saved_state['map'] and loaded['hp'] > 0, 'Save did not reload')
        require(abs(loaded['x'] - saved_state['x']) <= 2 and abs(loaded['y'] - saved_state['y']) <= 2,
                'Saved position did not reload')
        require(hashlib.sha256(saved.read_bytes()).hexdigest() == digest, 'Loading modified save bytes')
        sim.shot('loaded')
    return saved, digest


def negative(args, root, report, malformed):
    name = 'malformed' if malformed else 'missing'
    state, sock = root / name, root / (name + '.sock')
    media = simtools.discover(args, state, sock, name + '-discover', report)
    if malformed:
        (media / 'freedoom2.wad').write_bytes(b'IWAD\x01\0\0\0\xff\xff\xff\x7f')
    with simtools.Simulator(args, state, sock, name, report) as sim:
        sim.log_match('Invalid IWAD directory' if malformed else 'Freedoom missing IWAD')
        time.sleep(0.4)
        screen = sim.shot('error')
        require('DOOM ready' not in sim.logs(), 'Invalid data reached game loop')
        require(sum(v > 200 for v in screen.crop((8, 35, 312, 100)).convert('L').tobytes()) > 80,
                'Error page has no explanatory text')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'wads', 'output'):
        parser.add_argument('--' + name, type=lambda s: Path(s).resolve(), required=True)
    parser.add_argument('--capture-audio', action='store_true')
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--music-only', action='store_true', help='Run only music/settings/Host PCM checks')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    require(not any(args.output.iterdir()), '--output must be empty')
    report = {'passed': False, 'processes': [], 'screenshots': [], 'assertions': {}, 'cases': {}}
    with tempfile.TemporaryDirectory(prefix='pxa-freedoom-') as temporary:
        root = Path(temporary)
        try:
            if not args.music_only:
                saves = []
                for phase in (1, 2):
                    saves.append(gameplay(args, root, report, phase))
                    report['cases'][f'phase{phase}'] = {'passed': True}
                    print(f'PASS Phase {phase}', flush=True)
                for path, digest in saves:
                    require(hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Phases overwrote each other\'s saves')
                report['assertions']['isolated_phase_saves'] = True
                for malformed in (False, True):
                    negative(args, root, report, malformed)
            music_case(args, root, report)
            episode4_case(args, root, report)
            report['passed'] = True
        except Exception as error:
            report['error'] = str(error)
            report['traceback'] = traceback.format_exc()
        (args.output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(('PASS' if report['passed'] else 'FAIL') + ': ' + str(args.output / 'results.json'))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
