<!-- SPDX-License-Identifier: MIT -->
# Architecture

## Boundary

`synapse-monitor` is an independently useful C17 executable with four internal layers:

1. **Probe** reads bounded local kernel pseudo-files from `/proc` and `/sys`.
2. **Inventory** parses bounded service units, XDG autostart entries, socket
   tables and non-identifying machine fields from fixed local roots.
3. **Report** correlates identities, computes sampled rates, filters and sorts
   fixed typed records.
4. **Presentation** renders deterministic `en_US` text, interactive terminal
   frames, locale-neutral JSON, capability discovery or bounded NDJSON full
   frames for a separate graphical adapter.

`synapse-monitor-gui` is a separate native Qt executable with two additional
layers:

5. **Adapter** discovers only the sibling or installed core, owns fixed argv and
   child lifetime, validates exact contract majors/framing/sequence/identities,
   and publishes typed Qt values.
6. **QML presentation** renders responsive translated views, nullable-history
   charts, tables, detail cards and accessibility without receiving an
   executable path or constructing a command.

No observed input becomes a command, path argument or process-control request.
Filters are bounded printable-ASCII matches. Views, columns, grouping, sorting,
layout and theme come from closed identifier sets.

## Bounds

- at most 32,768 numeric process directories and 512 rendered rows;
- at most 4,096 logical CPUs and services;
- at most 1,024 startup items and 4,096 connections;
- at most 512 unique module basenames and 8,192 descriptors per inspection;
- at most 256 physical block devices and 256 non-loopback interfaces;
- at most 16 GPU devices, 256 temperature rows, 128 fan rows, 256 hwmon
  devices and 32 channels per sensor class;
- process names and filters at most 64 bytes;
- sample duration 100–2,000 ms and watch interval 250–10,000 ms;
- fixed per-file limits from 4 KiB to 2 MiB, plus a 4 MiB streaming cap
  for optional `pci.ids` model lookup;
- terminal and graphical performance history fixed to 60 samples;
- graphical stream frames fixed to at most 2 MiB with strictly increasing
  sequence metadata and blocking backpressure;
- graphical stderr fixed to 32 KiB and capability/process-inspection documents
  fixed to 256 KiB;
- graphical row models fixed to the core's advertised 512-row maximum and
  process inspection limited to one identity-revalidated child at a time.

Test roots are disabled unless `SYNAPSE_MONITOR_ALLOW_TEST_ROOTS=1`. Every
overridden root must be absolute and component-normalized.

## Sampling semantics

Process rows are correlated by PID and kernel start ticks, preventing PID-reuse
misattribution. A process first seen in the second sample has nullable rates.
Counter regression produces zero delta rather than unsigned wrap.

CPU detail comes from aggregate and `cpuN` kernel counters. Disk statistics
include non-partition block entries with a local sysfs `device` node and use the
kernel ABI's 512-byte sector unit. Network rates cover non-loopback interfaces.
GPU devices are inventoried independently from metric availability. An optional
bounded, cached lookup in the local `pci.ids` database provides a sanitized model
label without retaining a PCI path. Fixed unprivileged sysfs observations may
provide utilization, driver-reported VRAM,
temperature, current/maximum core clock, memory clock, average/input power,
power cap and fan RPM. Every value has an independent availability flag; a
VRAM window is not classified as physically dedicated memory. For exactly one
i915 device, the core may consume the fixed existing root-owned textfile cache
to obtain authoritative global GEM allocation bytes. The open file must be
regular, single-linked, safely owned and non-writable by group/others, no larger
than 128 KiB, no older than 120 seconds and unchanged across the bounded read;
required metrics must be unique exact integers. Monitor neither invokes nor
controls the privileged collector. Shared allocation use is independent from
total, explicitly overlaps system RAM and must not be summed with RAM views.

Unavailable history observations remain JSON `null` in graphical streams;
measured zero is retained as numeric zero. Full stream frames replace prior
view state and carry stable row identities, avoiding an unbounded patch queue.

Temperature rows come from bounded `hwmon` channels with a bounded thermal-zone
fallback and are classified as CPU package/core, GPU, storage, battery, system
or other. Optional maximum and critical thresholds remain separate. Duplicate
fallback observations are suppressed deterministically. Fan RPM uses bounded
`hwmon` channels. Sensor names and labels are sanitized and no sysfs path is
retained. CPU-package temperature is never substituted for an integrated GPU
without a dedicated GPU sensor; absence is reported as unavailable, not zero.

## Read-only inventories

### Services

Unit files are parsed directly from fixed systemd unit roots. Active state and a
bounded PID are inferred from populated system-slice cgroups. Enabled state is
inferred from reviewed wants/requires links; static, disabled and masked remain
distinct. No D-Bus method or service-control command is invoked. Full executable
paths are reduced to a bounded basename plus an explicit redaction marker.

### Startup applications

Only XDG autostart desktop entries from fixed system and current-user roots are
parsed. User entries override matching system IDs. Raw `Exec` values and source
paths are never retained; the report contains a safe executable basename,
argument-redaction marker and semantic `system|user autostart` location.

### Connections

TCP/UDP kernel tables are decoded in-process. Socket inodes are correlated with
bounded `/proc/PID/fd` link observations and PID/start-time process inventory.
Rows whose transient kernel entry has inode zero are omitted because they cannot
satisfy the declared stable identity; coverage reports them as
`identityUnavailable`. The executable calls no socket API. Endpoint observations
remain local output and are never transmitted.

### Process inspection

Explicit `inspect --pid` revalidates PID plus start ticks after collection.
Linux UID/GID, capability masks, seccomp and no-new-privileges fields are parsed
from status metadata. Mapped files are reduced to unique basenames; addresses,
full paths, descriptor targets, commands and environments are discarded.

### System information

Only non-identifying OS, processor, model, memory, firmware and uptime fields are
read. Host names, machine IDs and serial-number files are outside the contract.

## Graphical boundary

The core exposes `synapse.monitor.presentation/v1` capability discovery and
one-view NDJSON streams. It exposes identifiers, units, availability and stable
row identities but no command templates or visual styling. The Alpha 12 native
adapter owns fixed argv, child-process lifetime, framing, exact-major validation,
line/error caps, sequence checks, identity reconciliation and bounded local row
presentation. Native row changes use stable-identity insert/remove signals and
one atomic layout/data pair for recurring same-cohort reorders; the QML table creates only reusable viewport delegates instead of one
object tree per validated source row. A backend override
exists only behind explicit test authority; normal discovery considers the
sibling and `/usr/bin/synapse-monitor` fixed locations.

QML owns only layout, translated labels, generic typography, color, charts,
animation and accessibility. The GUI disables process-scoped transparent huge
pages before Qt initialization, selects Qt Quick's software graphics API and pins
Qt Quick Controls Basic before creating a window, avoiding disproportionate
renderer/GEM and controls-runtime footprints on the baseline integrated-GPU
target without changing the core observation contract.
After a real view transition accepts its first frame, the native shell processes
deferred deletion, collects QML garbage, trims unused component cache entries and
returns free glibc arenas; rapid superseding transitions cancel stale reclaim work. Once per 30 accepted frames, a bounded active-view maintenance pass processes deferred deletions, collects QML garbage and trims free allocator arenas without dropping the visible model. Locale is fixed before QML loads from the bounded
launch option or session locale; QML receives no locale switcher object and the
window contains no language selector. It receives typed maps/models and never parses JSON
or constructs a command, argv or executable path. A sortable header emits only
its reviewed sort identifier; the adapter checks it against the exact
presentation allowlist and reorders the accepted native model immediately. A
column filter passes only selected opaque tokens from the adapter's finite option
set. Sorting and filtering do not restart the stream, blank the view or invalidate
process inspection. Unavailable values remain last in either direction;
unavailable history remains a null chart gap and measured zero remains numeric.

## Connection-observation sandbox

Connection inventory is read from the host `/proc/net` and correlated to host
process descriptors. A graphical preview or package sandbox therefore must stay
in the host network namespace: `PrivateNetwork=yes` would replace `/proc/net`
with an empty private namespace and make valid connections disappear. This does
not grant network authority. The preview keeps `RestrictAddressFamilies=AF_UNIX`
and `IPAddressDeny=any`, so the GUI and core can use the local Wayland/session
sockets but cannot open IPv4 or IPv6 traffic.

Routine process-rate sampling reads bounded stat/status/I/O buffers, classifies
kernel threads from the documented stat flags field and reuses PID/start-tick
metadata for the second sample. It does not read process command lines. The GUI
defaults to a 2,000 ms cadence while retaining only the reviewed finite interval
choices.

## Authority

Alpha 12 is inspection-only. There is deliberately no signal, kill, dump,
priority, service/startup mutation, connection control, mount, cgroup mutation,
privileged-helper invocation, subprocess execution, listener or telemetry
interface. Reading a pre-existing validated root-owned collector cache does not
add mutation or privilege authority to Monitor.
