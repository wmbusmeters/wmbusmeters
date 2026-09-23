# 0005 - Tests are driven by real telegram hex, new driver behaviour needs a real key

## Status

Accepted

## Context

Each driver describes a meter that sits in someone's basement; protocol
regressions are expensive and synthetic fixtures keep missing the
quirks of real encodings.

## Decision

Tests are built from real telegrams:

- `simulations/*.txt` replay captured telegram hex through the normal
  device pipeline (`570c0c54`, 2020-09-08).
- Every xmq driver carries `tests { }` blocks with raw telegram hex and
  the expected json/fields output.
- Encrypted telegrams need the real AES key as part of the evidence; a
  test case without a decrypted telegram does not get added.
- Detection (manufacturer/version/type) is exercised through
  `--analyze`, not only with an explicit driver name.
- The shell tests under `tests/` share `tests/include.sh` and are
  registered in `test.sh`.

## Consequences

- Every new driver or decoding fix must bring a real telegram (and, for
  encrypted content, its key) as evidence.
- Test expectations in xmq files are updated by hand from actual
  program output; there is no snapshot tool.

## Evidence

- `6bff880a` (2018-03-05) modularized the source and added the first
  tests; `570c0c54` (2020-09-08) made simulations work end to end.
- `e39bfca4` (2021-08-09) adds automatic telegram fuzz seed generation
  from existing simulation test cases.
- `tests/test_analyze.sh` pins the `--analyze` detection path;
  issue #2098 adds detection tests for QDS,1a,07 and QDS,3f,37.

## Alternatives considered

- Synthetic fixtures only: rejected, real encodings are the failure
  mode.