# 0004 - Receivers are interchangeable sources, SDR receivers run as child processes

## Status

Accepted

## Context

The daemon must read telegrams from many receivers: im871a, amb8465,
cul, rc1180 USB dongles, RTL-SDR sticks running rtlwmbus or rtl433, and
simulation files. Linking every receiver's decoding into the daemon
would tie the build to librtlsdr and couple crashes in one receiver to
the whole program.

## Decision

Every receiver feeds the same telegram source abstraction; SDR
receivers run as child processes whose output is parsed like any other
device. rtlwmbus support arrived this way early (`b69b2770`,
`f214390c`, 2019-02). Multiple rtlwmbus devices are probed and detected
automatically, and a device can be excluded with `donotprobe=`
(`71804f1d`, `5e273ddb`, 2020). Broken link-layer frames are discarded
at the receiver, e.g. T1 telegrams with failed 3-out-of-6 checks
(`dfb6e19c`, 2026-09-21).

## Consequences

- The core never links librtlsdr; a crashing receiver is restarted
  instead of taking the daemon down.
- Simulation files use the same entry point as real devices, which is
  what the test policy (0005) relies on.

## Evidence

- `src/wmbus_rtlwmbus.cc`, `src/wmbus_rtl433.cc` and the
  `src/wmbus_*.cc` dongle implementations.
- Commits `b69b2770`, `f214390c`, `71804f1d`, `5e273ddb`, `dfb6e19c`.

## Alternatives considered

- Decoding the SDR samples inside the daemon: rejected, couples the
  build and the crash domain to librtlsdr.