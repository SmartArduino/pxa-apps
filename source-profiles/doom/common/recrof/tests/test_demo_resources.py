import copy
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile


SPEC = importlib.util.spec_from_file_location('demo_resources', Path(__file__).resolve().parents[1] / 'tools/build_demo_resources.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DemoResourcesTest(unittest.TestCase):
    def test_preserves_unmodified_doom_shareware_with_original_notice(self):
        recipes = Path(__file__).resolve().parents[1] / 'resources'
        notice = recipes / 'doom-shareware-license.txt'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / 'doom1.wad'
            original.write_bytes(b'IWAD: test original shareware bytes')
            entry = {'archive': {'bytes': original.stat().st_size,
                'sha256': MODULE.digest(original), 'url': 'https://example.test/doom1.wad'},
                'runtime_file': 'doom1.wad', 'license_file': notice.name,
                'license_sha256': MODULE.digest(notice), 'license_source': 'https://example.test/notice',
                'title': 'DOOM shareware', 'copyright': 'id Software', 'license': 'Original terms'}
            payload = root / 'payload'
            MODULE.prepare(entry, original, payload)
            self.assertEqual((payload / 'doom1.wad').read_bytes(), original.read_bytes())
            self.assertEqual((payload / 'licenses/demo-license.txt').read_bytes(), notice.read_bytes())
            self.assertEqual(sum(p.stat().st_size == original.stat().st_size for p in payload.rglob('*') if p.is_file()), 1)
            entry['license_sha256'] = '0' * 64
            with self.assertRaisesRegex(ValueError, 'license'):
                MODULE.prepare(entry, original, root / 'bad-license')
            self.assertFalse((root / 'bad-license').exists())
            entry['runtime_file'] = '../outside'
            with self.assertRaisesRegex(ValueError, 'Unsafe'):
                MODULE.prepare(entry, original, root / 'bad-path')

    def fixture(self, root, extra=None):
        archive = root / 'original.exe'
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('license.txt', b'Original agreement\r\n')
            output.writestr('Install/Data/baseq2/pak0.pak', b'original game bytes')
            output.writestr('Install/Data/baseq2/players/male/grunt.pcx', b'original skin')
            output.writestr('setup.exe', b'original installer')
            if extra:
                output.writestr(extra, b'invalid')
        entry = {'archive': {'name': 'original.exe', 'bytes': archive.stat().st_size,
            'sha256': MODULE.digest(archive)}, 'runtime_prefix': 'Install/Data/baseq2/',
            'runtime_root': 'baseq2', 'license_member': 'license.txt',
            'required_files': {'baseq2/pak0.pak': {'bytes': 19,
                'sha256': hashlib.sha256(b'original game bytes').hexdigest()}},
            'title': 'Demo', 'copyright': 'Original author', 'license': 'Demo license'}
        entry['required_files']['baseq2/pak0.pak']['bytes'] = len(b'original game bytes')
        return archive, entry

    def test_preserves_complete_original_and_runtime_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive, entry = self.fixture(root)
            payload = root / 'payload'
            MODULE.prepare(entry, archive, payload)
            self.assertEqual((payload / 'original/original.exe').read_bytes(), archive.read_bytes())
            self.assertEqual((payload / 'licenses/demo-license.txt').read_bytes(), b'Original agreement\r\n')
            self.assertEqual((payload / 'baseq2/pak0.pak').read_bytes(), b'original game bytes')
            self.assertEqual((payload / 'baseq2/players/male/grunt.pcx').read_bytes(), b'original skin')

    def test_rejects_archive_corruption_before_creating_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive, entry = self.fixture(root)
            archive.write_bytes(archive.read_bytes() + b'changed')
            with self.assertRaises(ValueError):
                MODULE.prepare(entry, archive, root / 'payload')
            self.assertFalse((root / 'payload').exists())

    def test_rejects_unsafe_and_case_colliding_runtime_paths(self):
        for extra in ('Install/Data/baseq2/../../outside', 'Install/Data/baseq2/PAK0.PAK'):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive, entry = self.fixture(root, extra)
                with self.assertRaises(ValueError):
                    MODULE.prepare(entry, archive, root / 'payload')
                self.assertFalse((root / 'payload').exists())

    def test_rejects_wrong_runtime_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive, entry = self.fixture(root)
            entry = copy.deepcopy(entry)
            entry['required_files']['baseq2/pak0.pak']['sha256'] = '0' * 64
            with self.assertRaisesRegex(ValueError, 'Runtime file'):
                MODULE.prepare(entry, archive, root / 'payload')


if __name__ == '__main__':
    unittest.main()
