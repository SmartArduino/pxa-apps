import importlib.util, tempfile, unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('import_data',Path(__file__).parents[1]/'tools/import_data.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class ImportTests(unittest.TestCase):
    def test_case_collision_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'pak0.pak').write_bytes(b'a');(root/'PAK0.PAK').write_bytes(b'b')
            with self.assertRaisesRegex(ValueError,'collision'):module.collect('quake',root)
    def test_missing_archive_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'Missing required'):module.collect('quake2',Path(d))
    def test_demo_layout(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);p=root/'Install/Data/baseq2';p.mkdir(parents=True);(p/'pak0.pak').write_bytes(b'demo')
            self.assertEqual(set(module.collect('quake2',root)),{'baseq2/pak0.pak'})
    def test_red_alert_loose_archives(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'LOCAL.MIX').write_bytes(b'local')
            self.assertEqual(set(module.collect('red-alert',root)),{'local.mix'})
    def test_cnc_dos_demo(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'DEMO.MIX').write_bytes(b'demo')
            self.assertEqual(set(module.collect('command-conquer',root)),{'demo.mix'})
    def test_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'real').write_bytes(b'data');(root/'PAK0.PAK').symlink_to(root/'real')
            with self.assertRaisesRegex(ValueError,'Symlinks'):module.collect('quake',root)

if __name__=='__main__':unittest.main()
