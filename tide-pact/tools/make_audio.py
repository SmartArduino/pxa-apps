"""Prepare the game loops and mono S16 clips from model-rendered FLAC files."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

import numpy as np

app=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1])
report_path=Path(sys.argv[2])
target=app/'assets/audio'
target.mkdir(parents=True,exist_ok=True)
spec=json.loads((app/'tools/audio-cues.json').read_text())
rate=16000
vorbis_quality='7'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load(name, highpass):
    path=source/f'{name}.flac'
    data=subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-af',
                f'highpass=f={highpass},aresample={rate}:resampler=soxr:precision=28:cutoff=0.95',
                '-ac','1','-ar',str(rate),'-f','f32le','-'])
    a=np.frombuffer(data,dtype='<f4').copy()
    assert len(a) and np.isfinite(a).all()
    return a

def normalize(a,peak,rms):
    absolute=max(1e-8,float(np.max(np.abs(a))))
    energy=max(1e-8,float(np.sqrt(np.mean(a*a))))
    return (a*min(peak/absolute,rms/energy)).astype('<f4')

def result(path,a):
    return {'seconds':round(len(a)/rate,3),'bytes':path.stat().st_size,
            'peak':round(float(np.max(np.abs(a))),4),'rms':round(float(np.sqrt(np.mean(a*a))),4),
            'sha256':digest(path)}

report={'music':{},'effects':{},'voices':{},'sample_rate':rate,'vorbis_quality':int(vorbis_quality),
        'source':{p.name:digest(p) for p in sorted(source.glob('*.flac'))}}
for name,info in spec['music'].items():
    wave=load(name,40)
    size=round(4*info['bars']*60/info['bpm']*rate)
    fade=round(4*60/info['bpm']*rate)
    start=round(info['start']*rate)
    seam=start+fade
    choices=np.arange(seam-160,seam+161)
    quiet=choices[np.argmin(np.abs(wave[choices]-wave[choices-1])+
                            .08*(np.abs(wave[choices])+np.abs(wave[choices-1])))]
    start+=int(quiet)-seam
    part=wave[start:start+size+fade]
    assert len(part)==size+fade,(name,len(wave)/rate)
    taper=(1-np.cos(np.linspace(0,np.pi,fade)))/2
    # Same musical position at either side of the reduced loop.
    loop=np.concatenate([part[fade:size],part[size:]*(1-taper)+part[:fade]*taper])
    loop=normalize(loop,.76,.19)
    path=target/f'{name}.ogg'
    subprocess.run(['ffmpeg','-v','error','-y','-f','f32le','-ac','1','-ar',str(rate),
                    '-i','-','-c:a','libvorbis','-q:a',vorbis_quality,str(path)],input=loop.tobytes(),check=True)
    report['music'][name]=dict(result(path,loop),loop_step=round(abs(float(loop[0]-loop[-1])),5))
    # Compare the actual encoded loop seam as decoded on normal players.
    decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-f','f32le','-'])
    encoded=np.frombuffer(decoded,'<f4')
    report['music'][name]['encoded_loop_step']=round(abs(float(encoded[0]-encoded[-1])),5)

for group in ('effects','voices'):
    entries=spec[group]
    if group=='voices':
        entries={f'voice-{key}':{'line':line} for key,line in entries['lines'].items()}
    for name,info in entries.items():
        is_voice=group=='voices'
        a=load(name,90 if is_voice else 54)
        hop=160
        count=len(a)//hop
        rms=np.sqrt(np.mean(a[:count*hop].reshape(count,hop)**2,axis=1))
        ceiling=float(max(rms))
        active=np.flatnonzero(rms>max(.0025,ceiling*(.035 if is_voice else .072)))
        assert len(active),(name,ceiling)
        # Keep a few milliseconds of pre-roll and a short decay in each clip.
        start=max(0,int(active[0])*hop-96)
        end=min(len(a),(int(active[-1])+1)*hop+480)
        if not is_voice:
            end=min(len(a),max(end,start+round(.18*rate)),start+round(info['duration']*rate))
        clip=a[start:end].copy()
        assert .06<len(clip)/rate<2.10,(name,len(clip)/rate)
        clip[:48]*=np.linspace(0,1,48)
        fall=min(len(clip)//5,560 if is_voice else round(.045*rate))
        clip[-fall:]*=np.linspace(1,0,fall)
        clip=normalize(clip,.83 if is_voice else .75,.25 if is_voice else .23)
        path=target/f'{name}.s16'
        path.write_bytes(np.rint(32767*clip).astype('<i2').tobytes())
        report[group][name]=dict(result(path,clip),source_start=round(start/rate,3))
        if is_voice:
            report[group][name]['line']=info['line']

report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:{n:r['seconds'] for n,r in report[k].items()} for k in ('music','effects','voices')},
                 ensure_ascii=False,indent=2))
