# Graphical presentation contracts

The Alpha 8 graphical shell is a separate, presentation-only consumer. The C17
core owns observation, bounded source selection, privacy and exact-major wire
contracts. The native Qt adapter owns transport validation, typed models and
bounded local row presentation. QML owns layout, generic typography, color,
charts, animation, translated labels and accessibility.

## Capability discovery

```text
synapse-monitor describe --format json
```

This emits one `synapse.monitor.presentation/v1` object and one trailing newline.
It lists the six ordered view IDs, their payload schemas, fixed sort/group/column
identifiers, stable row-identity fields, sampling limits, units, null semantics
and read-only authority. It deliberately contains no command template or path.

The machine-readable schema is `schemas/presentation-v1.schema.json`.
Consumers reject an unknown `presentation` major before starting a data stream.
Human labels are not part of this locale-neutral contract; the GUI translates
reviewed identifiers itself.

## Snapshots

```text
synapse-monitor snapshot --view VIEW --format json
```

A snapshot is exactly one existing view object on one line. A graphical adapter
can use snapshots for infrequently changing views or initial diagnostics.
Process rows expose `pid` plus boot-relative `startTicks`; connection rows expose
`socketInode`. These fields are stable local row identities, not mutation
handles. Services use `name`, startup entries use `id`, GPUs use `card`, and
resource rows use their documented typed keys.

## Bounded live stream

```text
synapse-monitor stream --view VIEW --format ndjson \
  --sample-ms 250 --interval-ms 750 --limit 512
```

`stream` emits `application/x-ndjson`. Each non-empty line is one complete view
object using the same exact-major schema as a snapshot. It additionally contains:

```json
"stream": {
  "schema": "synapse.monitor.stream-frame/v1",
  "sequence": 0,
  "intervalMilliseconds": 750
}
```

Sequence starts at zero and increases by one. Frames are full replacements, not
patches, so a GUI can atomically reconcile models by stable row identity. The
maximum contract line is 2 MiB. Backpressure blocks the producer; frames are not
queued or silently dropped. Probe failures go to stderr and terminate the
producer instead of contaminating the data stream with a second payload shape.

With no `--iterations`, a stream continues until its owning adapter closes or
terminates it. `--iterations 1..1000000` provides deterministic bounded runs for
tests and diagnostics. Sample and refresh intervals, output rows and retained
history remain bounded. The core opens no network socket and sends no telemetry.

Performance stream history contains at most 60 samples ordered oldest to newest.
Unavailable observations are JSON `null`; measured zero remains numeric `0`.
This distinction applies to CPU, RAM, GPU, disk and network chart series.

The stream metadata schema is `schemas/stream-frame-v1.schema.json`.

## Native adapter boundary

The native GUI adapter, not QML, owns executable discovery, process lifetime and
fixed argv. Normal discovery uses only the sibling or installed core. An absolute
backend override is rejected unless explicit test authority is present and is
never exposed to QML. QML may request typed view IDs, reviewed sort/group/column
IDs, bounded search text and presentation preferences through native methods.
Every visible table header maps to one declared sort ID and exposes the active
ascending/descending indicator. Sorting, loaded-row search and finite per-column
value filters are applied synchronously to at most 512 already validated rows;
they do not replace the child, clear the frame or reset stream sequence. Filter
values cross the QML boundary only as opaque 64-hex native tokens generated from
values already visible in the typed model. The adapter rejects unknown columns,
sort IDs, duplicate tokens and tokens not present in the current bounded cohort.
QML must not concatenate a command, construct argv, choose an executable or path,
invoke a shell, or receive stderr as display data.

The adapter must:

1. decode the capability object and reject unknown majors;
2. map typed enums to reviewed fixed argv internally;
3. cap each incoming line at 2 MiB before parsing;
4. require one JSON object and one expected view schema per line;
5. require strictly increasing stream sequence values;
6. preserve `null` as unavailable rather than converting it to zero;
7. reconcile rows using documented stable identity fields;
8. compare an inspection response's start ticks with the selected process row and discard it on mismatch;
9. validate every displayed row field's type and bound before publishing it;
10. keep unavailable sort values last in both directions and use stable identity as a deterministic tie-breaker;
11. retain inspection identity while rows reorder and filter;
12. stop and surface a bounded generic error if framing or schema validation fails;
13. terminate its child stream on GUI shutdown or view replacement.

Alpha 8 implements these checks before publishing any frame to QML. It also
bounds stderr to 32 KiB, capability/inspection documents to 256 KiB, row cohorts
and filter options to 512, and filter tokens to SHA-256-sized identifiers. Zero
socket inodes are reported as unavailable identity coverage and never enter the
connection row model.

GPU memory is never omitted from the top-level presentation. Driver sysfs used
and total counters are labelled as driver-reported graphics memory. An i915 GPU
may instead expose numeric global GEM allocated bytes from the existing bounded,
fresh, root-owned collector cache. Its total remains `null`,
`memoryOverlapsSystemRam` is true, and the GUI labels it shared and non-additive;
it is never called dedicated VRAM. The core accepts only the fixed collector,
requires a single unchanged regular file with safe ownership/mode, caps it at
128 KiB and 120 seconds, and parses unique exact metrics. The native adapter
accepts only `driver-sysfs|root-owned-fresh-collector|unavailable` source IDs and
rejects inconsistent kind/value/total/overlap/age combinations. Failure remains
explicitly unavailable rather than falling back to total RAM or Shmem.

Locale is selected once before QML loads from the bounded launch option or
session locale. No localization object or language selector is exposed to QML.

No process, service, startup, connection, GPU, thermal, fan, power or clock
mutation is authorized by these contracts. The graphical implementation adds no
such authority indirectly.
