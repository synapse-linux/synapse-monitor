# Informational view coverage

This matrix is the independent first-party completeness gate for the terminal
monitor. It names product requirements, not external products or visual
expression.

| Area | Alpha 4 content | Safety treatment |
|---|---|---|
| Processes | Application/system/kernel groups; name, PID, user, state, threads, CPU, RAM, read and write rates; live totals | PID plus start-time correlation; no command, environment or path |
| Performance | Aggregate CPU, logical CPUs, RAM, bounded GPU inventory and driver-exposed utilization/VRAM/temperature/clocks/power/fan parameters, CPU/GPU/storage/battery/system temperatures, general fans, physical disks, network interfaces, 60-sample history | Local bounded counters; unavailable is distinct from zero; integrated-GPU temperature is never inferred; no telemetry |
| Services | Name, description, active state, startup state, PID, user, executable identity | Executable basename only; full path redacted; no service control |
| Startup Apps | Name, publisher, status, type, semantic location and command identity | Basename only; arguments and full location redacted; no startup mutation |
| Connections | Protocol, local endpoint, remote endpoint, state, PID and owning process | In-process kernel-table decoding; no socket opened; no connection control |
| Information | OS, kernel, architecture, processor, vendor/model, memory, firmware and uptime | No host name, machine ID or serial number |
| Filtering | Bounded type-to-filter plus explicit `/` editor | Printable ASCII, 64-byte maximum |
| Presentation | Reviewed columns, sorting, grouping, dense/balanced/wide layout, default/contrast/mono theme | Closed identifiers; unknown and conflicting input fails closed |
| Graphical transport | Versioned capability discovery, JSON snapshots, bounded full-frame NDJSON, stable row identities and nullable history | Native adapter owns fixed argv and validation; QML owns only visual presentation |
| Deep inspection | Numeric Linux credentials, capabilities, seccomp, no-new-privileges, module basenames, descriptor/socket counts | Explicit PID; identity revalidation; no module paths, descriptor targets or dumps |

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
