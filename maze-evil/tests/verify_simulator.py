#!/usr/bin/env python3
"""Exercise the signed Maze Evil through real SDL pointer events.

Captures require visual review: startup, movement, turning, firing and release,
plus independent LVGL contacts for simultaneous walking and looking.
This is a functional smoke test, not a device latency measurement.
"""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--publisher-key', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='maze-sdl-') as temporary:
        control = Path(temporary)/'control.sock'
        with (args.output/'host.log').open('w') as log:
            process = subprocess.Popen([str(args.simulator.resolve()),
                '--package', str(args.package.resolve()),
                '--publisher-key', str(args.publisher_key.resolve()),
                '--state-root', temporary, '--width', '296', '--height', '240',
                '--pxadb-control-socket', str(control)], stdout=log, stderr=log,
                env={**os.environ, 'SDL_VIDEODRIVER': 'dummy', 'SDL_AUDIODRIVER': 'dummy'})
            try:
                for attempt in range(100):
                    if process.poll() is not None:
                        raise RuntimeError('Simulator exited; see host.log')
                    if control.exists():
                        break
                    time.sleep(.05)
                else:
                    raise RuntimeError('Simulator control did not start')

                def request(command):
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(5)
                        client.connect(str(control))
                        client.sendall((command+'\n').encode())
                        chunks = []
                        while chunk := client.recv(65536):
                            chunks.append(chunk)
                        return b''.join(chunks)

                def pointer(phase, x, y):
                    if request(f'POINTER {phase} {x} {y}') != b'OK\n':
                        raise RuntimeError('Pointer injection rejected')
                    time.sleep(.08)

                def shot(name):
                    response = request('SCREENSHOT AFTER_PRESENT')
                    header, payload = response.split(b'\n', 1)
                    if header != f'PNG {len(payload)}'.encode():
                        raise RuntimeError('Truncated screenshot')
                    (args.output/(name+'.png')).write_bytes(payload)

                def touch(phase, identity, x, y):
                    if request(f'TOUCH {phase} {identity} {x} {y}') != b'OK\n':
                        raise RuntimeError('Independent touch injection rejected')
                    time.sleep(.08)

                time.sleep(.5)
                shot('title')
                pointer('DOWN', 148, 120); pointer('UP', 148, 120)
                time.sleep(.3); shot('started')
                pointer('DOWN', 75, 150); pointer('MOVE', 75, 110)
                time.sleep(.45); shot('walking')
                pointer('UP', 75, 110)
                pointer('DOWN', 225, 115)
                for x in range(225, 270, 6):
                    pointer('MOVE', x, 115)
                pointer('UP', 267, 115)
                time.sleep(.2); shot('turned')
                pointer('DOWN', 220, 120); pointer('UP', 220, 120)
                time.sleep(.12); shot('fired')
                time.sleep(.7); shot('released')

                # TOUCH goes through two independent LVGL pointer devices, not
                # two sequences sharing SDL's one mouse. Reject missing DOWN,
                # invalid contacts and a duplicate DOWN without altering owners.
                assert request('TOUCH MOVE 1 220 120') == b'ERR invalid_touch\n'
                assert request('TOUCH DOWN 2 220 120') == b'ERR invalid_touch\n'
                touch('DOWN', 0, 75, 150)
                touch('MOVE', 0, 75, 110)
                shot('two-finger-walk')
                touch('DOWN', 1, 225, 115) # Create second indev while first held.
                assert request('TOUCH DOWN 0 220 120') == b'ERR invalid_touch\n'
                for index in range(6):
                    touch('MOVE', 0, 75 + index, 110)
                    touch('MOVE', 1, 225 + index * 6, 115)
                shot('two-finger-walk-look')
                touch('UP', 1, 260, 115)
                time.sleep(.2); shot('two-finger-look-released')
                touch('DOWN', 1, 220, 120)
                touch('CANCEL', 1, 220, 120) # Must not turn a cancelled tap into fire.
                shot('two-finger-look-cancelled')
                touch('DOWN', 1, 220, 120)
                touch('UP', 1, 220, 120) # Fire while walking remains held.
                time.sleep(.1); shot('two-finger-walk-fire')
                touch('UP', 0, 80, 110)
                time.sleep(.2); shot('two-finger-all-released')
                touch('DOWN', 0, 75, 150)
                touch('DOWN', 1, 220, 120)
                assert request('TOUCH CANCEL_ALL') == b'OK\n'
                assert request('TOUCH MOVE 0 75 100') == b'ERR invalid_touch\n'
                assert request('TOUCH MOVE 1 230 120') == b'ERR invalid_touch\n'
                if process.poll() is not None:
                    raise RuntimeError('Simulator exited during gameplay')
                print('SDL + dual-contact gameplay smoke passed; visually review twelve captures')
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()


if __name__ == '__main__':
    main()
