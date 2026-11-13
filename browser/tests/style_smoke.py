#!/usr/bin/env python3
"""Style-only DOM updates through real PXA clicks and rendered Canvas colors."""
import argparse
import json
import time
from pathlib import Path

from PIL import Image
from css_fixture import Fixture
from harness import Browser


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    a = p.parse_args()
    f = Fixture().start()
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, corner=0)
    colors = {'button': (208, 96, 32), 'red': (156, 36, 54),
              'blue': (36, 104, 172), 'purple': (128, 48, 160), 'green': (24, 122, 66)}
    rows = []
    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        mark = len(b.logs())
        b.navigate('http://127.0.0.1:' + str(f.server.server_port) + '/inline')
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:] and
               'BROWSER JS UPDATE' in b.logs()[mark:])
        button = None
        for phase, expected in enumerate((None, 'blue', None, 'purple', 'green', 'red', 'blue')):
            if phase:
                mark = len(b.logs())
                b.tap(*button)
                b.wait(lambda: 'BROWSER JS UPDATE' in b.logs()[mark:])
            name = 'phase-' + str(phase)
            b.shot(name)
            im = Image.open(a.output / (name + '.png')).convert('RGB')
            x, y, w, h = b.box('content')
            points = {key: [] for key in colors}
            for py in range(y, min(y + h, im.height)):
                for px in range(x, min(x + w, im.width)):
                    pixel = im.getpixel((px, py))
                    for key, color in colors.items():
                        if all(abs(v - target) < 9 for v, target in zip(pixel, color)):
                            points[key].append((px, py))
            assert len(points['button']) > 15, 'missing style action'
            button = ((min(v[0] for v in points['button']) + max(v[0] for v in points['button'])) // 2,
                      (min(v[1] for v in points['button']) + max(v[1] for v in points['button'])) // 2)
            for key in ('red', 'blue', 'purple', 'green'):
                assert (len(points[key]) > 15) == (key == expected), (phase, key, len(points[key]))
            if phase == 1:
                original_left = min(v[0] for v in points['blue'])
            if phase == 6:
                assert min(v[0] for v in points['blue']) > original_left + 8, 'center alignment did not move text'
            rows.append({'phase': phase, 'expected': expected,
                         'color_pixels': {key: len(value) for key, value in points.items()}})
        mark = len(b.logs())
        b.tap(*button)
        time.sleep(.5)
        assert 'BROWSER JS UPDATE' not in b.logs()[mark:], 'unchanged style triggered a snapshot'
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        assert f.requests['/inline'] == 1, f.requests
        rows.append({'unchanged_style_stays_idle': True, 'page_requests': f.requests['/inline']})
        (a.output / 'results.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2) + '\n')
        print('inline style simulator PASS:', len(rows), 'flows', flush=True)
    finally:
        b.close()
        f.close()


if __name__ == '__main__':
    main()
