# Synapse Monitor

Synapse Monitor is a first-party, read-only system inspector for Synapse Linux.
Its C17 core provides a dense terminal interface and versioned JSON contracts
without requiring a graphical session.

Alpha 2 includes six complete informational views:

1. **Processes** — bounded application, system and kernel groups with CPU,
   memory, I/O, state, thread, PID and user observations.
2. **Performance** — overall and logical-processor CPU activity, RAM,
   optional GPU/VRAM, per-physical-disk rates, per-interface network rates and
   a 60-sample terminal history.
3. **Services** — name, description, active state, startup state, PID, user and
   a safe executable label.
4. **Startup Apps** — name, publisher when declared, state, type, semantic
   location and a safe command label with arguments redacted.
5. **Connections** — TCP/UDP protocol, local and remote endpoint, state and
   bounded socket-inode correlation to PID/process identity.
6. **Information** — operating system, kernel, architecture, processor,
   system model, physical memory, firmware and uptime without host names or
   serial numbers.

Explicit process inspection also reports PID/start-time identity, Linux
UID/GID and capability metadata, seccomp/no-new-privileges state, module
basenames, and descriptor/socket counts. It never exposes module paths,
descriptor targets, command lines or environments.

There are no kill, signal, dump, priority, service, startup, cgroup,
connection-control or privilege operations. The program constructs no
subprocess command, opens no network socket or listener, and sends no telemetry.

## Build and test

```bash
make clean all test
```

## Use

```bash
synapse-monitor snapshot
synapse-monitor snapshot --view performance --format json
synapse-monitor snapshot --view services --sort startup
synapse-monitor snapshot --view startup --filter portal
synapse-monitor snapshot --view connections --sort local
synapse-monitor snapshot --view information
synapse-monitor inspect --pid 1234 --format json
synapse-monitor watch
```

Interactive watch keys:

- `1`–`6` or Tab select a view;
- start typing or press `/` to filter the current table;
- uppercase `S`, `G`, and `C` cycle reviewed sorting, grouping and column sets;
- uppercase `L` and `T` cycle dense/balanced/wide layouts and default/contrast/mono themes;
- uppercase `Q` exits.

Human output is deterministic `en_US`. Machine output is locale-neutral and
uses exact-major contracts documented in `docs/json-contracts.md`. Unknown
views, options, identifiers, duplicate columns and oversized inputs fail closed.

Full paths, raw launch commands, authentication credentials, secrets and serial
numbers remain private. Safe executable basenames, semantic source scopes and
numeric kernel credential metadata are deliberately distinguished from those
private values.

The CLI contract remains provisional during Alpha 2, so translated manual pages
remain deferred until the command surface is definitive.
