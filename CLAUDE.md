# CLAUDE.md

Guidance for AI assistants (and human contributors) working in this repo.
Keep this file general and evergreen; it should rarely need editing.
Project overview and end-user docs are in `README.md`.

## Build & test

```
./configure && make              # builds build/wmbusmeters, build/testinternals
make test                        # full suite: unit tests + telegram simulations (CI runs this)
./build/testinternals [pattern]  # unit tests only; pattern substring-matches a test name, e.g. "dvparser"
cd drivers && make testonly DRIVER=iperl   # run one driver's <test> blocks
```

- Build deps (see `configure.ac`, `.github/workflows/build_ubuntu.yml`): `libxml-2.0`, `librtlsdr`, `libusb-1.0`. `./configure` is GNU autoconf and writes `build/<host>/spec.gmk`; the Makefile refuses to build until you run it. C++17 (`-std=c++17`).
- `make test` runs `./test.sh build/wmbusmeters`, which runs `build/testinternals` followed by the telegram simulation suite. The suite forces `TZ=UTC`.
- A unit test reports failure by `assert(...)` (aborts, non-zero exit). `main` always returns 0, so judge pass/fail by the exit code / an abort message, not a printed counter.
- Telegram decode tests are `<test>` blocks inside the driver files `drivers/src/*.xmq`. They are aggregated into `tests/generated_tests.xmq` by `cd drivers && make tests`; `cd drivers && make install` runs `pretty` + `database` + `tests` and copies the generated artifacts back into `src/generated_database.cc` and `tests/generated_tests.xmq` when they change. `tests/testit.sh` runs one telegram → expected-json → expected-fields triple.
- Driver formatting (CI lint, see `.github/workflows/lint.yml`): `cd drivers && make pretty`. CI runs this in `drivers/` and fails the build if it rewrites any file, so run it after editing any `drivers/src/*.xmq` and commit the result.
- Adding a driver is documented in README.md ("How to add a new driver"): collect a telegram with `--logtelegrams`, inspect with `wmbusmeters --analyze <hex>`, copy an existing `*.xmq` and adapt it, then `cd drivers && make; make install` and rebuild.

## Development methodology

- **Test-first.** Write a failing test that pins down the spec'd behaviour, then implement until it passes. For decoding logic use a unit test in `src/testinternals.cc` or a `<test>` block in the relevant `drivers/src/*.xmq`. A failing-test commit followed by the implementation commit is welcome.
- **Don't speculate.** Implement only behaviour backed by (a) the standard (EN 13757-3 / OMS) or (b) the reference implementation. If you cannot point to either, leave a `// TODO` and ask before implementing. Do not invent data types, encodings, or unit suffixes.
- **Cite the source.** When implementing spec behaviour, cite the clause in a one-line code comment, matching the style already in `src/dvparser.cc` (e.g. `// EN 13757-3:2018 Annex A, Table A.8 (Type I / CP48)`).
- **Reference implementation.** Cross-check M-Bus decoding against libmbus (rscada/libmbus, `mbus/mbus-protocol.c`, e.g. `mbus_data_tm_decode`) — the de-facto reference. It is **not** vendored here; neither are the EN 13757 / OMS specifications — obtain them externally. `3rdparty/` holds build tooling (xmq), not specs.
- **Controlled units.** Units are intentionally enumerated and controlled (`wmbusmeters --listunits`; the table is `LIST_OF_UNITS` in `src/units.h`). Do not synthesize or concatenate new unit strings on the fly; add a real unit to that table instead.

## Research tool (use this before deciding)

When you don't yet know something — a data type, an encoding, a driver's behaviour, a spec clause — research it before writing code. This is the project's research protocol:

- **Epistemic humility.** Treat your own hypotheses as incomplete drafts. State the assumption ("I assume X; I will probe Y to verify"), then actively look for evidence that *falsifies* it. Don't assert from memory.
- **Evidence-first.** Tie every factual claim to a specific source: a file:line in this repo, a clause in EN 13757 / OMS, or a function in the libmbus reference. If a claim has no source, it is a hypothesis, not a fact — label it as such.
- **Search → reason → loop.** Map the problem space, plan a few sub-questions, search broadly, then read the primary sources deeply. Re-evaluate after each step; stop when new searches add little.
- **Adversarial synthesis.** Before settling on a finding, try to find counter-evidence and reconcile any contradicting sources explicitly rather than ignoring them.
- **Verify against the repo, not assumptions.** The repo is the ground truth for this project: read `src/`, `drivers/`, `tests/`, and the CI workflows. If the repo and your memory disagree, trust the repo.
- **Flag the unknowns.** If you cannot verify something, say so and leave a `// TODO` rather than guessing. Make no project decision on unverified ground.

## DevMind (working principles)

A language-agnostic engineering mindset (a project-tailored form of the standalone `DevMind` spec). Keep it in mind for any non-trivial change:

- **Readability counts; simple is better than complex; DRY.** Prefer the simplest correct solution; factor out redundancy.
- **Test everything that can break**, test-first.
- **Pragmatism over dogmatism** — pick the tool the project already uses; match its idioms rather than importing a foreign style.
- **Surgical changes** — change the minimum, don't reformat unrelated code, and don't add comments that restate what the code does. Don't rewrite code you don't yet understand; research it first (see above).
- **Problem decomposition** — split work into small, independently testable units; choose the right data structure; consider complexity.
- **Architecture** — respect the existing module boundaries (core in `src/`, declarative drivers in `drivers/src/*.xmq`, generated artifacts in `src/generated_database.cc` and `tests/generated_tests.xmq`); design clear contracts between them.
- **Debugging/profiling** — isolate a reproducer before fixing; treat root cause, not symptom; profile before optimizing.
- **Version control & collaboration** — one logical change per commit, clean history, small focused PRs, respectful review.

## Code style & changes

- Keep diffs surgical: change the minimum needed, do not reformat unrelated code, and do not add comments that restate what the code does.
- Match surrounding style (4-space indent, brace placement, naming). C++17.
- Source lives in `src/`; meter drivers are declarative `drivers/src/*.xmq`; test harnesses and generated test data are under `tests/`; build tooling (xmq) is vendored under `3rdparty/`.

## Commits

- Conventional Commits prefixes are used: `feat:`, `fix:`, `test:`, `chore:`, `build:`, `docs:`. Plain imperative mood is also accepted (e.g. `Add missing ultrimisv2.xmq file.`).
- One logical change per commit. Reference the issue/PR in the subject with `(#NNNN)` when relevant.