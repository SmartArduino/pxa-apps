#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the signed recorder with real Host services and three-key input.

SDL dummy capture supplies silence. Artifacts are written only to --output.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def exercise(args):
    sys.path.insert(0, str(args.workspace / 'local/pxa-apps/common/study/tests'))
    from test_three_key_simulator import ThreeKey
    os.environ.pop('PXA_SIMULATOR_PERF', None)
    with ThreeKey(args.workspace, args.package_root, args.output, 'recorder',
                  args.width, args.height, 'flow', args.simulator) as sim:
        def log():
            return sim.log_path.read_text()

        def wait(predicate, seconds=15):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if predicate():
                    return
                assert sim.process.poll() is None, log()[-3000:]
                time.sleep(.05)
            sim.screenshot('timeout')
            raise AssertionError((sim.focus(), log()[-3000:]))

        def choose(label):
            for _ in range(20):
                if sim.focus().startswith(label):
                    sim.key('ENTER')
                    return
                sim.key('DOWN')
            raise AssertionError(('unreachable', label, sim.focus()))

        def saved_files():
            return list(sim.state.rglob('rec-*.opus'))

        wait(lambda: sim.focus() == '开始录音')
        sim.screenshot('home')
        sim.key('UP')
        sim.expect('退出')
        sim.key('DOWN')
        choose('开始录音')
        wait(lambda: 'Recorder recording' in log())
        time.sleep(1.1)
        sim.screenshot('recording')
        choose('暂停录音')
        wait(lambda: sim.focus() == '继续录音')
        time.sleep(.4)
        partial = next(sim.state.rglob('rec-*.part'))
        paused_size = partial.stat().st_size
        time.sleep(.6)
        assert partial.stat().st_size == paused_size, 'paused recording kept writing'
        sim.screenshot('paused')
        choose('继续录音')
        time.sleep(.6)
        choose('停止并保存')
        wait(lambda: 'Recorder saved ' in log())
        wait(lambda: sim.focus() == '开始录音')
        assert len(saved_files()) == 1
        original = saved_files()[0]
        assert not list(sim.state.rglob('*.part'))
        saved_data = original.read_bytes()
        assert saved_data.startswith(b'OggS')
        shutil.copyfile(original, args.output / 'recorded.opus')
        probe = subprocess.run(['ffprobe', '-v', 'error', '-show_streams', '-show_format',
                                '-of', 'json', str(original)], capture_output=True, text=True, check=True)
        metadata = json.loads(probe.stdout)
        assert metadata['streams'][0]['codec_name'] == 'opus'
        assert metadata['streams'][0]['channels'] == 1
        assert float(metadata['format']['duration']) > 1
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(original), '-f', 'null', '-'], check=True)
        choose('我的录音')
        sim.screenshot('library')
        choose('录音 000001')
        wait(lambda: sim.focus() == '播放录音')
        sim.screenshot('detail')
        choose('播放录音')
        wait(lambda: 'Recorder playing ' in log())
        sim.screenshot('playing')
        wait(lambda: 'Recorder playback ended' in log())
        wait(lambda: sim.focus() == '播放录音')
        choose('播放录音')
        wait(lambda: log().count('Recorder playing ') == 2)
        choose('停止播放')
        wait(lambda: 'Recorder playback stopped' in log())
        choose('删除录音')
        sim.expect('保留录音')
        sim.screenshot('delete-confirm')
        choose('保留录音')
        wait(lambda: sim.focus() == '播放录音')
        assert original.read_bytes() == saved_data

        # Restart the real signed package against the same filesystem.
        sim.process.terminate()
        sim.process.wait(timeout=5)
        starts_before = log().count('STUDY_FOCUS 开始录音')
        sim.process = subprocess.Popen(sim.process.args, stdout=sim.log, stderr=subprocess.STDOUT,
                                       env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
        wait(lambda: Path(sim.socket).exists() and log().count('STUDY_FOCUS 开始录音') > starts_before)
        choose('我的录音')
        choose('录音 000001')
        wait(lambda: sim.focus() == '播放录音')
        choose('删除录音')
        choose('确认删除')
        wait(lambda: 'Recorder deleted ' in log())
        wait(lambda: sim.focus() == '去录音')
        assert not saved_files()
        sim.screenshot('empty-library')

        # Seed ordinary app files to exercise pagination and collision avoidance.
        for index in range(1, 9):
            (original.parent / f'rec-{index:06d}.opus').write_bytes(saved_data)
        choose('去录音')
        choose('开始录音')
        wait(lambda: log().count('Recorder recording') == 2)
        time.sleep(.4)
        choose('停止并保存')
        wait(lambda: log().count('Recorder saved ') == 2)
        choose('我的录音')
        choose('下一页')
        sim.screenshot('library-page-2')
        choose('上一页')
        assert len(saved_files()) == 9
        assert original.read_bytes() == saved_data
        choose('返回')
        choose('开始录音')
        wait(lambda: log().count('Recorder recording') == 3)
        time.sleep(.4)
        choose('退出')
        wait(lambda: sim.process.poll() is not None)
        assert log().count('Recorder saved ') == 3, 'exit did not finish saving'
        assert len(saved_files()) == 10
        assert not list(sim.state.rglob('*.part'))
        for error in ('Recorder error', 'auxiliary stack', 'out of bounds memory',
                      'unreachable instruction', 'WASM module instantiate failed'):
            assert error not in log(), (error, log()[-3000:])
        result = dict(result='passed', viewport=[args.width, args.height], three_key=True,
                      capture='SDL dummy silence', pause_resume=True, standard_opus=True,
                      playback_end_and_stop=True, persistence=True, delete_confirmation=True,
                      pagination=True, no_overwrite=True, save_on_exit=True, metadata=metadata)
        (args.output / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        print(json.dumps(result, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('workspace', 'package-root', 'simulator', 'output'):
        parser.add_argument('--' + name, type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument('--width', type=int, default=320)
    parser.add_argument('--height', type=int, default=240)
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    exercise(arguments)
