# 0003 - Meter drivers are declarative xmq files, compiled into the binary

## Status

Accepted

## Context

Each meter model used to require a hand written `driver_*.cc` in C++.
That blocks most contributors, and the number of meter models grows
constantly.

## Decision

Meter drivers are declarative `drivers/src/*.xmq` files. At build time
`drivers/generate.sh` turns them into `src/generated_database.cc` (and
`tests/generated_tests.xmq`); at runtime the driver is found by its
manufacturer, version and type triple and loaded on demand. New drivers
are accepted only as xmq; the few remaining C++ drivers are legacy.
During development a driver file can be used directly with
`--analyze=drivers/src/<name>.xmq <hex>` (`9a99daf0`, 2023-11-12).

Shared field definitions are moving into `drivers/src/library.xmq`,
which is installed into the C++ code by the build (`962d01b4`,
`7ba673a4`, work in progress 2026-09).

## Consequences

- `src/generated_database.cc` and `tests/generated_tests.xmq` are
  generated and must never be edited by hand; the `xmq` tool becomes a
  build dependency (built from `3rdparty/xmq/`).
- Driver behaviour is testable without C++ knowledge, and the tests live
  next to the driver definition.

## Evidence

- `f954860d` (2024-05-28) adds the `drivers/` xmq directory,
  `1071817e` (2024-06-20) converts the first C++ driver
  (`sensostar.cc` -> `sensostar.xmq`).
- The `--analyze=<file.xmq>` path was added in `9a99daf0`.
- Driver matching by mvt triple lives in `src/drivers.cc`
  (`builtins_mvts_`).

## Alternatives considered

- Keep writing C++ drivers: rejected, contributor barrier is too high.
- XML driver files: rejected in favour of xmq, a more human friendly
  format.