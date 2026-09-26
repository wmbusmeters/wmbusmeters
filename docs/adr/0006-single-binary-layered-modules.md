# 0006 - Single binary, structure grows by extracting modules from util.cc

## Status

Accepted

## Context

`util.cc` had grown into a grab-bag containing logging, crypto,
framing, filesystem, download, alarm and process logic. Users deploy
the program as one daemon; a library split would change packaging for
everyone.

## Decision

Keep shipping a single binary. Grow the structure by extracting
cohesive modules out of `util.cc` into their own files and directories,
without changing the deployment shape. The language standard moved to
C++17 in the same effort (`5c8bc74e`, 2026-03-21).

Extracted so far: logging (`15ea8aef`), license (`5c8bc74e`), crc16
(`80253a25`), crypto into `src/crypto` (`1522f927`), alarm into
`utils/alarm.cc` (`8381b778`), download (`db8d8c57`), access check
(`918f9db2`), signal handling (`4a0f5485`), link mode into
`wmbus/link_mode.cc` (`d9dde2ec`), slip framing (`5630b9a4`) and
filesystem (`ad20d83a`).

## Consequences

- The Makefile keeps producing one binary; new modules stay
  library-free internal units with their own unit tests.
- Each extraction is a separate `chore:` commit, easy to review and
  revert on its own.

## Evidence

- The commit list above; each extraction commit compiles and tests
  standalone.

## Alternatives considered

- Split into separate daemons or a library: rejected for now, it would
  change packaging and break single-binary deployments.