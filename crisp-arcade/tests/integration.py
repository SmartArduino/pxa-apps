#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the real signed Crisp Arcade package with the existing PXA helpers.

Simulator (pass a .pxa with its expanded sibling, or the expanded directory):
  python3 local/pxa-apps/crisp-arcade/tests/integration.py simulator \
    --simulator PATH --package PATH --publisher-key PATH --output ARTIFACT_DIR
Device (package must already be installed; never flashes or installs):
  python3 local/pxa-apps/crisp-arcade/tests/integration.py device \
    --port PORT --output ARTIFACT_DIR [--skip-idle-lock]

Only output paths below ignored local/artifacts are accepted. Simulator state,
logs, screenshots, checksummed saves and results stay there. Serial is opened
only by the explicit device subcommand. Device controller coverage depends on
INPUT CAPABILITIES; unavailable injection is reported and touch coverage still
runs. Simulator controller coverage is always enabled. Pillow and pyserial are
required.
"""
import argparse
import json
import re
import struct
import sys
import tempfile
import time
import traceback
import uuid
import zlib
from pathlib import Path

sys.dont_write_bytecode = True
APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[2]
HELPERS = APP.parent / 'common/pocket-game/tests'
sys.path.insert(0, str(HELPERS))
sys.path.insert(0, str(ROOT / 'tools/pxadb'))
import simulator as common_sim
from PIL import Image, ImageChops, ImageDraw

IDENTITY = 'pxa-crisp-arcade'
NAMES = ('CAST N', 'GRAPPLING H', 'REFLECTOR', 'COLOR ROLL')
BOXES = ((12, 32, 276, 208), (84, 30, 204, 210), (54, 30, 234, 210), (54, 30, 234, 210))
FAULTS = re.compile(r'Guest exception|callback failed|out of bounds memory access|unreachable instruction|stack overflow|wasm trap|input queue overflow|event queue overflow|exit flush timed out|Guru Meditation', re.I)
require = common_sim.require


def decode(payload):
    require(len(payload) == 48 and payload[:5] == b'CRSP\x01', 'Bad Crisp save identity/version/size')
    require(payload[5] < 4 and payload[6:8] == bytes(2) and payload[40:44] == bytes(4), 'Bad save selection/reserved fields')
    checksum = 2166136261
    for value in payload[:44]:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    require(struct.unpack_from('<I', payload, 44)[0] == checksum, 'Bad Crisp save checksum')
    values = struct.unpack_from('<8I', payload, 8)
    require(max(values) <= 99999999, 'Save score/play range exceeded')
    return {'selected': payload[5], 'best': list(values[:4]), 'plays': list(values[4:]), 'hex': payload.hex()}


def host_save(state):
    candidates = []
    for path in state.rglob('.pxa-kv-*'):
        if not path.is_file():
            continue
        raw = path.read_bytes()
        if len(raw) < 24:
            continue
        magic, version, _, generation, size, crc = struct.unpack('<4sHHQII', raw[:24])
        body = raw[24:]
        if magic != b'PXKV' or version != 1 or len(body) != size or zlib.crc32(body) != crc:
            continue
        try:
            count = struct.unpack_from('<H', body)[0]
            at, values = 2, {}
            for _ in range(count):
                length = body[at]; at += 1
                key = body[at:at+length].decode(); at += length
                length = struct.unpack_from('<H', body, at)[0]; at += 2
                values[key] = body[at:at+length]; at += length
            if at == len(body) and IDENTITY+'.save.v1' in values:
                candidates.append((generation, values[IDENTITY+'.save.v1']))
        except (IndexError, struct.error, UnicodeDecodeError):
            continue  # A concurrent host generation write may be incomplete.
    return max(candidates, key=lambda item: item[0])[1] if candidates else None


def canvas(image, ox, oy):
    return image.crop((ox, oy, ox+288, oy+224))


def visible_game(image, ox, oy, game):
    image = canvas(image, ox, oy)
    # The runtime overlay covers x48..239/y32..187; compare only game pixels.
    ImageDraw.Draw(image).rectangle((48, 32, 239, 187), fill=(0, 0, 0))
    return image.crop(BOXES[game])


def equal(a, b, exact):
    return ImageChops.difference(a.convert('RGB'), b.convert('RGB')).getbbox() is None if exact else common_sim.difference(a, b) < .002


def hook(image, ox, oy):
    view = canvas(image, ox, oy).crop(BOXES[0])
    # Also recognize the desaturated/darker 2-pixel hook in device JPEGs;
    # the pale shore has substantially less saturation at high luminance.
    points = [(x, y) for y in range(view.height) for x in range(view.width)
              for r, g, b in [view.getpixel((x, y))]
              if r > 75 and g > 65 and b < 125 and r-g > -15 and r-b > 32
              and g-b > 25 and (r < 210 or r-b > 150)]
    require(len(points) >= 4, 'Fishing hook is not visible')
    return (sum(x for x, _ in points)/len(points)*150/view.width,
            sum(y for _, y in points)/len(points)*100/view.height)


class SimAPI:
    exact, ox, oy, controller = True, 16, 8, True
    def __init__(self, sim, args, report):
        self.sim, self.args, self.report, self.active, self.selected = sim, args, report, None, 0
    def pump(self, seconds): time.sleep(seconds)
    def key(self, name, down):
        key = {'A': 'Z', 'B': 'X', 'START': 'ENTER'}.get(name, name)
        common_sim.command(self.sim.sock, ('KEY_DOWN ' if down else 'KEY_UP ')+key)
        if down: self.sim.keys.add(key)
        else: self.sim.keys.discard(key)
    def pointer(self, phase, x, y): self.sim.pointer(phase, x, y)
    def cancel(self):
        for key in tuple(self.sim.keys): common_sim.command(self.sim.sock, 'KEY_UP '+key)
        common_sim.command(self.sim.sock, 'TOUCH CANCEL_ALL'); self.sim.keys.clear()
    def shot(self, label): return self.sim.shot(label)
    def logs(self): return self.sim.logs()
    def saved(self, label):
        self.pump(.35)
        payload = self.sim.wait(lambda: host_save(self.sim.state), 'durable Crisp save')
        (self.args.output/(label+'.save')).write_bytes(payload)
        self.report.setdefault('saves', {})[label] = decode(payload)
        return payload
    def exit(self):
        tap(self, 276, 10)
        self.pointer('DOWN', 140, 145)
        self.sim.wait(lambda: self.sim.proc.poll() is not None, 'menu EXIT')
        require(self.sim.proc.returncode == 0, 'Menu EXIT did not stop normally')


class DeviceAPI:
    exact, ox, oy = False, 4, 8
    MASKS = {'UP': 1, 'RIGHT': 2, 'DOWN': 4, 'LEFT': 8, 'A': 16, 'B': 32, 'START': 1024}
    def __init__(self, device, args, report):
        self.device, self.args, self.report, self.active, self.selected = device, args, report, None, 0
        self.keys = set()
        self.controller = report['controller_input']['available']
    def pump(self, seconds): self.device.pump(seconds)
    def key(self, name, down):
        require(self.controller, 'Controller injection is unavailable on this device')
        if down: self.keys.add(name)
        else: self.keys.discard(name)
        self.device.request('INPUT CONTROLLER 0 1 '+str(sum(self.MASKS[key] for key in self.keys)))
        self.device.request('INPUT SYNC')
    def pointer(self, phase, x, y): self.device.pointer(phase, x, y)
    def cancel(self): self.device.request('INPUT CANCEL'); self.keys.clear(); self.pump(.08)
    def shot(self, label): return self.device.shot(label)
    def logs(self): return '\n'.join(self.device.logs)
    def saved(self, label): return self.device.saved(label)
    def exit(self): self.device.menu_exit()


def press(api, name, duration=.08, settle=.10):
    api.key(name, True); api.pump(duration); api.key(name, False); api.pump(settle)


def tap(api, x, y, duration=.07, settle=.15):
    api.pointer('DOWN', x, y); api.pump(duration); api.pointer('UP', x, y); api.pump(settle)


def home(api):
    if api.active is not None:
        # The S3 Host consumes touches at the left screen edge.
        tap(api, 20 if api.exact else 40, 12); api.active = None


def launch(api, game, method, held=.08):
    home(api)
    before = api.logs().count('crisp launch game='+NAMES[game])
    if method == 'touch':
        tap(api, 60+(game % 2)*140, 65+(game//2)*75)
    else:
        for _ in range((game-api.selected) % 4): press(api, 'RIGHT')
        press(api, 'A', held)
    api.selected = api.active = game
    require(api.logs().count('crisp launch game='+NAMES[game]) > before, 'Missing real '+method+' launch: '+NAMES[game])


def check(api, name, **details):
    api.report['checks'].append({'name': name, **details})


def fishing(api):
    if api.controller:
        launch(api, 0, 'controller', held=.85)
        point = hook(api.shot('a-selection-held'), api.ox, api.oy)
        require(point[0] < 12 and abs(point[1]-20) < 6, 'Launcher A leaked into fishing action')
        check(api, 'launcher_A_requires_release', hook=point)
        api.key('A', True); api.pump(.22)
        charged = hook(api.shot('fishing-controller-charge'), api.ox, api.oy)
        require(charged[0] < 12, 'Fishing cast before sustained A released')
        api.key('A', False); api.pump(.36)
        cast = hook(api.shot('fishing-controller-cast'), api.ox, api.oy)
        require(cast[0] > charged[0]+8, 'Fishing controller release did not cast')
        press(api, 'A', settle=.65)
        pulled = hook(api.shot('fishing-controller-pull'), api.ox, api.oy)
        require(pulled[0] < cast[0], 'Fishing tap did not pull')
        check(api, 'fishing_controller_hold_release_tap', charged=charged, cast=cast, pulled=pulled)
    launch(api, 0, 'touch')
    api.pointer('DOWN', 140, 120); api.pump(.22)
    charged = api.shot('fishing-touch-charge')
    if not api.controller:
        require(hook(charged, api.ox, api.oy)[0] < 12, 'Fishing touch cast before sustained hold released')
    api.pointer('MOVE', -4, 200); api.pointer('UP', -4, 200); api.pump(.35)
    outside = hook(api.shot('fishing-outside-up'), api.ox, api.oy)
    require(outside[0] > 14, 'Outside touch UP did not release the fishing cast')
    tap(api, 140, 120, settle=.65)
    require(hook(api.shot('fishing-touch-pull'), api.ox, api.oy)[0] < outside[0], 'Touch tap did not reel in')
    launch(api, 0, 'touch')
    api.pointer('DOWN', 140, 120); api.pump(.22); api.cancel(); api.pump(.35)
    cancelled = hook(api.shot('fishing-cancel'), api.ox, api.oy)
    require(cancelled[0] > 14, 'Pointer cancellation did not release fishing hold')
    check(api, 'fishing_touch_hold_outside_up_tap_cancel', outside_hook=outside, cancelled_hook=cancelled)


def pauses(api):
    launch(api, 0, 'touch')
    tap(api, 276, 10)
    first = api.shot('paused-first'); api.pump(.8); second = api.shot('paused-later')
    require(equal(visible_game(first, api.ox, api.oy, 0), visible_game(second, api.ox, api.oy, 0), api.exact), 'Game pixels advanced while paused')
    require(equal(canvas(first, api.ox, api.oy), canvas(second, api.ox, api.oy), api.exact), 'Paused UI changed without input')
    if api.controller:
        # A selects CONTINUE; physically holding it must not become game A.
        api.key('A', True); api.pump(.85)
        point = hook(api.shot('continue-held-A'), api.ox, api.oy)
        require(point[0] < 12 and abs(point[1]-20) < 6, 'Menu A hold leaked into gameplay')
        api.key('A', False); api.pump(.1)
        api.pointer('DOWN', 140, 120); api.pump(.2); api.pointer('UP', 140, 120); api.pump(.35)
        resumed = hook(api.shot('resumed-new-touch'), api.ox, api.oy)
        require(resumed[0] > 14, 'Fresh touch did not work after pause/held suppression')
        check(api, 'pause_exact_game_pixels_menu_hold_suppression_and_resume', exact=api.exact, resumed_hook=resumed)
        launch(api, 0, 'touch'); tap(api, 276, 10)
    else:
        check(api, 'pause_game_and_ui_pixels_frozen', exact=api.exact)
    api.pointer('DOWN', 140, 85); api.pump(.8)
    require(hook(api.shot('continue-held-touch'), api.ox, api.oy)[0] < 12, 'CONTINUE pointer leaked into gameplay')
    api.pointer('UP', 140, 85); api.pump(.1)
    check(api, 'continue_touch_capture_suppression')
    if not api.controller:
        api.pointer('DOWN', 140, 120); api.pump(.2); api.pointer('UP', 140, 120); api.pump(.35)
        resumed = hook(api.shot('resumed-new-touch'), api.ox, api.oy)
        require(resumed[0] > 14, 'Fresh touch did not work after CONTINUE touch release')
        check(api, 'pause_touch_capture_release_and_resume', resumed_hook=resumed)


def exercise(api):
    api.cancel()
    # Creates a save on a fresh install without changing the selected game.
    if api.controller:
        press(api, 'RIGHT'); press(api, 'LEFT'); api.pump(.35)
    else:
        tap(api, 276, 10); tap(api, 140, 85); api.pump(.35)
        for name in ([f'controller_launch_play_and_independent_save_{game}' for game in range(4)] +
                     ['launcher_A_requires_release', 'fishing_controller_hold_release_tap',
                      'pause_exact_game_pixels_menu_hold_suppression_and_resume']):
            check(api, name, status='unavailable', reason=api.report['controller_input']['reason'])
    baseline = decode(api.saved('baseline')); api.selected = baseline['selected']
    first_launcher = api.shot('launcher'); api.pump(.45)
    require(equal(canvas(first_launcher, api.ox, api.oy), canvas(api.shot('launcher-idle'), api.ox, api.oy), api.exact), 'Launcher animation/simulation changed idle pixels')
    latest = baseline
    methods = ('touch', 'controller') if api.controller else ('touch',)
    for method in methods:
        for game in range(4):
            home(api)
            before = decode(api.saved(method+'-'+str(game)+'-before'))
            launch(api, game, method)
            api.shot(method+'-'+str(game)+'-launch')
            if game == 0:
                if api.controller: press(api, 'A', .20, .3)
                else: tap(api, 140, 120, duration=.20, settle=.3)
                tap(api, 140, 120, settle=.4)
            elif game == 1:
                tap(api, 140, 120); api.pointer('DOWN', 140, 120); api.pump(.35); api.pointer('UP', 140, 120)
            elif game == 2:
                if api.controller: press(api, 'A')
                else: tap(api, 140, 120)
                api.pointer('DOWN', 140, 120); api.pump(.9); api.pointer('UP', 140, 120)
            else:
                for _ in range(2): tap(api, 140, 120, settle=.85)
            api.shot(method+'-'+str(game)+'-played')
            home(api); latest = decode(api.saved(method+'-'+str(game)+'-after'))
            require(latest['selected'] == game and latest['plays'][game] > before['plays'][game], 'Game selection/play count was not durable')
            require(all(latest['best'][i] >= before['best'][i] for i in range(4)), 'High score decreased')
            require(all(latest['best'][i] == before['best'][i] and latest['plays'][i] == before['plays'][i] for i in range(4) if i != game), 'Playing one game changed another record')
            if game == 3: require(latest['best'][3] > 0, 'COLOR ROLL actual shots never scored')
            check(api, method+'_launch_play_and_independent_save_'+str(game), best=latest['best'], plays=latest['plays'])
    require(all(latest['plays'][i] >= baseline['plays'][i]+len(methods) for i in range(4)), 'Not all four games were played through every available input route')
    fishing(api); pauses(api)
    home(api); launch(api, 3, 'touch'); tap(api, 140, 120, settle=.85); home(api)
    final = api.saved('before-exit'); value = decode(final)
    require(value['selected'] == 3 and value['best'][3] > 0, 'Final selection/high score missing')
    launcher = api.shot('final-launcher')
    return final, launcher


def audit(api):
    logs = re.sub(r'\x1b\[[0-9;]*m', '', api.logs())
    require(not FAULTS.search(logs), 'Runtime trap/stack/input queue fault; inspect logs')
    audio = re.findall(r'Pocket: (?:optional )?audio[^\n]*', logs)
    require(audio, 'No explicit audio ready/unsupported/denied/unavailable result in runtime logs')
    api.report['audio'] = list(dict.fromkeys(api.report.get('audio', [])+audio))
    events = re.findall(r'crisp score game=([^\n]*)', logs)
    api.report['score_events'] = list(dict.fromkeys(api.report.get('score_events', [])+events))
    check(api, 'runtime_no_traps_stack_or_input_queue_overflow')


def simulator_run(args, report):
    state = args.output / ('state-'+uuid.uuid4().hex[:8])
    report['state'] = str(state)
    with tempfile.TemporaryDirectory(prefix='crisp-ctrl-') as control:
        for index in range(2):
            with common_sim.Simulator(args, state, Path(control)/('control-'+str(index)+'.sock'), 'session-'+str(index), report) as sim:
                api = SimAPI(sim, args, report)
                if not index:
                    final, launcher = exercise(api)
                    audit(api); api.exit()
                    require(api.saved('after-exit') == final, 'EXIT changed game records')
                else:
                    require('crisp save-restored selected=3' in api.logs(), 'Relaunch did not restore selected game')
                    require(api.saved('reloaded') == final, 'Relaunch did not restore all independent records exactly')
                    require(equal(canvas(launcher, api.ox, api.oy), canvas(api.shot('restored-launcher'), api.ox, api.oy), True), 'Restored launcher pixels differ')
                    check(api, 'exit_relaunch_exact_48byte_records_and_launcher')
                    audit(api); api.exit()


def device_start(d, report):
    # First-run audio permission is a Host modal. Use only fresh logs for this
    # app, retain its screenshot, then click the real board's Allow button.
    # INPUT SYNC returns the device log clock, so history cannot satisfy this
    # launch with readiness/audio from an older process.
    clock = d.request('INPUT SYNC')[-1].payload
    match = re.search(r'(?:^|;)log_ms=([0-9]+)(?:;|$)', clock)
    launch_floor = int(match[1]) if match else None
    at = len(d.logs)
    d.request('PACKAGE run '+IDENTITY)
    replayed = False
    def replay_history():
        nonlocal replayed
        if replayed or not any(re.search(r'\bdropped=[1-9][0-9]*\b', line) for line in d.logs[at:]):
            return False
        require(launch_floor is not None, 'Missing device timestamp for safe LOGSUB replay')
        replayed = True
        start = len(d.logs)
        d.request('LOGSUB'); d.pump(.75)
        history = d.logs[start:]
        fresh = [line for line in history if (stamp := re.match(r'^([0-9]+)\t', line)) and int(stamp[1]) >= launch_floor]
        d.logs[start:] = fresh
        report.setdefault('startup_history_replays', []).append({'launch_log_ms_floor': launch_floor,
            'retained_lines': len(fresh), 'discarded_lines': len(history)-len(fresh)})
        return True
    def host_start_signals():
        return [line for line in d.logs[at:] if
                ('Started verified Package ' in line and ':'+IDENTITY in line) or
                ('Launch completion: ' in line and ':'+IDENTITY+' intent=0 status=0' in line)]
    def startup_ready():
        lines = d.logs[at:]
        return any('Pocket: ready app='+IDENTITY in line for line in lines) or (
            any(re.search(r'\bdropped=[1-9][0-9]*\b', line) for line in lines) and
            bool(host_start_signals()) and
            any('Created: 288x224 format=1' in line for line in lines) and
            any('First frame submitted:' in line for line in lines))
    try:
        d.until(startup_ready, 'Crisp startup and first frame', seconds=4)
    except TimeoutError:
        require(replay_history(), 'Startup timeout without recoverable log DROP')
        d.until(startup_ready, 'Crisp startup and first frame after LOGSUB', seconds=4)
    d.pump(.75)
    ready_lines = [line for line in d.logs[at:] if 'Pocket: ready app='+IDENTITY in line]
    if ready_lines:
        require(any('canvas=288x224 origin=4,8 scale=1' in line for line in ready_lines), 'Unexpected device touch geometry')
    else:
        label = 'startup-ready-with-log-drop-'+str(len(report.get('startup_log_drops', [])))
        d.shot(label)
        report.setdefault('startup_log_drops', []).append({'host_started': True,
            'host_start_signals': host_start_signals(),
            'canvas': '288x224', 'first_frame': True, 'screenshot': label+'.jpg',
            'drops': [line for line in d.logs[at:] if re.search(r'\bdropped=[1-9][0-9]*\b', line)]})
    def ready():
        return any('Pocket: audio ready 16000 Hz mono' in line for line in d.logs[at:])
    def prompt():
        return any('Runtime permission prompt queued:' in line and
                   ':'+IDENTITY+' permission=' in line and
                   ('audio.playback' in line or '音频播放' in line) for line in d.logs[at:])
    if not ready() and not prompt():
        replay_history()
    d.until(lambda: ready() or prompt(), 'Device audio ready or permission prompt', seconds=8)
    prompted = prompt() and not ready()
    if prompted:
        d.shot('audio-permission-'+str(len(report.get('audio_starts', []))))
        d.request('INPUT TAP 200 183'); d.request('INPUT SYNC'); d.pump(.6)
    d.until(ready, 'Device mixer audio ready after permission', seconds=8)
    report.setdefault('audio_starts', []).append({'permission_prompt': prompted, 'ready': True})


def device_run(args, report):
    import device as common_device
    common_device.IDS['crisp-arcade'] = IDENTITY
    args.game = 'crisp-arcade'
    class CrispDevice(common_device.Device):
        active_game = 0
        def board(self, label):
            return visible_game(self.shot(label), self.ox, self.oy, self.active_game)
    with (args.output/'device.jsonl').open('w') as stream:
        with common_device.pxadb.open_client(args.port, 30) as client:
            report['hello'] = client.device_info
            report['info'] = common_device.pxadb.info_properties(client.device_info)
            require(report['info'].get('target') == 'esp32s3', 'Expected ESP32-S3 target')
            d = CrispDevice(args, client, report, stream)
            try:
                require(not any(f.kind == 'PKG' and 'active=1' in f.payload for f in d.request('PACKAGES')), 'Another app is active')
                client.subscribe_logs()
                capabilities = d.request('INPUT CAPABILITIES')
                report['input'] = [f.payload for f in capabilities]
                props = common_device.pxadb.info_properties(capabilities[-1].payload)
                report['input_capabilities'] = props
                mask = int(props.get('controller_mask', '0'))
                required = DeviceAPI.MASKS['A'] | DeviceAPI.MASKS['LEFT'] | DeviceAPI.MASKS['RIGHT']
                report['controller_input'] = {'available': mask & required == required, 'mask': mask,
                    'reason': f'INPUT CAPABILITIES controller_mask={mask}; Crisp controller checks require mask {required}'}
                d.request('INPUT KEY HOME')
                d.request('INPUT SWIPE 148 192 148 48 600 8'); d.pump(.5); d.logs.clear(); device_start(d, report)
                api = DeviceAPI(d, args, report)
                final, launcher = exercise(api)
                if not args.skip_idle_lock:
                    launch(api, 0, 'touch'); d.active_game = 0
                    d.request('PERF CLEAR')
                    perf_started = time.monotonic()
                    d.request('PERF START'); d.pump(3)
                    report['foreground_perf'] = common_device.pxadb.info_properties(d.request('PERF STOP')[-1].payload)
                    elapsed = time.monotonic() - perf_started
                    presents = int(report['foreground_perf']['present'])
                    report['foreground_perf_sample'] = {'elapsed_seconds': elapsed,
                        'present_per_second': presents / elapsed,
                        'measurement': 'Host monotonic around PERF START, pump(3), PERF STOP; includes command overhead'}
                    require(presents > 0, 'No foreground frames')
                    tap(api, 276, 10)
                    # Keep an actual game paused while idle-lock measures zero
                    # host frames and verifies the same game pixels on unlock.
                    d.lifecycle(d.board('before-lock-paused'))
                    tap(api, 140, 85); home(api); launch(api, 3, 'touch'); home(api)
                    final = api.saved('after-lifecycle'); launcher = api.shot('after-lifecycle-launcher')
                audit(api); api.exit(); require(api.saved('after-exit') == final, 'EXIT changed records')
                at = len(d.logs); device_start(d, report)
                require(any('crisp save-restored selected=3' in line for line in d.logs[at:]), 'Relaunch selection not restored')
                require(api.saved('reloaded') == final, 'Device relaunch changed durable records')
                require(equal(canvas(launcher, 4, 8), canvas(api.shot('restored-launcher'), 4, 8), False), 'Device restored launcher differs')
                check(api, 'exit_relaunch_exact_48byte_records_and_launcher'); audit(api); api.exit()
            finally:
                d.request('INPUT CANCEL')
                active = any(f.kind == 'PKG' and ':'+IDENTITY+'\t' in f.payload and 'active=1' in f.payload for f in d.request('PACKAGES'))
                if active: d.request('PACKAGE stop '+IDENTITY)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    modes = parser.add_subparsers(dest='mode', required=True)
    sim = modes.add_parser('simulator')
    sim.add_argument('--simulator', required=True, type=Path)
    sim.add_argument('--package', required=True, type=Path)
    sim.add_argument('--publisher-key', required=True, type=Path)
    dev = modes.add_parser('device')
    dev.add_argument('--port', required=True)
    dev.add_argument('--lock-timeout', type=float, default=180)
    dev.add_argument('--skip-idle-lock', action='store_true')
    for child in (sim, dev): child.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output = args.output.resolve()
    require(args.output.is_relative_to(ROOT/'local/artifacts'), 'Output must be under ignored local/artifacts')
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'passed': False, 'game': 'crisp-arcade', 'mode': args.mode, 'checks': [], 'processes': []}
    try:
        if args.mode == 'simulator':
            for key in ('simulator', 'package', 'publisher_key'): setattr(args, key, getattr(args, key).resolve())
            if args.package.suffix == '.pxa':
                args.package = args.package.with_suffix('')
            require(args.package.is_dir(), 'Simulator requires the signed package expanded alongside the .pxa')
            simulator_run(args, report)
        else:
            device_run(args, report)
        report['passed'] = True
    except BaseException:
        report['error'] = traceback.format_exc()
        raise
    finally:
        (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
