#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C
core=${1:?core binary required}
gui=${2:?gui binary required}
adapter_test=${3:?adapter test required}
repo=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

[[ $(QT_QPA_PLATFORM=offscreen "$gui" --version) == 'synapse-monitor-gui 0.5.0-alpha.7' ]]
set +e
QT_QPA_PLATFORM=offscreen timeout 2 "$gui" --backend "$core" \
  >"$work/refused.stdout" 2>"$work/refused.stderr"
refused_status=$?
set -e
[[ $refused_status == 2 ]]
SYNAPSE_MONITOR_TEST_CORE="$core" "$adapter_test"
qmltestrunner=${QMLTESTRUNNER6:-$(qmake6 -query QT_HOST_BINS)/qmltestrunner}
[[ -x $qmltestrunner ]]
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
  "$qmltestrunner" -input "$repo/tests" -import "$repo/gui/qml" -o -,txt

for view in processes performance services startup connections information; do
  size=1280x800
  [[ $view != processes ]] || size=700x520
  SYNAPSE_MONITOR_ALLOW_TEST_BACKEND=1 QT_QPA_PLATFORM=offscreen \
    QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software QML_DISABLE_DISK_CACHE=1 \
    "$gui" --backend "$core" --view "$view" --locale it_IT \
      --test-exit-after-frames 1 --test-ready-timeout 10000 \
      --test-window-size "$size" --test-grab "$work/$view.png" \
      >"$work/$view.stdout" 2>"$work/$view.stderr"
  test -s "$work/$view.png"
  grep -Fq 'synapse-monitor-gui: typography=monospace resolved=' "$work/$view.stderr"
  ! grep -Eiq 'qrc:|QQml|TypeError|ReferenceError|Unable to assign|binding loop' \
    "$work/$view.stderr"
done

QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
  QML_DISABLE_DISK_CACHE=1 "$gui" --view information --locale en_US \
    --test-exit-after-frames 1 --test-ready-timeout 10000 \
    --test-window-size 900x600 --test-grab "$work/sibling-discovery.png" \
    >"$work/sibling-discovery.stdout" 2>"$work/sibling-discovery.stderr"
test -s "$work/sibling-discovery.png"
! grep -Eiq 'qrc:|QQml|TypeError|ReferenceError|Unable to assign|binding loop' \
  "$work/sibling-discovery.stderr"

python3 - "$repo/gui/i18n/synapse-monitor_en_US.ts" \
  "$repo/gui/i18n/synapse-monitor_it_IT.ts" <<'PY'
import sys,xml.etree.ElementTree as ET
sets=[]
for path in sys.argv[1:]:
 root=ET.parse(path).getroot();ids=[]
 for message in root.findall('.//message'):
  mid=message.get('id');translation=message.find('translation')
  assert mid and translation is not None and translation.text
  ids.append(mid)
 assert len(ids)==len(set(ids)) and len(ids)>=90
 sets.append(set(ids))
assert sets[0]==sets[1]
PY

python3 - "$repo/gui/qml/DataTable.qml" "$repo/gui/qml/ProcessesView.qml" \
  "$repo/gui/qml/InventoryView.qml" <<'PY'
import pathlib,sys
header=pathlib.Path(sys.argv[1]).read_text()
assert 'signal sortRequested(string sortId)' in header
assert 'onClicked: root.requestSort(headerCell.modelData)' in header
assert 'Keys.onReturnPressed: root.requestSort(headerCell.modelData)' in header
assert 'root.activeSortId === headerCell.sortId' in header
assert 'columnFilterOptions' in header and 'setColumnFilter' in header
assert 'synapse.monitor.filter.select-all' in header
for path in sys.argv[2:]:
 text=pathlib.Path(path).read_text()
 definitions=[line for line in text.splitlines() if '{ key:' in line]
 assert definitions and all('sortId:' in line for line in definitions), path
 assert 'sortableIds: root.adapter.sortIds' in text
 assert 'activeSortId: root.adapter.sortId' in text
 assert 'sortAscending: root.adapter.sortAscending' in text
 assert 'activeFilterIds: root.adapter.filteredColumnIds' in text
 assert 'filterController: root.adapter' in text
 assert 'root.adapter.requestSort(sortId)' in text
assert 'key: "uid", sortId: "user"' in pathlib.Path(sys.argv[2]).read_text()
assert 'key: "state", sortId: "status"' in pathlib.Path(sys.argv[3]).read_text()
assert 'key: "location", sortId: "location"' in pathlib.Path(sys.argv[3]).read_text()
PY

! grep -R -n -E '(/usr/bin|/usr/local|QProcess|subprocess|Process\s*\{|argv|system\(|popen\(|shell)' \
  "$repo/gui/qml"
grep -R -q 'monitorAdapter.selectView' "$repo/gui/qml"
grep -R -q 'monitorAdapter.setIntervalMilliseconds' "$repo/gui/qml"
grep -R -q 'adapter.inspectProcess' "$repo/gui/qml"

echo 'synapse-monitor GUI tests: PASS'
