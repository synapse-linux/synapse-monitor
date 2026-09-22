<!-- SPDX-License-Identifier: MIT -->
# Informational view coverage

This matrix is the independent first-party completeness gate for the C17
monitor and its Alpha 12 graphical projection. It names product requirements,
not external products.

| Area | Alpha 4 content | Safety treatment |
|---|---|---|
| Processes | Application/system/kernel groups; name, PID, user, state, threads, CPU, RAM, read and write rates; live totals | PID plus start-time correlation; no command, environment or path |
| Performance | Aggregate CPU, logical CPUs, RAM, one GPU summary with separately labelled utilization and memory, bounded GPU inventory and driver-exposed utilization/VRAM/temperature/clocks/power/fan parameters, fresh secure i915 GEM allocation when available, CPU/GPU/storage/battery/system temperatures, general fans, physical disks, network interfaces, 60-sample history | Local bounded counters; driver-reported graphics memory is distinguished from integrated shared allocations; the observed footprint explicitly adds complete process PSS and fresh i915 GEM while keeping kernel-used RAM and physical capacity separate; no fixed graphics total is fabricated; integrated-GPU temperature is never inferred; no telemetry |
| Services | Name, description, active state, startup state, PID, user, executable identity | Executable basename only; full path redacted; no service control |
| Startup Apps | Name, publisher, status, type, semantic location and command identity | Basename only; arguments and full location redacted; no startup mutation |
| Connections | Protocol, local endpoint, remote endpoint, state, PID and owning process | Host-namespace kernel-table decoding; zero-inode rows reported but omitted from stable GUI identities; IPv4/IPv6 socket families denied; no connection control |
| Information | OS, kernel, architecture, processor, vendor/model, memory, firmware and uptime | No host name, machine ID or serial number |
| Filtering | Bounded loaded-row search plus per-column value menu on every displayed table column | Search is printable ASCII and 64 bytes; value choices are finite native-generated tokens over at most 512 validated rows; QML materializes only reusable viewport delegates |
| Presentation | Reviewed columns with clickable ascending/descending indicators, continuous sorting, grouping, dense/balanced/wide layout, default/contrast/mono theme | Every visible table column maps to an exact native sort allowlist; local order/filter changes never restart the stream; unavailable values remain last and stable identity breaks ties |
| Graphical transport | Versioned capability discovery, bounded full-frame NDJSON, stable row identities, nullable history and identity-checked process detail | Native adapter owns fixed argv, child lifetime, 2 MiB cap, exact majors, sequence and identities; QML owns typed visual presentation only |
| Deep inspection | Numeric Linux credentials, capabilities, seccomp, no-new-privileges, module basenames, descriptor/socket counts | Explicit PID/start ticks through the native adapter; stale response discarded; no module paths, descriptor targets or dumps |
| Graphical views | Responsive Processes, Performance, Services, Startup Apps, Connections and Information layouts; launch-selected locale with no in-window selector; translated reviewed identifiers; generic global typography | No JSON parsing, executable discovery, localization mutation, paths, argv, shell text or mutation controls in QML |

## Deliberately absent authority

Informational completeness does not authorize destructive parity. The following
remain absent rather than disabled UI affordances:

- kill, signal, suspend, resume, priority or affinity changes;
- process dumps or memory extraction;
- service start, stop, restart, enable or disable;
- startup enable, disable, add, remove or command editing;
- connection termination or firewall mutation;
- mount, cgroup, kernel, package or privilege operations;
- subprocess execution, listeners, analytics or telemetry.
