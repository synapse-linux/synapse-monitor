# JSON contracts

Machine output is locale-neutral, contains one object, and ends with one newline.
Consumers must reject unknown schema majors. Every contract carries constant
`readOnly: true` and explicit coverage/truncation where rows are bounded.

## `synapse.monitor.snapshot/v1`

The Processes contract contains:

- sampled CPU/RAM/GPU/disk/network summary;
- fixed process selection, grouping, sorting, filter and columns;
- scan denials, races, malformed rows and truncation coverage;
- bounded PID/start-time-correlated process rows.

Rows contain PID, nullable UID, sanitized name, fixed class, state, threads,
resident bytes and nullable sampled CPU/I/O rates. They never contain commands,
environments, paths, open-file targets or mutation handles.

## `synapse.monitor.performance/v1`

The Performance contract contains:

- aggregate CPU and bounded nullable logical-processor milli-percent rows;
- memory totals;
- optional GPU/VRAM observations;
- bounded per-physical-disk read/write rates;
- bounded per-interface receive/transmit rates;
- optional bounded history arrays, empty for a single snapshot;
- observed/returned and truncation metadata.

Unavailable numeric values are `null`, never fabricated zeroes.

## `synapse.monitor.services/v1`

Each service row contains bounded name, description, `active|inactive` status,
`enabled|disabled|static|masked` startup state, nullable PID, user and safe
executable label. The label is a basename plus an explicit path-redaction marker;
unit paths and complete unit content are absent. `serviceMutation` is false.

## `synapse.monitor.startup/v1`

Each XDG startup row contains bounded ID, name, declared publisher or
`unavailable`, enabled/disabled state, desktop type, `system|user` scope,
semantic autostart location, safe command label and `launchPresent`. Arguments
and full source locations are discarded before rendering. `startupMutation` is
false.

## `synapse.monitor.connections/v1`

Each connection row contains fixed protocol, decoded local/remote endpoint,
state, nullable owning PID and nullable sanitized process name. Coverage includes
kernel rows, malformed input, denials, descriptor observations and owner-scan
truncation. The semantics declare `socketOpened: false` and
`connectionControl: false`.

Endpoints are local-session observations; no data is transmitted.

## `synapse.monitor.information/v1`

The Information contract contains nullable operating-system, kernel,
architecture, processor, system vendor/model, memory total, firmware
vendor/version/date and uptime fields. Host name and serial-number exposure are
explicitly false.

## `synapse.monitor.process-inspection/v1`

Explicit inspection contains:

- PID, start ticks and sanitized name;
- nullable real/effective/saved/filesystem UID and GID arrays;
- nullable inheritable, permitted, effective, bounding and ambient capability masks;
- nullable no-new-privileges and seccomp fields;
- unique bounded module basenames with mapping/truncation coverage;
- descriptor and socket counts with denial/truncation coverage.

PID plus start ticks are revalidated after collection. Module paths, mapping
addresses, descriptor targets, commands, environments, process dumps and
process-control handles are absent.
