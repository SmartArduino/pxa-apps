#!/usr/bin/env python3
"""Real PXA request counts distinguish body overflow from safe CSP fallback."""
import argparse
import json
from pathlib import Path
from harness import Browser
from http_limit_fixture import Fixture


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--legacy-host', action='store_true', help='Expect the safe unknown-reason fallback on an older Host')
    a = p.parse_args(); f = Fixture().start()
    root = 'http://127.0.0.1:' + str(f.server.server_port)
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, corner=0)
    rows = []

    def navigate(path, fails=False):
        mark = len(b.logs()); b.navigate(root + path)
        b.wait(lambda: ('BROWSER ERROR code=-16' if fails else 'BROWSER DONE status=200') in b.logs()[mark:])
        return mark

    def policy_not_executed(mark):
        assert 'BROWSER JS OPEN' not in b.logs()[mark:] and f.gets['/trap'] == 0

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        navigate('/ok'); b.wait(lambda: f.gets['/script-proof'] == 1)
        rows.append(dict(flow='ordinary page still executes its original script'))
        mark = navigate('/body-large', True)
        assert f.gets['/body-large'] == (2 if a.legacy_host else 1), dict(f.gets)
        policy_not_executed(mark); b.shot('body-limit')
        rows.append(dict(flow='known body overflow has no redundant GET', requests=f.gets['/body-large'], legacy=a.legacy_host))
        mark = navigate('/header-large'); policy_not_executed(mark)
        assert f.gets['/header-large'] == 2, dict(f.gets)
        b.shot('header-fallback'); rows.append(dict(flow='oversized CSP falls back once with scripts disabled'))
        mark = navigate('/both-large', True); policy_not_executed(mark)
        assert f.gets['/both-large'] == 2, dict(f.gets)
        rows.append(dict(flow='header then body overflow stops after bounded fallback'))
        mark = navigate('/policy'); policy_not_executed(mark)
        assert f.gets['/policy'] == 1
        rows.append(dict(flow='ordinary CSP remains effective without another request'))
        navigate('/form'); b.shot('post-form')
        x, y, w, h = b.box('content')
        mark = len(b.logs()); b.tap(x+w//2, y+b.dp(15))
        b.wait(lambda: 'BROWSER ERROR code=-16' in b.logs()[mark:]); policy_not_executed(mark)
        assert f.posts['/post-large'] == 1 and not f.gets['/post-large']
        rows.append(dict(flow='POST with oversized response is never replayed'))
        before = f.gets['/body-large']; navigate('/redirect-body', True)
        assert f.gets['/redirect-body'] == 1 and f.gets['/body-large']-before == (2 if a.legacy_host else 1)
        rows.append(dict(flow='redirect target body overflow does not restart navigation'))
        navigate('/ok'); b.wait(lambda: f.gets['/script-proof'] == 2)
        b.command('KEY HOME'); b.wait(lambda: 'PXA RESOURCE BUDGET' in b.logs())
        budget = [line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget, budget
        assert all(error not in b.logs() for error in ('BROWSER DRAW ERROR', 'PXA Guest exception:'))
        rows.append(dict(flow='normal navigation recovers and stop releases all budget charges'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows, gets=dict(f.gets), posts=dict(f.posts)), indent=2)+'\n')
        print('HTTP limits simulator PASS:', len(rows), 'flows; legacy=', a.legacy_host, flush=True)
    finally: b.close(); f.close()


if __name__ == '__main__': main()
