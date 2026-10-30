# SPDX-License-Identifier: CC0-1.0
"""Gen4 physical Up/Down/Confirm driver using real controller edges only."""
import re,time
from simulator import command,check
class ThreeKeys:
    def __init__(self,s,prefix='Level',menu_action=6):self.s=s;self.prefix=prefix;self.menu_action=menu_action
    def key(self,name,hold=.055):
        check(name in ('UP','DOWN','CONFIRM'),'only Gen4 physical keys are allowed')
        mapped='Z' if name=='CONFIRM' else name
        command(self.s.sock,'KEY_DOWN '+mapped);time.sleep(hold);command(self.s.sock,'KEY_UP '+mapped);time.sleep(.11)
    def focus(self):
        hits=re.findall(re.escape(self.prefix)+r': focus=(\d+) screen=(\d+)',self.s.logs());return tuple(map(int,hits[-1])) if hits else (0,0)
    def choose(self,action):
        for _ in range(16):
            if self.focus()[0]==action:self.key('CONFIRM');return
            self.key('DOWN')
        raise AssertionError(('focus unavailable',action,self.focus(),self.s.logs()[-1200:]))
    def menu(self):self.choose(self.menu_action)
    def close(self):
        self.menu();self.key('DOWN');self.key('DOWN');command(self.s.sock,'KEY_DOWN Z');self.s.proc.wait(timeout=5);check(self.s.proc.returncode==0,'clean Gen4 exit')
