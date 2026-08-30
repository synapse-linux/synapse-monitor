# Informational view coverage

This matrix is the independent first-party completeness gate for the C17
monitor and its Alpha 5 graphical projection. It names product requirements,
not external products.

| Area | Alpha 4 content | Safety treatment |
|---|---|---|
| Processes | Application/system/kernel groups; name, PID, user, state, threads, CPU, RAM, read and write rates; live totals | PID plus start-time correlation; no command, environment or path |
| Performance | Aggregate CPU, logical CPUs, RAM, bounded GPU inventory and driver-exposed utilization/VRAM/temperature/clocks/power/fan parameters, CPU/GPU/storage/battery/system temperatures, general fans, physical disks, network interfaces, 60-sample history | Local bounded counters; unavailable is distinct from zero; integrated-GPU temperature is never inferred; no telemetry |
| Services | Name, description, active state, startup state, PID, user, executable identity | Executable basename only; full path redacted; no service control |
| Startup Apps | Name, publisher, status, type, semantic location and command identity | Basename only; arguments and full location redacted; no startup mutation |
| Connections | Protocol, local endpoint, remote endpoint, state, PID and owning process | In-process kernel-table decoding; zero-inode rows reported but omitted from stable GUI identities; no socket opened; no connection control |
| Information | OS, kernel, architecture, processor, vendor/model, memory, firmware and uptime | No host name, machine ID or serial number |
| Filtering | Bounded type-to-filter plus explicit `/` editor | Printable ASCII, 64-byte maximum |
| Presentation | Reviewed columns, sorting, grouping, dense/balanced/wide layout, default/contrast/mono theme | Closed identifiers; unknown and conflicting input fails closed |
| Graphical transport | Versioned capability discovery, bounded full-frame NDJSON, stable row identities, nullable history and identity-checked process detail | Native adapter owns fixed argv, child lifetime, 2 MiB cap, exact majors, sequence and identities; QML owns typed visual presentation only |
| Deep inspection | Numeric Linux credentials, capabilities, seccomp, no-new-privileges, module basenames, descriptor/socket counts | Explicit PID/start ticks through the native adapter; stale response discarded; no module paths, descriptor targets or dumps |
| Graphical views | Responsive Processes, Performance, Services, Startup Apps, Connections and Information layouts; translated reviewed identifiers; generic global typography | No JSON parsing, executable discovery, paths, argv, shell text or mutation controls in QML |

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
