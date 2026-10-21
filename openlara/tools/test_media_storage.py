import hashlib
import tempfile
import unittest
from pathlib import Path
from media_storage import ROOT, external_destination, media_plan


class MediaPlanTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = {'files': []}
        for name, data in [('levels.idx', b'catalog'), ('level2.phd', b'level'), ('tracks.ad4', b'music')]:
            (self.root / name).write_bytes(data)
            self.manifest['files'].append({'file': name, 'bytes': len(data),
                                          'sha256': hashlib.sha256(data).hexdigest()})

    def test_catalog_last_and_no_hardcoded_mount(self):
        plan = media_plan(self.root, self.manifest, 'a' * 64)
        self.assertEqual(plan['app_id'], 'pxa-openlara')
        self.assertEqual(plan['files'][-1]['path'], 'levels.idx')
        self.assertEqual(plan['media_root'], 'host-resolved at deployment')

    def test_changed_resource_rejected_before_deployment(self):
        (self.root / 'level2.phd').write_bytes(b'other')
        with self.assertRaisesRegex(ValueError, 'changed'):
            media_plan(self.root, self.manifest, 'a' * 64)

    def test_resource_cannot_be_a_link(self):
        (self.root / 'level2.phd').unlink()
        (self.root / 'level2.phd').symlink_to(self.root / 'tracks.ad4')
        with self.assertRaisesRegex(ValueError, 'linked'):
            media_plan(self.root, self.manifest, 'a' * 64)

    def test_traversal_rejected(self):
        self.manifest['files'][0]['file'] = '../levels.idx'
        with self.assertRaisesRegex(ValueError, 'filename'):
            media_plan(self.root, self.manifest, 'a' * 64)

    def test_all_packaged_media_destinations_rejected(self):
        for name in ('assets', 'assets-simulator', 'assets-esp32s31'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'outside package'):
                external_destination(ROOT / name / 'game')


if __name__ == '__main__':
    unittest.main()
