#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Find the delivered destruction effects in an actual 16kHz mono SDL mix.

Use field_fixture.cpp's demolition save in the signed simulator, and capture
with SDL_AUDIODRIVER=disk. Reports belong under local/artifacts.
"""
from pathlib import Path
import argparse,json,numpy as np

p=argparse.ArgumentParser()
p.add_argument('--capture',type=Path,required=True)
p.add_argument('--report',type=Path,required=True)
args=p.parse_args()
app=Path(__file__).resolve().parents[1]
signal=np.frombuffer(args.capture.read_bytes(),dtype='<i2').astype(float)/32768
assert len(signal)>32000
checks=[]
for name,decibels in (('vehicle_destroy',-10),('building_destroy',-8)):
    reference=np.frombuffer((app/'assets/audio'/(name+'.s16')).read_bytes(),dtype='<i2').astype(float)/32768
    energy=np.cumsum(reference*reference)
    # A quiet release contributes background-music energy to normalized
    # correlation but almost no effect energy. Use the prefix containing
    # 99% of the reference energy; this window is independent of the mix.
    count=max(8000,int(np.searchsorted(energy,energy[-1]*.99))+1)
    ref=reference[:count]
    assert len(signal)>=len(ref)
    size=1<<(len(signal)+len(ref)-1).bit_length()
    correlation=np.fft.irfft(np.fft.rfft(signal,size)*np.conj(np.fft.rfft(ref,size)),size)[:len(signal)-len(ref)+1]
    at=int(np.argmax(correlation));power=float(np.dot(ref,ref))
    gain=float(correlation[at]/power)
    rho=float(correlation[at]/np.sqrt(power*np.dot(signal[at:at+len(ref)],signal[at:at+len(ref)])))
    retained=float(power/energy[-1]);expected=10**(decibels/20)
    checks.append({'effect':name,'start_seconds':at/16000,'window_seconds':len(ref)/16000,'reference_energy_retained':retained,'correlation':rho,'gain':gain,'expected_gain':expected})
    assert retained>=.99 and rho>.55 and .7*expected<gain<1.3*expected,checks[-1]
assert checks[1]['start_seconds']>checks[0]['start_seconds']
args.report.write_text(json.dumps({'passed':True,'capture':'actual SDK mixer, battle music retained','checks':checks},indent=2)+'\n')
print('Both destruction waveforms identified in the actual mix, retaining >=99% of effect energy and matching configured gains')
