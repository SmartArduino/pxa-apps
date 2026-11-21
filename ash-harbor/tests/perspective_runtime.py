#!/usr/bin/env python3
"""Real lobby occupancy and a P2 win, including local move/result voices."""
import argparse,json,re,socket,time
from pathlib import Path
from net_harness import Game,configure
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();configure(a.package,a.output);a.output=a.output.resolve()
def type_ip(game,address):
 footer=210-game.geometry()[5];step=min(24,(footer-85)//4)
 for ch in address:
  i='123456789.0'.index(ch);game.tap(94+i%3*65,81+i//3*step+(step-2)//2)
 game.tap(270,footer+10)
games=[]
try:
 host=Game('host',854,480);games.append(host);client=Game('client',320,240);games.append(client)
 for g in games:g.start()
 for _ in range(2):host.tap(232,76);client.tap(84,76)
 host.tap(84,118);host.wait('Ash LAN advertised')
 client.tap(232,118);client.wait('room-found');client.shot('room-character-stage');client.tap(160,76)
 for g in games:g.wait('online-play',45);g.wait('frame-bank ready');time.sleep(1.8)
 host.wait('voice=yan-intro.s16');client.wait('voice=zhen-intro.s16')
 # A third actual Guest must not see the occupied room, and direct joining
 # must return a full-room message without disturbing the match.
 visitor=Game('visitor',320,240,'buttons');games.append(visitor);visitor.start();visitor.tap(232,118);visitor.wait('Ash LAN opened host=0');time.sleep(3.5)
 assert 'room-found' not in visitor.text(),visitor.text()[-2000:]
 visitor.shot('occupied-room-hidden');visitor.tap(250,222-visitor.geometry()[5])
 route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);route.connect(('192.0.2.1',9));ip=route.getsockname()[0];route.close();type_ip(visitor,ip)
 visitor.wait('Ash LAN failed');assert 'online-loading' not in visitor.text();visitor.shot('direct-join-full');visitor.stop()
 client.key('S');client.wait('local_move=震潮掌');client.shot('p2-special-move')
 round_captured=False;until=time.monotonic()+65
 while 'Ash Harbor result ' not in client.text() and time.monotonic()<until:
  client.cmd('KEY_DOWN LEFT');time.sleep(.35);client.key('X');client.cmd('KEY_UP LEFT');time.sleep(.2)
  if not round_captured and re.search(r'(?:online-sync|online-hit).*phase=2',client.text()):
   for g in [host,client]:g.shot('p2-round-win')
   round_captured=True
 assert round_captured,'No local round-win frame captured'
 for g in [host,client]:g.wait('Ash Harbor result ',timeout=12);g.shot('p2-match-win')
 host.wait('voice=yan-lose.s16');client.wait('voice=zhen-win.s16')
 for g in [host,client]:assert not re.search(r'Ash Harbor error=|render error=|out of bounds|label elided|missing Chinese label',g.text())
 result={'passed':True,'checked':['room row character, IP and stage','occupied room hidden from third Guest','direct full-room rejection','P2 own intro','P2 own move notice','P2 round win and match win','local victory and defeat voices']}
 (a.output/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
finally:
 for g in games:g.stop()
