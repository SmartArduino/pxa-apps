"""Small control-socket harness for the real PXA simulator; no web engine."""
import os,shutil,socket,subprocess,tempfile,time
from pathlib import Path
from geometry import geometry,navigation
class Browser:
    def __init__(self,simulator,package,publisher,ime,output,width=296,height=240,dpi=160,corner=58,state=None,shell=False,round_screen=False):
        self.output=Path(output);self.output.mkdir(parents=True,exist_ok=True)
        self.scratch=tempfile.TemporaryDirectory(prefix='pxa-reader-')
        self.socket=Path(self.scratch.name)/'control.sock'
        self.state=Path(state) if state else Path(self.scratch.name)/'state';self.state.mkdir(exist_ok=True)
        self.log_path=self.output/'runtime.log';self.log=self.log_path.open('wb')
        args=[str(Path(simulator).resolve()),'--publisher-key',str(Path(publisher).resolve()),'--state-root',str(self.state),'--width',str(width),'--height',str(height),'--density-dpi',str(dpi),'--pxadb-control-socket',str(self.socket)]
        args+=['--round'] if round_screen else ['--corner-radius',str(corner)]
        if shell:
            packages=self.state/'packages';packages.mkdir(exist_ok=True)
            shutil.copytree(package,packages/'pxa-browser',dirs_exist_ok=True)
            shutil.copytree(ime,packages/'pxa-pinyin-ime',dirs_exist_ok=True)
            args+=['--installed-packages-root',str(packages),'--product-runner',str(Path(simulator).resolve().parent/'pxsys_product_simulator'),'--launch','pxa-browser','--gestures']
        else:args+=['--package',str(Path(package).resolve())]
        self.process=subprocess.Popen(args,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_IME_PACKAGE=str(Path(ime).resolve())),stdout=self.log,stderr=subprocess.STDOUT)
        self.dpi=dpi;self.wait(lambda:self.socket.exists())
    def logs(self):return self.log_path.read_text(errors='replace')
    def wait(self,condition,timeout=25):
        end=time.monotonic()+timeout
        while not condition():
            assert self.process.poll() is None,self.logs()
            assert time.monotonic()<end,self.logs()[-6000:]
            time.sleep(.04)
        time.sleep(.16)
    def command(self,value):
        with socket.socket(socket.AF_UNIX) as c:
            c.settimeout(20);c.connect(str(self.socket));c.sendall((value+'\n').encode());result=b''
            while block:=c.recv(65536):result+=block
        assert not result.startswith(b'ERR'),result.decode(errors='replace')
        return result
    def tap(self,x,y):
        self.command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.10)
        self.command(f'TOUCH UP 0 {x} {y}');time.sleep(.22)
    def nav(self,index):self.tap(*navigation(geometry(self.logs()),index))
    def box(self,name):return geometry(self.logs())[name]
    def dp(self,n):return max(1,n*self.dpi//160)
    def shot(self,name):
        header,png=self.command('SCREENSHOT').split(b'\n',1)
        assert header==f'PNG {len(png)}'.encode()
        (self.output/(name+'.png')).write_bytes(png)
    def enter(self,value,field=False):
        # URL editing preselects its text; input via the ordinary LVGL path.
        for at in range(0,len(value),30):self.command('TEXT '+value[at:at+30]);time.sleep(.07)
        x,y,w,h=self.box('address');self.tap(x+w-self.dp(15),y+h//2)
        time.sleep(.25)
    def navigate(self,url):
        self.nav(2) # Home supplies an empty URL editor; keep normal input behavior.
        x,y,w,h=self.box('address');self.tap(x+w//2,y+h//2)
        # Existing text is selected by Host focus (same as tapping URL bar).
        self.enter(url)
    def close(self):
        if self.process.poll() is None:self.process.terminate()
        self.process.wait(timeout=8);self.log.close();self.scratch.cleanup()
