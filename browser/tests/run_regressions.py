#!/usr/bin/env python3
"""Run real PXA simulator regressions with isolated workers and progress files."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
from threading import Thread
from fixture import Handler, ThreadingHTTPServer

SUITES = [
    ('classes', 'class_list_smoke.py', []), ('classes-compact', 'class_list_smoke.py', ['--width','296','--height','240']),
    ('fragments', 'fragment_smoke.py', []), ('fragments-compact', 'fragment_smoke.py', ['--width','296','--height','240']),
    ('text-modes', 'text_mode_smoke.py', []), ('text-modes-compact', 'text_mode_smoke.py', ['--width','296','--height','240']),
    ('selectors', 'selector_smoke.py', []), ('selectors-compact', 'selector_smoke.py', ['--width','296','--height','240']),
    ('forms', 'form_smoke.py', []), ('forms-compact', 'form_smoke.py', ['--width','296','--height','240']),
    ('events', 'event_smoke.py', []), ('events-compact', 'event_smoke.py', ['--width','296','--height','240']),
    ('style', 'style_smoke.py', []), ('style-compact', 'style_smoke.py', ['--width','296','--height','240']),
    ('responsive', 'responsive_smoke.py', []), ('responsive-compact', 'responsive_smoke.py', ['--width','296','--height','240']),
    ('css', 'css_smoke.py', []), ('css-landscape', 'css_smoke.py', ['--width','480','--height','320']),
    ('compat', 'compat_smoke.py', []), ('comic', 'comic_smoke.py', []), ('find', 'find_smoke.py', []),
    ('image-failures', 'image_failure_smoke.py', []),
    ('image-failures-compact', 'image_failure_smoke.py', ['--width','296','--height','240']),
    ('preview-recovery', 'preview_recovery_smoke.py', []),
    ('preview-recovery-compact', 'preview_recovery_smoke.py', ['--width','296','--height','240']),
    ('preview-foreground', 'preview_foreground_smoke.py', []),
    ('comic-live', 'comic_live_smoke.py', []),
]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--suite',action='append',choices=[s[0] for s in SUITES],help='Run only these suites; may be repeated')
    p.add_argument('--live',action='store_true',help='Include opt-in checks against the original author websites')
    a=p.parse_args()
    if a.suite and 'comic-live' in a.suite and not a.live:p.error('comic-live requires --live')
    selected=[s for s in SUITES if (not a.suite or s[0] in a.suite) and (a.live or s[0]!='comic-live')]
    a.output.mkdir(parents=True,exist_ok=True);rows=[];active=None;stopping=0
    results=a.output/'suite-results.json'
    results.write_text('[]\n')
    common=['--simulator',str(a.simulator.resolve()),'--package',str(a.package.resolve()),
            '--publisher-key',str(a.publisher_key.resolve()),'--ime-package',str(a.ime_package.resolve())]
    class QuietHandler(Handler):
        def log_message(self,*_):pass
    server=ThreadingHTTPServer(('127.0.0.1',31878),QuietHandler)
    thread=Thread(target=server.serve_forever,daemon=True);thread.start()
    def stop(signum,_frame):
        nonlocal stopping
        if stopping:return
        stopping=signum
        if active is not None and active.poll() is None:
            # Python workers unwind their finally blocks and close their own
            # simulator sessions. Never signal another suite's process group.
            try:os.killpg(active.pid,signal.SIGINT)
            except ProcessLookupError:pass
    old={sig:signal.signal(sig,stop) for sig in (signal.SIGINT,signal.SIGTERM)}
    try:
        for name,program,extra in selected:
            if stopping:break
            with (a.output/(name+'.log')).open('w') as log:
                active=subprocess.Popen([sys.executable,str(Path(__file__).parent/program),*common,
                    '--output',str((a.output/name).resolve()),*extra],stdout=log,stderr=subprocess.STDOUT,
                    start_new_session=True)
                code=active.wait();active=None
            rows.append({'name':name,'exit_code':code,'interrupted':bool(stopping)})
            results.write_text(json.dumps(rows,indent=2)+'\n')
            print(name+': '+('PASS' if code==0 and not stopping else 'FAILED'),flush=True)
            if code or stopping:break
        return 128+stopping if stopping else (0 if len(rows)==len(selected) and all(r['exit_code']==0 for r in rows) else 1)
    finally:
        server.shutdown();server.server_close();thread.join()
        for sig,handler in old.items():signal.signal(sig,handler)


if __name__=='__main__':raise SystemExit(main())
