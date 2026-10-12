"""Exercise simultaneous spelling/word rails and swipes, repeated unloads and app changes on 296x240 pai-touch.

Install pxa-pinyin-ime, pxa-ime-heartbeat and pxa-cpp-system-input first. This
script installs/removes no packages, requires an idle foreground, restores the
input-method selection and stops its fixtures. Screenshots need visual review;
passing the assertions alone does not prove their visual correctness.
"""
import argparse
import json
import pathlib
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--pxadb-dir', type=pathlib.Path)
    parser.add_argument('--phone-layout',action='store_true',help='Mobile keyboard coordinates on 296x240 pai-touch')
    args = parser.parse_args()
    if args.pxadb_dir is None:
        args.pxadb_dir = next((p / 'tools/pxadb' for p in pathlib.Path(__file__).resolve().parents
                             if (p / 'tools/pxadb/pxadb.py').is_file()), None)
    if args.pxadb_dir is None:
        parser.error('Specify --pxadb-dir')
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    args.output.mkdir(parents=True, exist_ok=True)
    rows, cycles, checks, cleanup_errors = [], [], [], []
    selected, local_memory, passed = None, False, False
    fixtures = set()
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.device_info = client.hello()
        assert 'serial=a4:cb:8f:d6:8d:a4' in client.device_info.lower()

        def request(command):
            frames = client.request(command, timeout=60)
            rows.append({'command': command,
                         'frames': [{'kind': f.kind, 'payload': f.payload} for f in frames]})
            return frames

        def timing():
            # A read-only query may miss a telemetry frame under serial load.
            # Retry this observation, never the preceding input or app command.
            for attempt in range(5):
                text = next((f.payload for f in request('IME') if f.kind == 'TIMING'), None)
                if text:
                    return dict(pair.split('=', 1) for pair in text.split(';'))
                time.sleep(.15)
            raise AssertionError('Missing IME timing responses')

        def wait_timing(field, value):
            for attempt in range(100):
                state = timing()
                if state[field] == value:
                    return state
                time.sleep(.08)
            raise AssertionError(f'{field} did not become {value}: {state}')

        def tap(x, y):
            request(f'INPUT POINTER DOWN {x} {y} 0'); time.sleep(.12)
            request(f'INPUT POINTER UP {x} {y} 0'); request('INPUT SYNC'); time.sleep(.08)

        def swipe(x, y, end_y, end_x=None):
            request(f'INPUT POINTER DOWN {x} {y} 0'); time.sleep(.12)
            request(f'INPUT POINTER MOVE {x if end_x is None else end_x} {end_y} 0'); time.sleep(.12)
            request(f'INPUT POINTER UP {x if end_x is None else end_x} {end_y} 0'); request('INPUT SYNC'); time.sleep(.15)

        def active(app):
            return any(f.kind == 'PKG' and ':'+app+'\t' in f.payload and 'active=1' in f.payload
                       for f in request('PACKAGES'))

        def run(app):
            request('PACKAGE run '+app); fixtures.add(app)
            for attempt in range(60):
                if active(app):
                    time.sleep(.3)
                    return
                time.sleep(.1)
            raise AssertionError(app+' failed to start')

        def stop(app):
            begin = time.monotonic()
            request('PACKAGE stop '+app)
            # ACK only means the runtime-owner command was queued. A bounded
            # synchronous load step can still be finishing; observe completion.
            for attempt in range(100):
                if not active(app):
                    rows.append({'stopped': app, 'observed_stop_ms': (time.monotonic()-begin)*1000})
                    return
                time.sleep(.1)
            raise AssertionError(app+' did not stop')

        def screenshot(name):
            assert not local_memory, 'Screenshot would contaminate the heap window'
            for attempt in range(5):
                try:
                    frames = request('SCREENSHOT JPEG')
                    pxadb.write_screenshot(pxadb.screenshot_capture(frames), args.output/(name+'.jpg'))
                    return
                except pxadb.PxaDbError:
                    if attempt == 4: raise
                    time.sleep(1.1)

        try:
            capabilities = request('INPUT CAPABILITIES')
            assert any('width=296;height=240;' in f.payload for f in capabilities)
            packages = request('PACKAGES')
            assert not any(f.kind == 'PKG' and 'active=1' in f.payload for f in packages)
            selected = next(f.payload for f in request('IME') if f.kind == 'SELECTED')
            provider = next(f.payload.split('\t', 1)[0] for f in packages
                            if f.kind == 'PKG' and ':pxa-pinyin-ime\t' in f.payload)
            request('IME select '+provider)
            request('INPUT KEY HOME'); time.sleep(.2)
            swipe(148, 216, 70)
            run('pxa-ime-heartbeat')
            tap(35, 55); wait_timing('candidates_ready', '1')
            tap(179 if args.phone_layout else 218,163); tap(50 if args.phone_layout else 69,163)  # 64: mi / ng / ni
            screenshot('words-before-swipe')
            swipe(230, 99, 99, 120); screenshot('spellings-after-swipe')
            swipe(120, 99, 99, 230)
            swipe(230, 119, 119, 90); screenshot('words-next-page')
            swipe(90, 119, 119, 230)
            before_choice = int(timing()['count'])
            tap(232, 99)
            # DOWN/UP ACK only acknowledges delivery. Wait for the Guest's
            # PRESENT before capturing the automatic return to Chinese words.
            wait_timing('count', str(before_choice + 1)); time.sleep(.2)
            screenshot('words-after-pinyin-choice')
            tap(45, 119)
            wait_timing('count', str(before_choice + 2)); time.sleep(.2)
            screenshot('committed')
            checks.append('swipe-and-choice-sequence; screenshots require visual review')
            hide_x=258 if args.phone_layout else 240
            tap(hide_x,209); wait_timing('keyboard_drawn','0')

            request('MEMORY START'); local_memory = True
            for cycle in range(8):
                tap(35, 55); state = wait_timing('candidates_ready', '1')
                tap(hide_x,209); wait_timing('keyboard_drawn','0')
                memory = [{'kind': f.kind, 'payload': f.payload} for f in request('MEMORY')]
                cycles.append({'cycle': cycle, 'ready': state, 'closed_memory': memory})
                print('cycle', cycle, 'ready_ms', state['candidates_ready_ms'], flush=True)
            peak = [{'kind': f.kind, 'payload': f.payload} for f in request('MEMORY STOP')]
            local_memory = False
            checks.append('eight-ready-close-cycles')

            # Switch the foreground while the verification lease is held.
            tap(35, 55)
            assert timing()['candidates_ready'] == '0'
            run('pxa-cpp-system-input')
            assert timing()['keyboard_drawn'] == '0' and not active('pxa-ime-heartbeat')
            stop('pxa-cpp-system-input')
            checks.append('foreground-switch-during-load')

            # Stopping during verification must release the same lease.
            run('pxa-ime-heartbeat'); tap(35, 55)
            assert timing()['candidates_ready'] == '0'
            stop('pxa-ime-heartbeat')
            run('pxa-cpp-system-input'); stop('pxa-cpp-system-input')
            checks.append('stop-during-load-and-restart')
            passed = True
        finally:
            if local_memory:
                try: request('MEMORY STOP')
                except Exception as error: cleanup_errors.append(str(error))
            for app in sorted(fixtures):
                try:
                    if active(app): stop(app)
                except Exception as error: cleanup_errors.append(str(error))
            if selected is not None:
                try: request('IME select '+(selected or 'builtin'))
                except Exception as error: cleanup_errors.append(str(error))
            document = {'passed': passed and not cleanup_errors, 'checks': checks,
                        'visual_review_required': True, 'cycles': cycles,
                        'peak': locals().get('peak', []), 'rows': rows,
                        'cleanup_errors': cleanup_errors}
            (args.output/'verification.json').write_text(
                json.dumps(document, ensure_ascii=False, indent=2)+'\n')
            if cleanup_errors: raise RuntimeError('; '.join(cleanup_errors))


if __name__ == '__main__':
    main()
