# Synapse Monitor

Synapse Monitor is a first-party, read-only system inspector for Synapse Linux.
Its C17 core produces a useful dense terminal view and a versioned JSON
snapshot without requiring a graphical session.

Alpha 1 covers:

- bounded process observations grouped by application, system or kernel class;
- sampled CPU and per-process CPU activity;
- physical RAM usage;
- unprivileged GPU busy and local-memory data when the driver exposes it;
- physical-disk and non-loopback network rates;
- fixed sorting, bounded name filtering and reviewed selectable columns;
- an interactive terminal watch with `/`, `s`, `g`, `c` and `q` controls.

It does not expose process command lines, environments, credentials, tokens or
paths. There are no kill, signal, dump, priority, service, startup, cgroup,
network-control or privilege operations. It opens no listener and sends no
telemetry.

## Build and test

```bash
make clean all test
```

## Use

```bash
synapse-monitor snapshot
synapse-monitor snapshot --format json --limit 50
synapse-monitor snapshot --filter compositor --sort memory
synapse-monitor snapshot --group name \
  --columns name,pid,cpu,memory,read,write
synapse-monitor watch
```

Human output is deterministic `en_US`. JSON uses the locale-neutral exact-major
contract `synapse.monitor.snapshot/v1`. Unknown options, sort/group identifiers,
columns, duplicate columns and oversized filters fail closed.

The CLI contract remains provisional during Alpha 1, so translated manual pages
are deferred until the command surface is declared definitive.
