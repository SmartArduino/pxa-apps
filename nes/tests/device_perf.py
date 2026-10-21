#!/usr/bin/env python3
"""Read live NES telemetry without touching flash inside measurement windows."""
import argparse
import contextlib
import json
import os
import pathlib
import re
import sys
import time

p = argparse.ArgumentParser()
p.add_argument('--port', required=True)
p.add_argument('--baud', type=int, default=115200)
p.add_argument('--board', choices=('pai-touch','doit-gamekit','doit-gamekit-1','esp32s31-korvo-1'), default='pai-touch')
p.add_argument('--serial', help='Require this device serial before injecting input')
p.add_argument('--input-mode', choices=('auto','touch','controller'), default='auto')
p.add_argument('--output', type=pathlib.Path, required=True)
p.add_argument('--pxadb-dir', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[4]/'tools/pxadb')
p.add_argument('--motion', choices=('none','mario'), default='none', help='Hold RIGHT and pulse A through touch or controller input')
p.add_argument('--samples', type=int, default=5)
p.add_argument('--warmup', type=float, default=6)
p.add_argument('--app-id', default='pxa-nes', help='Installed emulator app identity')
p.add_argument('--hz', type=int, choices=(50,60), default=60, help='ROM system rate: PAL=50, NTSC=60')
p.add_argument('--mapper', type=int, help='Reject samples after an unexpected ROM/mapper switch')
p.add_argument('--jpeg', action='store_true', help='Use a smaller evidence capture after measurement')
p.add_argument('--require-audio', action='store_true', help='Require sound=1 telemetry in every collected frame window (NES 0.1.26+)')
p.add_argument('--presentation-source', choices=('display','surface'), default='display',
               help='Korvo direct scanout reports distinct Surface completions through the surface probe')
args = p.parse_args()
if args.samples < 2:
    p.error('Use at least two complete five-second windows')
if args.baud <= 0:
    p.error('Baud must be positive')
os.environ['PXADB_BAUD'] = str(args.baud)
input_mode = args.input_mode
if input_mode == 'auto':
    input_mode = 'controller' if args.board in ('doit-gamekit','doit-gamekit-1') else 'touch'
expected_target = 'esp32s3' if args.board == 'pai-touch' else 'esp32s31'
sys.path.insert(0, str(args.pxadb_dir))
import pxadb

args.output.mkdir(parents=True, exist_ok=True)
report = {'motion':args.motion, 'board':args.board, 'input_mode':input_mode, 'baud':args.baud, 'display_samples': [], 'core_samples': [], 'scheduler_samples': [], 'samples': [], 'panel_fps_samples': [], 'app_id': args.app_id, 'expected_hz': args.hz, 'scope': 'Live NES simulation, Surface submissions and optional LCD DMA completions; excludes screenshots and storage queries; not optical panel Hz'}
state = {'collect': False, 'version': None, 'after': 0}
ansi = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
seen = set()
with (args.output/'serial.log').open('w', buffering=1) as log, pxadb.PxaDbClient(args.port, 15) as c, contextlib.ExitStack() as cleanup:
    def record(line):
        log.write(line+'\n')
        clean = ansi.sub('', line)
        if 'NES SCHED ' in clean:
            scheduler = dict(re.findall(r'([a-z0-9_]+)=([^\s]+)', clean.split('NES SCHED ', 1)[1]))
            if any(s.get('wall_us') == scheduler.get('wall_us') for s in report['samples']):
                report['scheduler_samples'].append(scheduler)
        if 'NES DISPLAY ' in clean:
            display = dict(re.findall(r'([a-z0-9_]+)=([^\s]+)', clean.split('NES DISPLAY ', 1)[1]))
            if state['collect']: report['display_samples'].append(display)
        if 'NES CORE ' in clean:
            core = dict(re.findall(r'([a-z0-9_]+)=([^\s]+)', clean.split('NES CORE ', 1)[1]))
            paired = state['collect'] or any(s.get('wall_us') == core.get('wall_us') for s in report['samples'])
            if paired and core.get('version') == state['version'] and int(core.get('wall_us', '0')) >= state['after']:
                report['core_samples'].append(core)
        if not state['collect'] or 'NES PERF ' not in clean:
            return
        row = dict(re.findall(r'([a-z0-9_]+)=([^\s]+)', clean.split('NES PERF ', 1)[1]))
        stamp = int(row.get('wall_us', '0'))
        if row.get('version') != state['version'] or stamp - int(row.get('elapsed_us', '0')) < state['after'] or stamp in seen:
            return
        if 'frames' in row and 'elapsed_us' in row and int(row['sim_x100'])!=int(row['frames'])*100000000//int(row['elapsed_us']):
            raise ValueError('Frame rate does not match completed frames and wall time')
        seen.add(stamp)
        report['samples'].append(row)
        print(row, flush=True)
    c.log_callback = lambda f: record(f.payload)
    c.raw_callback = lambda b: record(b.decode('utf-8', 'replace'))
    report['device'] = c.hello()
    properties = pxadb.info_properties(report['device'])
    if properties.get('target') != expected_target:
        raise SystemExit('Device CPU does not match --board')
    if args.serial and properties.get('serial') != args.serial:
        raise SystemExit('Device serial does not match --serial')
    report['info'] = [f.payload for f in c.request('INFO')]
    info = pxadb.info_properties(';'.join(report['info']))
    if info.get('board') != args.board:
        raise SystemExit('Device board does not match --board')
    caps = [f.payload for f in c.request('INPUT CAPABILITIES')]
    report['input_capabilities'] = caps
    input_caps = pxadb.info_properties(';'.join(caps))
    width = int(input_caps['width'])
    height = int(input_caps['height'])
    input_scale = 2 if (width,height) == (800,480) else 1
    canvas_height = height//input_scale
    multi_touch = int(input_caps.get('max_pointers','1')) >= 2
    report['motion_pattern'] = 'RIGHT plus A pulses' if input_mode == 'controller' or multi_touch else 'Alternate RIGHT and A with one pointer'
    if input_mode == 'controller' and not int(input_caps.get('controller_mask','0')):
        raise SystemExit('Device cannot inject controller state')
    right_x, a_x = 95*width//296, 275*width//296
    right_y, a_y = (canvas_height-61)*input_scale, (canvas_height-69)*input_scale
    c.subscribe_logs()
    report['packages'] = [f.payload for f in c.request('PACKAGES') if f.kind == 'PKG']
    active = next((row for row in report['packages'] if ':'+args.app_id+'\t' in row and 'active=1' in row), None)
    if not active:
        raise SystemExit('Unlock the selected board, start NES Emulator and load a ROM first')
    if input_mode in ('controller','touch'):
        def release_input():
            # Neutral injection still overrides GPIO. Clear the diagnostic
            # override before closing, including on a failed measurement.
            try:
                c.request('INPUT CANCEL', timeout=3)
            except pxadb.PxaDbError as error:
                report['input_cleanup_error'] = str(error)
        cleanup.callback(release_input)
    state['version'] = active.split('\t')[2]
    motion = {'right':False,'a':False,'next':0,'began':time.monotonic()}
    def input_async(command):
        # Input ACKs remain in the normal read stream. Waiting for every ACK
        # can stall telemetry collection when USB/log output is backpressured.
        c._write_command(c._next_sequence(), command)
    def move():
        now=time.monotonic()
        if args.motion!='mario' or now<motion['next']:return
        motion['next']=now+.12
        a=((now-motion['began'])%1.05)<.22
        if input_mode == 'controller':
            # PXA controller bits: RIGHT=8, A=16. The board reports these
            # through its normal controller-state path, without touch overlays.
            input_async(f'INPUT CONTROLLER 0 1 {8 | (16 if a else 0)}')
            motion['right']=True;motion['a']=a
        else:
            tx,ty=(a_x,a_y) if a and not multi_touch else (right_x,right_y)
            input_async('INPUT POINTER '+('MOVE' if motion['right'] else 'DOWN')+f' {tx} {ty} 0');motion['right']=True
            if multi_touch and a!=motion['a']:
                input_async('INPUT POINTER '+('DOWN' if a else 'UP')+f' {a_x} {a_y} 1');motion['a']=a
            motion['a']=a
    def tap(stamp=False):
        if args.motion=='mario':move()
        if input_mode == 'controller':
            bits = (8 | (16 if motion['a'] else 0)) if args.motion == 'mario' else 0
            command=f'INPUT CONTROLLER 0 1 {bits}'
        else:
            tx,ty=(a_x,a_y) if motion['a'] and not multi_touch else (right_x,right_y)
            command=f'INPUT POINTER MOVE {tx} {ty} 0' if args.motion=='mario' else f'INPUT TAP {width//2} {100*input_scale}'
        if not stamp:
            input_async(command)
            return 0
        for frame in c.request(command):
            props = pxadb.info_properties(frame.payload)
            if 'device_us' in props:
                return int(props['device_us'])
        return 0
    def pump(until):
        while time.monotonic() < until:
            c.pump_logs()
            move()
            time.sleep(.005)
    report['panel_probe'] = {}
    try:
        report['panel_probe']['mode'] = args.presentation_source
        report['panel_probe']['start'] = [f.payload for f in c.request(
            'PERF START display' if args.presentation_source == 'display' else 'PERF START')]
    except pxadb.PxaDbRejectedError as error:
        report['panel_probe']['display_error'] = str(error)
        report['panel_probe']['mode'] = 'legacy-raster'
        report['panel_probe']['start'] = [f.payload for f in c.request('PERF START')]
    warm_until = time.monotonic()+max(0, args.warmup)
    while time.monotonic() < warm_until:
        tap()
        pump(min(warm_until, time.monotonic()+2))
    state['after'] = tap(stamp=True)
    report['collection_start_us'] = state['after']
    state['collect'] = True
    deadline = time.monotonic()+(args.samples+2)*7
    next_tap = time.monotonic()+2
    next_fps = time.monotonic()+1
    fps_supported = 'perf-fps' in report['device']
    while len(report['samples']) < args.samples and time.monotonic() < deadline:
        c.pump_logs()
        move()
        if time.monotonic() >= next_tap:
            tap()
            next_tap = time.monotonic()+2
        if fps_supported and time.monotonic() >= next_fps:
            try:
                frames = c.request('PERF FPS')
                props = pxadb.info_properties(frames[-1].payload)
                report['panel_fps_samples'].append({'host_monotonic':time.monotonic(), 'fps_x10':int(props['presentation_fps_x10'])})
            except pxadb.PxaDbError as error:
                report['panel_fps_error'] = str(error)
                fps_supported = False
            next_fps = time.monotonic()+1
        time.sleep(.005)
    state['collect'] = False
    if motion['right']:
        if input_mode == 'controller':
            input_async('INPUT CONTROLLER 0 1 0')
        else:
            if multi_touch and motion['a']:input_async(f'INPUT POINTER UP {a_x} {a_y} 1')
            input_async(f'INPUT POINTER UP {right_x} {right_y} 0')
    if len(report['samples']) != args.samples:
        report['error'] = 'Fresh telemetry missing; check unlock, actual game readiness and installed/running versions'
    report['panel_probe']['stop'] = [f.payload for f in c.request('PERF STOP')]
    properties = pxadb.info_properties(';'.join(report['panel_probe']['stop']))
    if report['panel_probe']['mode'] == 'display' and int(properties.get('window_us','0')) > 0:
        report['panel_probe']['completed_fps'] = (
            int(properties['completed'])*1000000/int(properties['window_us']))
        report['panel_probe']['method'] = 'Completed display submissions over the firmware measurement window; not optical panel refresh Hz'
    if report['panel_probe']['mode'] != 'display':
        intervals = []
        count = int(properties.get('present','0'))
        for offset in range(0,count,16):
            records = c.request(f'PERF READ present {offset}')
            for response in records:
                if response.kind == 'OK':
                    block = pxadb.info_properties(response.payload)
                    values = [int(value) for value in block['values'].split(',') if value]
                    if int(block['offset']) != offset or int(block['count']) != len(values):
                        raise ValueError('Invalid presentation interval block')
                    intervals.extend(values)
        report['panel_probe']['interval_us'] = intervals
        report['panel_probe']['interval_truncated'] = int(properties.get('present_overflow','0')) != 0
        if intervals and len(intervals) == count and all(value > 0 for value in intervals):
            report['panel_probe']['completed_fps'] = len(intervals)*1000000/sum(intervals)
            report['panel_probe']['method'] = 'Distinct completed Surface frames over retained intervals; truncated samples cover only the start of the window; excludes repeated panel scanout'
    c.request('PERF CLEAR')
    # Screen capture can stall display/audio and stays outside the measured windows.
    c.unsubscribe_logs(timeout=15)
    for attempt in range(3):
        try:
            capture = pxadb.screenshot_capture(c.request('SCREENSHOT'+(' JPEG' if args.jpeg else ''), timeout=30))
            pxadb.write_screenshot(capture, args.output/('screen.jpg' if args.jpeg else 'screen.png'))
            report['screenshot'] = capture.metadata
            break
        except pxadb.PxaDbError as error:
            if attempt == 2 or not pxadb.screenshot_retryable(error):
                report['screenshot_error'] = str(error)
                break
            time.sleep(.3)
report['passed'] = len(report['samples']) == args.samples and all(
    abs(int(s['sim_x100'])-args.hz*100) <= 100 and int(s['debt_frames']) <= 2 for s in report['samples'])
report['audio_no_drops'] = bool(report['samples']) and all(int(s['audio_drop']) == 0 for s in report['samples'])
if args.mapper is not None:
    report['expected_mapper'] = args.mapper
    report['mapper_matched'] = bool(report['samples']) and all(
        int(s.get('mapper', '-1')) == args.mapper for s in report['samples'])
    if not report['mapper_matched']:
        report['passed'] = False
        report['rom_error'] = 'Unexpected mapper; do not label these samples as the intended game'
schedule_by_time = {s.get('wall_us'):s for s in report['scheduler_samples']}
report['audio_enabled'] = bool(report['samples']) and all(
    schedule_by_time.get(s['wall_us'],{}).get('sound') == '1' for s in report['samples'])
if args.require_audio and not report['audio_enabled']:
    report['passed'] = False
    report['audio_error'] = 'Sound enabled was not confirmed for every measurement window'
(args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
if not report['passed']:
    raise SystemExit('Simulation did not sustain the target; see results.json')
print(f'PASS: simulation remained near {args.hz} Hz without accumulating debt')
