# Architecture views

Each file is one mermaid view of how wmbusmeters works. The views show
the how; the why lives in [docs/adr/](../adr/0001-architecture-decision-records.md).
Existing prose documentation (`AGENTS.md`, `README.md`) stays the
canonical source for its topics; these views complement it.

| View | File | Shows | Records behind it |
|------|------|-------|-------------------|
| Telegram data flow | [dataflow.md](dataflow.md) | End to end: receiver -> decode -> driver -> output | 0004 |
| Decode security | [decode-security.md](decode-security.md) | Security modes 5/7/10, fail-closed paths, once-warning | 0002 |
| Driver pipeline | [driver-pipeline.md](driver-pipeline.md) | Build-time xmq -> generated database -> runtime match | 0003 |
| Module structure | [module-structure.md](module-structure.md) | Single binary and the modules extracted from util.cc | 0006 |