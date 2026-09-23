# 0001 - Architecture decision records

## Status

Accepted

## Context

Architecture decisions in this project are visible only in git history,
`CHANGES` and the how-to oriented `AGENTS.md`. Neither records why a
behaviour is the way it is, which makes the recurring protocol and
security decisions (decryption failure handling, driver format, receiver
abstraction) easy to relitigate or accidentally regress.

## Decision

Architecture decisions are recorded in this directory as
`NNNN-slug.md`, numbered thematically (0001 is this bootstrap, then
protocol, drivers, receivers, testing, source structure).

Each record uses the sections `## Status`, `## Context`,
`## Decision`, `## Consequences`, `## Evidence` and
`## Alternatives considered`. An accepted record is never edited in
place; a change is a new record with `Superseded by NNNN` noted in the
old one's `## Status`. Rejected ideas are recorded too, so closed
debates stay closed.

`docs/architecture/` holds mermaid views of the current architecture;
the records hold the why, the views the how. They link, they do not
duplicate.

## Consequences

New architectural changes are expected to add a record. The records are
plain markdown, greppable, and every `## Evidence` section names only
commit hashes, files and issue/PR numbers - no prose-only claims.

## Evidence

- The decision history is currently unrecoverable without commit
  archaeology; `git log` of this repo since 425ab12b (2017-08-06) is the
  only existing source.
- `AGENTS.md` documents build and driver workflow but has no decision
  records.

## Alternatives considered

- Chronological numbering: rejected, thematic groups are easier to
  navigate once several decisions touch the same area.
- A single `ARCHITECTURE.md`: rejected, one decision per file keeps
  supersession clean.