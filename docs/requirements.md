# Alpha 1 requirements

## Functional

- `MON-001`: produce one bounded host/process snapshot as deterministic text.
- `MON-002`: produce exact-major locale-neutral JSON with explicit availability.
- `MON-003`: show CPU, RAM, optional GPU, physical-disk and non-loopback network summaries.
- `MON-004`: correlate process CPU and I/O deltas by PID plus start time.
- `MON-005`: group rows by fixed `class|name|none` modes.
- `MON-006`: sort by fixed `cpu|memory|read|write|name|pid` keys.
- `MON-007`: filter sanitized names using at most 64 printable ASCII bytes.
- `MON-008`: select unique reviewed columns; Name remains mandatory.
- `MON-009`: provide an interactive text watch with bounded refresh behavior.

## Safety and privacy

- `MON-010`: expose no command line, environment, credential, token or path.
- `MON-011`: provide no process, service, startup, network or privilege mutation.
- `MON-012`: open no listener and emit no telemetry.
- `MON-013`: bound files, process scans, rows, devices, interfaces and timing.
- `MON-014`: distinguish unavailable observations from measured zero.
- `MON-015`: reject unknown, duplicate, malformed and oversized input values.

## Qualification

- `MON-016`: pass GCC, Clang, ASan/UBSan and focused static analyzers.
- `MON-017`: build byte-identically with the fixed reproducibility epoch.
- `MON-018`: retain PIE, NX stack, RELRO, BIND_NOW and x86-64-baseline ISA.
- `MON-019`: require explicit user authorization before packaging, installation or live preview.
