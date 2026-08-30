# Synapse Monitor

Synapse Monitor is a first-party, read-only system inspector for Synapse Linux.
Its C17 core provides a dense terminal interface and versioned JSON contracts
without requiring a graphical session.

Alpha 7 refines the first-party native Qt adapter and responsive graphical shell over the accepted Alpha 4 contracts. Every visible table column now sorts immediately in the validated native model, toggles direction without restarting the stream, and provides a bounded Excel-style value filter. The language selector is explicit and GPU memory is always represented with driver-reported, shared or unavailable semantics. Guarded previews retain host connection tables while denying IPv4/IPv6 socket authority. The C17 executable remains independently useful in a console. Alpha 3 corrected the earlier GPU/thermal gap:

1. **Processes** — bounded application, system and kernel groups with CPU,
   memory, I/O, state, thread, PID and user observations.
2. **Performance** — overall and logical-processor CPU activity, RAM,
   bounded multi-GPU inventory and optional local PCI model label, driver-exposed utilization/VRAM, temperature,
   core/memory clocks, power/cap and fan speed, CPU/GPU/storage/battery/system
   temperatures, general fans, per-physical-disk rates, per-interface network
   rates and a 60-sample terminal history.
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

When the Qt 6 Core/Gui/QML/Quick/QuickControls2/Test SDK is available, `all`
and `test` also build and qualify `synapse-monitor-gui`. `BUILD_GUI=0` keeps an
explicit core-only build; `BUILD_GUI=1` fails closed when the GUI SDK is absent.

## Use

```bash
synapse-monitor snapshot
synapse-monitor snapshot --view performance --format json
synapse-monitor describe --format json
synapse-monitor stream --view performance --format ndjson --interval-ms 750
synapse-monitor snapshot --view services --sort startup
synapse-monitor snapshot --view startup --filter portal
synapse-monitor snapshot --view connections --sort local
synapse-monitor snapshot --view information
synapse-monitor inspect --pid 1234 --format json
synapse-monitor watch
synapse-monitor-gui
```

Interactive watch keys:

- `1`–`6` or Tab select a view;
- start typing or press `/` to filter the current table;
- uppercase `S`, `G`, and `C` cycle reviewed sorting, grouping and column sets;
- uppercase `L` and `T` cycle dense/balanced/wide layouts and default/contrast/mono themes;
- uppercase `Q` exits.

Human output is deterministic `en_US`. Machine output is locale-neutral and
uses exact-major contracts documented in `docs/json-contracts.md`. `describe`
provides the closed capability catalogue; `stream` emits bounded full-view
NDJSON frames with stable sequence and row identities for a native GUI adapter.
Unknown views, formats, options, identifiers, duplicate columns and oversized
inputs fail closed.

QML remains presentation-only: it translates identifiers and chooses responsive
layout, typography, color, charts and accessibility. The native adapter discovers
the sibling or installed C17 core without a shell; owns fixed argv and child
lifetime; enforces the 2 MiB line cap, exact schema majors, contiguous sequence,
stable identities and bounded stderr; and discards stale process inspection.
It also owns deterministic local ordering, direction and finite per-column filter
tokens over at most 512 validated rows. These presentation changes never restart
the child or clear the currently validated frame.
The normal GUI has no executable-path option. An absolute backend override is
accepted only with explicit test authority for isolated qualification.

The GUI requests generic `monospace`, leaving the concrete global family to
Fontconfig, and currently ships the same provisional `en_US` and `it_IT`
catalogue boundary as other early Synapse GUIs. Expansion to the pinned locale
set remains a release gate. Details are in `docs/gui-contracts.md`.

Full paths, raw launch commands, authentication credentials, secrets and serial
numbers remain private. Safe executable basenames, semantic source scopes,
numeric kernel credential metadata and non-identifying sensor labels are
deliberately distinguished from those private values.

Every hardware field has explicit availability. In particular, Synapse Monitor
never substitutes CPU-package temperature for an integrated GPU that has no
dedicated kernel temperature sensor. GPU memory is shown explicitly: a dedicated
driver counter remains driver-reported graphics memory, while an integrated i915
GPU is labelled shared system memory and its use remains unavailable when no
unprivileged reliable counter exists. Driver-unexposed values remain `null` or
`unavailable`, not fabricated zeroes.

The CLI contract remains provisional during Alpha 4, so translated manual pages
remain deferred until the command surface is definitive.
