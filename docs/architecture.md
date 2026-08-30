# Architecture

## Boundary

`synapse-monitor` is one C17 executable with four internal layers:

1. **Probe** reads bounded local kernel pseudo-files from `/proc` and `/sys`.
2. **Inventory** parses bounded service units, XDG autostart entries, socket
   tables and non-identifying machine fields from fixed local roots.
3. **Report** correlates identities, computes sampled rates, filters and sorts
   fixed typed records.
4. **Presentation** renders deterministic `en_US` text, interactive terminal
   frames, locale-neutral JSON, capability discovery or bounded NDJSON full
   frames for a separate graphical adapter.

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
  sequence metadata and blocking backpressure.

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
VRAM window is not classified as physically dedicated memory.

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
The executable calls no socket API. Endpoint observations remain local output
and are never transmitted.

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
row identities but no command templates or visual styling. A native adapter
owns fixed argv, child-process lifetime, framing and exact-major validation.
QML owns only layout, translated labels, typography, color, animation and
accessibility; it never constructs a command, argv or executable path.

## Authority

Alpha 4 is inspection-only. There is deliberately no signal, kill, dump,
priority, service/startup mutation, connection control, mount, cgroup mutation,
privileged helper, subprocess execution, listener or telemetry interface.
