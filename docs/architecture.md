# Architecture

## Boundary

`synapse-monitor` is one C17 executable with three internal layers:

1. **Probe** reads bounded local kernel pseudo-files from `/proc` and `/sys`.
2. **Report** samples twice, correlates PID plus start time, computes deltas,
   filters and sorts fixed typed records.
3. **Presentation** renders deterministic `en_US` text, interactive terminal
   frames, or locale-neutral JSON.

No input becomes a command, path or process-control argument. The filter is a
bounded printable-ASCII name match. Columns, grouping and sorting come from
closed identifier sets.

## Bounds

- at most 32,768 numeric process directories;
- at most 512 rendered rows;
- process names and filters at most 64 bytes;
- sample duration 100–2,000 ms;
- watch interval 250–10,000 ms;
- at most 256 physical block devices and 256 non-loopback interfaces;
- fixed per-file read limits between 4 KiB and 256 KiB.

Test roots are disabled unless `SYNAPSE_MONITOR_ALLOW_TEST_ROOTS=1`; when enabled,
both roots must be absolute and component-normalized.

## Sampling semantics

CPU rows are correlated by PID and kernel start ticks, preventing PID-reuse
misattribution. A process first seen in the second sample has nullable sampled
rates. Disk statistics include only non-partition block entries with a local
sysfs `device` node and use the kernel ABI's 512-byte sector unit. Network rates
sum non-loopback interfaces. Counter regression produces a zero delta rather
than unsigned wrap.

GPU activity is optional. It is reported only when an unprivileged bounded
`gpu_busy_percent` or `gt_busy_percent` sysfs value is available; absence is not
reported as zero.

## Authority

Alpha 1 is inspection-only. There is deliberately no process signal, kill,
dump, priority, service, startup, connection-control, mount, cgroup or privileged
helper interface.
