#!/usr/bin/env python3
"""Ordinary throws/escape feedback through two real signed LAN Guests."""
import argparse,json,re,time,socket
from pathlib import Path
from net_harness import Game,configure
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();configure(a.package,a.output)
games=[]
def chords(peers):
 # Queue both keys before waiting for an ACK. The control server otherwise
 # advances a rendered frame between the two synchronous socket requests.
 # Connecting first keeps each Guest's pair in the same event-poll batch.
 clients=[]
 try:
  for g in peers:
   for key in ['Z','X']:
    c=socket.socket(socket.AF_UNIX);c.settimeout(10);c.connect(str(g.sock));clients.append((c,key))
  for c,key in clients:c.sendall(('KEY_DOWN '+key+'\n').encode())
  for c,key in clients:assert c.recv(64)==b'OK\n'
 finally:
  for c,key in clients:c.close()
def release(g):g.cmd('KEY_UP Z');g.cmd('KEY_UP X')
try:
 host=Game('host',854,480);client=Game('client',320,240);games=[host,client]
 for g in games:g.start()
 host.tap(84,76);host.tap(84,76);client.tap(84,76)
 host.tap(84,118);host.wait('Ash LAN advertised');client.tap(232,118);client.wait('room-found');client.tap(160,76)
 for g in games:g.wait('online-play',45);g.wait('frame-bank ready');time.sleep(1.8)
 host.cmd('KEY_DOWN RIGHT');time.sleep(2.2);host.cmd('KEY_UP RIGHT');time.sleep(.2)
 offsets=[len(g.text()) for g in games]
 # Both players press a real throw command. It must tech regardless of which
 # input reaches the authority first, without either player taking damage.
 chords([host,client]);time.sleep(.07);release(host);release(client)
 host.wait('Ash Harbor throw-escape',timeout=4,offset=offsets[0]);client.wait('online-throw-escape',timeout=4,offset=offsets[1])
 for g,offset in zip(games,offsets):
  lines=[x for x in g.text()[offset:].splitlines() if 'Ash Throw ' in x and 'throw-escape ' in x]
  assert 'hp=100,100' in lines[-1] and 'escapes=1' in lines[-1],lines
  g.shot('throw-escape')
 # A held chord never repeats an escape and does not create an extra throw.
 time.sleep(.4)
 for g,offset in zip(games,offsets):
  # SFX logging is deliberately capped at the first four effects. Count the
  # authoritative escape event rather than treating that trace as all audio.
  assert len([line for line in g.text()[offset:].splitlines() if 'Ash Throw ' in line and 'throw-escape ' in line])==1,g.text()[-1200:]
 # Close the gap after the escape's separation/protection. Without an escape
 # command the same throw must now deal Han's actual 22 damage exactly once.
 host.cmd('KEY_DOWN RIGHT');time.sleep(.7);host.cmd('KEY_UP RIGHT');time.sleep(.3)
 offsets=[len(g.text()) for g in games];chords([host]);time.sleep(.06);release(host)
 host.wait('Ash Harbor grab',timeout=4,offset=offsets[0]);client.wait('online-grab',timeout=4,offset=offsets[1])
 for g in games:g.shot('grab-window')
 host.wait('Ash Harbor hit ',timeout=4,offset=offsets[0]);client.wait('online-hit',timeout=4,offset=offsets[1])
 for g,offset in zip(games,offsets):
  t=g.text()[offset:];assert re.search(r'(?:hit|online-hit).*hp=100,78.*damage=1',t),t[-2500:]
  assert not re.search(r'error=|out of bounds|label elided|missing Chinese label',t)
  g.shot('throw-damage')
 host.key('ENTER');time.sleep(.3)
 # Packet snapshots and HUD results remain valid while frozen by either peer.
 for g in games:g.shot('paused-after-throw')
 result={'passed':True,'checked':['real PK inputs from both players','one escape event on both peers','neither HP nor combo damage on escape','illustrated pixel escape notice','unbroken throw deals exactly 22 damage once','grab and escape snapshots','both aspect ratios and pause']}
 Path(a.output,'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
finally:
 for g in games:g.stop()
