#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C
binary=${1:?binary required}
[[ $($binary --version) == 'synapse-monitor 0.1.0-alpha.1' ]]
$binary --help | grep -Fq 'The command is read-only'

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
root=$work/root
proc=$root/proc
sys=$root/sys
mkdir -p "$proc/net" "$sys/class/block/sda/device" \
  "$sys/class/drm/card0/device"

python3 - "$proc" "$sys" <<'PY'
import os,sys
proc,sysp=sys.argv[1:]
uid=os.getuid()
system_uid=65534 if uid==0 else 0

def stat_line(pid,name,state,utime,stime,threads,start,rss):
    f={i:0 for i in range(3,53)}
    f.update({3:state,14:utime,15:stime,20:threads,22:start,23:1000000,24:rss})
    return f"{pid} ({name}) "+" ".join(str(f[i]) for i in range(3,53))+"\n"

def atomic(path,text,binary=False):
    tmp=path+'.new'
    mode='wb' if binary else 'w'
    with open(tmp,mode) as h:h.write(text)
    os.replace(tmp,path)

def write(phase):
    if phase==0:
        cpu='cpu 100 0 50 850 0 0 0 0 0 0\n'
        disk='8 0 sda 10 0 100 0 20 0 200 0 0 0 0 0 0 0\n'
        net='Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n  lo: 10 0 0 0 0 0 0 0 10 0 0 0 0 0 0 0\neth0: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n'
        values={100:(20,10,1000,2000),200:(2,1,0,0),300:(5,5,100,100)}
    else:
        cpu='cpu 120 0 60 920 0 0 0 0 0 0\n'
        disk='8 0 sda 10 0 110 0 20 0 220 0 0 0 0 0 0 0\n'
        net='Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n  lo: 10 0 0 0 0 0 0 0 10 0 0 0 0 0 0 0\neth0: 1500 0 0 0 0 0 0 0 2600 0 0 0 0 0 0 0\n'
        values={100:(30,10,2000,4000),200:(2,1,0,0),300:(5,5,100,100)}
    atomic(os.path.join(proc,'stat'),cpu)
    atomic(os.path.join(proc,'diskstats'),disk)
    atomic(os.path.join(proc,'net/dev'),net)
    for pid,name,state,owner,cmd,start,rss in [
        (100,'alpha','R',uid,b'/usr/bin/alpha\0',1000,100),
        (200,'kworker/0:1','S',0,b'',2000,0),
        (300,'daemon','S',system_uid,b'/usr/bin/daemon\0',3000,50),
    ]:
        d=os.path.join(proc,str(pid));os.makedirs(d,exist_ok=True)
        utime,stime,readb,writeb=values[pid]
        atomic(os.path.join(d,'stat'),stat_line(pid,name,state,utime,stime,2,start,rss))
        atomic(os.path.join(d,'status'),f'Name:\t{name}\nUid:\t{owner}\t{owner}\t{owner}\t{owner}\n')
        atomic(os.path.join(d,'cmdline'),cmd,binary=True)
        atomic(os.path.join(d,'io'),f'read_bytes: {readb}\nwrite_bytes: {writeb}\n')
    bad=os.path.join(proc,'400');os.makedirs(bad,exist_ok=True)
    atomic(os.path.join(bad,'stat'),'x'*5000)

os.makedirs(proc,exist_ok=True)
atomic(os.path.join(proc,'meminfo'),'MemTotal:       1000000 kB\nMemAvailable:    400000 kB\n')
write(0)
atomic(os.path.join(sysp,'class/drm/card0/device/gpu_busy_percent'),'42\n')
atomic(os.path.join(sysp,'class/drm/card0/device/mem_info_vram_total'),'1000000\n')
atomic(os.path.join(sysp,'class/drm/card0/device/mem_info_vram_used'),'250000\n')
with open(os.path.join(proc,'.advance.py'),'w') as h:
    h.write('')
PY

advance() {
  sleep 0.05
  python3 - "$proc" <<'PY'
import os,sys
proc=sys.argv[1]
def atomic(path,text):
 p=path+'.next';open(p,'w').write(text);os.replace(p,path)
def stat_line(pid,name,state,utime,stime,threads,start,rss):
 f={i:0 for i in range(3,53)};f.update({3:state,14:utime,15:stime,20:threads,22:start,23:1000000,24:rss})
 return f"{pid} ({name}) "+" ".join(str(f[i]) for i in range(3,53))+"\n"
atomic(proc+'/stat','cpu 120 0 60 920 0 0 0 0 0 0\n')
atomic(proc+'/diskstats','8 0 sda 10 0 110 0 20 0 220 0 0 0 0 0 0 0\n')
atomic(proc+'/net/dev','Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n  lo: 10 0 0 0 0 0 0 0 10 0 0 0 0 0 0 0\neth0: 1500 0 0 0 0 0 0 0 2600 0 0 0 0 0 0 0\n')
for pid,name,state,utime,stime,start,rss,readb,writeb in [
 (100,'alpha','R',30,10,1000,100,2000,4000),
 (200,'kworker/0:1','S',2,1,2000,0,0,0),
 (300,'daemon','S',5,5,3001,50,100,100)]:
 atomic(f'{proc}/{pid}/stat',stat_line(pid,name,state,utime,stime,2,start,rss))
 atomic(f'{proc}/{pid}/io',f'read_bytes: {readb}\nwrite_bytes: {writeb}\n')
PY
}

export SYNAPSE_MONITOR_ALLOW_TEST_ROOTS=1
export SYNAPSE_MONITOR_PROC_ROOT=$proc
export SYNAPSE_MONITOR_SYS_ROOT=$sys
advance &
$binary snapshot --format json --sample-ms 100 --limit 6 >"$work/snapshot.json"
wait
python3 - "$work/snapshot.json" <<'PY'
import json,sys
raw=open(sys.argv[1],'rb').read();assert raw.endswith(b'\n') and raw.count(b'\n')==1
x=json.loads(raw)
assert x['schema']=='synapse.monitor.snapshot/v1' and x['readOnly'] is True
assert x['summary']['cpu']['available'] and x['summary']['cpu']['busyPercentMilli']==30000
assert x['summary']['memory']=={'available':True,'totalBytes':1024000000,'availableBytes':409600000,'usedBytes':614400000}
assert x['summary']['gpu']['available'] and x['summary']['gpu']['busyPercentMilli']==42000
assert x['summary']['disk']['available'] and x['summary']['disk']['readBytesPerSecond']>0 and x['summary']['disk']['writeBytesPerSecond']>0
assert x['summary']['network']['available'] and x['summary']['network']['receiveBytesPerSecond']>0
assert x['coverage']['rowsObserved']==3 and x['coverage']['rowsMatched']==3 and x['coverage']['malformed']==1
assert {r['class'] for r in x['rows']}=={'application','system','kernel'}
assert all('commandLine' not in r and 'path' not in r and 'environment' not in r for r in x['rows'])
a=next(r for r in x['rows'] if r['name']=='alpha')
d=next(r for r in x['rows'] if r['name']=='daemon')
assert a['sampled'] and a['cpuPercentMilli'] is not None and a['readBytesPerSecond']>0 and a['writeBytesPerSecond']>0
assert d['sampled'] is False and d['cpuPercentMilli'] is None and d['readBytesPerSecond'] is None
PY

# Filtering, fixed columns and deterministic en_US text.
advance &
LC_ALL=it_IT.UTF-8 $binary snapshot --sample-ms 100 --filter alpha --sort memory \
  --group name --columns name,pid,cpu,memory --limit 2 >"$work/text.txt"
wait
grep -Fq 'SYNAPSE MONITOR  read-only' "$work/text.txt"
grep -Fq 'Filter: alpha  |  Sort: memory  |  Group: name' "$work/text.txt"
grep -Fq '[alpha · 1 process]' "$work/text.txt"
if grep -Fq 'daemon' "$work/text.txt"; then exit 1; fi

# Non-TTY watch is bounded and carries no terminal clear sequence.
advance &
$binary watch --sample-ms 100 --interval-ms 250 --iterations 1 --limit 3 >"$work/watch.txt"
wait
if grep -q $'\033' "$work/watch.txt"; then exit 1; fi
grep -Fq 'Controls are inspection-only' "$work/watch.txt"

expect_two() {
  set +e
  "$@" >"$work/invalid.out" 2>"$work/invalid.err"
  status=$?
  set -e
  [[ $status == 2 ]]
  grep -Fq 'invalid arguments' "$work/invalid.err"
}
expect_two "$binary" snapshot --sort nope
expect_two "$binary" snapshot --columns name,cpu,cpu
expect_two "$binary" snapshot --columns name,cpu,
expect_two "$binary" snapshot --columns name,,cpu
expect_two "$binary" snapshot --columns pid,cpu
expect_two "$binary" snapshot --limit 513
expect_two "$binary" snapshot --limit '+2'
expect_two "$binary" snapshot --sample-ms 99
expect_two "$binary" snapshot --interval-ms 1000
expect_two "$binary" watch --format json
set +e
$binary mutate --pid 1 >"$work/mutate.out" 2>"$work/mutate.err"
status=$?
set -e
[[ $status == 2 ]]
grep -Fq 'expected snapshot or watch' "$work/mutate.err"

# Test roots reject traversal before probing.
set +e
SYNAPSE_MONITOR_PROC_ROOT="$root/../proc" $binary snapshot --sample-ms 100 \
  >"$work/root.out" 2>"$work/root.err"
status=$?
set -e
[[ $status == 1 ]]
grep -Fq 'invalid absolute normalized test root' "$work/root.err"

echo 'synapse-monitor tests: PASS'
