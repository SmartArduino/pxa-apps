#!/usr/bin/env python3
"""Stage a device build without a second copy of the embedded atlases.

FramePixels decodes every atlas from frame_dimensions.hpp. UI and fighter
pages share the same lossless bank, so duplicate raw atlas files are unnecessary in an
AOT-only installation. Original art stays in the source tree.
"""
from pathlib import Path
import argparse
import json
import re
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error('Output must be an empty staging directory')
    output.mkdir(parents=True, exist_ok=True)
    static = int(re.search(r'static_pages=(\d+)', (source / 'art.hpp').read_text())[1])
    resources = json.loads((source / 'resources.json').read_text())
    count = len(resources['assets'])
    assert 0 < static < count
    encoded = (source / 'frame_dimensions.hpp').read_text()
    lengths = re.search(r'page_rle_bytes\[[^]]*\]=\{([^}]+)\}', encoded)
    assert lengths, 'Missing embedded atlas lengths'
    lengths = [int(value) for value in lengths[1].split(',') if value.strip()]
    assert len(lengths) == count and all(lengths)
    for pattern in ('*.cpp', '*.hpp', '*.h', 'CMakeLists.txt', 'package.json'):
        for path in source.glob(pattern):
            shutil.copy2(path, output / path.name)
    common = output.parent / 'common/motion-games'
    common.mkdir(parents=True, exist_ok=True)
    for name in ('sound.hpp', 'codec.hpp'):
        shutil.copy2(source.parent / 'common/motion-games' / name, common / name)
    shutil.copytree(source / 'assets', output / 'assets')
    kept = []
    for item in kept:
        destination = output / item['source']
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / item['source'], destination)
    resources['assets'] = kept
    (output / 'resources.json').write_text(json.dumps(resources, indent=2) + '\n')
    print(json.dumps({'staged': str(output), 'static_resources': len(kept),
                      'embedded_resources_omitted': count,
                      'duplicate_bytes_omitted': sum(
                          int(value) for value in re.search(
                              r'page_bytes\[[^]]*\]=\{([^}]+)\}',
                              (source / 'art.hpp').read_text())[1].split(','))}))


if __name__ == '__main__':
    main()
