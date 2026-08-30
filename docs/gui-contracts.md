# Graphical presentation contracts

The Alpha 5 graphical shell is a separate, presentation-only consumer. The C17
core owns observation, filtering, ordering, bounds, privacy and exact-major wire
contracts. The native Qt adapter owns transport validation and typed models. QML
owns layout, generic typography, color, charts, animation, translated labels and
accessibility.

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
  --sample-ms 250 --interval-ms 750 --limit 128
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
IDs, bounded filter text and presentation preferences through native methods. It must
not concatenate a command, construct argv, choose an executable or path, invoke
a shell, or receive stderr as display data.

The adapter must:

1. decode the capability object and reject unknown majors;
2. map typed enums to reviewed fixed argv internally;
3. cap each incoming line at 2 MiB before parsing;
4. require one JSON object and one expected view schema per line;
5. require strictly increasing stream sequence values;
6. preserve `null` as unavailable rather than converting it to zero;
7. reconcile rows using documented stable identity fields;
8. compare an inspection response's start ticks with the selected process row and discard it on mismatch;
9. stop and surface a bounded generic error if framing or schema validation fails;
10. terminate its child stream on GUI shutdown or view replacement.

Alpha 5 implements these checks before publishing any frame to QML. It also
bounds stderr to 32 KiB and capability/inspection documents to 256 KiB. Zero
socket inodes are reported as unavailable identity coverage and never enter the
connection row model.

No process, service, startup, connection, GPU, thermal, fan, power or clock
mutation is authorized by these contracts. The graphical implementation adds no
such authority indirectly.
