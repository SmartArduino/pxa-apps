"""Isolated pai-touch acceptance via real UI and one PXADB connection.

Install a fresh pxa-novel-reader-qa built with the LAN fixture source first.
Only reads private files: PXADB deliberately disallows writing application data.
MEMORY START/STOP reports this run's real heap minimum, separate from boot history.
"""
import argparse, importlib.util, json, sys, time
from pathlib import Path
from PIL import Image

root = next(p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').is_file())
sys.path.insert(0, str(root/'tools/pxadb'))
spec = importlib.util.spec_from_file_location('reader_pxadb', root/'tools/pxadb/pxadb.py')
db = importlib.util.module_from_spec(spec); sys.modules[spec.name] = db; spec.loader.exec_module(db)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='/dev/ttyACM0')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--reset', action='store_true', help='Clear only the isolated QA app data before testing')
p.add_argument('--cached-only', action='store_true', help='Repeat offline checks against an already verified QA cache')
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
identity='pxa-novel-reader-qa'
private='pxa-state/data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171~'+identity
until=time.monotonic()+55
while True:
    try: client=db.open_client(a.port,15); break
    except Exception as e:
        if 'exclusively lock' not in str(e) or time.monotonic()>=until: raise
        time.sleep(.2)
fs=db.NormalFsClient(client); monitor=False; results={}
log=(a.output/'host.log').open('w')
client.log_callback=lambda frame: log.write(frame.payload+'\n')
client.raw_callback=lambda raw: log.write(raw.decode(errors='replace'))
def request(value, timeout=30): return client.request(value,timeout=timeout)
def ensure_active():
    active=[f.payload for f in request('PACKAGES') if f.kind=='PKG' and 'active=1' in f.payload]
    assert len(active)==1 and active[0].split('\t')[0].endswith(':'+identity), \
        'Device foreground was changed by another test: '+str(active)
def wait_active():
    end=time.monotonic()+10
    while time.monotonic()<end:
        active=[f.payload for f in request('PACKAGES') if f.kind=='PKG' and 'active=1' in f.payload]
        if active:
            ensure_active(); return
        time.sleep(.05)
    raise AssertionError('QA launch did not become active')
def stop():
    try: request('PACKAGE stop '+identity)
    except db.PxaDbError as e:
        if 'package_not_running' not in str(e): raise
def memory(command='MEMORY'):
    return {row['scope']:{k:int(v) for k,v in row.items() if k not in ('scope','name')}
            for frame in request(command) if frame.kind=='DATA'
            for row in [db.info_properties(frame.payload)] if row['scope'] in ('heap','budget','surface')}
def tap(x,y,delay=1.0):
    ensure_active()
    request(f'INPUT POINTER DOWN {x} {y} 0'); time.sleep(.3)
    request(f'INPUT POINTER UP {x} {y} 0'); time.sleep(delay)
def capture(name):
    for attempt in range(3):
        try:
            db.write_screenshot(db.screenshot_capture(request('SCREENSHOT JPEG AFTER_PRESENT')),
                                a.output/(name+'.jpg')); return
        except db.PxaDbError as e:
            limited='screenshot_rate_limited' in str(e)
            if attempt==2 or (not limited and not db.screenshot_retryable(e)): raise
            time.sleep(1.2 if limited else .2)
def start_window():
    global monitor
    request('MEMORY START'); monitor=True; return memory()
def end_window():
    global monitor
    value=memory('MEMORY STOP'); monitor=False; return value
def state(name):
    file=a.output/(name+'.json'); fs.get(private+'/reader-state.json',file)
    return json.loads(file.read_text())
def wake_unlock():
    request('INPUT TAP 148 25'); time.sleep(.8)
    request('INPUT SWIPE 148 220 148 40 400 20'); time.sleep(.8)
    request('INPUT CANCEL'); request('INPUT SYNC')
def cached_restarts():
    for repeat in range(3):
        wake_unlock()
        request('PACKAGE run '+identity); wait_active(); time.sleep(5)
        before=state(f'state-restart-{repeat}-before')['shelf'][1]['position']
        for attempt in range(3):
            tap(148,115,1)
            capture(f'offline-restart-{repeat}')
            with Image.open(a.output/f'offline-restart-{repeat}.jpg') as shot:
                frame=shot.convert('RGB')
                r,g,b=frame.getpixel((62,200))
                def ink(box):
                    pixels=frame.crop(box).tobytes()
                    return sum(max(pixels[i:i+3])<120 for i in range(0,len(pixels),3))
                if r>180 and ink((10,110,286,185))>200 and ink((25,200,105,225))>40: break
                assert r<90 and g>90 and b<160, 'Unexpected UI instead of reading or shelf'
        else: raise AssertionError('Cached book did not open')
        tap(246,211,1)
        after=state(f'state-restart-{repeat}-after')['shelf'][1]['position']
        assert after>before, 'Cached reading did not advance and persist its position'
        results[f'restart_{repeat}']=memory()
        capture(f'offline-restart-{repeat}')
        stop(); time.sleep(1)
    results['restarts']=3; results['after_stop']=memory()
try:
    try: request('MEMORY STOP')
    except db.PxaDbError as e:
        if 'not_monitoring' not in str(e): raise
    client.subscribe_logs()
    if a.cached_only:
        saved=state('state-cached')
        assert saved['shelf'][1]['complete'] and saved['shelf'][1]['total']==258777
        cached_restarts()
        print('Physical reader: 3 cached restarts OK',flush=True)
        raise SystemExit(0)
    # Waking the panel intentionally consumes one contact; unlock separately.
    wake_unlock()
    stop(); time.sleep(.7)
    if a.reset: request('PACKAGE clear-data '+identity)
    results['idle']=memory()
    results['initialization_start']=start_window()
    request('PACKAGE run '+identity); time.sleep(5)
    results['shelf']=memory(); tap(148,84,1.2)
    results['reading']=memory()
    results['initialization_end']=end_window(); capture('reading')
    results['steady_start']=start_window()
    tap(148,211); tap(78,146)  # Reading menu -> settings.
    for _ in range(10): tap(218,84,.8)
    assert state('state-font-largest')['settings']['font']==28
    for _ in range(10): tap(78,84,.8)
    assert state('state-font-smallest')['settings']['font']==12
    for _ in range(2): tap(218,84,.8)  # Restore 16 logical px.
    results['font_cycle']=memory()
    tap(148,211); tap(148,211); tap(218,146); tap(146,211)
    tap(100,84,.7); results['ime']=memory()
    results['steady_end']=end_window(); capture('system-ime')
    tap(251,182,.5); capture('ime-closed')
    saved=state('state-font'); assert saved['settings']['font']==16
    # Empty search in the fresh QA catalog returns the CC0 fixtures.
    tap(265,84,.7)
    capture('search-permission')
    # The default QA paper is light. A dark pixel here belongs to the modal,
    # so do not press the next-results button when permission is already granted.
    with Image.open(a.output/'search-permission.jpg') as shot:
        r,g,b=shot.convert('RGB').getpixel((148,40))
    if max(r,g,b)<120: tap(199,184,1.0)
    capture('search-results')
    results['download_start']=start_window()
    tap(148,146,.8)
    end=time.monotonic()+120; cache=None; full_part_at=None
    while time.monotonic()<end:
        files=fs.list(private)
        cache=next((name for kind,size,name in files
                    if kind=='F' and size==258777 and name.endswith('.txt')),None)
        if cache: break
        if any(size==258777 and name.endswith('.part') for _,size,name in files):
            full_part_at=full_part_at or time.monotonic()
            if time.monotonic()-full_part_at>8: break
        time.sleep(.4)
    if not cache:
        results['failed_download_end']=end_window(); capture('failed-publication')
    assert cache, 'No complete 258777-byte fixture cache was published'
    time.sleep(.8); results['download_end']=end_window(); capture('downloaded')
    frames=request('FSSHA '+db.client_encode_path(private+'/'+cache))
    digest=db.info_properties(frames[-1].payload)['sha256']
    assert digest=='f1401c49bf6a399717c05f9dbcd86ec63019f66e3e030f2a9a505078a574d7d9'
    results['download_sha256']=digest
    saved=state('state-downloaded')
    assert saved['shelf'][1]['complete'] and saved['shelf'][1]['total']==258777
    # Stop/start fast before state I/O finishes; the persisted shelf must survive.
    stop(); time.sleep(.5)
    for _ in range(3):
        request('PACKAGE run '+identity); wait_active(); stop()
    assert state('state-after-fast-stop')['shelf']==saved['shelf']
    cached_restarts()
    print('Physical reader: fonts, system IME, exact download hash, startup persistence and 3 cached restarts OK',flush=True)
finally:
    if monitor:
        try: request('MEMORY STOP')
        except Exception: pass
    (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    try: stop()
    finally: client.close(); log.close()
