#!/usr/bin/env python3
"""Original 40-second exploration score and eight bounded game sounds.
No external samples or synthesis libraries. ffmpeg encodes the streaming music.
WAV previews and measurements go only to the supplied artifact directory.
Runtime sounds: 16 kHz, mono, unsigned 8-bit PCM, at most 32 KiB combined.
"""
import json, math, random, struct, subprocess, sys, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RATE=16000
TAU=math.tau

def env(t, length, attack=.012, release=.045):
    return min(1.,t/attack)**2*min(1.,max(0.,(length-t)/release))**2

def noise(length, seed, cutoff=1800):
    rng=random.Random(seed); a=1-math.exp(-TAU*cutoff/RATE); smooth=0.; result=[]
    for _ in range(round(length*RATE)):
        smooth+=a*(rng.uniform(-1,1)-smooth); result.append(smooth)
    return result

def wav(path, samples):
    with wave.open(str(path),'wb') as stream:
        stream.setparams((1,2,RATE,0,'NONE','not compressed'))
        stream.writeframes(b''.join(struct.pack('<h',round(max(-1,min(1,s))*32767)) for s in samples))

def effect(name, length):
    result=[]
    for i,n in enumerate(noise(length,71+sum(map(ord,name)),1500 if name=='hit' else 2200)):
        t=i/RATE; u=t/length
        if name=='coin':
            v=math.sin(TAU*659.255*t)*math.exp(-27*t)+.35*math.sin(TAU*987.767*t)*math.exp(-36*t)
        elif name=='jump':
            v=n*math.sin(math.pi*u)**1.4*.85+.12*math.sin(TAU*(180*t+210*t*t))
        elif name=='slide':
            v=n*math.sin(math.pi*u)**1.2*(1-.45*u)+.08*math.sin(TAU*110*t)*math.exp(-18*t)
        elif name=='land':
            v=.7*math.sin(TAU*(112*t-170*t*t))*math.exp(-38*t)+.7*n*math.exp(-45*t)
        elif name=='turn':
            v=n*math.sin(math.pi*u)**1.5+.1*math.sin(TAU*165*t)*math.sin(math.pi*u)
        elif name=='hit':
            v=.6*math.sin(TAU*(120*t-100*t*t))*math.exp(-18*t)+n*.7*math.exp(-22*t)
        elif name=='confirm':
            v=(math.sin(TAU*440*t)+.3*math.sin(TAU*659.255*t))*math.exp(-32*t)
        else:
            v=0.
            for start,freq in [(0.,329.628),(.09,440.),(.18,659.255)]:
                local=t-start
                if local>=0:
                    v+=(math.sin(TAU*freq*local)+.15*math.sin(TAU*freq*2*local))*math.exp(-13*local)*min(1,local/.012)
        result.append(v*env(t,length,.008 if name in ('coin','confirm') else .014))
    mean=sum(result)/len(result); peak=max(abs(s-mean) for s in result)
    return [(s-mean)*(.48 if name=='reward' else .58)/peak*env(i/RATE,length,.004,.012) for i,s in enumerate(result)]

def score():
    samples=[0.]*(40*RATE); beat=.625
    def add(start,length,instrument,freq,gain):
        for i in range(round(length*RATE)):
            t=i/RATE; phase=TAU*freq*t; envelope=env(t,length,.055 if instrument=='flute' else .025,.25)
            if instrument=='bass': v=(math.sin(phase)+.18*math.sin(2*phase))*math.exp(-1.5*t)
            elif instrument=='pad':
                v=(math.sin(phase)+.15*math.sin(2*phase))*.5; envelope=env(t,length,.42,.7)
            elif instrument=='flute': v=(math.sin(phase+.035*math.sin(TAU*4.8*t))+.09*math.sin(2*phase))*math.exp(-1.4*t)
            else: v=(math.sin(phase)+.19*math.sin(2.01*phase))*math.exp(-8*t)
            samples[(round(start*RATE)+i)%len(samples)]+=v*envelope*gain
    chords=[(110.,220.,261.626,329.628),(87.307,174.614,220.,261.626),(130.813,196.,261.626,329.628),(98.,196.,246.942,293.665)]
    phrases=[(329.628,392.,440.),(329.628,293.665,261.626),(261.626,329.628,392.),(293.665,261.626,220.)]
    for bar in range(16):
        start=bar*4*beat; chord=chords[bar%4]; add(start,1.9,'bass',chord[0],.085)
        for freq in chord[1:]: add(start,2.75,'pad',freq,.018)
        for j,freq in enumerate(phrases[(bar//2)%4]):
            if bar%4==3 and j==2: continue
            at=start+(j*.75+(.5 if bar%2 else 0))*beat; instrument='flute' if bar%4 in (0,3) else 'wood'
            add(at,.62,instrument,freq,.047); add(at+.22,.62,instrument,freq,.006)
        for subdivision in range(4):
            at=start+subdivision*beat
            for i in range(round(.18*RATE)):
                t=i/RATE; drum=math.sin(TAU*(58*t+1.7*(1-math.exp(-22*t))))*math.exp(-32*t)
                samples[(round(at*RATE)+i)%len(samples)]+=drum*(.065 if subdivision in (0,2) else .024)*env(t,.18,.009,.04)
        for subdivision in (1,3):
            for i,v in enumerate(noise(.085,900+bar*4+subdivision,2300)):
                samples[(round((start+(subdivision+.5)*beat)*RATE)+i)%len(samples)]+=v*.025*env(i/RATE,.085,.012,.045)
    # Wrap note tails above; preserve the periodic boundary rather than fading it.
    mean=sum(samples)/len(samples); peak=max(abs(s-mean) for s in samples)
    return [(s-mean)*.25/peak for s in samples]

def main():
    out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
    assets=ROOT/'assets/sfx'; assets.mkdir(exist_ok=True); report={}; preview=[]
    durations={'coin':.14,'jump':.18,'slide':.24,'land':.11,'turn':.15,'hit':.28,'confirm':.12,'reward':.48}
    for name,length in durations.items():
        samples=effect(name,length); data=bytes(max(0,min(255,round(128+s*127))) for s in samples)
        (assets/(name+'.pcm')).write_bytes(data); wav(out/(name+'.wav'),samples); preview.extend(samples+[0.]*round(.18*RATE))
        report[name]={'bytes':len(data),'seconds':length,'peak':max(map(abs,samples)),'rms':math.sqrt(sum(s*s for s in samples)/len(samples)),'endpoints':[data[0],data[-1]]}
    wav(out/'sound-palette.wav',preview); music=score(); wav(out/'temple-warm-score.wav',music)
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(out/'temple-warm-score.wav'),'-c:a','libvorbis','-q:a','2','-map_metadata','-1',str(ROOT/'assets/temple.ogg')],check=True)
    report['music']={'seconds':len(music)/RATE,'peak':max(map(abs,music)),'rms':math.sqrt(sum(s*s for s in music)/len(music)),'seam_step':abs(music[-1]-music[0])}
    (out/'audio-assets.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Original sound palette:',sum(v['bytes'] for v in report.values() if 'bytes' in v),'resident PCM bytes; music 40 seconds')
if __name__=='__main__': main()
