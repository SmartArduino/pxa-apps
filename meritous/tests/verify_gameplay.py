#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise a signed Meritous package in an isolated real PXA simulator.

The SDK's default input-command timeout is intentional: a blocking New Game
callback must fail this regression, even if generation eventually completes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import signal
import struct
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--kit', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', default='espp4-a')
    args = parser.parse_args()
    sdk, kit, package = args.sdk.resolve(), args.kit.resolve(), args.package.resolve()
    assert package.is_file() and (sdk / 'bin/pxa').is_file()
    assert (kit / 'pxa_agent/prepared_run.py').is_file()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    result = {'passed': False, 'package': str(package), 'sha256': digest,
              'profile': args.profile, 'steps': [], 'physical_device_tested': False,
              'store_screenshots': True, 'input_command_timeout': 'SDK default',
              'started': time.time()}
    log = out / 'run.log'
    process = None
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', UV_OFFLINE='true',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', PXA_SIM_CAMERA_DEVICE='off')
    env.pop('PXA_SIMULATOR_PERF', None)

    def save():
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')

    def text():
        return log.read_text(errors='replace') if log.exists() else ''

    def check():
        failures = [line for line in text().splitlines() if any(token in line for token in
                    ('PXA Guest exception', 'callback failed', 'start status=-', 'Meritous fatal exit='))]
        if failures:
            raise RuntimeError('Actual Guest failed: ' + '; '.join(failures))
        if process.poll() is not None:
            raise RuntimeError('Owned simulator exited before gameplay assertions')

    def wait(predicate, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            check()
            value = predicate()
            if value:
                return value
            time.sleep(.15)
        raise RuntimeError('Timed out waiting for actual Meritous engine state')

    def states():
        return [{key: int(value) for key, value in re.findall(
            r'(t|x|y|room|circuit|release|running|paused|exit|enemies|killed)=(-?\d+)', line)}
            for line in text().splitlines() if 'Meritous t=' in line]

    def command(label, arguments):
        check()
        # Do not add --timeout: the normal CLI/Host responsiveness is tested.
        invocation = [str(sdk / 'bin/pxa'), 'device', *map(str, arguments), '--simulator', selector]
        completed = subprocess.run(invocation, env=env, capture_output=True, text=True, timeout=35)
        (out / (label + '.log')).write_text(completed.stdout + completed.stderr)
        result['steps'].append({'label': label, 'argv': invocation, 'exit_code': completed.returncode})
        save()
        if completed.returncode:
            raise RuntimeError(label + ' failed: ' + (completed.stdout + completed.stderr)[-1600:])
        return completed.stdout

    def point(x, y):
        width, height = struct.unpack('>II', (out / 'menu.png').read_bytes()[16:24])
        view_width, view_height = width, width * 528 // 640
        if view_height > height:
            view_width, view_height = height * 640 // 528, height
        return ((width - view_width) // 2 + x * view_width // 640,
                (height - view_height) // 2 + y * view_height // 528)

    def active_identity(packages):
        return [line.split('\t', 1)[0] for line in packages.splitlines()
                if line.split('\t', 1)[0].rsplit(':', 1)[-1] == 'pxa-meritous'
                and re.search(r'(?:^|[;\t])active=1(?:$|[;\t])', line)]

    try:
        with tempfile.TemporaryDirectory(prefix='mt-play-') as sockets:
            env['PXA_SIMULATOR_SOCKET_ROOT'] = sockets
            invocation = [str(kit / '.venv/bin/python'), str(kit / 'pxa_agent/prepared_run.py'),
                          '--sdk', str(sdk), '--package', str(package), '--sha256', digest,
                          '--app-id', 'pxa-meritous', '--profile', args.profile,
                          '--state', str(out / 'state'), '--duration-ms', '180000', '--store-screenshots']
            with log.open('w') as stream:
                process = subprocess.Popen(invocation, env=env, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=True)
                result.update(pid=process.pid, argv=invocation,
                              start_ticks=Path(f'/proc/{process.pid}/stat').read_text().split(') ', 1)[1].split()[19])
                save()
                selector = wait(lambda: (re.findall(r'PXADB: pxa device info --simulator (\S+)', text()) or [None])[-1], 30)
                # INFO is read-only and may precede bridge readiness. Never retry input.
                end = time.monotonic() + 30
                while True:
                    check()
                    probe = subprocess.run([str(sdk / 'bin/pxa'), 'device', 'info', '--simulator', selector],
                                           env=env, capture_output=True, text=True, timeout=30)
                    if probe.returncode == 0:
                        break
                    assert time.monotonic() < end, 'Bridge INFO startup timeout'
                    time.sleep(.2)
                (out / 'info.log').write_text(probe.stdout + probe.stderr)
                identities = active_identity(command('packages-before', ['package', 'list']))
                assert len(identities) == 1, 'Requested Guest must be uniquely active'
                wait(lambda: states() and states()[-1]['t'] > 7000, 75)
                command('menu', ['screenshot', out / 'menu.png'])
                x, y = point(352, 504)
                command('start-new-game', ['input', 'swipe', x, y, x, y, '--duration-ms', 400])
                initial = wait(lambda: next((state for state in reversed(states()) if state['x'] > 100
                    and state['y'] > 100 and state['enemies'] > 0 and state['running'] == 1), None), 90)
                result['new_game_state'] = initial
                command('new-game', ['screenshot', out / 'new-game.png'])
                x, y = point(224, 504)
                command('move-right', ['input', 'swipe', x, y, x, y, '--duration-ms', 1200])
                moved = wait(lambda: next((state for state in reversed(states()) if state['t'] > initial['t']
                    and (state['x'], state['y']) != (initial['x'], initial['y'])), None), 75)
                assert moved['running'] == 1 and moved['exit'] == 0
                result['moved_state'] = moved
                command('after-input', ['screenshot', out / 'after-input.png'])
                assert active_identity(command('packages-after', ['package', 'list'])) == identities
                check()
                result['passed'] = True
            result['cleanup'] = 'SIGINT only to this owned prepared-run process group after assertions'
            os.killpg(process.pid, signal.SIGINT)
            result['shell_exit_code'] = process.wait(timeout=20)
    except BaseException as error:
        result['error'] = repr(error)
        result['passed'] = False
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=12)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    finally:
        result['finished'] = time.time()
        result['package_unchanged'] = hashlib.sha256(package.read_bytes()).hexdigest() == digest
        result['passed'] = result['passed'] and result['package_unchanged']
        save()
    print(json.dumps({'passed': result['passed'], 'result': str(out / 'result.json')}))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
