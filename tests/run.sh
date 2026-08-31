#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C
binary=${1:?binary required}
repo=$(cd "$(dirname "$0")/.." && pwd)
[[ $($binary --version) == 'synapse-monitor 0.5.0-alpha.11' ]]
$binary --help | grep -Fq 'The command is read-only'
$binary describe --format json >"${TMPDIR:-/tmp}/synapse-monitor-presentation-$$.json"
python3 - "${TMPDIR:-/tmp}/synapse-monitor-presentation-$$.json" \
  "$repo/schemas/presentation-v1.schema.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));schema=json.load(open(sys.argv[2]))
assert schema['$schema']=='https://json-schema.org/draft/2020-12/schema'
assert schema['properties']['schema']['const']=='synapse.monitor.presentation/v1'
assert x['schema']=='synapse.monitor.presentation/v1' and x['readOnly'] is True
assert x['producer']['version']=='0.5.0-alpha.11'
assert [v['id'] for v in x['views']]==['processes','performance','services','startup','connections','information']
assert [v['ordinal'] for v in x['views']]==[1,2,3,4,5,6]
assert x['formats']['stream']['mediaType']=='application/x-ndjson'
assert x['formats']['stream']['maximumLineBytes']==2*1024*1024
assert x['history']['unavailableSample'] is None
assert x['history']['measuredZeroDistinctFromUnavailable'] is True
assert x['localization']['humanLabelsOwnedByGui'] is True
assert x['localization']['runtimeSelector'] is False
assert x['localization']['selection']=='launch-or-session'
assert not any(x['authority'].values()) and not any(x['privacy'].values())
assert 'command' not in x['formats']['stream'] and 'argv' not in x['formats']['stream']
PY
rm -f "${TMPDIR:-/tmp}/synapse-monitor-presentation-$$.json"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
root=$work/root
proc=$root/proc
sys=$root/sys
etc=$root/etc
usr=$root/usr
run=$root/run
var=$root/var
home=$root/home
mkdir -p "$proc/net" "$proc/sys/kernel" "$sys/class/block/sda/device" \
  "$sys/class/drm/card0/device/hwmon/hwmon1" \
  "$sys/class/drm/card1/device" "$sys/class/drm/card32/device" \
  "$sys/class/hwmon/hwmon0" \
  "$sys/class/hwmon/hwmon1" "$sys/class/hwmon/hwmon2" \
  "$sys/class/thermal/thermal_zone0" "$sys/class/dmi/id" \
  "$sys/fs/cgroup/system.slice/demo.service" \
  "$etc/systemd/system/multi-user.target.wants" \
  "$usr/lib/systemd/system" "$usr/share/hwdata" "$run/systemd/system" \
  "$var/lib/node_exporter/textfile_collector" \
  "$etc/xdg/autostart" "$home/.config/autostart"

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
        cpu='cpu 100 0 50 850 0 0 0 0 0 0\ncpu0 50 0 25 425 0 0 0 0 0 0\ncpu1 50 0 25 425 0 0 0 0 0 0\n'
        disk='8 0 sda 10 0 100 0 20 0 200 0 0 0 0 0 0 0\n'
        net='Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n  lo: 10 0 0 0 0 0 0 0 10 0 0 0 0 0 0 0\neth0: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n'
        values={100:(20,10,1000,2000),200:(2,1,0,0),300:(5,5,100,100)}
    else:
        cpu='cpu 120 0 60 920 0 0 0 0 0 0\ncpu0 60 0 30 460 0 0 0 0 0 0\ncpu1 60 0 30 460 0 0 0 0 0 0\n'
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
        atomic(os.path.join(d,'status'),f'Name:\t{name}\nUid:\t{owner}\t{owner}\t{owner}\t{owner}\nGid:\t{owner}\t{owner}\t{owner}\t{owner}\nCapInh:\t0000000000000000\nCapPrm:\t0000000000000001\nCapEff:\t0000000000000001\nCapBnd:\t000000000000ffff\nCapAmb:\t0000000000000000\nNoNewPrivs:\t1\nSeccomp:\t2\n')
        atomic(os.path.join(d,'cmdline'),cmd,binary=True)
        atomic(os.path.join(d,'io'),f'read_bytes: {readb}\nwrite_bytes: {writeb}\n')
    bad=os.path.join(proc,'400');os.makedirs(bad,exist_ok=True)
    atomic(os.path.join(bad,'stat'),'x'*5000)

os.makedirs(proc,exist_ok=True)
atomic(os.path.join(proc,'meminfo'),'MemTotal:       1000000 kB\nMemAvailable:    400000 kB\n')
write(0)
gpu0=os.path.join(sysp,'class/drm/card0/device')
atomic(os.path.join(gpu0,'vendor'),'0x1002\n')
atomic(os.path.join(gpu0,'device'),'0x9999\n')
atomic(os.path.join(gpu0,'uevent'),'DRIVER=amdgpu\nPCI_ID=1002:9999\n')
atomic(os.path.join(gpu0,'gpu_busy_percent'),'42\n')
atomic(os.path.join(gpu0,'mem_info_vram_total'),'1000000\n')
atomic(os.path.join(gpu0,'mem_info_vram_used'),'250000\n')
gpuh=os.path.join(gpu0,'hwmon/hwmon1')
for name,value in {
 'name':'amdgpu\n','temp1_input':'55000\n','temp1_label':'edge\n',
 'temp2_input':'65000\n','temp2_label':'junction\n','freq1_input':'700000000\n',
 'freq2_input':'1000000000\n','power1_average':'32000000\n',
 'power1_cap':'45000000\n','fan1_input':'1800\n'}.items():atomic(os.path.join(gpuh,name),value)
gpu1=os.path.join(sysp,'class/drm/card1/device')
atomic(os.path.join(gpu1,'vendor'),'0x8086\n')
atomic(os.path.join(gpu1,'device'),'0x191e\n')
atomic(os.path.join(gpu1,'uevent'),'DRIVER=i915\nPCI_ID=8086:191E\n')
for base,values in {
 'hwmon0':{'name':'coretemp\n','temp1_input':'48000\n','temp1_label':'Package id 0\n',
           'temp1_max':'100000\n','temp1_crit':'105000\n','temp2_input':'47000\n',
           'temp2_label':'Core 0\n','temp3_input':'999999\n',
           'temp4_input':'-5000\n','temp4_label':'Core 1\x01private\n'},
 'hwmon1':{'name':'amdgpu\n','temp1_input':'55000\n','temp1_label':'edge\n',
           'fan1_input':'1800\n','fan1_label':'GPU fan\n'},
 'hwmon2':{'name':'nvme\n','temp1_input':'39850\n','temp1_label':'Composite\n',
           'temp1_max':'85850\n','temp1_crit':'87850\n',
           'temp33_input':'40000\n','fan33_input':'1000\n'}}.items():
    for name,value in values.items():atomic(os.path.join(sysp,'class/hwmon',base,name),value)
atomic(os.path.join(sysp,'class/thermal/thermal_zone0/type'),'x86_pkg_temp\n')
atomic(os.path.join(sysp,'class/thermal/thermal_zone0/temp'),'48000\n')
with open(os.path.join(proc,'.advance.py'),'w') as h:
    h.write('')
PY

cat >"$usr/lib/systemd/system/demo.service" <<'EOF'
[Unit]
Description=Demo Service
[Service]
User=demo
ExecStart=/usr/lib/private/demo-service --secret token
[Install]
WantedBy=multi-user.target
EOF
cat >"$usr/lib/systemd/system/static.service" <<'EOF'
[Unit]
Description=Static Worker
[Service]
Type=oneshot
EOF
python3 - "$usr/lib/systemd/system/huge.service" \
  "$etc/xdg/autostart/huge.desktop" <<'PY'
import sys
for path in sys.argv[1:]:open(path,'w').write('X'*70000)
PY
ln -s /usr/lib/systemd/system/demo.service \
  "$etc/systemd/system/multi-user.target.wants/demo.service"
printf 'populated 1\nfrozen 0\n' >"$sys/fs/cgroup/system.slice/demo.service/cgroup.events"
printf '100\n' >"$sys/fs/cgroup/system.slice/demo.service/cgroup.procs"
cat >"$etc/xdg/autostart/demo.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Demo Startup
X-AppStream-DeveloperName=Synapse Test
Exec=/usr/bin/demo --private-value
X-GNOME-Autostart-enabled=true
EOF
cat >"$home/.config/autostart/user-tool.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=User Tool
Exec=/home/user/private-tool --token secret
Hidden=true
EOF
cat >"$proc/net/tcp" <<'EOF'
  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
   0: 0100007F:1F90 0200000A:01BB 01 00000000:00000000 00:00000000 00000000 1000 0 12345 1 0000000000000000
   1: 0100007F:1F91 0200000A:01BB 06 00000000:00000000 00:00000000 00000000 1000 0 0 1 0000000000000000
EOF
cat >"$proc/net/tcp6" <<'EOF'
  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
   0: 00000000000000000000000001000000:0016 00000000000000000000000000000000:0000 0a 00000000:00000000 00:00000000 00000000 0 0 34567 1 0000000000000000
EOF
cat >"$proc/net/udp" <<'EOF'
  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
   0: 00000000:14E9 00000000:0000 07 00000000:00000000 00:00000000 00000000 1000 0 23456 1 0000000000000000
   malformed hostile row
EOF
printf '  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n' >"$proc/net/udp6"
mkdir -p "$proc/100/fd"
ln -s 'socket:[12345]' "$proc/100/fd/7"
cat >"$proc/100/maps" <<'EOF'
00400000-00401000 r-xp 00000000 08:01 100 /usr/lib/libdemo.so
00500000-00501000 r--p 00000000 08:01 101 /opt/private/libsecret.so
00600000-00601000 r--p 00000000 00:00 0 [heap]
00700000-00701000 r-xp 00000000 08:01 100 /usr/lib/libdemo.so
EOF
printf 'PRETTY_NAME="Synapse Test OS"\n' >"$etc/os-release"
printf '6.12.0-test\n' >"$proc/sys/kernel/osrelease"
printf 'model name\t: Test Processor 2.0\n' >"$proc/cpuinfo"
printf '90061.25 123.00\n' >"$proc/uptime"
printf 'Synapse Labs\n' >"$sys/class/dmi/id/sys_vendor"
printf 'TestBook1,1\n' >"$sys/class/dmi/id/product_name"
printf 'Firmware Vendor\n' >"$sys/class/dmi/id/bios_vendor"
printf '1.2.3\n' >"$sys/class/dmi/id/bios_version"
printf '08/30/2026\n' >"$sys/class/dmi/id/bios_date"
printf '1002  Advanced Micro Devices, Inc.\n\t9999  Test Graphics Adapter\n8086  Intel Corporation\n\t191e  Skylake-Y GT2 [HD Graphics 515]\n' >"$usr/share/hwdata/pci.ids"
cat >"$var/lib/node_exporter/textfile_collector/synapse_memory.prom" <<'EOF'
# HELP synapse_memory_i915_gem_probe_available Whether global i915 GEM was readable.
# TYPE synapse_memory_i915_gem_probe_available gauge
synapse_memory_i915_gem_probe_available 1
synapse_memory_i915_gem_cached_collector 0
synapse_memory_i915_gem_bytes 167227392
synapse_memory_i915_gem_objects 104
synapse_memory_process_pss_bytes 1000000000
synapse_memory_processes_permission_denied 0
synapse_memory_processes_vanished 0
synapse_memory_process_scan_truncated 0
EOF

reset_sample() {
  python3 - "$proc" <<'PY'
import os,sys
proc=sys.argv[1]
def atomic(path,text):
 p=path+'.reset';open(p,'w').write(text);os.replace(p,path)
def stat_line(pid,name,state,utime,stime,threads,start,rss):
 f={i:0 for i in range(3,53)};f.update({3:state,14:utime,15:stime,20:threads,22:start,23:1000000,24:rss})
 return f"{pid} ({name}) "+" ".join(str(f[i]) for i in range(3,53))+"\n"
atomic(proc+'/stat','cpu 100 0 50 850 0 0 0 0 0 0\ncpu0 50 0 25 425 0 0 0 0 0 0\ncpu1 50 0 25 425 0 0 0 0 0 0\n')
atomic(proc+'/diskstats','8 0 sda 10 0 100 0 20 0 200 0 0 0 0 0 0 0\n')
atomic(proc+'/net/dev','Inter-| Receive | Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n  lo: 10 0 0 0 0 0 0 0 10 0 0 0 0 0 0 0\neth0: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n')
for pid,name,state,utime,stime,start,rss,readb,writeb in [
 (100,'alpha','R',20,10,1000,100,1000,2000),
 (200,'kworker/0:1','S',2,1,2000,0,0,0),
 (300,'daemon','S',5,5,3000,50,100,100)]:
 atomic(f'{proc}/{pid}/stat',stat_line(pid,name,state,utime,stime,2,start,rss))
 atomic(f'{proc}/{pid}/io',f'read_bytes: {readb}\nwrite_bytes: {writeb}\n')
PY
}

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
atomic(proc+'/stat','cpu 120 0 60 920 0 0 0 0 0 0\ncpu0 60 0 30 460 0 0 0 0 0 0\ncpu1 60 0 30 460 0 0 0 0 0 0\n')
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
export SYNAPSE_MONITOR_ETC_ROOT=$etc
export SYNAPSE_MONITOR_USR_ROOT=$usr
export SYNAPSE_MONITOR_RUN_ROOT=$run
export SYNAPSE_MONITOR_VAR_ROOT=$var
export SYNAPSE_MONITOR_HOME_ROOT=$home
advance &
$binary snapshot --format json --sample-ms 100 --limit 6 >"$work/snapshot.json"
wait
python3 - "$work/snapshot.json" <<'PY'
import json,sys
raw=open(sys.argv[1],'rb').read();assert raw.endswith(b'\n') and raw.count(b'\n')==1
x=json.loads(raw)
assert x['schema']=='synapse.monitor.snapshot/v1' and x['readOnly'] is True
assert x['summary']['cpu']['available'] and x['summary']['cpu']['busyPercentMilli']==30000
memory=x['summary']['memory']
assert memory['available'] is True and memory['totalBytes']==1024000000
assert memory['availableBytes']==409600000 and memory['usedBytes']==614400000
assert memory['observedFootprintAvailable'] is True
assert memory['observedFootprintBytes']==1167227392
assert x['summary']['gpu']['present'] and x['summary']['gpu']['available']
assert x['summary']['gpu']['busyPercentMilli']==42000
assert x['summary']['gpu']['memoryKind']=='driver-reported-vram'
assert x['summary']['gpu']['memorySource']=='driver-sysfs'
assert x['summary']['gpu']['memoryOverlapsSystemRam'] is None
assert x['summary']['gpu']['memorySampleAgeMilliseconds'] is None
assert x['summary']['disk']['available'] and x['summary']['disk']['readBytesPerSecond']>0 and x['summary']['disk']['writeBytesPerSecond']>0
assert x['summary']['network']['available'] and x['summary']['network']['receiveBytesPerSecond']>0
assert x['coverage']['rowsObserved']==3 and x['coverage']['rowsMatched']==3 and x['coverage']['malformed']==1
assert {r['class'] for r in x['rows']}=={'application','system','kernel'}
assert all('commandLine' not in r and 'path' not in r and 'environment' not in r for r in x['rows'])
a=next(r for r in x['rows'] if r['name']=='alpha')
d=next(r for r in x['rows'] if r['name']=='daemon')
assert a['startTicks']==1000 and d['startTicks']==3001
assert a['sampled'] and a['cpuPercentMilli'] is not None and a['readBytesPerSecond']>0 and a['writeBytesPerSecond']>0
assert d['sampled'] is False and d['cpuPercentMilli'] is None and d['readBytesPerSecond'] is None
PY

# Performance view includes logical processors, per-device rates and no process rows.
reset_sample
advance &
$binary snapshot --view performance --format json --sample-ms 100 \
  >"$work/performance.json"
wait
python3 - "$work/performance.json" <<'PY'
import json,sys
raw=open(sys.argv[1]).read();x=json.loads(raw)
assert x['schema']=='synapse.monitor.performance/v2' and x['view']=='performance'
assert '/sys/' not in raw and '/usr/share/' not in raw
assert x['cpu']['logicalProcessorCount']==2
assert x['cpu']['logicalProcessors']==[30000,30000]
assert x['gpu']['memoryKind']=='driver-reported-vram'
assert x['gpus']['rowsObserved']==2 and x['gpus']['truncated'] is True
assert x['gpus']['integratedGpuTemperatureInferred'] is False
amd,intel=x['gpus']['rows']
assert amd['vendor']=='AMD' and amd['driver']=='amdgpu' and amd['model']=='Test Graphics Adapter'
assert amd['utilizationPercentMilli']==42000 and amd['memoryUsedBytes']==250000
assert amd['temperatureMillidegreesCelsius']==65000 and amd['temperatureLabel']=='junction'
assert amd['coreClockHz']==700000000 and amd['memoryClockHz']==1000000000
assert amd['powerMicrowatts']==32000000 and amd['powerCapMicrowatts']==45000000
assert amd['fanRpm']==1800 and amd['memoryKind']=='driver-reported-vram'
assert intel['vendor']=='Intel' and intel['driver']=='i915' and intel['memoryKind']=='shared'
assert intel['model']=='Skylake-Y GT2 [HD Graphics 515]'
assert intel['memoryAvailable'] is True and intel['memoryUsedBytes']==167227392
assert intel['memoryTotalBytes'] is None
assert intel['memorySource']=='root-owned-fresh-collector'
assert intel['memoryOverlapsSystemRam'] is True
assert 0 <= intel['memorySampleAgeMilliseconds'] <= 120000
memory=x['memory']
assert memory['observedFootprintAvailable'] is True
assert memory['processPssBytes']==1000000000
assert memory['sharedGpuBytes']==167227392
assert memory['observedFootprintBytes']==1167227392
assert memory['observedFootprintBytes']==memory['processPssBytes']+memory['sharedGpuBytes']
assert memory['observedFootprintAccounting']=='process-pss-plus-global-i915-gem'
assert memory['observedFootprintComponentsMayOverlap'] is True
assert intel['utilizationPercentMilli'] is None and intel['temperatureMillidegreesCelsius'] is None
classes={r['class'] for r in x['thermals']['temperatures']}
assert {'cpu-package','cpu-core','gpu','storage'} <= classes
pkg=next(r for r in x['thermals']['temperatures'] if r['class']=='cpu-package')
assert pkg['temperatureMillidegreesCelsius']==48000 and pkg['criticalMillidegreesCelsius']==105000
cold=next(r for r in x['thermals']['temperatures'] if r['temperatureMillidegreesCelsius']==-5000)
assert cold['class']=='cpu-core' and cold['label']=='Core 1?private'
assert x['thermals']['fans'][0]['rpm']==1800
assert x['thermals']['malformed']>=1 and x['thermals']['temperatureTruncated'] is True
assert x['thermals']['fanTruncated'] is True
assert [r['name'] for r in x['disks']['rows']]==['sda']
assert [r['name'] for r in x['network']['rows']]==['eth0']
assert x['semantics']['telemetry'] is False
assert x['semantics']['sharedGpuMemoryNonAdditive'] is True
assert x['semantics']['observedFootprintAddsSharedGpu'] is True
assert x['semantics']['observedFootprintBase']=='process-pss' and 'rows' not in x
PY

# Shared i915 accounting rejects writable, duplicate, stale and symlink caches.
collector="$var/lib/node_exporter/textfile_collector/synapse_memory.prom"
cp "$collector" "$work/valid-collector.prom"
assert_i915_collector_rejected() {
  local label=$1
  reset_sample
  advance &
  local advance_pid=$!
  $binary snapshot --view performance --format json --sample-ms 100 \
    >"$work/rejected-$label.json"
  wait "$advance_pid"
  python3 - "$work/rejected-$label.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));intel=next(r for r in x['gpus']['rows'] if r['driver']=='i915')
assert intel['memoryKind']=='shared' and intel['memoryAvailable'] is False
assert intel['memoryUsedBytes'] is None and intel['memoryTotalBytes'] is None
assert intel['memorySource']=='unavailable'
assert intel['memoryOverlapsSystemRam'] is True
assert intel['memorySampleAgeMilliseconds'] is None
memory=x['memory'];assert memory['observedFootprintAvailable'] is False
assert memory['processPssBytes'] is None and memory['sharedGpuBytes'] is None
assert memory['observedFootprintBytes'] is None
assert memory['observedFootprintAccounting'] is None
assert memory['observedFootprintComponentsMayOverlap'] is None
PY
}
chmod 0666 "$collector"
assert_i915_collector_rejected writable
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
printf 'synapse_memory_i915_gem_bytes 1\n' >>"$collector"
assert_i915_collector_rejected duplicate
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
python3 - "$collector" <<'PY'
import pathlib,sys
path=pathlib.Path(sys.argv[1]);path.write_bytes(path.read_bytes()+b'\0synapse_memory_i915_gem_bytes 1\n')
PY
assert_i915_collector_rejected embedded-nul
python3 - "$collector" <<'PY'
import pathlib,sys
pathlib.Path(sys.argv[1]).write_bytes(b'x'*(128*1024+1))
PY
assert_i915_collector_rejected oversized
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
ln "$collector" "$collector.hardlink"
assert_i915_collector_rejected hardlink
rm "$collector.hardlink"
python3 - "$collector" <<'PY'
import os,sys,time
future=time.time()+1
os.utime(sys.argv[1],(future,future))
PY
assert_i915_collector_rejected future
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
python3 - "$collector" <<'PY'
import os,sys,time
stale=time.time()-121
os.utime(sys.argv[1],(stale,stale))
PY
assert_i915_collector_rejected stale
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
mv "$collector" "$collector.target"
ln -s "$collector.target" "$collector"
assert_i915_collector_rejected symlink
rm "$collector"
mv "$collector.target" "$collector"
chmod 0644 "$collector"
touch "$collector"

# PSS completeness fails independently while the valid GEM observation remains.
assert_observed_accounting_rejected() {
  local label=$1
  reset_sample
  advance &
  local advance_pid=$!
  $binary snapshot --view performance --format json --sample-ms 100 \
    >"$work/accounting-rejected-$label.json"
  wait "$advance_pid"
  python3 - "$work/accounting-rejected-$label.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));intel=next(r for r in x['gpus']['rows'] if r['driver']=='i915')
assert intel['memoryAvailable'] is True and intel['memoryUsedBytes']==167227392
m=x['memory'];assert m['observedFootprintAvailable'] is False
assert m['processPssBytes'] is None and m['sharedGpuBytes'] is None
assert m['observedFootprintBytes'] is None and m['observedFootprintAccounting'] is None
PY
}
cp "$work/valid-collector.prom" "$collector"
printf 'synapse_memory_process_pss_bytes 1\n' >>"$collector"
assert_observed_accounting_rejected duplicate-pss
cp "$work/valid-collector.prom" "$collector"
python3 - "$collector" <<'PY'
import pathlib,sys
p=pathlib.Path(sys.argv[1]);p.write_text(p.read_text().replace(
 'synapse_memory_processes_permission_denied 0',
 'synapse_memory_processes_permission_denied 1'))
PY
assert_observed_accounting_rejected permission-denied
cp "$work/valid-collector.prom" "$collector"
python3 - "$collector" <<'PY'
import pathlib,sys
p=pathlib.Path(sys.argv[1]);p.write_text(p.read_text().replace(
 'synapse_memory_processes_vanished 0',
 'synapse_memory_processes_vanished 1'))
PY
assert_observed_accounting_rejected vanished-pss
cp "$work/valid-collector.prom" "$collector"
python3 - "$collector" <<'PY'
import pathlib,sys
p=pathlib.Path(sys.argv[1]);p.write_text(p.read_text().replace(
 'synapse_memory_process_scan_truncated 0',
 'synapse_memory_process_scan_truncated 1'))
PY
assert_observed_accounting_rejected truncated-pss
cp "$work/valid-collector.prom" "$collector"
chmod 0644 "$collector"
touch "$collector"
mkdir -p "$sys/class/drm/card2/device"
printf '0x8086\n' >"$sys/class/drm/card2/device/vendor"
printf '0x191e\n' >"$sys/class/drm/card2/device/device"
printf 'DRIVER=i915\nPCI_ID=8086:191E\n' >"$sys/class/drm/card2/device/uevent"
reset_sample
advance &
advance_pid=$!
$binary snapshot --view performance --format json --sample-ms 100 \
  >"$work/ambiguous-i915.json"
wait "$advance_pid"
python3 - "$work/ambiguous-i915.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));rows=[r for r in x['gpus']['rows'] if r['driver']=='i915']
assert len(rows)==2
assert all(r['memoryAvailable'] is False and r['memorySource']=='unavailable' for r in rows)
m=x['memory'];assert m['observedFootprintAvailable'] is False
assert m['observedFootprintBytes'] is None
PY
rm -rf "$sys/class/drm/card2"

# Every view can be delivered as one bounded full-frame NDJSON object.
for view in processes performance services startup connections information; do
  if [[ $view == processes || $view == performance ]]; then
    reset_sample
    advance &
    advance_pid=$!
    $binary stream --view "$view" --iterations 1 --interval-ms 250 \
      --sample-ms 100 >"$work/stream-$view.ndjson"
    wait "$advance_pid"
  else
    $binary stream --view "$view" --iterations 1 --interval-ms 250 \
      >"$work/stream-$view.ndjson"
  fi
done
python3 - "$work" "$repo/schemas/stream-frame-v1.schema.json" <<'PY'
import json,os,sys
root=sys.argv[1];schema=json.load(open(sys.argv[2]))
assert schema['properties']['schema']['const']=='synapse.monitor.stream-frame/v1'
expected={'processes':'synapse.monitor.snapshot/v1','performance':'synapse.monitor.performance/v2','services':'synapse.monitor.services/v1','startup':'synapse.monitor.startup/v1','connections':'synapse.monitor.connections/v1','information':'synapse.monitor.information/v1'}
for view,want in expected.items():
 raw=open(os.path.join(root,f'stream-{view}.ndjson'),'rb').read()
 assert raw.endswith(b'\n') and raw.count(b'\n')==1 and len(raw)<=2*1024*1024
 x=json.loads(raw);assert x['schema']==want and x['view']==view and x['readOnly'] is True
 frame=x['stream'];assert set(frame)=={'schema','sequence','intervalMilliseconds'}
 assert frame['schema']=='synapse.monitor.stream-frame/v1'
 assert frame['sequence']==0 and frame['intervalMilliseconds']==250
PY

# Streaming history grows oldest-first, carries sequence, and preserves null.
reset_sample
advance &
$binary stream --view performance --sample-ms 100 --interval-ms 250 \
  --iterations 2 --limit 36 >"$work/performance-stream.ndjson"
wait
python3 - "$work/performance-stream.ndjson" <<'PY'
import json,sys
raw=open(sys.argv[1],'rb').read();assert b'\n\n' not in raw
rows=[json.loads(line) for line in raw.splitlines()];assert len(rows)==2
assert [x['stream']['sequence'] for x in rows]==[0,1]
assert [len(x['history']['cpuPercentMilli']) for x in rows]==[1,2]
assert [len(x['history']['memoryPercentMilli']) for x in rows]==[1,2]
assert all(len(line)<=2*1024*1024 for line in raw.splitlines())
PY
mv "$sys/class/drm/card0/device/gpu_busy_percent" \
  "$sys/class/drm/card0/device/gpu_busy_percent.saved"
reset_sample
advance &
$binary stream --view performance --sample-ms 100 --interval-ms 250 \
  --iterations 1 --limit 36 >"$work/performance-unavailable.ndjson"
wait
mv "$sys/class/drm/card0/device/gpu_busy_percent.saved" \
  "$sys/class/drm/card0/device/gpu_busy_percent"
python3 - "$work/performance-unavailable.ndjson" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));assert x['gpu']['available'] is False
assert x['history']['gpuPercentMilli']==[None]
assert x['history']['cpuPercentMilli'][0] is not None
PY

# Services expose status/startup/PID/user without executable paths or mutation.
$binary snapshot --view services --format json --sort startup --limit 8 \
  >"$work/services.json"
python3 - "$work/services.json" <<'PY'
import json,sys
raw=open(sys.argv[1]).read();x=json.loads(raw)
assert x['schema']=='synapse.monitor.services/v1' and x['readOnly']
row=next(r for r in x['rows'] if r['name']=='demo.service')
assert row['description']=='Demo Service' and row['status']=='active'
assert row['startup']=='enabled' and row['pid']==100 and row['user']=='demo'
assert row['executable']=='demo-service (path redacted)'
assert x['coverage']['malformed']>=1
assert x['semantics']['serviceMutation'] is False
assert '--secret' not in raw and '/usr/' not in raw and 'path' not in ''.join(x['rows'][0].keys()).lower()
PY

# Startup inventory preserves useful metadata while suppressing launch commands/locations.
$binary snapshot --view startup --format json --sort scope --limit 8 \
  >"$work/startup.json"
python3 - "$work/startup.json" <<'PY'
import json,sys
raw=open(sys.argv[1]).read();x=json.loads(raw)
assert x['schema']=='synapse.monitor.startup/v1'
d=next(r for r in x['rows'] if r['name']=='Demo Startup')
u=next(r for r in x['rows'] if r['name']=='User Tool')
assert d['publisher']=='Synapse Test' and d['status']=='enabled' and d['launchPresent']
assert d['location']=='system autostart' and d['command']=='demo (arguments redacted)'
assert u['scope']=='user' and u['status']=='disabled'
assert u['location']=='user autostart' and u['command']=='private-tool (arguments redacted)'
assert x['coverage']['malformed']>=1
assert '--private-value' not in raw and '--token' not in raw and '/home/' not in raw
assert x['semantics']['startupMutation'] is False
PY

# Connections decode endpoints and correlate socket inode ownership without opening sockets.
$binary snapshot --view connections --format json --sort local --limit 8 \
  >"$work/connections.json"
python3 - "$work/connections.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));assert x['schema']=='synapse.monitor.connections/v1'
t=next(r for r in x['rows'] if r['protocol']=='tcp')
assert t['socketInode']==12345
assert t['local']=='127.0.0.1:8080' and t['remote']=='10.0.0.2:443'
assert t['state']=='established' and t['pid']==100 and t['process']=='alpha'
v6=next(r for r in x['rows'] if r['protocol']=='tcp6')
assert v6['local']=='[::1]:22' and v6['remote']=='[::]:0' and v6['state']=='listening'
assert x['coverage']['malformed']>=1
assert x['coverage']['identityUnavailable']==1
assert all(r['socketInode']>0 for r in x['rows'])
assert x['semantics']['socketOpened'] is False and x['semantics']['connectionControl'] is False
PY

# Every visible table column has a reviewed sorting path.
for sort in name class pid user state threads cpu memory read write; do
  $binary snapshot --view processes --sort "$sort" --group none \
    --sample-ms 100 --limit 2 >/dev/null
done
for sort in name description status startup pid user executable; do
  $binary snapshot --view services --sort "$sort" --limit 2 >/dev/null
done
for sort in name publisher status type scope location command; do
  $binary snapshot --view startup --sort "$sort" --limit 2 >/dev/null
done
for sort in protocol local remote status pid process; do
  $binary snapshot --view connections --sort "$sort" --limit 2 >/dev/null
done
$binary snapshot --view processes --format json --sort class --group none \
  --sample-ms 100 --limit 8 >"$work/processes-class-sort.json"
$binary snapshot --view startup --format json --sort location --limit 8 \
  >"$work/startup-location-sort.json"
python3 - "$work/processes-class-sort.json" "$work/startup-location-sort.json" <<'PY'
import json,sys
processes=json.load(open(sys.argv[1]))['rows']
startup=json.load(open(sys.argv[2]))['rows']
assert [r['class'] for r in processes] == sorted(r['class'] for r in processes)
assert [r['location'] for r in startup] == sorted(r['location'] for r in startup)
PY

# System information excludes host names and serial numbers.
$binary snapshot --view information --format json >"$work/information.json"
python3 - "$work/information.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));i=x['information']
assert x['schema']=='synapse.monitor.information/v1'
assert i['operatingSystem']=='Synapse Test OS' and i['kernel']=='6.12.0-test'
assert i['processor']=='Test Processor 2.0' and i['systemModel']=='TestBook1,1'
assert i['firmwareVersion']=='1.2.3' and i['memoryTotalBytes']==1024000000
assert i['uptimeSeconds']==90061
assert x['semantics']['hostNameExposed'] is False and x['semantics']['serialNumbersExposed'] is False
PY

# Deep process inspection is read-only and exposes basenames/credentials, not paths.
$binary inspect --pid 100 --format json >"$work/inspect.json"
python3 - "$work/inspect.json" <<'PY'
import json,sys
raw=open(sys.argv[1]).read();x=json.loads(raw)
assert x['schema']=='synapse.monitor.process-inspection/v1' and x['readOnly']
assert x['identity']=={'pid':100,'startTicks':1000,'name':'alpha'}
assert x['credentials']['uids'][0] == __import__('os').getuid()
assert x['credentials']['noNewPrivileges'] is True
assert x['credentials']['seccompMode']==2
assert x['modules']['names']==['libdemo.so','libsecret.so']
assert x['descriptors']['count']==1 and x['descriptors']['socketCount']==1
assert '/usr/' not in raw and '/opt/' not in raw
assert x['semantics']['processControl'] is False and x['semantics']['processDump'] is False
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

# A pseudo-terminal exercises all six tabs, direct type-to-filter and clean Q exit.
python3 - "$binary" <<'PY'
import os,pty,select,struct,sys,termios,time
binary=sys.argv[1]
pid,fd=pty.fork()
if pid==0:
    os.execv(binary,[binary,'watch','--sample-ms','100','--interval-ms','250','--limit','12'])
fcntl_data=struct.pack('HHHH',50,160,0,0)
import fcntl
fcntl.ioctl(fd,termios.TIOCSWINSZ,fcntl_data)
data=b''
def until(needle,timeout=5):
    global data
    end=time.monotonic()+timeout
    while needle not in data and time.monotonic()<end:
        ready,_,_=select.select([fd],[],[],0.1)
        if ready:
            try:data+=os.read(fd,65536)
            except OSError:break
        if len(data)>2_000_000:data=data[-1_000_000:]
    assert needle in data,(needle,data[-1000:])
until(b'PROCESSES')
os.write(fd,b'3');until(b'SERVICES')
os.write(fd,b'd');time.sleep(0.05);os.write(fd,b'emo\r');until(b'Filter: demo')
os.write(fd,b'4');until(b'STARTUP APPS')
os.write(fd,b'5');until(b'CONNECTIONS')
os.write(fd,b'6');until(b'SYSTEM INFORMATION')
os.write(fd,b'2');until(b'PERFORMANCE');until(b'GRAPHICS PROCESSORS');until(b'TEMPERATURES')
os.write(fd,b'Q')
end=time.monotonic()+5
status=None
while time.monotonic()<end:
    done,value=os.waitpid(pid,os.WNOHANG)
    if done:
        status=value;break
    time.sleep(0.05)
if status is None:
    os.kill(pid,9);os.waitpid(pid,0);raise AssertionError('TUI did not exit')
assert os.waitstatus_to_exitcode(status)==0
assert b'\x1b[' in data
PY

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
expect_two "$binary" snapshot --view nope
expect_two "$binary" snapshot --theme neon
expect_two "$binary" snapshot --layout giant
expect_two "$binary" snapshot --view services --sort cpu
expect_two "$binary" snapshot --view services --group class
expect_two "$binary" snapshot --view services --columns name,cpu
expect_two "$binary" snapshot --view startup --columns status,scope
expect_two "$binary" snapshot --view connections --columns protocol,remote
expect_two "$binary" snapshot --view performance --columns name
expect_two "$binary" snapshot --view performance --filter secret
expect_two "$binary" snapshot --view information --filter secret
expect_two "$binary" snapshot --view services --sample-ms 100
expect_two "$binary" inspect
expect_two "$binary" inspect --pid 0
expect_two "$binary" inspect --pid 100 --sort cpu
expect_two "$binary" watch --format json
expect_two "$binary" watch --format ndjson
expect_two "$binary" snapshot --format ndjson
expect_two "$binary" stream --format json --iterations 1
expect_two "$binary" stream --format text --iterations 1
expect_two "$binary" describe --format text
expect_two "$binary" inspect --pid 100 --format ndjson
set +e
$binary mutate --pid 1 >"$work/mutate.out" 2>"$work/mutate.err"
status=$?
set -e
[[ $status == 2 ]]
grep -Fq 'expected snapshot, stream, watch, describe or inspect' "$work/mutate.err"

# Test roots reject traversal before probing.
set +e
SYNAPSE_MONITOR_PROC_ROOT="$root/../proc" $binary snapshot --sample-ms 100 \
  >"$work/root.out" 2>"$work/root.err"
status=$?
set -e
[[ $status == 1 ]]
grep -Fq 'invalid absolute normalized test root' "$work/root.err"

echo 'synapse-monitor tests: PASS'
