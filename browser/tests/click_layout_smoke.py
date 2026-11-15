#!/usr/bin/env python3
"""Real PXA tag CSS, block rows, nested owners, disabled descendants and images."""
import argparse
import json
import time
from pathlib import Path
from PIL import Image, ImageChops
from click_layout_fixture import Fixture
from harness import Browser

COLORS = dict(alpha=(24, 122, 66), beta=(153, 50, 64), heading=(112, 32, 128),
              entry=(208, 96, 32), pick=(117, 67, 36), blocked=(138, 84, 26),
              image=(245, 210, 40))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--probe', action='store_true')
    a = p.parse_args()
    f = Fixture().start()
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, corner=0)
    rows = []

    def box(color, name, line=None):
        b.shot(name)
        im = Image.open(a.output / (name + '.png')).convert('RGB')
        x, y, w, h = b.box('content')
        crop = im.crop((x, y, x+w, y+h))
        diff = ImageChops.difference(crop, Image.new('RGB', crop.size, color))
        r, g, blue = diff.split()
        mask = ImageChops.lighter(ImageChops.lighter(r, g), blue).point(lambda v: 255 if v < 8 else 0)
        if line:
            ys = [py for py in range(h) if mask.crop((0, py, w, py+1)).getbbox()]
            if not ys:
                return None
            groups = [[ys[0]]]
            for py in ys[1:]:
                if py-groups[-1][-1] > 3:
                    groups.append([])
                groups[-1].append(py)
            group = groups[0] if line == 'first' else groups[-1]
            band = Image.new('L', mask.size)
            band.paste(mask.crop((0, group[0], w, group[-1]+1)), (0, group[0]))
            mask = band
        rect = mask.getbbox()
        return None if rect is None else (x+rect[0], y+rect[1], x+rect[2], y+rect[3])

    def point(color, name, edge=None, line=None):
        rect = box(color, name, line)
        assert rect, name
        left, top, right, bottom = rect
        px = left+2 if edge == 'left' else right-2 if edge == 'right' else (left+right)//2
        return px, (top+bottom)//2

    def load(case):
        mark, before = len(b.logs()), len(f.events)
        url = 'http://127.0.0.1:'+str(f.server.server_port)+'/'+case
        b.navigate(url)
        mark = b.logs().rfind('BROWSER NAV '+url)
        assert mark >= 0
        b.wait(lambda: 'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:] and len(f.events) > before)
        assert f.events[-1]['kind'] == 'ready'

    def click(at, expected, target, label):
        before, mark = len(f.events), len(b.logs())
        b.tap(*at)
        b.wait(lambda: len(f.events) >= before+len(expected) and 'BROWSER JS UPDATE' in b.logs()[mark:])
        found = sorted(f.events[before:], key=lambda e: int(e['seq']))
        assert [r['kind'] for r in found] == expected, found
        assert all(r['target'] == target for r in found), found
        assert all(r['owner'] == {'link':'nav','image':'pic'}.get(r['kind'], r['kind']) for r in found), found
        rows.append(dict(flow=label,events=found))

    def idle(at, label):
        before, mark = len(f.events), len(b.logs())
        b.tap(*at)
        time.sleep(.3)
        assert len(f.events) == before and 'BROWSER JS UPDATE' not in b.logs()[mark:], label
        rows.append(dict(flow=label,events=0))

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        load('blocks')
        names = ('alpha', 'beta', 'heading', 'entry', 'pick')
        boxes = {name: box(COLORS[name], 'blocks-'+name) for name in names}
        ordered = all(boxes.values()) and all(boxes[names[i]][3] <= boxes[names[i+1]][1] for i in range(4))
        rows.append(dict(flow='block-tag-css-and-rows',boxes=boxes,ordered=bool(ordered)))
        if not a.probe:
            assert ordered, boxes
            for name in names:
                click(point(COLORS[name], 'click-'+name), [name], name, 'block-'+name)
            load('nested')
            click(point(COLORS['alpha'], 'outer-prefix', 'left'), ['outer'], 'outer', 'outer-prefix')
            click(point(COLORS['heading'], 'inner'), ['inner', 'outer'], 'inner', 'inner-bubbles')
            click(point(COLORS['alpha'], 'outer-tail', 'right'), ['outer'], 'outer', 'outer-after-inner')
            click(point(COLORS['beta'], 'nested-link'), ['link', 'outer'], 'nav', 'link-prevented')
            assert f.navigations == 0
            idle(point(COLORS['pick'], 'plain-outside'), 'plain-after-parent-is-idle')
            idle(point(COLORS['blocked'], 'disabled-descendant'), 'disabled-descendant-is-idle')
            click(point(COLORS['entry'], 'toggle'), ['toggle'], 'toggle', 'toggle-stop')
            click(point(COLORS['heading'], 'inner-stopped'), ['inner'], 'inner', 'inner-stops-bubbling')
            load('image')
            b.wait(lambda: 'BROWSER IMAGE DONE' in b.logs() and f.images > 0)
            click(point(COLORS['image'], 'loaded-image'), ['image', 'outer'], 'pic', 'image-bubbles')
            click(point(COLORS['alpha'], 'after-image', line='last'), ['outer'], 'outer', 'parent-after-image')
            idle(point(COLORS['pick'], 'image-outside'), 'plain-after-image-parent-is-idle')
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR', 'BROWSER DRAW ERROR', 'PXA Guest exception:'))
        mark = len(b.logs())
        b.command('KEY HOME')
        b.wait(lambda: 'PXA RESOURCE BUDGET' in b.logs()[mark:])
        budget = [s for s in b.logs()[mark:].splitlines() if 'PXA RESOURCE BUDGET' in s][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget
        (a.output / 'results.json').write_text(json.dumps(dict(flows=rows,budget=budget),indent=2)+'\n')
        print('Click layout simulator PASS:',len(rows),'flows; tag CSS, separate rows, nested handlers, disabled descendants, images and release',flush=True)
    finally:
        b.close()
        f.close()


if __name__ == '__main__':
    main()
