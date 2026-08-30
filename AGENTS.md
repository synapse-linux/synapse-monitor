# Synapse Monitor agent contract

- The first-party runtime core is C17 and must remain useful without a GUI.
- Human CLI/TUI output is deterministic `en_US`; JSON identifiers and values are locale-neutral.
- Read only bounded local `/proc` and `/sys` observations. Report coverage gaps and races explicitly.
- Cap process scans, output rows, file sizes, devices, interfaces, filters and refresh intervals.
- Never expose command lines, environment, credentials, tokens, canonical paths or process-owned file contents.
- Alpha 1 provides no signal, kill, dump, priority, service, startup, connection-control, cgroup, mount or privilege authority.
- Never shell-evaluate data or construct mutation argv. No listener, telemetry, analytics or phoning home.
- Grouping, sorting and columns use fixed reviewed identifiers; unknown and duplicate values fail closed.
- Test roots require an explicit test-only switch and safe absolute normalized paths.
- Pass strict GCC/Clang, ASan/UBSan, analyzer, hostile fixtures, reproducibility, hardening and exact x86-64-baseline gates before packaging or deployment.
- Public documentation describes Synapse Monitor independently and does not name proprietary behavioral-research products.
