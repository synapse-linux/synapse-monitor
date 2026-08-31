# Changelog

## 0.5.0-alpha.10

- Replace the adjacent GPU and GPU-memory summary cards with one unambiguous GPU card.
- Label utilization and shared-memory values independently inside that card while retaining the full per-device GPU detail section.
- Disable transparent huge pages before Qt initialization and select the Qt Quick software graphics API to reduce GUI RSS/PSS and avoid adding renderer GEM to the monitored footprint.
- Preserve additive observed-memory accounting, the absent language selector, native sorting and filters, populated Connections and all read-only authority boundaries.

## 0.5.0-alpha.9

- Add complete all-process PSS and fresh global i915 GEM into one checked observed-memory footprint, matching the physical accounting review while keeping kernel-used RAM and physical capacity separate.
- Present the observed footprint in decimal units so the reconciled value is directly legible as approximately 1.2 GB on the target.
- Label shared GEM as included in the observed total instead of showing the previous over-broad non-additive warning.
- Keep the observed footprint independently unavailable unless the root-owned collector reports a complete, untruncated, permission-clean PSS scan from the same fresh immutable sample.
- Add exact arithmetic, accounting-method, overlap and collector-row validation in the native adapter plus hostile duplicate/incomplete PSS fixtures.

## 0.5.0-alpha.8

- Remove the in-window language selector; the GUI chooses its bounded locale once at launch from `--locale` or the session locale.
- Measure i915 shared graphics allocations through the existing fresh root-owned `synapse_memory.prom` collector without granting the Monitor process additional privilege.
- Accept the collector only when it is a single regular non-writable file owned by the required authority, no larger than 128 KiB, no older than 120 seconds, unchanged while read and contains unique exact integer metrics.
- Report shared i915 GEM allocation bytes independently from the unavailable total pool, label them as overlapping system RAM and explicitly non-additive.
- Add typed `memorySource`, `memoryOverlapsSystemRam` and nullable sample-age fields; reject inconsistent or unapproved combinations in the native GUI adapter.
- Add normal and hostile collector fixtures covering writable files, duplicate metrics, stale data and symlinks, plus GUI contract tests for inconsistent shared-memory claims.

## 0.5.0-alpha.7

- Keep the validated frame and table visible while changing presentation order; header sorting is now an immediate native-model transform with no child restart, loading overlay or stream-sequence flash.
- Toggle ascending and descending order on repeated header activation, keep unavailable values last in both directions and use stable row identity as the deterministic tie-breaker.
- Add bounded Excel-style per-column value filters with search, select-all/select-none, apply and clear actions; QML receives opaque native tokens and never constructs filter argv.
- Apply the existing loaded-row search locally and raise the bounded GUI cohort to the contract maximum of 512 rows.
- Preserve process inspection identity and validated payload state while rows reorder or filters change.
- Replace the ambiguous compact locale control with the explicit `Language/Lingua: Italiano/English` selector.
- Add a dedicated GPU-memory card and distinguish driver-reported graphics memory from integrated shared system memory whose reliable use is unavailable without added privilege.
- Validate the type and bound of every displayed table field before it can enter the native presentation model.
- Add native continuous-sort/filter tests and a Qt Quick interaction test for header sorting and the per-column filter popup.

## 0.5.0-alpha.6

- Make every visible Processes, Services, Startup Apps and Connections column clickable and sortable through exact native per-view allowlists.
- Show inactive and active natural-order indicators and support keyboard activation on sortable headers.
- Add reviewed `class` and `location` core sort identifiers so displayed columns never require local QML sorting.
- Validate exact sort, group and column identifier sets before publishing capabilities to QML.
- Correct the guarded preview policy: retain the host network namespace for `/proc/net` observation while denying IPv4/IPv6 socket families and all IP traffic.
- Preserve fixed argv, bounded streams, stable identities, read-only authority and the Alpha 5 graphical layout.

## 0.5.0-alpha.5

- Add the native `synapse-monitor-gui` adapter and responsive QML presentation for all six read-only views.
- Discover and validate `synapse.monitor.presentation/v1` before starting any stream.
- Own fixed argv and child lifetime natively; cap input at 2 MiB, bound stderr, require exact view/stream majors and contiguous zero-based sequence, and fail closed.
- Reconcile table rows by PID/start ticks, service name, startup ID or socket inode while preserving unavailable values as null.
- Add identity-revalidated graphical process inspection without command lines, environments, paths or descriptor targets.
- Add responsive summaries, tables, nullable-history charts, complete GPU/thermal/fan detail and system-information cards.
- Add generic global `monospace` inheritance plus provisional `en_US` and `it_IT` GUI catalogues.
- Exclude zero-inode kernel connection rows from GUI reconciliation and report them explicitly as `identityUnavailable` coverage.
- Add Qt contract tests, six-view offscreen rendering, hostile sequence/identity/null tests and guarded test-only backend override.
- Preserve the independent C17 CLI/TUI, privacy boundary and complete absence of mutation authority.

## 0.4.0-alpha.4

- Record physical acceptance that all required informational elements are present while retaining the terminal view as a deliberately rough diagnostic surface.
- Add `synapse.monitor.presentation/v1` capability discovery for a separate graphical shell.
- Add bounded full-frame `application/x-ndjson` streaming for every view with `synapse.monitor.stream-frame/v1` sequence metadata.
- Preserve at most 60 oldest-first chart samples and encode unavailable history as `null`, distinct from measured zero.
- Add process PID/start-tick and connection socket-inode row identities for deterministic graphical reconciliation.
- Add machine-readable schemas and the native-adapter/QML authority boundary.
- Preserve all read-only, privacy, no-socket, no-listener and no-telemetry constraints.

## 0.3.0-alpha.3

- Correct Alpha 2's missing GPU-parameter and thermal coverage.
- Inventory up to 16 GPUs even when their drivers expose no utilization metric.
- Add GPU vendor/device IDs, driver, optional local PCI model label and independently nullable utilization,
  driver-reported VRAM, temperature, core/memory clocks, power/cap and fan RPM.
- Add bounded CPU-package/core, GPU, storage, battery and system temperatures,
  optional maximum/critical thresholds and general fan RPM observations.
- Add sensor denial, malformed-input and truncation coverage.
- Never infer an integrated-GPU temperature from CPU-package temperature.
- Emit `synapse.monitor.performance/v2` with fixed units and explicit nulls.

## 0.2.0-alpha.2

- Add six read-only Processes, Performance, Services, Startup Apps,
  Connections and System Information views.
- Add logical-CPU, per-disk and per-interface detail with bounded 60-sample
  terminal history.
- Add direct bounded systemd, XDG autostart and kernel socket-table inventories.
- Add safe service/startup executable labels while redacting full paths and arguments.
- Add PID/start-time-safe process inspection for Linux credentials,
  capabilities, seccomp, module basenames and descriptor counts.
- Add view tabs, type-to-filter, reviewed per-view sorting and columns,
  three layouts and three terminal themes.
- Preserve the no-mutation, no-subprocess, no-socket and no-telemetry boundary.

## 0.1.0-alpha.1

- Add a bounded C17 read-only host and process sampler.
- Add deterministic text and `synapse.monitor.snapshot/v1` JSON.
- Add CPU, RAM, optional GPU, physical-disk and network summaries.
- Add fixed sorting, class/name grouping, bounded filtering and selectable columns.
- Add a dense interactive terminal watch without mutation authority.
