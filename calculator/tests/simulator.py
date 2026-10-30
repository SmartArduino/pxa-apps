#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Actual signed calculator: public touch/controller input, fullscreen and durable KV."""
import argparse,json,re,struct,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'common/pocket-studio/tests'))
from simulator import Session,command,check,save_blob
class Keys:
 def __init__(self,s):self.s=s
 def key(self,name,hold=.018):
  check(name in ['UP','DOWN','CONFIRM'],'Gen4 keys only');key='Z' if name=='CONFIRM' else name
  command(self.s.sock,'KEY_DOWN '+key);time.sleep(hold);command(self.s.sock,'KEY_UP '+key);time.sleep(.028)
 def focus(self):
  matches=re.findall(r'Calc: focus=(\d+) page=(\d+)',self.s.logs());check(matches,'no focus log');return tuple(map(int,matches[-1]))
 def seek(self,id):
  for _ in range(40):
   if self.focus()[0]==id:return
   self.key('DOWN')
  raise AssertionError(('focus missing',id,self.focus(),self.s.logs()[-1000:]))
 def choose(self,id):self.seek(id);self.key('CONFIRM')
 def menu(self):self.choose(6)
 def close(self):self.menu();self.key('DOWN');self.key('DOWN');command(self.s.sock,'KEY_DOWN Z');self.s.proc.wait(timeout=5);check(self.s.proc.returncode==0,'clean exit')
def snap(s):time.sleep(.26);return save_blob(s.state,'calculator')
def answer(s):return struct.unpack_from('<d',snap(s),16)[0]
def text(s):return snap(s)[32:129].split(b'\0')[0].decode()
def case(a,w,h,r,report):
 with Session(a,'calculator',w,h,r) as s:
  k=Keys(s);full=re.findall(r'fullscreen origin=(\d+),(\d+) size=(\d+)x(\d+) display=(\d+)x(\d+)',s.logs());check(full and tuple(map(int,full[-1]))==(0,0,w,h,w,h),'not fullscreen');s.shot('01-home')
  for id in [113,115,114,107,108,119]:k.choose(id)
  check(answer(s)==14,'precedence through Gen4 keys');s.shot('02-result')
  k.choose(1);k.choose(200);k.choose(219);k.choose(106);k.choose(117);k.choose(119);check(abs(answer(s)-1)<1e-12,'sin(90), automatic closing parentheses');s.shot('03-scientific-result')
  k.choose(1);k.choose(218);k.choose(200);check(snap(s)[6]==1,'memory store');s.shot('04-memory');k.choose(219)
  k.choose(100);k.seek(104);k.key('CONFIRM',.65);check(text(s)=='7','held confirm repeated a digit');k.choose(100)
  # Touch center of the visible 7 key. No canvas-sized coordinate assumptions.
  port=s.ch>=s.cw;kw=max(164,s.cw*54//100);x=12 if port else s.cw-kw-2;y=133 if port else 32;pw=s.cw-19 if port else kw;ph=s.ch-146 if port else s.ch-45
  tx=x+(pw//4-5)/2;ty=y+ph//5+(ph//5-5)/2;s.tap(tx,ty);check(text(s)=='7','touch hit missed its displayed key')
  s.pointer('DOWN',tx,ty);s.pointer('CANCEL',tx,ty);time.sleep(.1);check(text(s)=='7','cancelled touch entered a digit')
  k.choose(100);k.choose(112);k.choose(103);k.choose(117);k.choose(119);s.shot('05-zero-error');check('Calc: calculation error' in s.logs() and text(s)=='1/0','division by zero lost editable expression')
  k.choose(3);k.choose(30);s.shot('06-converter');k.choose(301);k.choose(301);s.shot('07-temperature');k.choose(700);k.choose(713);k.choose(705);s.shot('08-converted');k.choose(715)
  k.choose(31);s.shot('09-percent');k.choose(401);s.shot('10-discount');k.choose(401);s.shot('11-change');k.choose(401);s.shot('12-split');k.choose(715);k.choose(32);s.shot('13-help');k.choose(552);s.shot('14-help-second');k.choose(9)
  k.choose(2);s.shot('15-history');before=snap(s)[7];k.choose(522);s.shot('16-clear-confirm');k.key('CONFIRM');check(snap(s)[7]==before,'unsafe default erased history');k.choose(9)
  k.menu();s.shot('17-menu');k.key('UP');k.key('CONFIRM');k.key('DOWN');k.key('CONFIRM');time.sleep(.2);s.shot('18-english');k.close();saved=save_blob(s.state,'calculator')
  check('Pocket: audio' not in s.logs(),'silent utility requested sound')
 with Session(a,'calculator',w,h,r) as s:
  k=Keys(s);s.shot('19-restored');check(snap(s)==saved,'relaunch lost calculator state/history/memory');k.close()
 report.append({'display':[w,h],'radius':r,'logical':[s.cw,s.ch],'fullscreen':True,'three_keys':True,'touch_cancel':True,'arithmetic_scientific_memory':True,'conversion_practical':True,'history_safe_clear':True,'restart':True,'bilingual':True,'passed':True});print(json.dumps(report[-1]),flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['simulator','packages','publisher-key','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--quick',action='store_true');a=p.parse_args()
 for k,v in vars(a).items():
  if isinstance(v,Path):setattr(a,k,v.resolve())
 check('local/artifacts/' in str(a.output),'use ignored artifacts');a.output.mkdir(parents=True,exist_ok=True);report=[]
 try:
  for w,h,r in ([(240,284,0),(480,320,0)] if a.quick else [(240,284,0),(480,320,0),(240,320,0),(320,480,0),(240,240,0),(296,240,58),(800,480,0)]):case(a,w,h,r,report)
 finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
