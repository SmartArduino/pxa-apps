import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace as NS
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import migrate_sd
from test_stage_sd import PUBLISHER

class Files:
    def __init__(self):
        self.files={'state/old/data/save':b'save', 'card/PXA/media/old/pak0.pak':b'game'}
        self.directories={'','state','state/old','state/old/data','state/new','card','card/PXA',
                          'card/PXA/media','card/PXA/media/old','card/PXA/media/new'}
        self.moves=[]
    def list(self,root):
        if root not in self.directories:raise ValueError('missing directory '+root)
        prefix=root+'/' if root else ''
        return [('D',0,p[len(prefix):]) for p in sorted(self.directories) if p.startswith(prefix) and p!=root and '/' not in p[len(prefix):]] + [
          ('F',len(data),p[len(prefix):]) for p,data in sorted(self.files.items()) if p.startswith(prefix) and '/' not in p[len(prefix):]]
    def mkdir(self,path):self.directories.add(path)
    def _expected_digest(self,path,size):
        data=self.files[path];return hashlib.sha256(data).hexdigest(),len(data)
    def get(self,path,local):local.write_bytes(self.files[path])
    def move(self,source,target):
        assert target not in self.files
        self.files[target]=self.files.pop(source);self.moves.append((source,target))

class Client:
    device_info='serial=device'
    def __init__(self,files):self.files=files;self.commands=[];self.active=False
    def request(self,command,**kwargs):
        self.commands.append(command)
        if command=='PACKAGES':return [NS(kind='PKG',payload=f'{PUBLISHER}:pxa-quake\tQuake\t0.5.0\tinstalled=1;active={int(self.active)}')]
        if command.startswith('PACKAGE paths '):
            old=command.endswith('-sd');leaf='old' if old else 'new'
            return [NS(kind='PATH',payload='private\tstate/'+leaf),NS(kind='PATH',payload='media\tcard/PXA/media/'+leaf)]
        if command.startswith('PACKAGE move-data '):
            if 'state/old/data/save' in self.files.files:
                assert 'state/new/data' not in self.files.directories
                self.files.move('state/old/data/save','state/new/data/save')
                self.files.directories.remove('state/old/data');self.files.directories.add('state/new/data')
            return []
        raise AssertionError(command)
class PXADB:
    @staticmethod
    def info_properties(info):return dict(x.split('=',1) for x in info.split(';'))
    @staticmethod
    def NormalFsClient(client):return client.files

class Migration(unittest.TestCase):
    def test_plan_is_read_only(self):
        fs=Files();c=Client(fs)
        record=migrate_sd.migrate(c,PXADB,'quake',PUBLISHER,'device')
        self.assertFalse(record['complete']);self.assertEqual(fs.moves,[])
        self.assertFalse(any('move-data' in cmd for cmd in c.commands))
    def test_verified_backup_move_and_retry(self):
        fs=Files();c=Client(fs)
        with tempfile.TemporaryDirectory() as directory:
            backup=Path(directory)
            result=migrate_sd.migrate(c,PXADB,'quake',PUBLISHER,'device',backup,True)
            self.assertTrue(result['complete']);self.assertEqual((backup/'saves/save').read_bytes(),b'save')
            self.assertEqual(fs.files,{'state/new/data/save':b'save','card/PXA/media/new/pak0.pak':b'game'})
            again=migrate_sd.migrate(c,PXADB,'quake',PUBLISHER,'device',backup,True)
            self.assertTrue(again['complete']);self.assertEqual(len(fs.moves),2)
    def test_conflicting_resource_keeps_saves_and_source(self):
        fs=Files();fs.files['card/PXA/media/new/pak0.pak']=b'other';c=Client(fs)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError,'Different destination'):
                migrate_sd.migrate(c,PXADB,'quake',PUBLISHER,'device',Path(directory),True)
        self.assertEqual(fs.moves,[])
    def test_wrong_device_or_publisher_and_active_refused(self):
        for publisher,serial,active in [(PUBLISHER,'wrong',False),('0'*64,'device',False),(PUBLISHER,'device',True)]:
            fs=Files();c=Client(fs);c.active=active
            with self.assertRaises(ValueError):migrate_sd.migrate(c,PXADB,'quake',publisher,serial)
            self.assertEqual(fs.moves,[])
    def test_corrupt_backup_refused_before_mutation(self):
        fs=Files();c=Client(fs)
        with tempfile.TemporaryDirectory() as directory:
            backup=Path(directory);(backup/'saves').mkdir();(backup/'saves/save').write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError,'backup differs'):
                migrate_sd.migrate(c,PXADB,'quake',PUBLISHER,'device',backup,True)
        self.assertEqual(fs.moves,[])

if __name__=='__main__':unittest.main()
