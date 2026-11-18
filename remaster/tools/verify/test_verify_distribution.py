import hashlib,json,tempfile,unittest
from pathlib import Path
from verify_distribution import audit,ENGINE
class Tests(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.r=Path(self.t.name);(self.r/'packs').mkdir();h=hashlib.sha256()
  names=ENGINE+['libbotf_core.web.template_release.wasm32.wasm']
  for n in names:
   b=n.encode();(self.r/n).write_bytes(b);h.update(b);h.update(b)
  self.bid=h.hexdigest()[:12];(self.r/'index.html').write_text('<meta name="botf-build" content="'+self.bid+'">');(self.r/'VERSION.txt').write_text('synthetic')
  self.p='art_web.'+hashlib.sha256(b'pack').hexdigest()[:12]+'.pck';(self.r/'packs'/self.p).write_bytes(b'pack')
  self.m={'build':self.bid,'files':{'art_web.pck':self.p},'bytes':{self.p:4}};self.s={'files':[{'file':n,'raw':(self.r/n).stat().st_size} for n in ['index.html',*names,'packs/'+self.p]]};self.save()
 def save(self):
  (self.r/'sizes.json').write_text(json.dumps(self.s));(self.r/'packs/manifest.json').write_text(json.dumps(self.m))
 def test_valid(self):
  x=audit(self.r);self.assertEqual(x['runtime_files'],9);self.assertTrue(all(len(f['sha256'])==64 for f in x['files']))
 def test_missing(self):
  (self.r/'packs'/self.p).unlink()
  with self.assertRaisesRegex(ValueError,'missing file'):audit(self.r)
 def test_corrupt_pack(self):
  (self.r/'packs'/self.p).write_bytes(b'FAIL')
  with self.assertRaisesRegex(ValueError,'hash mismatch'):audit(self.r)
 def test_size(self):
  (self.r/'packs'/self.p).write_bytes(b'X')
  with self.assertRaisesRegex(ValueError,'byte-size'):audit(self.r)
 def test_duplicate(self):
  self.s['files'].append(self.s['files'][0]);self.save()
  with self.assertRaisesRegex(ValueError,'duplicate'):audit(self.r)
 def test_paths(self):
  for name in ['../private','/private','C:\\private','packs/../private']:
   with self.subTest(name=name):
    self.s['files'][0]['file']=name;self.save()
    with self.assertRaisesRegex(ValueError,'unsafe'):audit(self.r)
 def test_symlink(self):
  (self.r/'index.js').unlink();(self.r/'index.js').symlink_to(self.r/'index.wasm')
  with self.assertRaisesRegex(ValueError,'symlink'):audit(self.r)
 def test_disagree(self):
  self.m['bytes'][self.p]=5;self.save()
  with self.assertRaisesRegex(ValueError,'disagree'):audit(self.r)
 def test_engine_build(self):
  self.m['build']='0'*12;self.save()
  with self.assertRaisesRegex(ValueError,'engine build'):audit(self.r)
 def test_html_build(self):
  p=self.r/'index.html';p.write_text(p.read_text().replace(self.bid,'0'*12))
  with self.assertRaisesRegex(ValueError,'HTML build'):audit(self.r)
 def test_corrupt_engine(self):
  p=self.r/'index.js';p.write_bytes(b'X'*p.stat().st_size)
  with self.assertRaisesRegex(ValueError,'engine build'):audit(self.r)
 def test_core_omitted(self):
  self.s['files']=[f for f in self.s['files'] if f['file']!='index.js'];self.save()
  with self.assertRaisesRegex(ValueError,'core file not listed'):audit(self.r)
if __name__=='__main__':unittest.main()
