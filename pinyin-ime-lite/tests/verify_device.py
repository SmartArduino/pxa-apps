"""Verify literal Chinese input with the mobile keyboard on 296x240 pai-touch.

Requires pxa-pinyin-ime-lite and the shared pxa-ime-heartbeat fixture installed.
Installs/removes no packages. Restores IME selection and stops its fixture; the
foreground must initially be idle. JPEG capture occurs outside heap sampling.
"""
import argparse
import json
from pathlib import Path
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pxadb-dir', type=Path)
    args = parser.parse_args()
    if args.pxadb_dir is None:
        args.pxadb_dir = next((p/'tools/pxadb' for p in Path(__file__).resolve().parents
                              if (p/'tools/pxadb/pxadb.py').is_file()), None)
    if args.pxadb_dir is None:
        parser.error('Specify --pxadb-dir')
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    args.output.mkdir(parents=True, exist_ok=True)
    rows, logs, cleanup_errors = [], [], []
    selected, started, passed = None, False, False
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.device_info = client.hello()
        assert 'serial=a4:cb:8f:d6:8d:a4' in client.device_info.lower()

        def log(frame):
            if frame.kind == 'LOG' and 'IME_HEARTBEAT' not in frame.payload:
                logs.append({'kind': frame.kind, 'payload': frame.payload})
        client.log_callback = log
        client.subscribe_logs()

        def request(command):
            frames = client.request(command, timeout=30)
            rows.append({'command': command,
                         'frames': [{'kind': f.kind, 'payload': f.payload} for f in frames]})
            return frames

        def active():
            return any(f.kind == 'PKG' and ':pxa-ime-heartbeat\t' in f.payload and 'active=1' in f.payload
                       for f in request('PACKAGES'))

        def tap(x, y):
            request(f'INPUT POINTER DOWN {x} {y} 0')
            time.sleep(.12)
            request(f'INPUT POINTER UP {x} {y} 0')
            request('INPUT SYNC')
            time.sleep(.4)  # ACK precedes Guest processing and LCD presentation.

        def capture(name):
            for attempt in range(3):
                try:
                    frames = client.request('SCREENSHOT JPEG', timeout=30)
                    pxadb.write_screenshot(pxadb.screenshot_capture(frames), args.output/(name+'.jpg'))
                    return
                except pxadb.PxaDbError:
                    if attempt == 2:
                        raise
                    time.sleep(.2)

        try:
            capabilities = request('INPUT CAPABILITIES')
            assert any('width=296;height=240;' in f.payload for f in capabilities)
            selected = next(f.payload for f in request('IME') if f.kind == 'SELECTED')
            packages = request('PACKAGES')
            assert not any('active=1' in f.payload for f in packages if f.kind == 'PKG')
            provider = next(f.payload.split('\t', 1)[0] for f in packages
                            if f.kind == 'PKG' and ':pxa-pinyin-ime-lite\t' in f.payload)
            assert any(':pxa-ime-heartbeat\t' in f.payload for f in packages if f.kind == 'PKG')
            request('IME select '+provider)
            request('INPUT KEY HOME')
            request('INPUT POINTER DOWN 148 216 0')
            time.sleep(.12)
            request('INPUT POINTER MOVE 148 70 0')
            time.sleep(.12)
            request('INPUT POINTER UP 148 70 0')
            request('INPUT SYNC')
            request('PACKAGE run pxa-ime-heartbeat')
            started = True
            for attempt in range(60):
                if active():
                    break
                time.sleep(.1)
            else:
                raise AssertionError('Fixture did not start')
            time.sleep(.5)
            tap(35, 55)
            for attempt in range(60):
                timing = next((f.payload for f in request('IME') if f.kind == 'TIMING'), '')
                if 'candidates_ready=1;' in timing:
                    break
                time.sleep(.1)
            else:
                raise AssertionError('Provider did not become ready')
            capture('idle')
            tap(179, 163)  # 6
            tap(50, 163)   # 4 -> mi / ng / ni
            capture('nine-64')
            tap(232, 99)   # ni spelling, not a Chinese commit
            capture('nine-ni')
            tap(45, 119)   # 你
            capture('committed')
            tap(30, 119)   # idle toolbar gear
            capture('tools')
            tap(210, 142)  # 全键
            capture('full-idle')
            tap(161, 163)  # h
            tap(32, 163)   # a
            tap(239, 142)  # o
            capture('full-hao')
            tap(45, 119)   # 好
            capture('hello-committed')
            request('IME')
            tap(265, 184)  # 完成; fixture logs actual submitted TextInput state.
            time.sleep(.5)
            request('PING')
            assert any('level=INFO] 你好' in frame['payload'] for frame in logs), 'Unexpected submitted text'
            passed = True
        finally:
            if started:
                try:
                    request('PACKAGE stop pxa-ime-heartbeat')
                    for attempt in range(60):
                        if not active():
                            break
                        time.sleep(.1)
                    else:
                        raise AssertionError('Fixture did not stop')
                except Exception as error:
                    cleanup_errors.append(str(error))
            if selected is not None:
                try:
                    request('IME select '+(selected or 'builtin'))
                except Exception as error:
                    cleanup_errors.append(str(error))
            (args.output/'input.json').write_text(json.dumps({
                'passed': passed and not cleanup_errors, 'rows': rows, 'logs': logs,
                'selected_restored': selected, 'cleanup_errors': cleanup_errors,
                'visual_review_required': True}, ensure_ascii=False, indent=2)+'\n')
            if cleanup_errors:
                raise RuntimeError('; '.join(cleanup_errors))


if __name__ == '__main__':
    main()
