"""Measure IME loading and foreground callback gaps on 296x240 pai-touch.

Install pxa-ime-heartbeat and pxa-pinyin-ime first. No packages are installed or
removed by this script. Selection, foreground fixture and local heap sampling
are restored/stopped in finally. Guest clock gaps measure callback service,
not game FPS, touch-to-photon latency or LCD completion.
"""
import argparse
import json
import pathlib
import re
import statistics
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--pxadb-dir', type=pathlib.Path)
    parser.add_argument('--phone-layout',action='store_true',help='Mobile keyboard coordinates on 296x240 pai-touch')
    parser.add_argument('--provider', default='pxa-pinyin-ime', help='Installed IME package ID')
    parser.add_argument('--reopens', type=int, default=0, choices=range(9), help='Additional measured Chinese open/close cycles')
    args = parser.parse_args()
    if args.pxadb_dir is None:
        args.pxadb_dir = next((p / 'tools/pxadb' for p in pathlib.Path(__file__).resolve().parents
                             if (p / 'tools/pxadb/pxadb.py').is_file()), None)
    if args.pxadb_dir is None:
        parser.error('Specify --pxadb-dir')
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    args.output.mkdir(parents=True, exist_ok=True)
    rows, logs, phases = [], [], []
    selected, local_memory, started_fixture, passed = None, False, False, False
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.device_info = client.hello()
        assert 'serial=a4:cb:8f:d6:8d:a4' in client.device_info.lower()
        client.log_callback = lambda frame: logs.append({'kind': frame.kind, 'payload': frame.payload})
        def request(command):
            begin = time.monotonic()
            frames = client.request(command, timeout=60)
            rows.append({'command': command, 'elapsed_ms': (time.monotonic()-begin)*1000,
                         'frames': [{'kind': f.kind, 'payload': f.payload} for f in frames]})
            return frames
        def wait(seconds):
            deadline = time.monotonic()+seconds
            while time.monotonic() < deadline:
                request('PING')
                time.sleep(.08)
        def tap(x, y, duration=.12):
            request(f'INPUT POINTER DOWN {x} {y} 0'); time.sleep(duration)
            request(f'INPUT POINTER UP {x} {y} 0'); request('INPUT SYNC'); time.sleep(.08)
        def timing():
            text = next(f.payload for f in request('IME') if f.kind == 'TIMING')
            return dict(pair.split('=', 1) for pair in text.split(';'))
        def phase(name, action=None, heap=False):
            nonlocal local_memory
            if heap:
                request('MEMORY')
                start=request('MEMORY START'); local_memory=True
                assert any(f.kind=='OK' and f.payload=='minimum_scope=local' for f in start)
            begin = len(logs)
            if action: action()
            wait(4)
            state = timing()
            gaps = [int(match.group(1)) for row in logs[begin:]
                    if (match := re.search(r'IME_HEARTBEAT now_us=\d+ gap_us=(\d+)', row['payload']))
                    and int(match.group(1))]
            assert gaps, 'No foreground heartbeat callbacks observed'
            record = {'name': name, 'callbacks': len(gaps), 'median_gap_us': statistics.median(gaps),
                      'p95_gap_us': sorted(gaps)[min(len(gaps)-1, int(len(gaps)*.95))],
                      'max_gap_us': max(gaps), 'timing': state}
            # Read while the local window is still active; STOP's trailing
            # scope describes the next observation, after monitoring ended.
            record['memory'] = [{'kind': f.kind, 'payload': f.payload} for f in request('MEMORY')]
            record['memory_scope']='local' if heap else 'boot'
            if heap:
                request('MEMORY STOP')
                local_memory=False
            phases.append(record)
            print(json.dumps(record, ensure_ascii=False), flush=True)
            return state
        try:
            for attempt in range(60):
                try:
                    capabilities=request('INPUT CAPABILITIES'); break
                except pxadb.PxaDbRejectedError: time.sleep(.5)
            else: raise RuntimeError('Input adapter did not become ready')
            assert any('width=296;height=240;' in f.payload for f in capabilities)
            selected = next(f.payload for f in request('IME') if f.kind == 'SELECTED')
            packages = request('PACKAGES')
            assert any(':pxa-ime-heartbeat\t' in f.payload for f in packages if f.kind == 'PKG')
            ime = next(f.payload.split('\t',1)[0] for f in packages
                       if f.kind == 'PKG' and ':'+args.provider+'\t' in f.payload)
            assert not any('active=1' in f.payload for f in packages if f.kind == 'PKG'), 'Stop the foreground App first'
            request('IME select '+ime)
            request('INPUT KEY HOME'); time.sleep(.2)
            request('INPUT POINTER DOWN 148 216 0'); time.sleep(.12)
            request('INPUT POINTER MOVE 148 70 0'); time.sleep(.12)
            request('INPUT POINTER UP 148 70 0'); request('INPUT SYNC')
            request('PACKAGE run pxa-ime-heartbeat'); started_fixture=True
            for attempt in range(40):
                if any(':pxa-ime-heartbeat\t' in f.payload and 'active=1' in f.payload
                       for f in request('PACKAGES') if f.kind=='PKG'): break
                time.sleep(.2)
            else: raise RuntimeError('Heartbeat fixture failed to launch')
            client.subscribe_logs(); wait(.4)
            phase('closed-pxa-selected')
            state=phase('chinese-load',lambda: tap(35,55),True)
            assert state['keyboard_drawn']=='1' and state['candidates_ready']=='1'
            def english():
                if args.phone_layout: tap(109,209); tap(32,163)
                else: tap(54,209); tap(71,142); tap(55,107)
            phase('english',english,True)
            hide_x=258 if args.phone_layout else 240
            phase('closed-after-use',lambda: tap(hide_x,209))
            for cycle in range(args.reopens):
                state=phase(f'chinese-reopen-{cycle}',lambda: tap(35,55),True)
                assert state['candidates_ready']=='1'
                phase(f'closed-reopen-{cycle}',lambda: tap(hide_x,209))
            def cancel():
                tap(35,55,.06); tap(hide_x,209,.06)
            state=phase('cancel-close',cancel,True)
            assert state['keyboard_drawn']=='0'
            request('IME select builtin')
            phase('closed-builtin-selected')
            passed=True
        finally:
            cleanup_errors=[]
            for command, needed in [('MEMORY STOP',local_memory),
                                    ('PACKAGE stop pxa-ime-heartbeat',started_fixture),
                                    ('IME select '+(selected or 'builtin'),selected is not None)]:
                if needed:
                    try: request(command)
                    except Exception as error: cleanup_errors.append(str(error))
            if cleanup_errors: passed=False
            document={'passed':passed,'provider':args.provider,'device':client.device_info,
                      'phases':phases,'rows':rows,'logs':logs,'cleanup_errors':cleanup_errors}
            (args.output/'measurement.json').write_text(json.dumps(document,ensure_ascii=False,indent=2)+'\n')
            if cleanup_errors: raise RuntimeError('; '.join(cleanup_errors))

if __name__=='__main__':
    main()
