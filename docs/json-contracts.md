# JSON contracts

Machine output is locale-neutral, contains one object, and ends with one newline.
Consumers must reject unknown schema majors. Every contract carries constant
`readOnly: true` and explicit coverage/truncation where rows are bounded.

## `synapse.monitor.presentation/v1`

`describe --format json` returns the locale-neutral GUI capability catalogue:
producer version, snapshot and stream framing, all ordered view/schema IDs,
closed sort/group/column sets, stable row identities, sampling bounds, units,
null/history semantics and explicit read-only/privacy authority. Human labels
and command templates are absent. Its machine-readable schema is
`schemas/presentation-v1.schema.json`.

## `synapse.monitor.stream-frame/v1`

Every object produced by `stream --format ndjson` retains its normal view schema
and adds a `stream` member containing this metadata schema, a zero-based strictly
increasing sequence and the selected interval in milliseconds. Each line is a
complete replacement frame capped by contract at 2 MiB. Errors remain on stderr
and terminate the stream. The metadata schema is
`schemas/stream-frame-v1.schema.json`.

## `synapse.monitor.snapshot/v1`

The Processes contract contains:

- sampled CPU/RAM/GPU/disk/network summary;
- fixed process selection, grouping, sorting, filter and columns;
- scan denials, races, malformed rows and truncation coverage;
- bounded PID/start-time-correlated process rows.

Rows contain PID plus boot-relative start ticks as stable identity, nullable
UID, sanitized name, fixed class, state, threads, resident bytes and nullable
sampled CPU/I/O rates. They never contain commands,
environments, paths, open-file targets or mutation handles.

## `synapse.monitor.performance/v2`

The Performance contract contains:

- aggregate CPU and bounded nullable logical-processor milli-percent rows;
- memory totals;
- a bounded multi-GPU inventory with PCI vendor/device identifiers, driver and
  an optional sanitized local `pci.ids` model label;
- independently nullable GPU utilization, driver-reported VRAM, hottest
  dedicated GPU sensor, current/maximum core clock, memory clock, average/input
  power, power cap and fan RPM;
- bounded CPU-package, CPU-core, GPU, storage, battery and other system
  temperature rows with optional maximum/critical thresholds;
- bounded general fan rows and sensor denial/malformed/truncation coverage;
- bounded per-physical-disk read/write rates;
- bounded per-interface receive/transmit rates;
- optional bounded history arrays, empty for a single snapshot and oldest-first
  in a stream; unavailable samples are `null` while measured zero is `0`;
- observed/returned and truncation metadata.

Units are fixed: milli-percent, bytes, hertz, microwatts, RPM and
millidegrees Celsius. Unavailable numeric values are `null`, never fabricated
zeroes. A driver-reported VRAM window is not claimed to be physically dedicated
memory. Integrated-GPU temperature is never inferred from CPU-package or thermal
zone values.

`performance/v1` remains a historical Alpha 2 contract; Alpha 3 and later emit v2.

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

Each connection row contains fixed protocol, a non-zero local socket-inode row
identity, decoded local/remote endpoint, state, nullable owning PID and nullable
sanitized process name. Transient kernel rows whose inode is zero cannot satisfy
the declared identity and are omitted. Coverage includes
`identityUnavailable` for those rows plus kernel rows, malformed input, denials,
descriptor observations and owner-scan truncation. The semantics declare `socketOpened: false` and
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
