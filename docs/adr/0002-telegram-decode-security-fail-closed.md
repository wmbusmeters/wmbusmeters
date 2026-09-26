# 0002 - Telegram decode is fail-closed, warnings are once-per-telegram

## Status

Accepted

## Context

Meters send encrypted telegrams (EN 13757 security modes 5 and 7, and
OMS security profile D, mode 10, AES-CCM). A wrong or missing key must
never turn into plausible-looking meter values, and Walk-by storms must
not flood the log with repeated warnings.

## Decision

The decode chain is fail-closed:

1. An encrypted telegram arriving without a key is not decoded; a
   warning is logged (`d60493ee`, 2019-03-15).
2. A telegram whose decryption or MAC fails is ignored as a meter
   update (`e711d6ef`, 2020-02-03).
3. A decode failure for the same telegram warns only once
   (`6638e4f8`, 2021-01-06).
4. For OMS security profile D (TPL security mode 10) the AES-CCM
   authentication tag is verified before the payload is used; a failed
   tag yields `FAILED_DECODE` and follows the same once-warning policy
   (`84e6ad97`, `dea0e07e`, issue #2085).

## Consequences

- Missing keys produce quiet meters, not wrong readings; the fix is
  always "configure the key", never "trust the bytes anyway".
- The once-per-telegram warning keeps multi-telegram Walk-by captures
  readable.
- Corrupt or forged telegrams cannot smuggle values into the output.

## Evidence

- `src/wmbus.cc` implements the security modes and the tag check;
  `drivers/src/kamwater.xmq` carries the real-key test telegrams for
  mode 10 (`84e6ad97`, `dea0e07e`).
- `tests/test_qwds_walkby_nokey.sh` pins the once-warning and
  `FAILED_DECODE` behaviour for a Walk-by telegram sent twice without
  and with a wrong key.

## Alternatives considered

- Warn on every failing telegram: rejected, floods the log.
- Accept payloads whose tag or MAC does not verify: rejected, corruption
  and forgeries would surface as meter readings.