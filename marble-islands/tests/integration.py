#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Exercise the signed Marble Islands package through real PXA transports.

Simulator: --simulator EXE --package EXPANDED_DIR --publisher-key DER --output DIR
Device: --port SERIAL --output DIR [--skip-idle-lock]
Outputs must be under local/artifacts. The device mode expects an already
installed package, owns the serial port exclusively, and never flashes,
installs, deletes saves or removes packages. By default it also solves the
first island with slow, feedback-controlled touch steering; --skip-solve
omits that longer achievement check. Pillow and pyserial are required.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import traceback
import zlib

sys.dont_write_bytecode = True
APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[2]
sys.path.insert(0, str(APP.parent / 'common/pocket-game/tests'))
sys.path.insert(0, str(ROOT / 'tools/pxadb'))
import simulator as common_sim

IDENTITY = 'pxa-marble-islands'
SAVE_KEY = IDENTITY+'.save.v1'
FAULTS = re.compile(r'Guest exception|callback failed|out of bounds memory access|unreachable instruction|stack overflow|wasm trap|input queue overflow|event queue overflow|exit flush timed out|Guru Meditation', re.I)
require = common_sim.require
PERSISTENT = ('unlocked', 'medals', 'best_ms')
RUN = ('level', 'phase', 'elapsed_ms', 'coins', 'fall_ms', 'x', 'y', 'vx', 'vy')


def decode(raw):
    """Independently decode the actual 88-byte little-endian game wire format."""
    require(len(raw) == 88 and raw[:5] == b'MISL\x01', 'Invalid Marble save header/version/size')
    checksum = 2166136261
    for byte in raw[:84]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    require(checksum == struct.unpack_from('<I', raw, 84)[0], 'Bad Marble checksum')
    require(raw[72:84] == bytes(12), 'Nonzero Marble reserved bytes')
    unlocked, level, phase = raw[5:8]
    require(1 <= unlocked <= 8 and level < unlocked and phase <= 2, 'Invalid unlock/level/phase')
    medals = list(raw[8:16]); best = list(struct.unpack_from('<8I', raw, 16))
    require(max(medals) <= 3 and max(best) <= 3600000, 'Invalid medal/best time')
    elapsed, coins, fall = struct.unpack_from('<IHH', raw, 48)
    require(elapsed <= 3600000 and fall <= 1000 and coins < (1 << (3, 2, 3, 3, 3, 4, 4, 4)[level]), 'Invalid run fields')
    result = dict(zip(('x', 'y', 'vx', 'vy'), (v/65536 for v in struct.unpack_from('<4i', raw, 56))))
    result.update(unlocked=unlocked, level=level, phase=phase, medals=medals, best_ms=best,
                  elapsed_ms=elapsed, coins=coins, fall_ms=fall)
    return result


def host_kv(raw):
    if len(raw) < 26:
        return None
    magic, version, _, generation, size, crc = struct.unpack_from('<4sHHQII', raw)
    body = raw[24:]
    if magic != b'PXKV' or version != 1 or len(body) != size or zlib.crc32(body) != crc:
        return None
    at, values = 2, {}
    try:
        for _ in range(struct.unpack_from('<H', body)[0]):
            length = body[at]; at += 1
            key = body[at:at+length].decode(); at += length
            length = struct.unpack_from('<H', body, at)[0]; at += 2
            require(at+length <= len(body), 'Short KV value')
            values[key] = body[at:at+length]; at += length
        require(at == len(body), 'Trailing KV bytes')
        if SAVE_KEY in values:
            return generation, values[SAVE_KEY]
    except (IndexError, struct.error, UnicodeDecodeError):
        return None
    return None


def same(a, b, names):
    return all(a[key] == b[key] for key in names)


class Session:
    MASKS = {'UP': 1, 'RIGHT': 2, 'DOWN': 4, 'LEFT': 8, 'A': 16, 'B': 32, 'X': 64, 'START': 1024}
    SDL = {'A': 'Z', 'B': 'X', 'X': 'A', 'START': 'ENTER'}

    def __init__(self, backend, report, device=False):
        self.b, self.report, self.device = backend, report, device
        self.paused, self.keys, self.index = False, set(), 0
        self.touch_only = device or getattr(backend.args, 'touch_only', False)

    def check(self, condition, name, **evidence):
        require(condition, name)
        self.report['checks'].append({'name': name, **evidence})

    def logs(self):
        return '\n'.join(self.b.logs) if self.device else self.b.logs()

    def pump(self, seconds):
        if self.device:
            self.b.pump(seconds)
        else:
            time.sleep(seconds)
            require(self.b.proc.poll() is None, 'Simulator stopped unexpectedly')
        require(not FAULTS.search(self.logs()), 'Guest/runtime fault')

    def key(self, key, down):
        if self.device:
            supported = int(self.report.get('input_capabilities', {}).get('controller_mask', '0'), 0)
            require(supported & self.MASKS[key], 'Device does not advertise controller injection for '+key)
        if down:
            self.keys.add(key)
        else:
            self.keys.discard(key)
        if self.device:
            self.b.request('INPUT CONTROLLER 0 1 '+str(sum(self.MASKS[k] for k in self.keys)))
            self.b.request('INPUT SYNC')
        else:
            sdl = self.SDL.get(key, key)
            common_sim.command(self.b.sock, ('KEY_DOWN ' if down else 'KEY_UP ')+sdl)
            if down:
                self.b.keys.add(sdl)
            else:
                self.b.keys.discard(sdl)

    def press(self, key, seconds=.08):
        self.key(key, True); self.pump(seconds); self.key(key, False); self.pump(.10)

    def pointer(self, phase, x, y):
        self.b.pointer(phase, x, y)

    def tap(self, x, y, settle=.16):
        self.pointer('DOWN', x, y); self.pump(.07); self.pointer('UP', x, y); self.pump(settle)

    def cancel(self):
        if self.device:
            self.b.request('INPUT CANCEL')
        else:
            common_sim.command(self.b.sock, 'TOUCH CANCEL_ALL')
            for key in tuple(self.b.keys):
                common_sim.command(self.b.sock, 'KEY_UP '+key)
            self.b.keys.clear()
        self.keys.clear(); self.pump(.10)

    def shot(self, label):
        return self.b.shot(label)

    def canvas(self, image):
        ox = 4 if self.device else 16
        return image.crop((ox, 8, ox+288, 232))

    def pause(self):
        if not self.paused:
            self.tap(276, 10, settle=.20); self.paused = True

    def resume(self):
        if self.paused:
            self.tap(140, 85, settle=.08); self.paused = False

    def saved(self, label):
        self.index += 1
        label = f'{getattr(self.b, "label", "device")}-{self.index:03d}-{label}'
        if self.device:
            fs = self.b.module.pxadb.NormalFsClient(self.b.client)
            paths = self.b.request('PACKAGE paths '+self.b.module.PUBLISHER+':'+IDENTITY)
            private = next(f.payload.split('\t', 1)[1] for f in paths if f.kind == 'PATH' and f.payload.startswith('private\t'))+'/.pxa-storage'
            candidates = []
            for kind, _, name in fs.list(private):
                if kind == 'F' and name.startswith('.pxa-kv-'):
                    local = self.b.args.output/(label+'-'+name.removeprefix('.pxa-'))
                    for attempt in range(3):
                        try:
                            fs.get(private+'/'+name, local)
                            break
                        except self.b.module.pxadb.PxaDbError as error:
                            if not str(error).startswith('file transfer checksum mismatch ') or attempt == 2:
                                raise
                            self.report.setdefault('kv_read_retries', []).append({'snapshot': label, 'file': name, 'attempt': attempt+1})
                            self.pump(.2)
                    parsed = host_kv(local.read_bytes())
                    if parsed:
                        candidates.append(parsed)
        else:
            candidates = []
            for path in self.b.state.rglob('.pxa-kv-*'):
                try:
                    parsed = host_kv(path.read_bytes())
                    if parsed:
                        candidates.append(parsed)
                except FileNotFoundError:
                    pass  # The host atomically replaces storage generations.
        require(candidates, 'No durable Marble save yet')
        generation, raw = max(candidates, key=lambda row: row[0])
        state = decode(raw)
        (self.b.args.output/(label+'.save')).write_bytes(raw)
        self.report.setdefault('saves', {})[label] = dict(state, generation=generation, sha256=hashlib.sha256(raw).hexdigest())
        return state

    def flush(self, label):
        before = self.logs().count('Pocket: saved')
        already = self.paused
        self.pause()
        if not already:
            deadline = time.monotonic()+10
            while self.logs().count('Pocket: saved') <= before:
                require(time.monotonic() < deadline, 'Timed out saving current paused snapshot')
                self.pump(.10)
        return self.saved(label)

    def first_island(self):
        self.pause(); self.tap(140, 115, settle=.15); self.paused = False
        self.tap(43, 203); self.tap(70, 62)
        state = self.flush('first-island')
        self.check(state['level'] == 0 and state['phase'] == 0 and abs(state['x']-1.5) < .001 and abs(state['y']-5.5) < .001,
                   'UI_selects_original_first_island_and_preserves_progress')
        return state

    def exit(self):
        self.pause()
        self.pointer('DOWN', 140, 145)
        if self.device:
            def stopped():
                return any(f.kind == 'PKG' and ':'+IDENTITY+'\t' in f.payload and 'active=0' in f.payload for f in self.b.request('PACKAGES'))
            self.b.until(stopped, 'Marble menu EXIT', seconds=10)
            self.b.request('INPUT CANCEL')
        else:
            self.b.wait(lambda: self.b.proc.poll() is not None, 'Marble menu EXIT')
            require(self.b.proc.returncode == 0, 'Abnormal guest EXIT')
        self.paused = False


def input_and_pause(s):
    original = s.first_island()
    s.resume()
    if s.touch_only:
        s.pointer('DOWN', 144, 120); s.pointer('MOVE', 144, 90)
        s.pump(.7); s.pointer('UP', 144, 90)
    else:
        s.key('UP', True); s.pump(.7); s.key('UP', False)
    moving = s.flush('held-up')
    s.check(moving['y'] < original['y']-.06 and moving['vy'] < -.2,
            'held_touch_continuously_accelerates_real_ball' if s.touch_only else 'held_arrow_continuously_accelerates_real_ball',
            y=moving['y'], vy=moving['vy'])
    s.resume()
    if s.touch_only:
        s.pointer('DOWN', 253, 170); s.pump(.65); s.pointer('UP', 253, 170)
    else:
        s.key('A', True); s.pump(.65); s.key('A', False)
    braked = s.flush('touch-brake' if s.touch_only else 'controller-brake')
    s.check(abs(braked['vy']) < abs(moving['vy'])*.35,
            'held_touch_brake_stops_real_velocity' if s.touch_only else 'held_A_brakes_real_velocity',
            before=moving['vy'], after=braked['vy'])
    first = s.canvas(s.shot('paused-first')); s.pump(.8)
    later = s.canvas(s.shot('paused-later'))
    equal = common_sim.difference(first, later) < (.002 if s.device else .000001)
    s.check(equal and same(braked, s.saved('paused-later'), RUN+PERSISTENT), 'menu_freezes_pixels_timer_and_world_state')

    for ending in ('outside-UP', 'CANCEL'):
        before = s.first_island(); s.resume()
        s.pointer('DOWN', 144, 120); s.pointer('MOVE', 174, 120); s.pump(.55)
        if ending == 'CANCEL':
            s.cancel()
        else:
            s.pointer('UP', -2, 120)
        # Do not start another touch: that would mask a failed drag release.
        # On touch-only transports, natural damping must stop the ball itself.
        if s.touch_only:
            s.pump(1.6)
        else:
            s.key('A', True); s.pump(.55); s.key('A', False); s.pump(.30)
        stopped = s.flush(ending)
        s.check(stopped['x'] > before['x']+.03 and abs(stopped['vx']) < .07,
                'touch_drag_'+ending+'_releases_analog_input', x=stopped['x'], vx=stopped['vx'])

    s.first_island(); s.resume()
    # A downward tilt from the lowest row must fall into actual water.
    at = s.logs().count('Marble: fall')
    s.pointer('DOWN', 144, 120); s.pointer('MOVE', 144, 150)
    deadline = time.monotonic()+8
    while s.logs().count('Marble: fall') == at:
        require(time.monotonic() < deadline, 'Downward tilt did not reach water')
        s.pump(.15)
    s.pointer('UP', 144, 150); s.pump(1)
    retried = s.flush('automatic-retry')
    s.check(retried['phase'] == 0 and abs(retried['x']-1.5) < .001 and abs(retried['y']-5.5) < .001 and retried['coins'] == 0,
            'real_water_fall_automatically_retries_without_old_touch')
    s.resume(); s.tap(43, 203)
    first = s.canvas(s.shot('islands-first')); s.pump(.6)
    later = s.canvas(s.shot('islands-later'))
    s.check(common_sim.difference(first, later) < (.002 if s.device else .000001), 'island_selector_is_static')
    s.tap(70, 62)


def solve_first(s):
    before = s.first_island()
    # Single-pointer device input uses a three-pixel tilt: terminal speed is
    # about 0.2 cells/s without a second finger or controller. The simulator's
    # controller route retains simultaneous A brake and full touch tilt.
    targets = ((2.5, 4.5, 4), (2.5, 2.5, 1), (4.5, 3.5, 2), (5.5, 1.5, 0))
    state = before
    pulses = 0
    for tx, ty, coin in targets:
        for _ in range(140):
            if (coin and state['coins'] & coin) or (not coin and state['phase'] == 2):
                break
            require(state['phase'] == 0, 'Feedback steering unexpectedly fell or ended')
            dx, dy = tx-state['x'], ty-state['y']
            # Keep full slow tilt until the collection/goal radius is reached.
            magnitude = max(.001, math.hypot(dx, dy))
            tilt = 3 if s.touch_only else 30
            ix, iy = round(tilt*dx/magnitude), round(tilt*dy/magnitude)
            s.resume()
            # Near the goal, coasting during CONTINUE can already win. A new
            # A edge would then mean NEXT, so finish the short approach only
            # with the touch stick; its anchor is outside both win buttons.
            if not s.touch_only and (coin or magnitude > .7):
                s.key('A', True)
            s.pointer('DOWN', 144, 120); s.pointer('MOVE', 144+ix, 120+iy)
            s.pump(.8); s.pointer('UP', 144+ix, 120+iy)
            if not s.touch_only:
                s.key('A', False)
            state = s.flush('solve-waypoint')
            pulses += 1
        else:
            raise TimeoutError(f'Feedback steering target {(tx, ty)}; last={state}')
    s.check(state['phase'] == 2 and state['coins'] == 7 and state['unlocked'] >= 2 and state['medals'][0] >= 1 and state['best_ms'][0] > 0,
            'touch_physically_collects_all_three_gems_wins_and_unlocks_second_island', elapsed_ms=state['elapsed_ms'], pulses=pulses)
    s.check(state['unlocked'] >= before['unlocked'] and all(a >= b for a, b in zip(state['medals'], before['medals'])) and all(not old or 0 < new <= old for new, old in zip(state['best_ms'], before['best_ms'])),
            'earned_or_existing_medals_and_unlocks_survive_first_island_replay')
    s.shot('real-first-island-clear')
    return state


def interruption(s):
    s.first_island(); s.resume()
    if s.touch_only:
        s.pointer('DOWN', 144, 120); s.pointer('MOVE', 174, 120)
        s.pump(.35); s.pointer('UP', 174, 120)
    else:
        s.key('RIGHT', True); s.pump(.35); s.key('RIGHT', False)
    state = s.flush('interrupted-live-run')
    s.check(state['phase'] == 0 and state['elapsed_ms'] > 0 and state['x'] > 1.5 and state['vx'] > .2,
            'interrupted_run_has_real_position_velocity_and_elapsed')
    s.shot('interrupted-paused')
    s.exit()
    after = s.saved('after-exit')
    s.check(same(state, after, RUN+PERSISTENT), 'menu_EXIT_commits_exact_current_run')
    return after


def restoration(s, previous):
    require('Marble: load' in s.logs() and 'Pocket: load completed' in s.logs(), 'Guest did not restore its save')
    state = s.flush('reloaded-current-run')
    dt = (state['elapsed_ms']-previous['elapsed_ms'])/1000
    drag = math.exp(-2.6*max(0, dt))
    expected_x = previous['x']+previous['vx']*(1-drag)/2.6
    expected_y = previous['y']+previous['vy']*(1-drag)/2.6
    # The independent continuous damping solution tolerates the core's 10 ms
    # integration rounding. It detects a lost velocity as well as a lost pose.
    s.check(same(previous, state, PERSISTENT) and state['level'] == previous['level'] and state['phase'] == 0 and state['coins'] == previous['coins'] and
            0 <= dt < 3 and abs(state['x']-expected_x) < .05 and abs(state['y']-expected_y) < .05 and
            abs(state['vx']-previous['vx']*drag) < .07 and abs(state['vy']-previous['vy']*drag) < .07,
            'process_relaunch_restores_progress_pose_velocity_and_excludes_wall_time', before_ms=previous['elapsed_ms'], after_ms=state['elapsed_ms'])
    s.shot('restored-run')
    return state


def device_performance(s):
    """Read panel-completion intervals; host simulator rates are unrelated."""
    d = s.b
    s.resume(); s.pump(1)
    d.request('PERF CLEAR'); d.request('PERF START'); s.pump(3)
    props = d.module.pxadb.info_properties(d.request('PERF STOP')[-1].payload)
    s.report['foreground_perf'] = props
    samples = {}
    for kind in ('raster', 'present'):
        count = int(props[kind])
        # This game submits mapped RGB565 frames directly; host node raster
        # work may correctly be zero while panel presentations continue.
        require(count >= (1 if kind == 'present' else 0) and int(props[kind+'_overflow']) == 0, 'Invalid device '+kind+' capture')
        values = []
        for offset in range(0, count, 16):
            row = d.module.pxadb.info_properties(d.request(f'PERF READ {kind} {offset}')[-1].payload)
            require(int(row['offset']) == offset, 'Wrong PERF sample offset')
            chunk = [int(value) for value in row['values'].split(',') if value]
            require(len(chunk) == int(row['count']), 'Wrong PERF sample count')
            values.extend(chunk)
        require(len(values) == count and all(value > 0 for value in values), 'Malformed device '+kind+' intervals')
        samples[kind] = values
    require(int(props['surface_changed']) == 0, 'Game surface changed during foreground capture')
    s.report['foreground_samples_us'] = samples
    s.report['foreground_display_fps'] = 1e6*len(samples['present'])/sum(samples['present'])
    s.report['foreground_mean_raster_us'] = sum(samples['raster'])/len(samples['raster']) if samples['raster'] else None
    s.check(True, 'real_device_presents_foreground_3D_frames',
            fps=s.report['foreground_display_fps'], present_intervals=len(samples['present']))
    s.shot('live-3d-performance')
    d.request('PERF CLEAR')


def idle(s):
    d = s.b
    before = s.flush('before-idle')
    first = s.canvas(s.shot('before-idle-paused'))
    at = len(d.logs)
    def transition(value):
        return any('UI lifecycle:' in line and f'foreground={value}' in line and f'display_interactive={value}' in line for line in d.logs[at:])
    deadline, keepalive = time.monotonic()+d.args.lock_timeout, time.monotonic()+10
    while not transition(0) and time.monotonic() < deadline:
        s.pump(.1)
        if time.monotonic() >= keepalive:
            d.request('INFO'); keepalive += 10
    require(transition(0), 'No idle-lock lifecycle event')
    s.pump(1); d.request('PERF CLEAR'); d.request('PERF START'); s.pump(3)
    props = d.module.pxadb.info_properties(d.request('PERF STOP')[-1].payload)
    s.report['locked_perf'] = props
    s.check(int(props['raster']) == 0 and int(props['present']) == 0, 'idle_lock_produces_zero_raster_and_present')
    locked = s.saved('locked'); s.pump(1)
    s.check(same(before, locked, RUN+PERSISTENT) and same(locked, s.saved('locked-later'), RUN+PERSISTENT), 'background_and_locked_time_freeze_current_run')
    at = len(d.logs)
    d.request('PERF CLEAR'); d.request('PERF START')
    d.request('INPUT TAP 148 120'); s.pump(.3)
    d.request('INPUT SWIPE 148 192 148 48 600 8')
    d.until(lambda: transition(1), 'unlock foreground', seconds=15); s.pump(.6)
    props = d.module.pxadb.info_properties(d.request('PERF STOP')[-1].payload)
    s.report['resumed_perf'] = props
    unlocked = s.canvas(s.shot('unlocked-paused'))
    # The Host's FPS/heap overlay overlaps the top eight canvas rows; JPEG
    # blocks extend into the next eight. Compare the actual game below it.
    scene_box = (0, 16, 288, 224)
    scene_difference = common_sim.difference(first.crop(scene_box), unlocked.crop(scene_box))
    s.check(int(props['present']) > 0 and scene_difference < .005,
            'unlock_redraws_same_paused_scene', scene_difference=scene_difference,
            ignored_canvas_top_rows=16)


def simulator_run(args, report):
    state = Path(tempfile.mkdtemp(prefix='state-', dir=args.output))
    report['state_root'] = str(state)
    with tempfile.TemporaryDirectory(prefix='marble-sim-') as control:
        with common_sim.Simulator(args, state, Path(control)/'one.sock', 'game', report) as backend:
            s = Session(backend, report)
            input_and_pause(s)
            if not args.skip_solve:
                solve_first(s)
            previous = interruption(s)
        time.sleep(4)
        with common_sim.Simulator(args, state, Path(control)/'two.sock', 'reload', report) as backend:
            s = Session(backend, report)
            restoration(s, previous)
            s.resume(); s.pump(20.5); s.shot('live-3d-performance')
            samples = re.findall(r'Pocket: perf frames=(\d+) period_ms=(\d+) mean_draw_us=(\d+)', s.logs())
            report['runtime_perf'] = [dict(zip(('frames', 'period_ms', 'mean_draw_us'), map(int, sample))) for sample in samples]
            require(samples and int(samples[-1][0]) > 0, 'No real 3D foreground performance sample')
            s.exit()


def device_run(args, report):
    import device as common_device
    common_device.IDS['marble-islands'] = IDENTITY
    args.game = 'marble-islands'
    class MarbleDevice(common_device.Device):
        def start(self):
            clock = self.request('INPUT SYNC')[-1].payload
            match = re.search(r'(?:^|;)log_ms=([0-9]+)(?:;|$)', clock)
            launch_floor = int(match[1]) if match else None
            at = len(self.logs)
            self.request('PACKAGE run '+IDENTITY)
            replayed = False
            def dropped():
                return any(re.search(r'\bdropped=[1-9][0-9]*\b', line) for line in self.logs[at:])
            def replay_history():
                nonlocal replayed
                if replayed or not dropped():
                    return False
                require(launch_floor is not None, 'Missing device timestamp for safe LOGSUB replay')
                replayed = True
                start = len(self.logs)
                self.request('LOGSUB'); self.pump(.75)
                history = self.logs[start:]
                fresh = [line for line in history if (stamp := re.match(r'^([0-9]+)\t', line)) and int(stamp[1]) >= launch_floor]
                self.logs[start:] = fresh
                report.setdefault('startup_history_replays', []).append({'launch_log_ms_floor': launch_floor,
                    'retained_lines': len(fresh), 'discarded_lines': len(history)-len(fresh)})
                return True
            def host_started():
                return [line for line in self.logs[at:] if
                        ('Started verified Package ' in line and ':'+IDENTITY in line) or
                        ('Launch completion: ' in line and ':'+IDENTITY+' intent=0 status=0' in line)]
            def startup_ready():
                lines = self.logs[at:]
                return any('Pocket: ready app='+IDENTITY in line for line in lines) or (
                    dropped() and bool(host_started()) and
                    any('Created: 288x224 format=1' in line for line in lines) and
                    any('First frame submitted:' in line for line in lines))
            try:
                self.until(startup_ready, 'Marble startup and first frame', seconds=4)
            except TimeoutError:
                require(replay_history(), 'Startup timeout without recoverable log DROP')
                self.until(startup_ready, 'Marble startup after LOGSUB', seconds=4)
            self.pump(.75)
            ready_lines = [line for line in self.logs[at:] if 'Pocket: ready app='+IDENTITY in line]
            if ready_lines:
                require(any('canvas=288x224 origin=4,8 scale=1' in line for line in ready_lines), 'Unexpected device canvas geometry')
            else:
                label = 'startup-ready-with-log-drop-'+str(len(report.get('startup_log_drops', [])))
                self.shot(label)
                report.setdefault('startup_log_drops', []).append({'host_start_signals': host_started(),
                    'canvas': '288x224', 'first_frame': True, 'screenshot': label+'.jpg',
                    'drops': [line for line in self.logs[at:] if re.search(r'\bdropped=[1-9][0-9]*\b', line)]})
            def audio_ready():
                return any('Pocket: audio ready 16000 Hz mono' in line for line in self.logs[at:])
            def prompt():
                return any('Runtime permission prompt queued:' in line and ':'+IDENTITY+' permission=' in line and
                           ('音频播放' in line or 'audio.playback' in line) for line in self.logs[at:])
            if not audio_ready() and not prompt():
                replay_history()
            self.until(lambda: audio_ready() or prompt(), 'Marble audio ready or permission prompt', seconds=8)
            prompted = prompt() and not audio_ready()
            if prompted:
                self.shot('audio-permission')
                self.request('INPUT TAP 200 183'); self.request('INPUT SYNC'); self.pump(.6)
            self.until(audio_ready, 'Marble audio ready after permission', seconds=8)
            report.setdefault('audio_starts', []).append({'permission_prompt': prompted, 'ready': True})
    with (args.output/'serial.jsonl').open('w', buffering=1) as stream:
        with common_device.pxadb.open_client(args.port, 30) as client:
            report['device'] = common_device.pxadb.info_properties(client.device_info)
            require(report['device'].get('target') == 'esp32s3', 'Expected ESP32-S3')
            d = MarbleDevice(args, client, report, stream); d.module = common_device
            try:
                require(not any(f.kind == 'PKG' and 'active=1' in f.payload for f in d.request('PACKAGES')), 'Another package is active')
                client.subscribe_logs(); report['input'] = [f.payload for f in d.request('INPUT CAPABILITIES')]
                report['input_capabilities'] = common_device.pxadb.info_properties(report['input'][-1])
                report['controller_coverage'] = 'Not exercised on device; single-pointer touch route only'
                d.request('INPUT KEY HOME'); d.request('INPUT SWIPE 148 192 148 48 600 8'); d.pump(.5); d.logs.clear(); d.start()
                s = Session(d, report, True)
                if args.device_from == 'full':
                    input_and_pause(s)
                    if not args.skip_solve:
                        solve_first(s)
                if args.device_from != 'performance':
                    previous = interruption(s); d.pump(4); d.start()
                    s = Session(d, report, True); restoration(s, previous)
                device_performance(s)
                if not args.skip_idle_lock:
                    idle(s)
                s.exit()
            finally:
                d.request('PERF STOP'); d.request('PERF CLEAR')
                d.request('INPUT CANCEL')
                active = any(f.kind == 'PKG' and ':'+IDENTITY+'\t' in f.payload and 'active=1' in f.payload for f in d.request('PACKAGES'))
                if active:
                    d.request('PACKAGE stop '+IDENTITY)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--port')
    parser.add_argument('--device-from', choices=('full', 'lifecycle', 'performance'), default='full',
                        help='Resume device EXIT/relaunch or only performance and idle coverage')
    parser.add_argument('--simulator', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--publisher-key', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--touch-only', action='store_true', help='Exercise the device single-pointer route in the simulator too')
    parser.add_argument('--skip-solve', action='store_true')
    parser.add_argument('--skip-idle-lock', action='store_true')
    parser.add_argument('--lock-timeout', type=float, default=180)
    args = parser.parse_args()
    args.output = args.output.resolve()
    require(args.output.is_relative_to(ROOT/'local/artifacts'), 'Output must be inside ignored local/artifacts')
    args.output.mkdir(parents=True, exist_ok=True)
    if not args.port:
        for name in ('simulator', 'package', 'publisher_key'):
            value = getattr(args, name)
            if value is None:
                parser.error('--'+name.replace('_', '-')+' is required without --port')
            value = value.resolve()
            if name == 'package' and value.suffix == '.pxa':
                value = value.with_suffix('')
            require(value.exists(), 'Missing '+name)
            setattr(args, name, value)
    report = {'passed': False, 'game': 'marble-islands', 'backend': 'device' if args.port else 'simulator',
              'device_from': args.device_from if args.port else None, 'checks': [], 'processes': []}
    try:
        (device_run if args.port else simulator_run)(args, report)
        report['passed'] = True
    except BaseException:
        report['error'] = traceback.format_exc()
    (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
