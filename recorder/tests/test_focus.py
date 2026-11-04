#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check focus through pending operations using the real three-key adapter.

Build the test simulator with --event-rounds 1 to exercise intermediate UI
states between asynchronous completions. SDL dummy capture supplies silence.
"""
import argparse
import json
from pathlib import Path
import sys
import time


def exercise(args):
    sys.path.insert(0, str(args.workspace / 'local/pxa-apps/common/study/tests'))
    from test_three_key_simulator import ThreeKey
    with ThreeKey(args.workspace, args.package_root, args.output, 'recorder',
                  args.width, args.height, 'focus', args.simulator) as sim:
        def log():
            return sim.log_path.read_text()

        def wait(predicate):
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if predicate():
                    return
                assert sim.process.poll() is None, log()[-3000:]
                time.sleep(.025)
            sim.screenshot('timeout')
            raise AssertionError((sim.focus(), log()[-3000:]))

        def expect(label):
            sim.expect(label)

        def press(label):
            expect(label)
            offset = len(log())
            sim.key('ENTER')
            return offset

        def stable_since(offset, *labels):
            lines = [line.removeprefix('STUDY_FOCUS ') for line in log()[offset:].splitlines()
                     if line.startswith('STUDY_FOCUS ')]
            assert all(any(line == label or (label and line.startswith(label)) for label in labels)
                       for line in lines), lines

        wait(lambda: sim.focus() == '开始录音')
        offset = press('开始录音')
        wait(lambda: 'Recorder recording' in log())
        expect('停止并保存')
        stable_since(offset, '正在准备', '停止并保存')
        sim.screenshot('recording-retained')
        time.sleep(.6)
        sim.key('DOWN')
        offset = press('暂停录音')
        expect('继续录音')
        time.sleep(.3)
        stable_since(offset, '继续录音')
        offset = press('继续录音')
        expect('暂停录音')
        time.sleep(.3)
        stable_since(offset, '暂停录音')
        sim.key('UP')
        offset = press('停止并保存')
        wait(lambda: 'Recorder saved ' in log())
        expect('开始录音')
        stable_since(offset, '正在保存', '开始录音')
        original = next(sim.state.rglob('rec-*.opus'))
        data = original.read_bytes()
        sim.key('DOWN')
        press('我的录音')
        wait(lambda: sim.focus().startswith('录音 000001'))
        damaged = bytearray(data)
        damaged[-1] ^= 1
        original.write_bytes(damaged)
        offset = press('录音 000001')
        wait(lambda: 'Recorder error -15' in log())
        expect('录音 000001')
        stable_since(offset, '录音 000001')
        original.write_bytes(data)
        offset = press('录音 000001')
        wait(lambda: sim.focus() == '播放录音')
        stable_since(offset, '录音 000001', '', '播放录音')
        offset = press('播放录音')
        wait(lambda: 'Recorder playing ' in log())
        expect('停止播放')
        stable_since(offset, '播放录音', '停止播放')
        offset = press('停止播放')
        wait(lambda: 'Recorder playback stopped' in log())
        expect('播放录音')
        stable_since(offset, '播放录音')
        offset = press('播放录音')
        wait(lambda: 'Recorder playback ended' in log())
        expect('播放录音')
        stable_since(offset, '播放录音', '停止播放')

        sim.key('DOWN')
        press('删除录音')
        wait(lambda: sim.focus() == '保留录音')
        press('保留录音')
        wait(lambda: sim.focus() == '播放录音')
        sim.key('DOWN')
        press('删除录音')
        wait(lambda: sim.focus() == '保留录音')
        sim.key('DOWN')
        original.unlink()  # A failed delete must retain the retry control.
        offset = press('确认删除')
        wait(lambda: 'Recorder error -5' in log())
        expect('确认删除')
        stable_since(offset, '确认删除')
        sim.screenshot('delete-retry-retained')
        original.write_bytes(data)
        press('确认删除')
        wait(lambda: 'Recorder deleted ' in log())
        wait(lambda: sim.focus() == '去录音')
        press('去录音')
        wait(lambda: sim.focus() == '开始录音')
        sim.key('UP')
        expect('退出')
        sim.key('DOWN')
        expect('开始录音')
        result = dict(result='passed', viewport=[args.width, args.height],
                      recording=True, pause_resume=True, save=True, playback_start_stop_end=True,
                      file_open_retry=True, delete_retry=True, navigation_reachable=True)
        (args.output / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        print(json.dumps(result, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('workspace', 'package-root', 'simulator', 'output'):
        parser.add_argument('--' + name, type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument('--width', type=int, default=320)
    parser.add_argument('--height', type=int, default=240)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    exercise(args)
