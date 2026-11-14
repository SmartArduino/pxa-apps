#!/usr/bin/env python3
"""Paired polling/projection counts in the real PXA simulator, identical UI AOT."""
import argparse,hashlib,json,subprocess,sys
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from threading import Thread


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','before-package','after-package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();packages={'before':a.before_package,'after':a.after_package};hashes=[]
    for package in packages.values():
        ui=list((package/'artifacts').glob('main.*.aot'));assert len(ui)==1;hashes.append(hashlib.sha256(ui[0].read_bytes()).hexdigest())
    assert len(set(hashes))==1,'UI AOT must match'
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_GET(self):
            data=b'<p>Ready</p>';self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    server=ThreadingHTTPServer(('127.0.0.1',31878),Handler);thread=Thread(target=server.serve_forever,daemon=True);thread.start();a.output.mkdir(parents=True,exist_ok=True);rows=[]
    try:
        for round in range(2):
            for label in (['before','after'] if round==0 else ['after','before']):
                folder=a.output/(label+'-'+str(round));args=[sys.executable,str(Path(__file__).with_name('snapshot_smoke.py'))]
                for key in ('simulator','publisher_key','ime_package'):args+=['--'+key.replace('_','-'),str(getattr(a,key))]
                args+=['--package',str(packages[label]),'--output',str(folder),'--measure','--case','noop','--case','detached']
                with (a.output/(label+'-'+str(round)+'.log')).open('w') as log:subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True)
                for row in json.loads((folder/'results.json').read_text())['flows']:rows.append(dict(round=round,package=label,**row));print(json.dumps(rows[-1]),flush=True)
        for round in range(2):
            for case in ('noop','detached'):
                data={r['package']:r for r in rows if r['round']==round and r['flow']==case};assert data['before']['poll_requests_at_first_update_observation']==48,data
                assert data['after']['poll_requests_at_first_update_observation']<48,data
                assert data['after']['batches_to_first_update']<data['before']['batches_to_first_update'],data
        (a.output/'results.json').write_text(json.dumps(dict(ui_aot_sha256=hashes[0],results=rows,note='Actual interpreted Wasm snapshot work and polling progress; not device/FPS measurements.'),indent=2)+'\n')
    finally:server.shutdown();server.server_close();thread.join()


if __name__=='__main__':main()
