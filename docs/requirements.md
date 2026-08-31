# Alpha 10 requirements

## Functional views

- `MON-001`: produce deterministic text and exact-major locale-neutral JSON.
- `MON-002`: provide Processes, Performance, Services, Startup Apps,
  Connections and System Information views.
- `MON-003`: show CPU, RAM, bounded GPU inventory, physical-disk and non-loopback network summaries.
- `MON-004`: show bounded logical-CPU, multi-GPU, temperature, fan, per-disk and per-interface performance detail.
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
- `MON-033`: report independently nullable GPU utilization, driver-reported VRAM, temperature, clocks, power/cap and fan RPM where fixed sysfs interfaces expose them.
- `MON-034`: report bounded CPU-package, CPU-core, GPU, storage, battery and system temperatures with optional maximum/critical thresholds.
- `MON-035`: report bounded general fan RPM observations.
- `MON-036`: never infer an integrated-GPU temperature from CPU-package or generic thermal-zone temperature.
- `MON-037`: expose a versioned locale-neutral presentation-capability contract with all view schemas, closed controls, units, bounds and authority.
- `MON-038`: emit bounded NDJSON streams containing one complete view object per non-empty line.
- `MON-039`: attach zero-based strictly increasing stream sequence and selected interval metadata without changing the payload view schema.
- `MON-040`: retain at most 60 oldest-first graphical history samples and encode unavailable samples as `null`, distinct from measured zero.
- `MON-041`: expose stable read-only row identities for graphical reconciliation, including process PID/start ticks and connection socket inode.
- `MON-042`: keep command templates and visual styling out of the core capability contract; translated labels and visual composition belong to the GUI.
- `MON-043`: require a native adapter to own fixed argv, child lifetime, line caps and schema validation; QML must not construct commands or executable paths.
- `MON-044`: provide a responsive graphical projection of all six informational views without reducing C17 CLI/TUI utility.
- `MON-045`: validate the presentation contract before streaming and require exact view major, read-only flag, view ID, zero-based contiguous sequence and selected interval for every frame.
- `MON-046`: reconcile processes by PID/start ticks, services by name, startup entries by ID and connections by non-zero socket inode.
- `MON-047`: stop the child and present only a bounded generic error when framing, schema, sequence, identity or stderr validation fails.
- `MON-048`: expose process inspection only through the native adapter and discard a response when PID/start ticks no longer match the selected row.
- `MON-049`: render complete GPU, thermal, fan, disk and network availability without converting null to zero; chart nulls remain gaps.
- `MON-050`: translate reviewed identifiers in QML and request generic global `monospace` without coupling the font family to color themes.
- `MON-051`: omit zero-inode transient connection rows from graphical identity sets and report an explicit `identityUnavailable` coverage count.
- `MON-056`: make every visible process, service, startup and connection column sortable from its header through an exact native allowlist, with an active natural-order indicator.
- `MON-057`: observe host connection tables from the host network namespace while denying IPv4/IPv6 socket authority in the graphical sandbox.
- `MON-058`: apply table sorting in the validated native model without restarting the stream, clearing the current frame or exposing argv construction to QML.
- `MON-059`: toggle ascending/descending order, keep unavailable values last in both directions and use stable identity as the deterministic tie-breaker.
- `MON-060`: provide bounded Excel-style per-column value filters using only native-generated opaque tokens over at most 512 validated loaded rows.
- `MON-061`: expose no in-window locale selector; select one bounded catalogue at launch from the explicit option or session locale.
- `MON-062`: show GPU memory explicitly and distinguish driver-reported counters, integrated shared memory and unavailable use without inference.
- `MON-063`: preserve selected process identity and inspection payload while rows reorder or filter.
- `MON-064`: consume i915 shared allocation bytes only from the fixed fresh root-owned collector after bounded ownership, mode, link, size, age, unchanged-read and unique-metric validation; never add privilege to Monitor.
- `MON-065`: keep shared allocation use and total independently nullable, mark i915 GEM as system-RAM-backed, and never substitute total RAM or Shmem for a graphics pool.
- `MON-066`: when one fresh secure collector proves both a complete process-PSS scan and one unambiguous i915 GEM observation, expose and visibly present the exact checked sum as the observed-memory footprint while retaining kernel-used RAM and physical capacity separately.
- `MON-067`: mark observed-footprint components as potentially overlapping, reject incomplete PSS scans and fail closed on inconsistent arithmetic.
- `MON-068`: use one GPU summary card with independently labelled utilization and memory values; do not present adjacent GPU and GPU-memory summary cards.
- `MON-069`: disable transparent huge pages for the GUI process before Qt initialization and use the Qt Quick software graphics API, while preserving complete rendering and the native core boundary.

## Safety and privacy

- `MON-017`: expose no command line, environment, authentication credential, secret, serial number or full path.
- `MON-018`: represent executable and startup content only as bounded safe labels and semantic scopes.
- `MON-019`: provide no process, service, startup, network, mount, cgroup or privilege mutation.
- `MON-020`: provide no process dump or descriptor-target interface.
- `MON-021`: construct no subprocess command, open no network socket/listener and emit no telemetry.
- `MON-022`: bound files, directories, processes, rows, units, startup entries, connections, modules, descriptors, GPUs, sensor devices/channels, fans, temperatures, interfaces, history and timing.
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
- `MON-052`: exercise native contract decoding, wrong majors, sequence gaps, duplicate identities, null-versus-zero and process-identity mismatch.
- `MON-053`: render every graphical view offscreen at bounded full and compact sizes with zero QML runtime diagnostics.
- `MON-054`: reject a GUI backend-path override unless explicit test authority is present; never expose that path to QML.
- `MON-055`: treat the current two GUI catalogues as provisional and keep expansion to the pinned locale set as an independent release gate.
