# CLAUDE.md

Guidance for AI assistants (and human contributors) working in this repo.
Keep this file general and evergreen; it should rarely need editing.
Project overview and end-user docs are in `README.md`.

## Build & test

```
./configure && make              # builds build/wmbusmeters, build/testinternals
make test                        # full suite: unit tests + telegram simulations (CI runs this)
./build/testinternals [pattern]  # unit tests only; pattern matches a test name, e.g. "dvparser"
```

- `make test` runs `./test.sh build/wmbusmeters`, which runs `build/testinternals` followed by the telegram simulation suite. The suite forces `TZ=UTC`.
- A unit test reports failure by `assert(...)` (aborts, non-zero exit). `main` always returns 0, so judge pass/fail by the exit code / an abort message, not a printed counter.
- Telegram decode tests are `<test>` blocks inside the driver files `drivers/src/*.xmq`. They are aggregated into `tests/generated_tests.xmq` by `cd drivers && make database`. `tests/testit.sh` runs one telegram → expected-json → expected-fields triple.
- Driver formatting (CI lint, see `.github/workflows/lint.yml`): `cd drivers && make pretty`. CI fails the build if this rewrites any file, so run it after editing any `drivers/src/*.xmq` and commit the result.

## Development methodology

- **Test-first.** Write a failing test that pins down the spec'd behaviour, then implement until it passes. For decoding logic use a unit test in `src/testinternals.cc` or a `<test>` block in the relevant `drivers/src/*.xmq`. A failing-test commit followed by the implementation commit is welcome.
- **Don't speculate.** Implement only behaviour backed by (a) the standard (EN 13757-3 / OMS) or (b) the reference implementation. If you cannot point to either, leave a `// TODO` and ask before implementing. Do not invent data types, encodings, or unit suffixes.
- **Cite the source.** When implementing spec behaviour, cite the clause in a one-line code comment, matching the style already in `src/dvparser.cc` (e.g. `// EN 13757-3:2018 Annex A, Table A.8 (Type I / CP48)`).
- **Reference implementation.** M-Bus decoding is tracked against libmbus (rscada/libmbus, `mbus/mbus-protocol.c`, e.g. `mbus_data_tm_decode`). The EN 13757 / OMS specifications are **not** vendored in this repo — obtain them externally. `3rdparty/` holds build tooling (xmq), not specs.
- **Controlled units.** Units are intentionally enumerated and controlled (`wmbusmeters --listunits`). Do not synthesize or concatenate new unit strings on the fly; register a real unit instead.

## Code style & changes

- Keep diffs surgical: change the minimum needed, do not reformat unrelated code, and do not add comments that restate what the code does.
- Match surrounding style (4-space indent, brace placement, naming). C++17.
- Source lives in `src/`; meter drivers are declarative `drivers/src/*.xmq`; test harnesses and generated test data are under `tests/`.

## Commits

- Conventional Commits prefixes are used: `feat:`, `fix:`, `test:`, `chore:`, `build:`, `docs:`. Plain imperative mood is also accepted (e.g. `Add missing ultrimisv2.xmq file.`).
- One logical change per commit. Reference the issue/PR in the subject with `(#NNNN)` when relevant.