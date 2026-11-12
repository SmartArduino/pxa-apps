# SPDX-License-Identifier: GPL-3.0-or-later
"""Check pinned downloads, phase identity and resource package binding."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('freedoom_data', Path(__file__).resolve().parents[1] / 'tools/prepare-data.py')
data = importlib.util.module_from_spec(spec)
spec.loader.exec_module(data)


def wad(phase):
    names = [b'PLAYPAL', b'COLORMAP', b'TITLEPIC', b'FREEDOOM', b'E1M1' if phase == 1 else b'MAP01']
    return b'IWAD' + struct.pack('<II', len(names), 12) + b''.join(struct.pack('<II8s', 0, 0, n) for n in names)


class PrepareTests(unittest.TestCase):
    def test_identity_and_phase(self):
        for phase in (1, 2):
            self.assertEqual(data.validate_wad(wad(phase), phase)['lumps'], 5)
            with self.assertRaises(ValueError):
                data.validate_wad(wad(phase), 3 - phase)
        with self.assertRaises(ValueError):
            data.validate_wad(wad(1).replace(b'FREEDOOM', b'ORIGINAL'), 1)

    def test_invalid_boundaries(self):
        for contents in (b'', b'PWAD' + wad(1)[4:], b'IWAD' + struct.pack('<II', 65536, 0x7fffffff)):
            with self.assertRaises(ValueError):
                data.validate_wad(contents, 1)

    def test_archive_integrity_notices_and_precedence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / 'release.zip'
            payload = {'freedoom1.wad': wad(1), 'freedoom2.wad': wad(2),
                       **{n: b'Original license and credits' for n in data.NOTICES}}
            with zipfile.ZipFile(archive, 'w') as z:
                for n, value in payload.items():
                    z.writestr('freedoom-test/' + n, value)
            recipe = root / 'recipe.json'
            recipe.write_text(json.dumps({'version': 'test', 'commit': 'test-commit',
                'archive': {'name': archive.name, 'url': 'https://invalid.test/release',
                            'bytes': archive.stat().st_size, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()},
                'members': {n: {'bytes': len(v), 'sha256': hashlib.sha256(v).hexdigest()} for n, v in payload.items()}}))
            with patch.object(data, 'RECIPE', recipe):
                output = root / 'files'
                result = data.prepare(output, (1, 2), archive)
                self.assertFalse(result['modified'])
                for n in data.NOTICES:
                    self.assertEqual((output / 'licenses' / n).read_bytes(), payload[n])
                self.assertEqual((output / 'freedoom1.wad').read_bytes(), wad(1))
                with self.assertRaises(ValueError):
                    data.prepare(output, (1,), archive)
                archive.write_bytes(archive.read_bytes()[:-1] + b'!')
                with self.assertRaisesRegex(ValueError, 'SHA-256'):
                    data.prepare(root / 'invalid', (1,), archive)

    def test_reject_package_output(self):
        with self.assertRaises(ValueError):
            data.prepare(data.APP / 'assets/game')


if __name__ == '__main__':
    unittest.main()
