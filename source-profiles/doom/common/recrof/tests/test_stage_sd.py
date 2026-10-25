import json
from pathlib import Path
import sys
import tempfile
import unittest
TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import stage_sd

PUBLISHER = '17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171'

class MediaLayout(unittest.TestCase):
    def test_plan_fixed_root_and_imported_files_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'quake').mkdir()
            (root/'quake/package.json').write_text(json.dumps({'id':'pxa-quake'}))
            data = root/'.game-data/quake/files';data.mkdir(parents=True)
            (data/'pak0.pak').write_bytes(b'converted fixture')
            (data/'catalog.txt').write_text('metadata')
            layout = stage_sd.plan(root, 'quake', PUBLISHER)
            self.assertEqual([f['path'] for f in layout['files']], ['pak0.pak'])
            self.assertEqual(layout['files'][0]['bytes'], 17)
            self.assertEqual(layout['app_id'], 'pxa-quake')
            self.assertEqual(layout['media_root'], 'host-resolved at deployment')
            self.assertEqual(len(layout['files'][0]['sha256']), 64)

    def test_wrong_manifest_id_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'quake').mkdir()
            (root/'quake/package.json').write_text(json.dumps({'id':'pxa-other'}))
            with self.assertRaisesRegex(ValueError,'unexpected application ID'):
                stage_sd.plan(root,'quake',PUBLISHER)

    def test_invalid_publisher_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'quake').mkdir()
            (root/'quake/package.json').write_text(json.dumps({'id':'pxa-quake'}))
            for publisher in [PUBLISHER.upper(),'0'*63,'../author']:
                with self.assertRaisesRegex(ValueError,'publisher key ID'):
                    stage_sd.plan(root,'quake',publisher)

    def test_nested_parent_creation_before_transfer(self):
        class Files:
            def __init__(self):self.tree={'':{'sdcard':'D'},'sdcard':{}};self.created=[]
            def list(self, parent):return [(kind,0,name) for name,kind in self.tree[parent].items()]
            def mkdir(self, path):
                parent,_,name=path.rpartition('/')
                self.tree[parent][name]='D';self.tree[path]={};self.created.append(path)
        fs=Files();path='sdcard/PXA/media/key/baseq2/pak0.pak'
        stage_sd.ensure_parent(fs,path)
        self.assertEqual(fs.created,['sdcard/PXA','sdcard/PXA/media','sdcard/PXA/media/key','sdcard/PXA/media/key/baseq2'])
        stage_sd.ensure_parent(fs,path);self.assertEqual(len(fs.created),4)
        missing=Files();missing.tree['']={}
        with self.assertRaisesRegex(ValueError,'SD storage is unavailable'):
            stage_sd.ensure_parent(missing,path)
        self.assertEqual(missing.created,[])
        fs.tree['sdcard/PXA/media/key']['collision']='F'
        with self.assertRaises(ValueError):stage_sd.ensure_parent(fs,'sdcard/PXA/media/key/collision/data')

    def test_wrong_device_does_not_mutate(self):
        class Client:
            device_info = 'serial=wrong'
            def __enter__(self):return self
            def __exit__(self, *args):pass
            def request(self, command):raise AssertionError('Unexpected request')
        class Fake:
            @staticmethod
            def open_client(*args):return Client()
            @staticmethod
            def info_properties(value):return {'serial':'wrong'}
        with self.assertRaisesRegex(ValueError, 'serial differs'):
            stage_sd.deploy(Fake(), '/dev/test', 'expected', {})

class HostPaths(unittest.TestCase):
    def test_host_authoritative_binding(self):
        from types import SimpleNamespace
        class Client:
            def request(self, command):
                self.command=command
                return [SimpleNamespace(kind='PATH',payload='private\tstate/data/author/quake'),
                        SimpleNamespace(kind='PATH',payload='media\tcard2/PXA/media/author/quake')]
        c=Client();paths=stage_sd.package_paths(c,PUBLISHER+':pxa-quake')
        self.assertEqual(paths['media'],'card2/PXA/media/author/quake')
        self.assertEqual(c.command,'PACKAGE paths '+PUBLISHER+':pxa-quake')
    def test_unsupported_host_does_not_guess(self):
        class Client:
            def request(self, command):return []
        with self.assertRaisesRegex(ValueError,'Firmware does not support'):
            stage_sd.package_paths(Client(),PUBLISHER+':pxa-quake')

if __name__ == '__main__':unittest.main()
