#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise missing embedded catalog/message/fallback without runtime path overrides."""
from pathlib import Path
import os,shlex,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];build=Path(sys.argv[1]);cxx=sys.argv[2]
def run(args):subprocess.run(args,check=True,env=dict(os.environ,QT_QPA_PLATFORM='offscreen'))
libexec=Path(subprocess.check_output(['qmake6','-query','QT_HOST_LIBEXECS'],text=True).strip())
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Gui'],text=True))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)
 (p/'ar.ts').write_text('<TS language="ar" version="2.1"><context><name></name><message id="synapse.monitor.title"><source>synapse.monitor.title</source><translation>TEST AR</translation></message></context></TS>')
 run(['lrelease6','-silent','-fail-on-unfinished','-fail-on-invalid',str(p/'ar.ts'),'-qm',str(p/'ar.qm')])
 (p/'partial.qrc').write_text('<RCC><qresource prefix="/i18n"><file alias="synapse-monitor_ar.qm">'+str(p/'ar.qm')+'</file><file alias="synapse-monitor_en_US.qm">'+str(build/'i18n/synapse-monitor_en_US.qm')+'</file></qresource></RCC>')
 run([str(libexec/'rcc'),str(p/'partial.qrc'),'-o',str(p/'partial.cpp')])
 for mode in ['partial','missing']:
  command=[cxx,'-std=c++17','-fPIC','-fstack-protector-strong','-march=x86-64','-I'+str(root/'gui'),str(root/'tests/test_localization.cpp'),str(root/'gui/localization.cpp'),str(build/'moc_localization.cpp')]
  if mode=='partial':command.append(str(p/'partial.cpp'))
  run(command+flags+['-o',str(p/mode)])
  run([str(p/mode),mode])
