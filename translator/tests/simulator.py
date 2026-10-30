#!/usr/bin/env python3
"""Exercise signed packages through the real PXA UI, HTTP, capture and audio."""
import argparse
from array import array
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import io,json,os,re,socket,struct,subprocess,tempfile,threading,time,zlib
from pathlib import Path
from PIL import Image

class Simulator:
    def __init__(self,args,label,width=480,height=320,state=None,audio='dummy',extra=()):
        self.args=args;self.root=args.output/label;self.root.mkdir(parents=True,exist_ok=True)
        self.state=state or self.root/'state';self.state.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='pxa-translate-');self.sock=Path(self.temp.name)/'control.sock'
        self.log_path=self.root/'simulator.log';self.pcm=self.root/'mixed.s16';self.size=(width,height);self.audio=audio;self.extra=extra
    def logs(self):return self.log_path.read_text(errors='replace')
    def wait(self,predicate,label,seconds=25):
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            assert self.proc.poll() is None,self.logs()[-2000:]
            assert not re.search(r'Guest exception|wasm trap|out of bounds|callback failed|image unavailable|canvas presentation failed',self.logs(),re.I),self.logs()[-1800:]
            if value:=predicate():return value
            time.sleep(.05)
        raise TimeoutError(label+': '+self.logs()[-1400:])
    def command(self,value):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(10);client.connect(str(self.sock));client.sendall((value+'\n').encode());raw=bytearray()
            while block:=client.recv(65536):raw.extend(block)
        assert not raw.startswith(b'ERR'),(value,raw)
        return bytes(raw)
    def key(self,value):self.command('KEY_DOWN '+value);time.sleep(.05);self.command('KEY_UP '+value);time.sleep(.2)
    def tap(self,x,y):self.command(f'TOUCH DOWN 1 {x} {y}');time.sleep(.06);self.command(f'TOUCH UP 1 {x} {y}');time.sleep(.2)
    def shot(self,name):
        header,_,data=self.command('SCREENSHOT AFTER_PRESENT').partition(b'\n');assert header==f'PNG {len(data)}'.encode()
        with Image.open(io.BytesIO(data)) as image:assert image.size==self.size
        (self.root/(name+'.png')).write_bytes(data)
    def enter(self,text):
        self.key('A') # Controller X opens the original-text editor.
        self.tap(self.size[0]-32,self.size[1]-22) # Clear the existing draft.
        self.tap(self.size[0]//2,90)
        self.command('TEXT '+text);time.sleep(.15);self.shot('editor')
        self.key('ENTER')
    def __enter__(self):
        self.file=self.log_path.open('wb')
        argv=[str(self.args.simulator),'--package',str(self.args.package),'--publisher-key',str(self.args.publisher_key),'--state-root',str(self.state),'--locale','zh-CN','--width',str(self.size[0]),'--height',str(self.size[1]),'--pxadb-control-socket',str(self.sock),*self.extra]
        self.proc=subprocess.Popen(argv,stdout=self.file,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER=self.audio,SDL_DISKAUDIOFILE=str(self.pcm),SDL_DISKAUDIODELAY='32',PXA_SIM_NAV_MODE='gesture-no-bar',PXA_SIMULATOR_PERF='0'))
        try:self.wait(lambda:self.sock.exists(),'control socket');self.wait(lambda:'Translator: ready' in self.logs(),'startup');time.sleep(.3);return self
        except BaseException:self.close();raise
    def close(self):
        if hasattr(self,'proc') and self.proc.poll() is None:
            try:self.command('KEY HOME');self.proc.wait(timeout=8)
            except Exception:self.proc.terminate();self.proc.wait(timeout=4)
        if hasattr(self,'file'):self.file.close()
        self.temp.cleanup()
    def __exit__(self,*_):self.close()

def seed_settings(state,url):
    # Independent on-disk settings fixture, never an installed user's data.
    fields=[url+'/translate','',url+'/inference','','fixture-model']
    value=b'TRSC'+bytes([1,1,0,1,1,2])
    for field in fields:raw=field.encode();value+=struct.pack('<H',len(raw))+raw
    checksum=2166136261
    for byte in value:checksum=((checksum^byte)*16777619)&0xffffffff
    value+=struct.pack('<I',checksum)
    key=b'translator.settings.v1';body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(value))+value
    roots=list((state/'app-data').iterdir());assert len(roots)==1
    for name in ['a','b']:(roots[0]/('.pxa-kv-'+name)).write_bytes(struct.pack('<4sHHQII',b'PXKV',1,0,100,len(body),zlib.crc32(body))+body)

@contextmanager
def fixture(output):
    calls=[]
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_POST(self):
            raw=self.rfile.read(int(self.headers.get('Content-Length','0')));calls.append((self.path,raw))
            if self.path=='/translate':
                request=json.loads(raw);assert request['format']=='text'
                if request['q']=='Failure probe.':self.send_error(429);return
                if request['q']=='Cancel probe.':time.sleep(2)
                response={'translatedText':'你好，世界。' if request['q']=='Hello world.' else '测试译文：'+request['q']}
            elif self.path=='/inference':
                assert b'name="model"\r\n\r\nfixture-model' in raw and b'filename="speech.ogg"' in raw
                audio=raw.split(b'Content-Type: audio/ogg\r\n\r\n',1)[1].rsplit(b'\r\n--pxa-translator-multipart-v1--',1)[0]
                path=output/'captured.ogg';path.write_bytes(audio)
                subprocess.run(['ffmpeg','-v','error','-i',str(path),'-f','null','-'],check=True,capture_output=True)
                response={'text':'Hello world.'} # Protocol fixture; not a real ASR accuracy claim.
            else:self.send_error(404);return
            data=json.dumps(response,ensure_ascii=False).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass # Expected when the user cancels.
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:yield 'http://127.0.0.1:'+str(server.server_address[1]),calls
    finally:server.shutdown();server.server_close();thread.join(timeout=2)

if __name__=='__main__':
    from native_ui import main
    main()
