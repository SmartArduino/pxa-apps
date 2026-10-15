"""Check the signed production browser's held actions without trace macros or network."""
import argparse, hashlib, json, tempfile, time
from pathlib import Path
from PIL import Image, ImageChops
from harness import Browser
p=argparse.ArgumentParser(description=__doc__)
for key in ('simulator','package','publisher-key','ime-package','output'):
    p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='browser-release-') as tmp:
    state=Path(tmp)/'state';state.mkdir()
    publisher=hashlib.sha256(a.publisher_key.read_bytes()).hexdigest()
    private=state/'private-files'/(publisher+'-pxa-browser');private.mkdir(parents=True)
    saved=private/'bookmarks.txt';site='http://fixture.invalid/held-bookmark'
    saved.write_text('#prefs 15 0\n'+site+'\n')
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,state=state)
    try:
        time.sleep(.7);b.shot('home')
        # Fixed, documented 296x240/160dpi/corner58 profile; first saved site below search/navigation.
        x,y=148,137
        b.command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.1)
        b.command(f'TOUCH MOVE 0 {x+24} {y}');time.sleep(.55)
        b.command(f'TOUCH MOVE 0 {x} {y}');b.command(f'TOUCH UP 0 {x} {y}');time.sleep(.2)
        assert site in saved.read_text(),'drag turned into a held removal'
        b.command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.1)
        b.command(f'TOUCH CANCEL 0 {x} {y}');time.sleep(.6)
        assert site in saved.read_text(),'cancelled touch removed a bookmark'
        began=time.monotonic();b.command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.2)
        assert site in saved.read_text(),'long press fired too early'
        b.wait(lambda:site not in saved.read_text(),timeout=4)
        held_ms=round((time.monotonic()-began)*1000,1);b.shot('removed-while-held')
        b.command(f'TOUCH UP 0 {x} {y}');time.sleep(.3);b.shot('release')
        held=Image.open(a.output/'removed-while-held.png').convert('RGB')
        released=Image.open(a.output/'release.png').convert('RGB')
        body=(20,37,276,169)
        assert not ImageChops.difference(held.crop(body),released.crop(body)).getbbox(),'UP opened the replacement action'
        assert site not in saved.read_text()
        assert 'BROWSER ERROR' not in b.logs() and 'BROWSER DRAW ERROR' not in b.logs(),b.logs()
        report=dict(passed=True,production=True,held_before_release=True,no_release_clickthrough=True,
                    drag_out_and_back_cancels=True,cancel=True,persist_observed_ms=held_ms)
        (a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    finally:b.close()
