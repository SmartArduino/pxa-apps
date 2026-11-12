#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fetch the original v154 Ogg soundtrack and effects, with pinned provenance."""
from pathlib import Path
import concurrent.futures,hashlib,json,subprocess,time,urllib.request
ROOT=Path(__file__).resolve().parents[1]
REV='d1e0307d3906bb350e80d8969a2464ed0b771ffb'
BASE=f'https://raw.githubusercontent.com/Anuken/Mindustry/{REV}/core/assets/'
MUSIC=['menu','game1','game2','game3','game4','game5','game6','game7','game8','game9','fine','boss1','boss2','land','launch','editor']
SOUNDS={'click':'ui/uiButton','wave':'ui/waveSpawn','shoot':'shoot/shootDuo','explode':'explosions/blockExplode1',
 'place':'block/blockPlace1','break':'block/blockBreak1','unit':'block/unitCreate','unlock':'ui/uiUnlock',
 'alpha':'shoot/shootAlpha','arc':'shoot/shootArc','unitshot':'shoot/shoot','heal':'block/healWave',
 'buildloop':'loops/loopBuild','mineloop':'loops/loopMineBeam','drillloop':'loops/loopDrill','smeltloop':'loops/loopSmelter','factoryloop':'loops/loopUnitBuilding'}
def fetch(pair):
 path,source=pair;dest=ROOT/path;dest.parent.mkdir(parents=True,exist_ok=True)
 if not dest.exists():
  for attempt in range(4):
   try:dest.write_bytes(urllib.request.urlopen(BASE+source,timeout=60).read());break
   except Exception:
    if attempt==3:raise
    time.sleep(attempt+1)
 return {'file':path,'source':'core/assets/'+source,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'bytes':dest.stat().st_size}
def main():
 pairs=[(f'audio-source/music/{n}.ogg',f'music/{n}.ogg') for n in MUSIC]
 pairs += [(f'assets/upstream/{n}.ogg','sounds/'+p+'.ogg') for n,p in SOUNDS.items()]
 with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:records=list(pool.map(fetch,pairs))
 converted=[]
 for name in MUSIC:
  dest=ROOT/f'assets/music/{name}.ogg';dest.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(['ffmpeg','-v','error','-y','-i',str(ROOT/f'audio-source/music/{name}.ogg'),'-ac','1','-ar','16000','-c:a','libvorbis','-q:a','5',str(dest)],check=True)
  converted.append({'file':str(dest.relative_to(ROOT)),'source_file':f'audio-source/music/{name}.ogg','sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'bytes':dest.stat().st_size})
 for name in SOUNDS:
  subprocess.run(['ffmpeg','-v','error','-y','-i',str(ROOT/f'assets/upstream/{name}.ogg'),'-ac','1','-ar','16000','-f','s16le',str(ROOT/f'assets/{name}.s16')],check=True)
 (ROOT/'audio-upstream.json').write_text(json.dumps({'repository':'https://github.com/Anuken/Mindustry','revision':REV,'tag':'v154','license':'GPL-3.0-only','music_conversion':'ffmpeg -ac 1 -ar 16000 -c:a libvorbis -q:a 5; original recordings retained in audio-source/music','effects_conversion':'16000 Hz mono PCM S16LE','files':records,'converted_music':converted},indent=2)+'\n')
 print('Original music:',len(MUSIC),'tracks;',sum(r['bytes'] for r in records if '/music/' in r['file']),'bytes; installed music:',sum(r['bytes'] for r in converted),'bytes; effects:',len(SOUNDS))
if __name__=='__main__':main()
