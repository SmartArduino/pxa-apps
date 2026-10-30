#!/usr/bin/env python3
"""Stage the miner's small-screen source tree without changing original assets."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil


def prepare(output):
    source = Path(__file__).resolve().parents[2]
    output = output.resolve()
    if output.exists():
        raise SystemExit('Output must be a new directory.')
    output.mkdir(parents=True)
    (output / 'common').symlink_to(source / 'common', target_is_directory=True)
    original = source / 'milkyfrog-miner'
    app = output / original.name
    shutil.copytree(original, app, ignore=shutil.ignore_patterns('art', 'store', '__pycache__'))
    pattern = re.compile(r'\{"([^"]+)",(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\}')
    kept = set()
    for name in ['sprites.hpp', 'heroes.hpp']:
        header = app / name
        lines = []
        for line in header.read_text().splitlines():
            if line.startswith('inline constexpr milky::Sprite '):
                sprite = line.split()[3].split('{')[0]
                regions = pattern.findall(line)
                assert len(regions) == 4, sprite
                if sprite.startswith('rare_'):
                    regions[0] = regions[1] = regions[2]
                else:
                    regions[0] = regions[1]
                kept.update(region[0] for region in regions)
                queue = iter(regions)
                def replace(_):
                    region = next(queue)
                    return '{"' + region[0] + '",' + ','.join(region[1:]) + '}'
                line = pattern.sub(replace, line)
            lines.append(line)
        header.write_text('\n'.join(lines) + '\n')
    for image in (app / 'assets/images').glob('*.png'):
        if image.relative_to(app).as_posix() not in kept:
            image.unlink()
    for relative in kept:
        assert hashlib.sha256((app / relative).read_bytes()).digest() == hashlib.sha256((original / relative).read_bytes()).digest(), relative
    for audio in (original / 'assets').iterdir():
        if audio.suffix in ('.s16', '.ogg'):
            assert audio.read_bytes() == (app / 'assets' / audio.name).read_bytes(), audio
    data = json.loads((app / 'SOURCES.json').read_text())
    data['device_profile'] = {
        'name': 'small-screen', 'native_static_limit': 128, 'native_rare_limit': 64,
        'source_version': json.loads((app / 'package.json').read_text())['version'],
        'audio': 'All original recordings and pitch variants retained byte for byte.',
        'images': 'Alias large descriptors to existing small-screen variants; no resampling.',
    }
    (app / 'SOURCES.json').write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
    spec = importlib.util.spec_from_file_location('milky_assets', Path(__file__).with_name('prepare_assets.py'))
    assets = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(assets)
    assets.refresh_generated_sources(app)
    print(f'Staged {app}: {len(kept)} unchanged textures, complete original audio.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    prepare(parser.parse_args().output)
