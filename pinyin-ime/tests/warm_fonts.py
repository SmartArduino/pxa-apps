"""Normalize system numeric glyph caches before paired IME measurements.

Requires the test-only pxa-ime-font-warmup package and an idle foreground.
Run after each reboot on both firmware versions, outside heap peak windows.
This does not warm the Chinese dictionary or change the input-method selection.
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
    args = parser.parse_args()
    if args.pxadb_dir is None:
        args.pxadb_dir = next((p / 'tools/pxadb' for p in pathlib.Path(__file__).resolve().parents
                             if (p / 'tools/pxadb/pxadb.py').is_file()), None)
    if args.pxadb_dir is None:
        parser.error('Specify --pxadb-dir')
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    args.output.mkdir(parents=True, exist_ok=True)
    rows, started, passed = [], False, False
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        device = client.hello()

        def request(command):
            frames = client.request(command, timeout=60)
            rows.append({'command': command, 'frames': [
                {'kind': f.kind, 'payload': f.payload} for f in frames
                if f.kind not in ('DATA', 'META') or command != 'SCREENSHOT JPEG']})
            return frames

        def active():
            return any(f.kind == 'PKG' and ':pxa-ime-font-warmup\t' in f.payload
                       and 'active=1' in f.payload for f in request('PACKAGES'))

        try:
            assert not any(f.kind == 'PKG' and 'active=1' in f.payload
                           for f in request('PACKAGES')), 'Stop the foreground App first'
            request('PACKAGE run pxa-ime-font-warmup'); started = True
            for attempt in range(60):
                if active():
                    break
                time.sleep(.1)
            else:
                raise AssertionError('Font warmup fixture did not start')
            time.sleep(2)
            for attempt in range(3):
                try:
                    frames = request('SCREENSHOT JPEG')
                    pxadb.write_screenshot(pxadb.screenshot_capture(frames), args.output/'fonts.jpg')
                    break
                except pxadb.PxaDbError:
                    if attempt == 2:
                        raise
                    time.sleep(.1)
            passed = True
        finally:
            try:
                if started:
                    request('PACKAGE stop pxa-ime-font-warmup')
                    for attempt in range(100):
                        if not active():
                            break
                        time.sleep(.1)
                    else:
                        raise AssertionError('Font warmup fixture did not stop')
            except Exception:
                passed = False
                raise
            finally:
                (args.output/'warmup.json').write_text(json.dumps(
                    {'passed': passed, 'device': device, 'rows': rows,
                     'visual_review_required': True}, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    main()
