# Alpha 2 requirements

## Functional views

- `MON-001`: produce deterministic text and exact-major locale-neutral JSON.
- `MON-002`: provide Processes, Performance, Services, Startup Apps,
  Connections and System Information views.
- `MON-003`: show CPU, RAM, optional GPU, physical-disk and non-loopback network summaries.
- `MON-004`: show bounded logical-CPU, per-disk and per-interface performance detail.
- `MON-005`: retain at most 60 terminal history samples for live mini-graphs.
- `MON-006`: correlate process CPU and I/O deltas by PID plus start time.
- `MON-007`: group process rows by fixed `class|name|none` modes.
- `MON-008`: provide fixed view-appropriate sorting and reviewed selectable columns.
- `MON-009`: filter table views using at most 64 printable ASCII bytes.
- `MON-010`: provide fixed dense, balanced and wide layouts.
- `MON-011`: provide fixed default, contrast and mono terminal themes.
- `MON-012`: enumerate bounded service metadata and active/startup state without control authority.
- `MON-013`: enumerate bounded XDG startup metadata without retaining raw launch commands.
- `MON-014`: decode bounded TCP/UDP endpoints and correlate socket ownership without opening sockets.
- `MON-015`: show non-identifying OS, CPU, system, memory, firmware and uptime fields.
- `MON-016`: provide explicit PID/start-time-safe process inspection with numeric Linux credential metadata, capability masks, seccomp state, module basenames and descriptor counts.

## Safety and privacy

- `MON-017`: expose no command line, environment, authentication credential, secret, serial number or full path.
- `MON-018`: represent executable and startup content only as bounded safe labels and semantic scopes.
- `MON-019`: provide no process, service, startup, network, mount, cgroup or privilege mutation.
- `MON-020`: provide no process dump or descriptor-target interface.
- `MON-021`: construct no subprocess command, open no network socket/listener and emit no telemetry.
- `MON-022`: bound files, directories, processes, rows, units, startup entries, connections, modules, descriptors, devices, interfaces, history and timing.
- `MON-023`: distinguish unavailable observations from measured zero.
- `MON-024`: reject unknown, duplicate, malformed and oversized input values.
- `MON-025`: revalidate PID plus start ticks during explicit process inspection.

## Qualification

- `MON-026`: pass GCC, Clang, ASan/UBSan and focused static analyzers.
- `MON-027`: exercise every view with deterministic normal and hostile fixtures.
- `MON-028`: build byte-identically with the fixed reproducibility epoch.
- `MON-029`: retain PIE, NX stack, RELRO, BIND_NOW and exact x86-64-baseline target ISA.
- `MON-030`: keep public documentation independent from private behavioral research.
- `MON-031`: require explicit authorization before packaging or installation.
- `MON-032`: use guarded, hash-verified, compositor-tiled previews with bounded rollback.
