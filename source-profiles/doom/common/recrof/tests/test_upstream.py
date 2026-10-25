import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('upstream',Path(__file__).resolve().parents[1]/'tools/upstream.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)

class UpstreamTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.base=Path(self.tmp.name)
        self.repo=self.base/'repo';self.repo.mkdir();self.root=self.base/'apps';self.root.mkdir()
        subprocess.run(['git','init','-q',str(self.repo)],check=True)
        (self.repo/'src').mkdir()
        self.text='first\n'+''.join(f'line{i}\n' for i in range(20))+'last\n'
        (self.repo/'src/code.c').write_text(self.text)
        (self.repo/'src/removed.c').write_text('old\n')
        self.old=self.commit()
        (self.root/'demo/vendor').mkdir(parents=True)
        (self.root/'demo/vendor/code.c').write_text(self.text.replace('first','PXA'))
        (self.root/'demo/vendor/removed.c').write_text('old\n')
        (self.root/'demo/upstream.json').write_text(json.dumps({'url':'https://example.test/engine','commit':self.old}))
        self.cfg={'metadata':['demo/upstream.json'],'maps':{'src':'demo/vendor'}}
        (self.repo/'src/code.c').write_text(self.text.replace('last','UPSTREAM'))
        (self.repo/'src/removed.c').unlink();(self.repo/'src/added.c').write_text('added\n')
        self.new=self.commit();self.review=self.base/'review'
    def tearDown(self):self.tmp.cleanup()
    def commit(self):
        u.git(self.repo,'add','.')
        u.git(self.repo,'-c','user.name=Test','-c','user.email=test@example.test','commit','-qm','fixture')
        return u.git(self.repo,'rev-parse','HEAD').stdout.decode().strip()
    def prepare(self):return u.prepare(self.root,self.cfg,self.repo,self.new,self.review)
    def test_merge_add_delete_and_pin(self):
        plan=self.prepare();self.assertFalse(any(x['conflict'] for x in plan['items']))
        self.assertIn('PXA',(self.root/'demo/vendor/code.c').read_text())
        self.assertEqual(u.apply(self.root,self.review,self.cfg),4)
        self.assertEqual((self.root/'demo/vendor/code.c').read_text(),self.text.replace('first','PXA').replace('last','UPSTREAM'))
        self.assertFalse((self.root/'demo/vendor/removed.c').exists())
        self.assertEqual(json.loads((self.root/'demo/upstream.json').read_text())['commit'],self.new)
        self.assertIn('PXA',(self.review/'adaptations.patch').read_text())
    def test_stale_edit_is_not_overwritten(self):
        self.prepare();(self.root/'demo/vendor/code.c').write_text('another edit')
        with self.assertRaisesRegex(ValueError,'changed after prepare'):u.apply(self.root,self.review,self.cfg)
        self.assertFalse((self.root/'demo/vendor/added.c').exists())
        self.assertEqual(json.loads((self.root/'demo/upstream.json').read_text())['commit'],self.old)
    def test_review_edit_requires_resolution(self):
        self.prepare();(self.review/'files/demo/vendor/code.c').write_text('edited')
        with self.assertRaisesRegex(ValueError,'without resolve'):u.apply(self.root,self.review,self.cfg)
    def test_conflict_and_explicit_resolution(self):
        (self.root/'demo/vendor/code.c').write_text(self.text.replace('last','LOCAL'))
        plan=self.prepare();self.assertTrue(any(x['conflict'] for x in plan['items']))
        with self.assertRaisesRegex(ValueError,'unresolved'):u.apply(self.root,self.review,self.cfg)
        with self.assertRaisesRegex(ValueError,'markers'):u.resolve(self.review,'demo/vendor/code.c','merged')
        u.resolve(self.review,'demo/vendor/code.c','upstream');u.apply(self.root,self.review,self.cfg)
        self.assertIn('UPSTREAM',(self.root/'demo/vendor/code.c').read_text())
    def test_binary_conflict(self):
        merged,conflict=u.merge(b'\0local',b'\0base',b'\0upstream')
        self.assertTrue(conflict);self.assertEqual(merged,b'\0local')
    def test_no_update_retains_local_patch(self):
        plan=u.prepare(self.root,self.cfg,self.repo,self.old,self.review)
        self.assertEqual(plan['items'],[]);self.assertIn('PXA',(self.review/'adaptations.patch').read_text())
    def test_symlink_and_traversal(self):
        (self.root/'escape').symlink_to(self.repo,target_is_directory=True)
        for name in ('../outside','escape/src/code.c','.git/config','demo//vendor','.'):
            with self.assertRaises(ValueError):u.safe_path(self.root,name)
    def test_mode_changed_after_prepare(self):
        self.prepare();(self.root/'demo/vendor/code.c').chmod(0o755)
        with self.assertRaisesRegex(ValueError,'mode changed'):u.apply(self.root,self.review,self.cfg)
    def test_plan_cannot_modify_another_app(self):
        self.prepare();p=self.review/'plan.json';plan=json.loads(p.read_text())
        plan['items'][0]['path']='browser/main.cpp';p.write_text(json.dumps(plan))
        with self.assertRaisesRegex(ValueError,'outside engine mappings'):u.apply(self.root,self.review,self.cfg)
    def test_non_utf8_adaptation_is_lossless(self):
        (self.root/'demo/vendor/code.c').write_bytes(self.text.encode().replace(b'first',b'PXA\xe9'))
        self.prepare();self.assertIn(b'PXA\xe9',(self.review/'adaptations.patch').read_bytes())
    def test_snapshot_size_is_bounded(self):
        with patch.object(u,'MAX_BYTES',10):
            with self.assertRaisesRegex(ValueError,'exceeds limit'):self.prepare()
        self.assertFalse(self.review.exists())
    def test_failed_apply_rolls_back(self):
        self.prepare();write=u.atomic_write;count=0
        def failing(*args):
            nonlocal count
            count+=1
            if count==2:raise OSError('simulated failure')
            return write(*args)
        with patch.object(u,'atomic_write',failing):
            with self.assertRaises(OSError):u.apply(self.root,self.review,self.cfg)
        self.assertFalse((self.root/'demo/vendor/added.c').exists())
        self.assertIn('PXA',(self.root/'demo/vendor/code.c').read_text())
        self.assertNotIn('UPSTREAM',(self.root/'demo/vendor/code.c').read_text())
        self.assertEqual(json.loads((self.root/'demo/upstream.json').read_text())['commit'],self.old)

if __name__=='__main__':unittest.main()
