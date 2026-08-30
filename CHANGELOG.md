# Changelog

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
