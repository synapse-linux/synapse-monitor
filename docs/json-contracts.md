# JSON contracts

Machine output is locale-neutral, contains unsigned byte/rate fields and ends
with one newline. Consumers must reject unknown schema majors.

## `synapse.monitor.snapshot/v1`

Top-level fields:

- `schema` and constant `readOnly: true`;
- `sampledMilliseconds`;
- `selection`: fixed `sort`, `group`, bounded `filter`, `limit`, and selected
  presentation columns;
- `summary`: typed CPU, memory, GPU, disk and network observations;
- `coverage`: process scan counts, denials, races, malformed rows, I/O gaps and
  truncation;
- bounded `rows`;
- `semantics`.

Unavailable numeric observations are `null`, never fabricated zeroes.

Each process row contains:

- PID and nullable numeric UID;
- sanitized bounded `name`, fixed `class`, and nullable/typed `groupKey`;
- state and thread count;
- whether the PID/start-time identity existed for the complete sample;
- nullable sampled CPU milli-percent;
- resident bytes;
- explicit I/O availability and nullable read/write bytes per second.

Rows never contain command lines, environment variables, credentials, tokens,
paths, open files or mutation handles. `groupKey` is display metadata, not an
executable identifier.
