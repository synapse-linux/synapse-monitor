#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C
binary=${1:?binary required}
[[ $($binary --version) == 'synapse-monitor 0.2.0-alpha.2' ]]
$binary --help | grep -Fq 'The command is read-only'

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
root=$work/root
proc=$root/proc
sys=$root/sys
etc=$root/etc
usr=$root/usr
run=$root/run
home=$root/home
mkdir -p "$proc/net" "$proc/sys/kernel" "$sys/class/block/sda/device" \
  "$sys/class/drm/card0/device" "$sys/class/dmi/id" \
  "$sys/fs/cgroup/system.slice/demo.service" \
  "$etc/systemd/system/multi-user.target.wants" \
  "$usr/lib/systemd/system" "$run/systemd/system" \
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
atomic(os.path.join(sysp,'class/drm/card0/device/gpu_busy_percent'),'42\n')
atomic(os.path.join(sysp,'class/drm/card0/device/mem_info_vram_total'),'1000000\n')
atomic(os.path.join(sysp,'class/drm/card0/device/mem_info_vram_used'),'250000\n')
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

# Performance view includes logical processors, per-device rates and no process rows.
reset_sample
advance &
$binary snapshot --view performance --format json --sample-ms 100 \
  >"$work/performance.json"
wait
python3 - "$work/performance.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]))
assert x['schema']=='synapse.monitor.performance/v1' and x['view']=='performance'
assert x['cpu']['logicalProcessorCount']==2
assert x['cpu']['logicalProcessors']==[30000,30000]
assert [r['name'] for r in x['disks']['rows']]==['sda']
assert [r['name'] for r in x['network']['rows']]==['eth0']
assert x['semantics']['telemetry'] is False and 'rows' not in x
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
assert t['local']=='127.0.0.1:8080' and t['remote']=='10.0.0.2:443'
assert t['state']=='established' and t['pid']==100 and t['process']=='alpha'
v6=next(r for r in x['rows'] if r['protocol']=='tcp6')
assert v6['local']=='[::1]:22' and v6['remote']=='[::]:0' and v6['state']=='listening'
assert x['coverage']['malformed']>=1
assert x['semantics']['socketOpened'] is False and x['semantics']['connectionControl'] is False
PY

# Every visible inventory column has a reviewed sorting path.
for sort in name description status startup pid user executable; do
  $binary snapshot --view services --sort "$sort" --limit 2 >/dev/null
done
for sort in name publisher status type scope command; do
  $binary snapshot --view startup --sort "$sort" --limit 2 >/dev/null
done
for sort in protocol local remote status pid process; do
  $binary snapshot --view connections --sort "$sort" --limit 2 >/dev/null
done

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
os.write(fd,b'2');until(b'PERFORMANCE')
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
set +e
$binary mutate --pid 1 >"$work/mutate.out" 2>"$work/mutate.err"
status=$?
set -e
[[ $status == 2 ]]
grep -Fq 'expected snapshot, watch or inspect' "$work/mutate.err"

# Test roots reject traversal before probing.
set +e
SYNAPSE_MONITOR_PROC_ROOT="$root/../proc" $binary snapshot --sample-ms 100 \
  >"$work/root.out" 2>"$work/root.err"
status=$?
set -e
[[ $status == 1 ]]
grep -Fq 'invalid absolute normalized test root' "$work/root.err"

echo 'synapse-monitor tests: PASS'
