#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import contextlib,importlib.util,io,json,shutil,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('catalogs',ROOT/'tools/catalogs.py');catalogs=importlib.util.module_from_spec(spec);spec.loader.exec_module(catalogs)
class CatalogValidation(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
  shutil.copytree(ROOT/'gui',self.root/'gui')
  catalogs.ROOT=self.root;catalogs.DIR=self.root/'gui/i18n'
 def tearDown(self):self.tmp.cleanup()
 def check(self):
  with contextlib.redirect_stdout(io.StringIO()):catalogs.check()
 def test_current(self):self.check()
 def test_changed_inventory_rejected(self):
  p=catalogs.DIR/'locales.json';p.write_text(p.read_text()+'\n')
  with self.assertRaises(AssertionError):self.check()
 def test_missing_catalog_rejected(self):
  (catalogs.DIR/'synapse-monitor_ar.ts').unlink()
  with self.assertRaises(AssertionError):self.check()
 def test_fallback_cannot_claim_authored_translation(self):
  p=catalogs.DIR/'coverage.json';d=json.loads(p.read_text());d['catalogs']['ar']['authored']=157;d['catalogs']['ar']['fallback']=0;p.write_text(json.dumps(d))
  with self.assertRaises(AssertionError):self.check()
 def test_unfinished_rejected(self):
  p=catalogs.DIR/'synapse-monitor_it_IT.ts';p.write_text(p.read_text().replace('<translation>','<translation type="unfinished">',1))
  with self.assertRaises(AssertionError):self.check()
 def test_placeholder_change_rejected(self):
  p=catalogs.DIR/'synapse-monitor_it_IT.ts';p.write_text(p.read_text().replace('<translation>','<translation>%99 ',1))
  with self.assertRaises(AssertionError):self.check()
 def test_unknown_qml_id_rejected(self):
  (self.root/'gui/qml/Invalid.qml').write_text('qsTrId("synapse.monitor.unknown")')
  with self.assertRaises(AssertionError):self.check()
if __name__=='__main__':unittest.main()
