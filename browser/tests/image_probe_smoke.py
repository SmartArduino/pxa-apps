#!/usr/bin/env python3
"""Exercise the offline image fixture in the real PXA simulator."""
import argparse
import json
from pathlib import Path
import re
import time
from PIL import Image, ImageChops
from harness import Browser


def controls(path):
    """Locate this fixture's three buttons under either light or dark themes."""
    pic=Image.open(path).convert('RGB')
    def colored(x,y):
        pixel=pic.getpixel((x,y))
        return max(pixel)-min(pixel)>45 and max(pixel)>80
    rows=[y for y in range(30,min(130,pic.height))
          if sum(colored(x,y) for x in range(pic.width))>100]
    assert rows, 'No fixture button row'
    top,bottom=min(rows),max(rows)+1
    regions=[];start=None
    for x in range(pic.width+1):
        active=x<pic.width and sum(colored(x,y) for y in range(top,bottom)) >= (bottom-top)*.4
        if active and start is None:start=x
        if not active and start is not None:
            if x-start>=30:regions.append((start,top,x-start,bottom-top))
            start=None
    assert len(regions)==3 and all(20<=h<=60 and 30<=w<=120 for _,_,w,h in regions),regions
    return {key:(x+w//2,y+h//2)
            for key,(x,y,w,h) in zip(('Decode','Release','Large'),regions)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','ime-package','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    browser=Browser(args.simulator,args.package,args.publisher_key,args.ime_package,args.output)
    result={}
    try:
        browser.wait(lambda:'IMAGE PROBE READY large=0' in browser.logs(),timeout=40)
        browser.shot('ready');buttons=controls(args.output/'ready.png')
        def tap(name):browser.tap(*buttons[name])
        for case in ('common','repeat','large'):
            if case=='large':
                tap('Large');browser.wait(lambda:'IMAGE PROBE READY large=1' in browser.logs())
            mark=len(browser.logs());tap('Decode')
            browser.wait(lambda:'IMAGE PROBE DONE' in browser.logs()[mark:])
            browser.shot(case)
            pic=Image.open(args.output/(case+'.png')).convert('RGB').crop((30,105,230,180))
            pixels=getattr(pic,'get_flattened_data',pic.getdata)()
            assert sum(max(p)-min(p)>65 for p in pixels)>500,'No decoded image pixels'
            if case=='common':
                time.sleep(.7);browser.shot('animated')
                assert ImageChops.difference(Image.open(args.output/'common.png').convert('RGB'),
                                            Image.open(args.output/'animated.png').convert('RGB')).getbbox()
            mark=len(browser.logs());tap('Release')
            browser.wait(lambda:'IMAGE PROBE RELEASED' in browser.logs()[mark:])
        assert not any(error in browser.logs() for error in
                       ('IMAGE PROBE ERROR','IMAGE PROBE DECODE_ERROR','Guest exception'))
        result={'passed':True,'decode_metrics':re.findall(r'IMAGE PROBE DONE[^\n\x1b]*',browser.logs())}
    finally:
        browser.close();result['forced_shutdown']=browser.forced_shutdown
        (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    assert not result['forced_shutdown']
    print('Offline image decode/release/reload/animation smoke passed')


if __name__=='__main__':main()
