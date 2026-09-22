#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source/distribution invariants; no GUI session or package manager."""
from pathlib import Path
import json,subprocess,tempfile,unittest,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
class Distribution(unittest.TestCase):
 def test_all_pinned_catalogs_present(self):
  self.assertEqual(64,len(list((ROOT/'gui/i18n').glob('*.ts'))))
  manifest=json.loads((ROOT/'gui/i18n/locales.json').read_text())
  self.assertEqual({x['synapseId'] for x in manifest['locales']},{p.stem.removeprefix('synapse-monitor_') for p in (ROOT/'gui/i18n').glob('*.ts')})
 def test_catalog_coverage_is_explicit(self):
  p=ROOT/'gui/i18n/coverage.json';self.assertTrue(p.exists())
  coverage=json.loads(p.read_text());self.assertFalse(coverage['productionComplete'])
  self.assertEqual(64,len(coverage['catalogs']))
  for loc,counts in coverage['catalogs'].items():
   messages=ET.parse(ROOT/f'gui/i18n/synapse-monitor_{loc}.ts').findall('.//message')
   self.assertEqual(len(messages),counts['authored']+counts['fallback'])
 def test_core_only_does_not_discover_qt(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);probe=p/'qt-probe';marker=p/'called'
   probe.write_text(f'#!/bin/sh\ntouch "{marker}"\nexit 1\n');probe.chmod(0o755)
   r=subprocess.run(['make','-n','BUILD_GUI=0',f'QMAKE6={probe}',f'PKG_CONFIG={probe}', 'cli'],cwd=ROOT,capture_output=True)
   self.assertEqual(0,r.returncode,r.stderr.decode());self.assertFalse(marker.exists())
 def test_split_install_targets_exist(self):
  text=(ROOT/'Makefile').read_text()
  self.assertIn('install-cli:',text);self.assertIn('install-gui:',text)
  self.assertIn('-fail-on-unfinished',text)
 def test_invalid_gui_mode_fails_closed(self):
  for mode in ['typo','','0 1']:
   r=subprocess.run(['make','-n','BUILD_GUI='+mode,'cli'],cwd=ROOT,capture_output=True)
   self.assertNotEqual(0,r.returncode)
 def test_gui_diagnostic_guards_fail_closed(self):
  helper=ROOT/'tests/assert-no-match.sh';self.assertTrue(helper.exists())
  with tempfile.TemporaryDirectory() as tmp:
   sample=Path(tmp)/'log';sample.write_text('QML diagnostic\n')
   for pattern,path,expected in [('absent',str(sample),0),('QML',str(sample),1),('QML',str(sample)+'-missing',2)]:
    r=subprocess.run(['bash','-c','source "$1"; set -e; require_no_match -q "$2" "$3"; echo success','test',str(helper),pattern,path],capture_output=True)
    self.assertEqual(expected,r.returncode)
    if expected:self.assertNotIn(b'success',r.stdout)
 def test_public_docs_do_not_retain_research_comparisons(self):
  for p in [ROOT/'README.md',ROOT/'CHANGELOG.md',*(ROOT/'docs').glob('*.md')]:
   self.assertNotIn('Excel-style',p.read_text(),str(p))
if __name__=='__main__':unittest.main()
