#!/usr/bin/env python3
"""Exercise the running SDL Host through real keyboard events (PC demo City)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--socket',type=Path,required=True)
p.add_argument('--log',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--swap',action='store_true',help='also verify swapped native confirmation/cancel and persisted hints')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
report={'passed':False,'input_path':'SDL keyboard events -> desktop controller -> Core 2 controller state -> complete OpenLara Guest'}
def command(text):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
        s.settimeout(10);s.connect(str(a.socket));s.sendall((text+'\n').encode());data=bytearray()
        while chunk:=s.recv(65536):data.extend(chunk)
    if text.startswith('SCREENSHOT'):
        header,_,png=bytes(data).partition(b'\n');assert header==b'PNG '+str(len(png)).encode();return png
    assert bytes(data)==b'OK\n',(text,data)
def logs():return a.log.read_text(errors='replace')
def wait(text,after,timeout=100):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if text in logs()[after:]:return
        time.sleep(.1)
    raise RuntimeError('timed out: '+text)
def key(name,seconds=.12):
    command('KEY_DOWN '+name);time.sleep(seconds);command('KEY_UP '+name);time.sleep(.2)
def menu(index):
    key('TAB')
    for _ in range(index):key('DOWN')
    key('Z')
def capture(name):
    data=command('SCREENSHOT AFTER_PRESENT');(a.output/(name+'.png')).write_bytes(data)
    return hashlib.sha256(data).hexdigest()
def position():
    matches=re.findall(r'pos=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+)',logs())
    assert matches,'no position log';return list(map(int,matches[-1]))
try:
    start=len(logs());menu(4);wait('OpenLara ready level=3',start);time.sleep(5.3)
    capture('city');key('RIGHT',1.2);key('UP',1.2);time.sleep(5.3)
    saved=position();start=len(logs());menu(1);wait('OpenLara save committed',start)
    capture('saved');key('RIGHT',1.0);key('UP',1.0);time.sleep(5.3)
    moved=position();assert moved!=saved,'SDL input did not move Lara'
    start=len(logs());menu(2);wait('OpenLara save restored',start);time.sleep(5.3)
    restored=position()
    # Upstream saves yaw in 16 bits; its float round trip can change the log
    # by one unit (360/65536 degrees). Integer position must remain exact.
    assert restored[:3]==saved[:3] and abs((restored[3]-saved[3]+32768)%65536-32768)<=1,(saved,restored)
    report['yaw_log_tolerance_units']=1
    report.update(saved_position=saved,moved_position=moved,restored_position=restored,save_load=True)
    before=capture('restored');key('ENTER');time.sleep(1);inventory=capture('inventory')
    assert before!=inventory;key('ENTER');time.sleep(1);capture('inventory-return')
    report['start_inventory']=True
    key('TAB');capture('pause');key('TAB');report['select_pause']=True
    menu(8);capture('controls');key('X')
    start=len(logs());menu(9);key('DOWN');key('Z');time.sleep(.5)
    capture('missing-level');assert 'ready level=4' not in logs()[start:]
    key('X');key('X');report['missing_level_guard']=True
    report['controller_only_level_selection']=True
    menu(10);capture('import-game-data');key('X');key('X')
    report['original_data_entry']=True
    if a.swap:
        from PIL import Image
        start=len(logs());menu(6);wait('OpenLara ready level=0',start);time.sleep(1.2)
        menu(7);key('DOWN');key('DOWN');start=len(logs());key('Z')
        wait('OpenLara settings committed',start);key('X');time.sleep(1.2);capture('swapped-title')
        def band(name,left):
            with Image.open(a.output/(name+'.png')) as im:
                assert im.size==(320,240)
                return im.crop((16 if left else 200,202,112 if left else 304,208)).tobytes()
        closed=band('swapped-title',False);key('X');time.sleep(1);capture('swapped-chosen')
        assert band('swapped-chosen',False)!=closed,'B did not confirm the native option'
        key('Z');time.sleep(1);capture('swapped-cancelled')
        assert band('swapped-cancelled',False)==closed,'A did not cancel the native option'
        command('PACKAGE stop pxa-openlara');time.sleep(.5);start=len(logs())
        command('PACKAGE run pxa-openlara');wait('OpenLara ready level=0',start);time.sleep(1.2)
        capture('swapped-restarted');assert band('swapped-restarted',True)==band('swapped-title',True)
        report['native_swapped_confirm_cancel']=True;report['swapped_hint_survives_restart']=True
        menu(7);key('DOWN');key('DOWN');start=len(logs());key('Z')
        wait('OpenLara settings committed',start);key('X')
        start=len(logs());menu(4);wait('OpenLara ready level=3',start);time.sleep(1)
    capture('final');report['passed']=True
finally:
    (a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
