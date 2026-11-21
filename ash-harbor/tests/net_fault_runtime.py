#!/usr/bin/env python3
"""Exercise real Guests through a bounded UDP loss/reordering proxy."""
import argparse,collections,heapq,json,select,socket,threading,time,re
from pathlib import Path
from net_harness import Game,configure
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();configure(a.package,a.output);a.output=a.output.resolve()
class Link:
 def __init__(self,host):
  self.host=host;self.client=None;self.mode='jitter';self.count=collections.Counter();self.queue=[];self.serial=0;self.running=True;self.error=None
  self.socket=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.socket.bind(('127.0.0.2',37940));self.socket.setblocking(False)
  self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
 def run(self):
  try:
   while self.running:
    if select.select([self.socket],[],[],.005)[0]:
     b,source=self.socket.recvfrom(4096)
     if not b.startswith(b'AHLN') or len(b)<20:continue
     to_client=source==self.host
     if not to_client:self.client=source
     target=self.client if to_client else self.host
     if not target:continue
     direction='down' if to_client else 'up';kind=b[5];self.count[direction]+=1;self.count[direction+'-kind-'+str(kind)]+=1;n=self.count[direction]
     if self.mode=='block_state' and to_client and kind==6:self.count['blocked-state']+=1;continue
     if self.mode=='block_input' and not to_client and kind==5:self.count['blocked-input']+=1;continue
     if self.mode=='jitter' and n%7==0:self.count['lost']+=1;continue
     delay=.11 if self.mode=='jitter' and n%13==0 else .012
     if delay>.02:self.count['delayed']+=1
     self.serial+=1;heapq.heappush(self.queue,(time.monotonic()+delay,self.serial,target,b))
     if self.mode=='jitter' and n%11==0:
      self.serial+=1;heapq.heappush(self.queue,(time.monotonic()+delay+.045,self.serial,target,b));self.count['duplicates']+=1
    while self.queue and self.queue[0][0]<=time.monotonic():
     _,_,target,b=heapq.heappop(self.queue);self.socket.sendto(b,target)
    assert len(self.queue)<=64,'Proxy exceeded its bounded queue'
  except Exception as e:self.error=repr(e)
 def close(self):
  self.running=False;self.thread.join(timeout=2);self.socket.close()
  assert not self.thread.is_alive();assert self.error is None,self.error

def input_ip(game,text):
 footer=210-game.geometry()[5];step=min(24,(footer-85)//4)
 for ch in text:
  i='123456789.0'.index(ch);game.tap(94+i%3*65,81+i//3*step+(step-2)//2)
 game.tap(270,footer+10)
def state(game):
 line=[line for line in game.text().splitlines() if 'Ash Harbor online-sync ' in line][-1]
 return dict(re.findall(r'(phase|hp|meter|round|net_tick|remote_meter)=([0-9,]+)',line))
games=[];link=None
try:
 route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);route.connect(('192.0.2.1',9));ip=route.getsockname()[0];route.close()
 link=Link((ip,37940));host=Game('host',854,480);games.append(host);client=Game('client',320,240);games.append(client)
 for g in games:g.start()
 for _ in range(3):host.tap(84,76)
 client.tap(84,76);host.tap(84,118);host.wait('Ash LAN opened host=1')
 client.tap(160,156);client.wait('Ash LAN opened host=0');input_ip(client,'127.0.0.2')
 for g in games:g.wait('online-play',45);g.wait('frame-bank ready');time.sleep(1.8)
 host.key('S');host.wait('Ash Harbor hit');time.sleep(.8);client.qcf(True);time.sleep(1.1)
 host.wait('voice=ling-fire.s16');host.wait('voice=lan-fire.s16');host.wait('Ash Harbor hit');client.wait('online-hit')
 assert host.text().count('voice=lan-fire.s16')==1,'Retransmission created an extra special move'
 host.key('ENTER');time.sleep(2)
 assert state(host)==state(client),(state(host),state(client))
 for g in games:g.shot('jitter-paused')
 host.key('Z');time.sleep(.5)
 # Welcomes still arrive, but no State arrives: both simulation and watchdog
 # must depend on useful match traffic, not on a duplicate greeting.
 offsets=[len(g.text()) for g in games];welcomes=link.count['down-kind-4'];link.mode='block_state';time.sleep(2.1)
 before=state(host);time.sleep(1.2);after=state(host)
 assert before['net_tick']==after['net_tick'],('Host advanced while the guest had no combat data',before,after)
 assert link.count['down-kind-4']>welcomes,'Heartbeat-only condition was not exercised'
 client.shot('state-wait');link.mode='jitter'
 for g,offset in zip(games,offsets):g.wait('online-play',timeout=5,offset=offset)
 time.sleep(.5)
 for g in games:g.shot('recovered')
 # The opposite direction must also hold both peers and recover.
 offsets=[len(g.text()) for g in games];link.mode='block_input';time.sleep(2.1)
 before=state(host);time.sleep(1.2);after=state(host)
 assert before['net_tick']==after['net_tick'],('Host advanced without remote inputs',before,after)
 client.shot('input-wait');link.mode='jitter'
 for g,offset in zip(games,offsets):g.wait('online-play',timeout=5,offset=offset)
 time.sleep(.5)
 offsets=[len(g.text()) for g in games];link.mode='block_state';began=time.monotonic()
 client.wait('online-disconnected',timeout=8,offset=offsets[1]);client_timeout=time.monotonic()-began
 host.wait('online-disconnected',timeout=8,offset=offsets[0])
 for g in games:g.shot('state-timeout')
 result={'passed':True,'transport':'two real PXA Guests over UDP proxy','loss_pattern':'one in seven packets dropped; duplicates and 110 ms delayed packets','proxy':dict(link.count),'client_state_timeout_seconds':round(client_timeout,3),'checked':['ordered motion and exactly one special move','equal paused combat state despite packet loss','both peers freeze for missing downstream State','recovery in both directions before timeout','duplicate Welcome does not keep a dead match alive','both peers leave after prolonged state loss']}
 (a.output/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
finally:
 for g in games:g.stop()
 if link:link.close()
