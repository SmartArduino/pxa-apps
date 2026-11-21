#!/usr/bin/env python3
"""Exercise each CPU fighter through actual menus in the official simulator."""
import argparse,json,re,time
from pathlib import Path
from net_harness import Game,configure
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();configure(a.package,a.output)
results=[]
for level,c,width in [(0,1,320),(1,2,854),(2,3,854),(2,0,320)]:
 g=Game(f'cpu-{c}-level-{level}',width,240 if width==320 else 480,'buttons')
 try:
  g.wait('Ash Harbor ready');time.sleep(.3)
  if 'launch view=5' in g.text():g.key('X');g.wait('story-finished')
  for _ in range((c+1)%4):g.tap(207,150)
  if level==0:g.tap(30,116)
  elif level==2:g.tap(78,116)
  g.tap(160,211-g.geometry()[6]);g.wait('opponent-open')
  g.tap(80 if c%2==0 else 235,55 if c<2 else 120);g.tap(250,229-g.geometry()[5]);g.wait('frame-bank ready');time.sleep(1.8)
  g.tap(160,24);g.wait('practice-open')
  for _ in range(3):g.key('RIGHT')
  offset=len(g.text());g.key('X');g.wait('practice-close',offset=offset);g.wait('frame-bank ready',offset=offset)
  assert re.search(fr'practice-close.*opponent={c}.*difficulty={level} practice=3',g.text()[offset:]),g.text()[-1800:]
  began=time.monotonic();g.shot('start')
  # Alternate movement, grounded attacks, projectiles and jump-ins. These are
  # actual user controls, with no mutation of the CPU's state or match rules.
  while time.monotonic()-began<18:
   g.cmd('KEY_DOWN RIGHT');time.sleep(.2);g.key('Z');g.cmd('KEY_UP RIGHT')
   g.key('S');time.sleep(.4);g.key('UP');time.sleep(.25);g.key('X');time.sleep(.4)
  g.shot('sparring');g.key('ENTER');g.shot('pause')
  t=g.text()[offset:]
  assert re.search(r'Ash Harbor (hit|block) ',t),'No combat contact in the live Guest'
  prefix=['yan','lan','zhen','ling'][c]
  assert re.search(r'voice='+prefix+r'-(?:attack_|fire|rise|rush|super|throw)',t),'CPU never attacked'
  assert not re.search(r'Ash Harbor error=|render error=|out of bounds|label elided',g.text())
  g.key('Z');time.sleep(.3)
  results.append({'character':c,'difficulty':level,'viewport':[width,g.height],'passed':True,'seconds':round(time.monotonic()-began,1)})
 finally:g.stop()
out=Path(a.output);(out/'result.json').write_text(json.dumps({'passed':True,'cases':results,'checked':['four CPU characters','three difficulty settings','training free spar','actual jump/poke/projectile controls','CPU attacks and contact','audio','pause/resume','bounded Chinese HUD']},indent=2));print(json.dumps(results))
