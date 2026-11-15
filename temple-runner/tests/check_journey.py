#!/usr/bin/env python3
"""Assert real simulator journeys, telemetry, persistence and capture health."""
import json,re,sys
from pathlib import Path
from PIL import Image
root=Path(sys.argv[1]);result=json.loads((root/'result.json').read_text())
assert result['runtime_passed'],result.get('error')
for profile in result['profiles']:
    p=root/profile['name'];log=(p/'run.log').read_text()
    assert profile['steps'],profile['name']+': no interaction'
    assert 'RELIC input mode=1' in log and 'RELIC input mode=2' in log
    assert re.search(r'RELIC input mode=1 .*lane=-1',log),'left swipe was not applied'
    assert re.search(r'RELIC dead mode=3',log),'failure was not exercised'
    best=[int(n) for n in re.findall(r'RELIC loaded best=(\d+)',log)]
    assert len(best)>=2 and best[-1]>0,('best score did not persist',best)
    samples=[(int(a),int(b),int(c)) for a,b,c in re.findall(r'rejected=(\d+) bytes=\d+ faces=(\d+) peak=(\d+)',log)]
    assert samples and all(rejected==0 and peak<920 for rejected,faces,peak in samples),samples
    for capture in profile['screenshots']:
        if capture['path'].endswith('/before.png'):continue # startup can precede first presented frame
        with Image.open(root/capture['path']) as im:
            assert im.size==(capture['width'],capture['height'])
            colors=im.convert('RGB').getcolors(im.width*im.height)
            assert len(colors)>32,('blank capture',capture['path'])
            if im.mode=='RGBA':assert im.getextrema()[3]==(255,255),'physical mask in store capture'
    print(profile['name'], 'PASS', 'best',best[-1],'peak faces',max(s[2] for s in samples))
print('All touch journeys, persistence and render budgets passed; inspect original images for visual acceptance.')
