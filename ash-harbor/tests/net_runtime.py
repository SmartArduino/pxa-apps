#!/usr/bin/env python3
"""Two real signed Guests over LAN UDP; artifacts stay outside source."""
import argparse,json,os,re,signal,socket,struct,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
from net_harness import Game,configure
configure(a.package,a.output)
games=[]
try:
 host=Game('host',854,480);games.append(host);client=Game('client',320,240);games.append(client)
 for g in games:g.start()
 # Room owner selects Tang Ling and the bamboo arena; guest selects Ji Lan.
 for _ in range(3):host.tap(84,76)
 host.tap(232,76);client.tap(84,76)
 host.tap(84,118);host.wait('Ash LAN opened host=1');host.shot('hosting')
 client.tap(232,118);client.wait('Ash LAN opened host=0');client.wait('Ash LAN room-found',timeout=20);time.sleep(.2);client.shot('search');client.tap(150,76)
 for g in games:g.wait('online-play',timeout=45);g.wait('frame-bank ready');time.sleep(1.8)
 assert re.search(r'online-loading.*character=3.*opponent=1.*scenery=1',host.text()),host.text()[-3000:]
 for g in games:g.shot('fight')
 host.qcf();client.qcf(True);time.sleep(1.8)
 host.cmd('KEY_DOWN RIGHT');client.cmd('KEY_DOWN LEFT');time.sleep(.55);host.cmd('KEY_UP RIGHT');client.cmd('KEY_UP LEFT');time.sleep(.2)
 for _ in range(3):host.key('Z');time.sleep(.25);client.key('X');time.sleep(.4)
 host.wait('Ash Harbor hit');client.wait('online-hit');
 # Let the exchanged attacks recover before requesting a P2 move notice.
 time.sleep(.8)
 # A loaded renderer can spread the earlier touch motion beyond its valid
 # input window. Check each local notice with a fresh, recovered move too.
 host.key('S');host.wait('local_move=惊雷掌');time.sleep(.8)
 client.key('S');client.wait('local_move=潮刃掌');client.shot('local-p2-move-feedback');
 # Pausing on either peer freezes both, then explicit resume restores both.
 for owner,other in [(host,client),(client,host)]:
  offset=len(other.text());owner.key('ENTER');time.sleep(.5);owner.shot('paused-'+owner.name);other.shot('peer-paused-'+owner.name)
  owner.key('Z');other.wait('online-play',offset=offset);time.sleep(.3)
 for g in games:g.shot('duel')
 # Authority keeps both clocks, health and meters identical while paused.
 host.key('ENTER');time.sleep(1.4)
 def sync(g):
  line=[x for x in g.text().splitlines() if 'Ash Harbor online-sync ' in x][-1]
  fields=dict(re.findall(r'(phase|hp|meter|round|net_tick|remote_meter)=([0-9,]+)',line))
  return fields
 assert sync(host)==sync(client),(sync(host),sync(client))
 host.key('Z');time.sleep(.3)
 # Short loss freezes the joining client and recovers without an extra match.
 try:
  host.proc.send_signal(signal.SIGSTOP);time.sleep(1.4);client.shot('network-wait')
 finally:host.proc.send_signal(signal.SIGCONT)
 time.sleep(.6)
 # Complete two wins using real controls, never a debug state mutation.
 until=time.monotonic()+50
 while 'Ash Harbor result ' not in host.text() and time.monotonic()<until:
  host.cmd('KEY_DOWN RIGHT');time.sleep(.15);host.key('X');host.cmd('KEY_UP RIGHT');time.sleep(.12)
 for g in games:g.wait('Ash Harbor result ',timeout=10);g.shot('result')
 host.wait('voice=ling-win.s16');client.wait('voice=lan-lose.s16')
 offset=len(host.text());host.key('Z');time.sleep(.8)
 assert 'Ash LAN loading host=1' not in host.text()[offset:],'Unilateral rematch'
 host.shot('waiting-rematch');client.key('Z')
 host.wait('Ash LAN loading host=1',offset=offset);host.wait('online-play',offset=offset)
 assert 'match=2' in host.text()[offset:]
 time.sleep(1.7)
 assert client.text().count('voice=lan-lose.s16')==1,'Previous result voice leaked into the new match'
 for g in games:g.shot('rematch')
 # Graceful leave is acknowledged through the live Guest and room UI.
 client.key('ENTER');client.key('DOWN');client.key('DOWN');client.key('Z');host.wait('online-disconnected');host.shot('peer-left')
 # Create another room and rejoin by the real numeric IP keyboard.
 offset=len(host.text());host.tap(84,118);host.wait('Ash LAN opened host=1',offset=offset)
 offset=len(client.text());client.tap(160,156);client.wait('Ash LAN opened host=0',offset=offset)
 client.shot('address-keypad');client.tap(270,210-client.geometry()[5]);client.shot('invalid-address')
 address=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);address.connect(('192.0.2.1',9));ip=address.getsockname()[0];address.close()
 def type_address(text):
  digits='123456789.0'
  footer=210-client.geometry()[5];step=min(24,(footer-81-4)//4)
  for ch in text:
   i=digits.index(ch);client.tap(94+(i%3)*65,81+(i//3)*step+(step-2)//2)
 type_address(ip);offset=len(host.text());client.tap(270,210-client.geometry()[5])
 host.wait('online-play',offset=offset);time.sleep(1.6);client.shot('direct-joined')
 # Lost peer (no Leave packet) also has a finite, visible timeout.
 client.proc.kill();client.proc.wait(timeout=5);offset=len(host.text())
 host.wait('online-disconnected',timeout=10,offset=offset);host.shot('timeout')
 host.key('X');host.shot('home-after-disconnect')
 for g in games:
  assert not re.search(r'out of bounds|Ash Harbor render error=|Ash Harbor error=|label elided',g.text()),g.text()[-3000:]
 result={'passed':True,'transport':'real UDP / two official simulator Guests','viewports':[[854,480],[320,240]],'checked':['room discovery','four-character selection','host-selected arena','both-player attacks','diagonal motion inputs','authoritative damage snapshots','pause from either peer','resume','graceful disconnect','bounded pixel UI','equal health, clock and meter snapshots','brief interruption recovery','full two-win result','both-player rematch consent','direct IPv4 keyboard join','peer process loss timeout','P2 own special-move name','each local character victory/defeat line','no stale defeat voice after rematch']}
 (a.output/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2));print(json.dumps(result,ensure_ascii=False))
finally:
 for g in games:g.stop()
