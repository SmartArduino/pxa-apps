#!/usr/bin/env python3
"""Compare ordered T9 states with a source baseline (Linux native test)."""
import argparse
import ctypes as c
import itertools
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-root', required=True, type=Path, help='Directory containing both baseline app sources')
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='pxa-t9-diff-') as directory:
        libraries = []
        for name, path in [('before', args.baseline_root.resolve() / 'pinyin-ime'), ('after', app)]:
            target = Path(directory) / (name + '.so')
            sources = [path / 'core/ime_t9.c', path / 'core/ime_trie.c']
            generated = path / 'generated/ime_t9_syllables.c'
            if generated.exists():
                sources.append(generated)
            subprocess.run([os.environ.get('CC', 'cc'), '-O3', '-shared', '-fPIC', '-I' + str(path),
                            *map(str, sources), '-o', str(target)], check=True)
            lib = c.CDLL(str(target)); libraries.append(lib)
            for fn in ('len', 'count', 'selected_index'):
                getattr(lib, 'ime_t9_' + fn).restype = c.c_size_t
            for fn in ('digits', 'selected_pinyin', 'pinyin'):
                getattr(lib, 'ime_t9_' + fn).restype = c.c_char_p
            for fn in ('push_digit', 'split', 'select', 'restore'):
                getattr(lib, 'ime_t9_' + fn).restype = c.c_bool
            for fn in ('reset', 'backspace'):
                getattr(lib, 'ime_t9_' + fn).restype = None
            lib.ime_t9_push_digit.argtypes = [c.c_char]
            lib.ime_t9_restore.argtypes = [c.c_char_p]
            lib.ime_t9_select.argtypes = lib.ime_t9_pinyin.argtypes = [c.c_size_t]
        checks = 0

        def send(fn, *values):
            nonlocal checks
            previous = libraries[0].ime_t9_selected_index()
            results = [getattr(lib, 'ime_t9_' + fn)(*values) for lib in libraries]
            if fn in ('push_digit', 'split', 'select', 'restore'):
                assert results[0] == results[1], (fn, values, results)
            if fn == 'push_digit' and not results[1]:
                # Deliberate fix: a rejected digit preserves the chosen reading.
                assert libraries[1].ime_t9_selected_index() == previous
                if previous < libraries[0].ime_t9_count():
                    libraries[0].ime_t9_select(previous)
            states = [(lib.ime_t9_digits(), lib.ime_t9_selected_index(),
                       [lib.ime_t9_pinyin(i) for i in range(lib.ime_t9_count())]) for lib in libraries]
            assert states[0] == states[1], (fn, values, states)
            checks += 1

        for length in range(1, 5):
            for digits in itertools.product(b'23456789', repeat=length):
                send('reset')
                for digit in digits:
                    send('push_digit', bytes([digit]))
        rng = random.Random(42)
        for _ in range(10000):
            action = rng.randrange(8)
            if action < 3: send('push_digit', bytes([rng.randrange(48, 58)]))
            elif action == 3: send('backspace')
            elif action == 4: send('split')
            elif action == 5: send('select', rng.randrange(35))
            elif action == 6: send('restore', rng.choice([b'ni', b'hao', b"ni'hao", b"ni'", b'wrong', b'', b"a'wrong", b'xi\x00an']))
            else: send('reset')
        print(json.dumps({'matching_states': checks, 'random_operations': 10000,
                          'exhaustive_digit_lengths': [1, 2, 3, 4],
                          'known_change': 'rejected digit preserves selection'}))


if __name__ == '__main__':
    main()
