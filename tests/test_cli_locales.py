#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""All pinned locale requests leave informative CLI contracts byte-identical."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];binary=sys.argv[1]
locales=[r['synapseId'] for r in json.loads((root/'gui/i18n/locales.json').read_text())['locales']]
with tempfile.TemporaryDirectory() as home:
 for args in [['--version'],['--help'],['describe','--format','json']]:
  expected=None
  for locale in ['C',*locales]:
   r=subprocess.run([binary,*args],env=dict(os.environ,HOME=home,LANG=locale,LC_MESSAGES=locale,LC_ALL=locale),capture_output=True,check=True,timeout=10)
   pair=(r.stdout,r.stderr)
   if expected is None:expected=pair
   assert pair==expected,(args,locale)
 assert not list(Path(home).iterdir()),'Informative commands wrote user state'
print('CLI locale invariance: 3 contracts across C and all 64 pinned requests; no user state writes.')
