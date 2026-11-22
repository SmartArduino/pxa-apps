#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check map UI transitions against real Host nodes and unaltered screenshots."""
import argparse
import json
from pathlib import Path
import socket
import sys
import time

from keys_simulator import Simulator

WORKSPACE = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(WORKSPACE / 'tools/pxadb'))
from ui_query import expect, tap_target, validate_snapshot


class Journey(Simulator):
    def tree(self):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(8)
            connection.connect(self.socket)
            connection.sendall(b'UI INSPECT\n')
            with connection.makefile('rb') as stream:
                header = stream.readline()
                assert header.startswith(b'JSON '), header
                return validate_snapshot(json.loads(stream.read(int(header.split()[1]))))

    def button(self, text):
        return tap_target(self.tree(), {'type': 'button', 'text': text})

    def tap(self, text):
        target = self.button(text)
        self.command(f'TAP {target["x"]} {target["y"]}')
        time.sleep(.25)

    def has(self, text, enabled=True):
        expect(self.tree(), {'type': 'button', 'text': text}, {'visible': True, 'enabled': enabled})

    def enter(self, text):
        target = tap_target(self.tree(), {'type': 'text-input'})
        self.command(f'TAP {target["x"]} {target["y"]}')
        self.command('TEXT '+text)
        time.sleep(.4)  # Let Host input events drain before the next pointer batch.
        self.tap('搜索')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--runner', type=Path, default=Path('build/simulator/pai-touch/pxsys_product_simulator'))
    p.add_argument('--publisher-key', type=Path, default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--density-dpi', type=int, default=160)
    a = p.parse_args()
    assert '/local/artifacts/' in str(a.output.resolve())
    s = Journey(a, settings={})
    try:
        s.wait(lambda: Path(s.socket).exists() and 'Navigation: ready' in s.logs())
        time.sleep(.4)
        s.shot('home')
        for _ in range(3):
            s.tap('缩小地图')
        s.tap('搜索地点'); s.shot('search-empty')
        s.enter('114.054000,22.542000')
        s.tap('114.054000,22.542000'); s.has('去这里'); s.shot('place-detail')
        s.tap('偏好设置'); s.tap('返回'); s.has('去这里')
        s.tap('去这里'); s.has('选择起点'); s.shot('plan-destination')
        s.tap('选择起点'); s.enter('114.050000,22.540000')
        s.tap('114.050000,22.540000'); s.has('交换起终点'); s.shot('plan-ready')
        s.tap('交换起终点'); s.shot('plan-swapped')
        s.tap('返回'); s.has('去这里')
        s.tap('返回'); s.has('搜索'); s.has('114.054000,22.542000'); s.shot('results-restored')
        s.tap('偏好设置'); s.tap('返回'); s.has('搜索')
        s.tap('清空搜索'); s.has('餐饮'); s.shot('search-cleared')
        s.enter('一个尚未连接服务的地点'); s.shot('search-unavailable')
        s.tap('返回'); s.has('模拟导航')
        s.tap('模拟导航'); s.shot('navigation')
        s.tap('行程路书'); s.tap('下一段'); s.shot('roadbook'); s.tap('返回')
        s.tap('结束导航'); s.has('继续导航'); s.shot('exit-confirm')
        s.tap('继续导航'); s.has('结束导航')
        s.command('KEY BACK'); time.sleep(.25); s.has('继续导航')
        s.command('KEY BACK'); time.sleep(.25); s.has('结束导航')
        s.tap('结束导航'); s.tap('结束导航'); s.has('开始导航'); s.shot('route-preview')
        s.tap('清除路线'); s.has('模拟导航'); s.shot('route-cleared')
        assert not any(word in s.logs() for word in ('Exception:', 'PXA task failed', 'map frame rejected'))
        print(f'PASS: {a.width}x{a.height}@{a.density_dpi}: place, plan, nested returns, clear, roadbook, exit confirmation')
    finally:
        s.close()


if __name__ == '__main__':
    main()
