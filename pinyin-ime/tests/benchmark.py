#!/usr/bin/env python3
"""Build controlled O3 native provider workloads; run --memory separately."""
import argparse
import json
import os
import resource
from pathlib import Path
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    app = Path(__file__).resolve().parents[1]
    parser.add_argument('--source-root', type=Path, default=app.parent)
    parser.add_argument('--sdk', type=Path, default=Path(os.environ.get('PXA_SYSTEM_DIR', app.parents[2] / 'deps/pxa-system')))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--memory', action='store_true')
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--iterations', type=int, default=100)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--workload', type=int, choices=range(4))
    parser.add_argument('--allow-failures', action='store_true', help='Record known broken baseline workloads and continue')
    parser.add_argument('--check', action='store_true', help='Also run full-engine semantic and ownership regressions')
    parser.add_argument('--build-only', action='store_true', help='Build executables for alternating baseline/current measurements')
    parser.add_argument('--cpu', type=int, help='Pin measurements to one allowed Linux CPU (default: first allowed)')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    allowed = os.sched_getaffinity(0)
    cpu = min(allowed) if args.cpu is None else args.cpu
    if cpu not in allowed:
        parser.error('cpu is outside the allowed CPU set')
    if args.samples < 1 or args.iterations < 1:
        parser.error('samples and iterations must be positive')
    if args.sanitize and args.memory:
        parser.error('run sanitizers and allocation tracing separately')
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    sdk = args.sdk.resolve(); full = args.source_root.resolve() / 'pinyin-ime'
    cxx = os.environ.get('CXX', 'clang++'); cc = os.environ.get('CC', 'cc')
    cflags = ['-O3', '-I' + str(full)]
    instrumentation = ['-O1', '-g', '-UNDEBUG', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    cflags += instrumentation
    model_flags = ['-DPINYINIME_SYSTEM_DICT_ONLY'] if 'PINYINIME_SYSTEM_DICT_ONLY' in (full / 'CMakeLists.txt').read_text() else []
    cppflags = ['-O3', '-std=c++2c', '-fno-exceptions', '-fno-rtti', '-Wno-attributes', '-DPINYINIME_RUNTIME_ONLY',
                '-I' + str(full), '-I' + str(full / 'include'),
                '-I' + str(sdk / 'sdk/guest-cpp/include'), '-I' + str(sdk / 'sdk/testing')]
    cppflags += instrumentation + model_flags
    if args.memory:
        cppflags += ['-DIME_BENCHMARK_MEMORY', '-I' + str(sdk / 'tools/benchmarks')]
    objects = []
    for name in ('ime_t9', 'ime_trie'):
        obj = output / (name + '.o'); objects.append(str(obj))
        subprocess.run([cc, *cflags, '-c', str(full / 'core' / (name + '.c')), '-o', str(obj)], check=True)
    table = full / 'generated/ime_t9_syllables.c'
    if table.exists():
        obj = output / 'ime_t9_syllables.o'; objects.append(str(obj))
        subprocess.run([cc, *cflags, '-c', str(table), '-o', str(obj)], check=True)
    decoder = []
    for source in sorted((full / 'third_party/libgooglepinyin/src').glob('*.cpp')):
        if source.stem in ('dictbuilder', 'utf16reader'):
            continue
        obj = output / (source.stem + '.o'); decoder.append(str(obj))
        subprocess.run([cxx, '-O3', '-DNDEBUG', '-w', '-DPINYINIME_RUNTIME_ONLY', '-I' + str(full / 'third_party/libgooglepinyin/include'),
                        *instrumentation, *model_flags, '-include', str(full / 'port/single_owner_mutex.h'), '-c', str(source), '-o', str(obj)], check=True)
    results = {'memory_tracing': args.memory, 'pinned_cpu': cpu, 'samples': []}
    for variant in ('full', 'lite'):
        exe = output / variant
        sources = [str(app / 'tests/provider_benchmark.cpp'), str(sdk / 'sdk/guest-cpp/src/runtime.cpp')]
        extra = ['-DPXA_IME_LITE', '-I' + str(full.parent / 'pinyin-ime-lite')] if variant == 'lite' else [
            '-I' + str(full / 'third_party/libgooglepinyin/include'), str(full / 'core/ime_engine_port.cpp'), *decoder]
        link = ['-pthread']
        if args.memory:
            link.append('-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free')
        subprocess.run([cxx, *cppflags, *sources, *extra, *objects, *link, '-o', str(exe)], check=True)
        if variant == 'full' and args.check:
            check_sources = [str(app / 'tests/full_session_test.cpp'), sources[1]]
            check_exe = output / 'full-session-test'
            subprocess.run([cxx, *cppflags, *check_sources, *extra, *objects, *link, '-o', str(check_exe)], check=True)
            subprocess.run([str(check_exe), str(full / 'assets/dict_pinyin.dat')], check=True)
        if args.build_only:
            continue
        for workload in range(4) if args.workload is None else (args.workload,):
            samples = []
            for _ in range(args.samples):
                run = subprocess.run([str(exe), str(full / 'assets/dict_pinyin.dat'), str(workload), str(args.iterations)],
                                     text=True, capture_output=True, preexec_fn=lambda: os.sched_setaffinity(0, {cpu}))
                if run.returncode:
                    if not args.allow_failures:
                        raise RuntimeError(run.stderr or f'Provider exited with {run.returncode}')
                    results['samples'].append({'engine': variant, 'workload': workload,
                                               'failed': True, 'returncode': run.returncode})
                    print(json.dumps(results['samples'][-1]), flush=True)
                    break
                raw = run.stdout
                value = json.loads(raw.splitlines()[-1]); samples.append(value)
                results['samples'].append({'engine': variant, **value})
            if samples:
                assert len({v['reply_hash'] for v in samples}) == 1
                print(json.dumps({'engine': variant, 'workload': workload,
                                  'cpu_us_per_cycle': statistics.median(v['cpu_us_per_cycle'] for v in samples),
                                  'reply_hash': samples[0]['reply_hash']}), flush=True)
            (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
