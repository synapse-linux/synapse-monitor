# Changelog

## 0.2.0-alpha.2

- Add complete read-only Processes, Performance, Services, Startup Apps,
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
