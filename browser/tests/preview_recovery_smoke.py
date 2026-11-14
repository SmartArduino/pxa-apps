#!/usr/bin/env python3
"""Native GIF preview recovers after a valid first frame and corrupt second frame."""
import argparse
import json
from pathlib import Path
from PIL import Image
from geometry import navigation, content_crop
from harness import Browser
from image_failure_fixture import Fixture


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    a = p.parse_args(); f = Fixture().start()
    root = 'http://127.0.0.1:' + str(f.server.server_port)
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, corner=0)
    rows = []

    def points(name, color):
        b.shot(name); im = Image.open(a.output/(name+'.png')).convert('RGB')
        left, top, right, bottom = content_crop({'content':b.box('content'), 'status':b.box('status')})
        return [(x, y) for y in range(top, bottom) for x in range(left, right)
                if all(abs(c-v) < 9 for c, v in zip(im.getpixel((x, y)), color))]

    def middle(found):
        assert len(found) > 15, len(found)
        return ((min(p[0] for p in found)+max(p[0] for p in found))//2,
                (min(p[1] for p in found)+max(p[1] for p in found))//2)

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        for page, source, linked in (('/preview', '/preview.gif', False),
                                     ('/preview-linked', '/linked-preview.gif', True)):
            name = page[1:]; mark = len(b.logs()); b.navigate(root + page)
            b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
            assert 'BROWSER GIF ERROR' not in b.logs()[mark:]
            at = middle(points(name+'-first-frame', (220, 45, 65)))
            if linked:
                b.command(f'TOUCH DOWN 0 {at[0]} {at[1]}')
                b.wait(lambda: 'BROWSER IMAGE VIEW index=0' in b.logs()[mark:])
                b.command(f'TOUCH UP 0 {at[0]} {at[1]}')
            else:
                b.tap(*at); b.wait(lambda: 'BROWSER IMAGE VIEW index=0' in b.logs()[mark:])
            b.wait(lambda: 'BROWSER GIF ERROR code=-15' in b.logs()[mark:])
            assert f.counts[source] == 1, dict(f.counts)
            retry_at = middle(points(name+'-frame-error', (20, 105, 172)))
            rows.append(dict(flow=name+' shows a retry control after later-frame failure'))
            mark = len(b.logs()); b.tap(*retry_at)
            b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
            b.wait(lambda: 'BROWSER GIF FRAME index=0' in b.logs()[mark:])
            assert f.counts[source] == 2 and f.counts[page] == 1, dict(f.counts)
            # The replacement's second frame is actually blue, rather than an
            # error placeholder or a retained copy of the original first frame.
            assert len(points(name+'-recovered-frame', (35, 105, 220))) > 150
            assert 'BROWSER COMIC' not in b.logs(), 'preview retry entered comic mode'
            assert not f.counts['/trap'], 'linked preview retry followed its hyperlink'
            rows.append(dict(flow=name+' retries once and resumes native animation'))
            b.tap(*navigation({'toolbar':b.box('toolbar')}, 0, 4))
            b.shot(name+'-returned')
            assert f.counts[source] == 2 and f.counts[page] == 1, dict(f.counts)
            rows.append(dict(flow=name+' close retains webpage and loaded image'))
        assert all(error not in b.logs() for error in ('BROWSER DRAW ERROR', 'PXA Guest exception:'))
        b.command('KEY HOME')
        b.wait(lambda: 'PXA RESOURCE BUDGET' in b.logs())
        budget = [line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget, budget
        rows.append(dict(flow='normal stop releases animation and image budget charges'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows, requests=dict(f.counts)), indent=2)+'\n')
        print('Preview recovery simulator PASS:', len(rows), 'flows', flush=True)
    finally: b.close(); f.close()


if __name__ == '__main__': main()
