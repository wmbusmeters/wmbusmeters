# AGENTS.md


This file provides guidance to AI coding agents when working with code in this repository.

## Build Commands

```bash
# First-time setup (run once after clone)
./configure

# Standard build
make

# Debug build (ASAN + gcov coverage)
make DEBUG=true

# Cross-compile for Raspberry Pi ARM
make HOST=arm

# Remove build artifacts
make clean

# Install as system daemon
sudo make install

# Run all tests (same as CI) — invokes ./test.sh which runs:
#   build/testinternals (C++ unit tests), then ~60 shell scripts in tests/
#   covering protocol variants, config parsing, formulas, AES, mbus, etc.,
#   and finally tests/test_drivers.sh (all driver XMQ test cases)
make test

# Run tests with explicit binary path
./test.sh build/wmbusmeters

# Run all driver tests
cd drivers && make test

# Test a single driver
cd drivers && make testonly DRIVER=dme173

# Debug a specific telegram
build/wmbusmeters --debug simulation/serial_rtlwmbus_ok.msg
# --trace for even more verbose output
```

## Linting / Formatting

There is no C++ linter. The only code-style check that runs in CI is XMQ formatting:

```bash
# Auto-format all XMQ driver files (required before committing driver changes)
cd drivers && make pretty

# CI will fail with "Code is not properly formatted" if this is not run
```

`make pretty` pipes every `drivers/src/*.xmq` through the `xmq` tool and overwrites the file in place. Always run it after editing any `.xmq` file.

## Coding Conventions

- **Indentation**: 4 spaces (not tabs) in `.cc`/`.h` files
- **Header guards**: `#ifndef FOO_H` / `#define FOO_H` / `#endif` — not `#pragma once`
- **Includes**: project headers use `"quoted"` form without spaces: `#include"util.h"`; system headers use `<angle>` brackets; no enforced ordering
- **Naming**: free functions and variables use `snake_case`; types (`struct`, `class`, `enum`) use `PascalCase`; enum values may use either
- **Macros**: `X`-macro pattern used extensively for enums — see `LIST_OF_VIF_RANGES`, `LIST_OF_METER_TYPES`, `LIST_OF_TESTS`, etc.
- **Copyright header**: every `.cc` and `.h` file must start with `// Copyright (C) <year> Author (gpl-3.0-or-later)` (block-comment style for `.cc`, line-comment for `.xmq`)

## Commit Message Style

The project uses a loose conventional-commit style mixed with short imperatives. From recent history:

```
fix: Fix M-Bus Type G invalid date parsing (0xffff)
test: add unit tests for Type F and Type I date/time decoding
chore: extract fs.cc out of util.cc, fix some bugs
Add new ultrimisv2 driver.
Version 3.0.0 2026-06-17
```

Prefix with `fix:`, `test:`, `chore:`, or `build(deps):` when it applies. Short imperative sentences without prefix are also accepted. No strict enforcement.

## Driver Development

Drivers live in `drivers/src/*.xmq` and are written in XMQ format (a human-friendly XML/JSON alternative). The files `src/generated_database.cc` and `tests/generated_tests.xmq` are auto-generated from those XMQ sources — never edit them by hand.

**Workflow for adding/modifying a driver:**
1. Edit or create `drivers/src/<name>.xmq`
2. `cd drivers && make pretty` — auto-formats the XMQ file (required by CI lint check)
3. `cd drivers && make install` — re-embeds all drivers into `src/generated_database.cc` and updates `tests/generated_tests.xmq` (idempotent; diffs before copying)
4. `make` from the root — rebuilds the binary
5. `cd drivers && make testonly DRIVER=<name>` — runs only that driver's tests

## XMQ Driver Format

```xmq
// Copyright (C) <year> Author Name (gpl-3.0-or-later)
driver {
    name           = mydriver
    meter_type     = WaterMeter    // WaterMeter|GasMeter|HeatMeter|ElectricityMeter|
                                   // HeatCostAllocationMeter|TempHygroMeter|...
    default_fields = name,id,status,total_m3,timestamp
    manufacturer   = Kamstrup      // optional human-readable manufacturer name
    aliases        = multical21,flowiq2200  // alternative names users can specify

    detect {
        mvt = KAM,1c,16  // 3-char manufacturer code, version byte (hex), type byte (hex)
                         // multiple mvt lines allowed
    }

    mfct_tpl_status_bits {
        // Decodes manufacturer-specific error bits packed into the transport-layer
        // header status byte (not in a DV data record). Result is automatically
        // merged into any STATUS / INCLUDE_TPL_STATUS field.
        mask_bits       = 0xf8
        default_message = OK
        map {
            name  = PERMANENT_ERROR
            value = 0x08
            test  = Set    // Set|NotSet
        }
    }

    compact_frame_formats {
        // Hex DIF+VIF sequences that identify compact telegram frames.
        difvif = 040504FB8275...
    }

    library {
        use = total_m3            // pull in a shared field definition (see library fields below)
        use = flow_temperature_c  // comma-separated list also accepted
    }

    fields {
        field {
            name              = total
            quantity          = Volume    // Volume|Energy|Power|Temperature|Flow|
                                          // PointInTime|Text|Counter|Pressure|RH|
                                          // HCA|Dimensionless|Voltage|Amperage|...
            info              = 'Human-readable description of what this field measures.'
            vif_scaling       = Auto      // Auto|None|Tenth|... (default: Auto)
            dif_signedness    = Signed    // Signed (default) | Unsigned
                                          // Use Unsigned for non-compliant meters that send
                                          // unsigned binary values in DIF fields the standard
                                          // would treat as signed (e.g. counters that wrap at
                                          // 2^32 instead of going negative)
            display_unit      = m3        // override default unit for output
            readable_string   = Normal    // Normal|Reversed — byte order for text fields
            force_scale       = 0.001     // override the VIF-derived scale factor
            null_value        = 0xffff    // treat this raw value as null in output
            calculate         = 'expr'    // formula expression (see Formula System below)
            match_entire_payload = true   // rare: parse raw mfct payload as ixml
            attributes        = STATUS    // comma-separated PrintProperty flags:
                                          //   REQUIRED – print even with no data (NaN/null)
                                          //   DEPRECATED – field being phased out
                                          //   STATUS – primary status field; "OK" if no bits set
                                          //   INCLUDE_TPL_STATUS – also fold in TPL status byte
                                          //   INJECT_INTO_STATUS – merge into the STATUS field
                                          //   HIDE – suppress from output (intermediate calc only)
            match {
                measurement_type = Instantaneous  // Any|Instantaneous|Minimum|Maximum|AtError
                vif_range        = Volume          // common values:
                                                   //   Volume, VolumeFlow, EnergyWh, EnergyMJ,
                                                   //   PowerW, FlowTemperature, ReturnTemperature,
                                                   //   ExternalTemperature, TemperatureDifference,
                                                   //   Date, DateTime, Pressure, RelativeHumidity,
                                                   //   ErrorFlags, FabricationNo, FirmwareVersion,
                                                   //   HeatCostAllocation, Dimensionless,
                                                   //   AnyEnergyVIF, AnyVolumeVIF, AnyPowerVIF
                                                   // Full list: LIST_OF_VIF_RANGES in dvparser.h
                storage_nr       = 1              // optional: specific storage slot
                tariff_nr        = 2              // optional: specific tariff number
                add_combinable   = AtPhase1        // optional: require this VIFE combinable
                                                   // common: AtPhase1/2/3, ForwardFlow,
                                                   //   BackwardFlow, Average, UpperLimit, LowerLimit
                                                   // Full list: LIST_OF_VIF_COMBINABLES in dvparser.h
                difvifkey        = 0E833C          // alternative: exact DIF+VIF bytes (hex)
            }

            lookup {
                name            = ERROR_FLAGS
                map_type        = BitToString    // BitToString|IndexToString|DecimalsToString
                mask_bits       = 0xffffffff
                default_message = OK
                map {
                    name  = LEAK
                    value = 0x01
                    test  = Set
                }
            }
        }
    }

    tests {
        test {
            args     = 'metername mydriver meterid NOKEY'
            // Raw hex; _ separates DLL/TPL header from payload. NOKEY = unencrypted.
            // Replace NOKEY with a 32-hex-char AES key for encrypted meters.
            telegram = AABBCCDD...
            json     = '{"_":"telegram","id":"...","total_m3":1.23,...}'
            fields   = 'metername;meterid;1.23;1111-11-11 11:11.11'
            comment  = 'Optional description.'
        }
    }
}
```

**Key rules:**
- Every file requires a copyright header comment — `drivers/generate.sh` validates this at generation time.
- Field `name` must not include a unit suffix (write `total`, not `total_m3`). The unit suffix is appended automatically from `quantity` + `display_unit`.
- `force_scale` overrides the scale factor normally derived from the VIF byte. Use it when a meter encodes a value with a non-standard scale (e.g. `force_scale = 0.001` when the meter sends milliwatts but the VIF says watts).
- `null_value` specifies a raw numeric value that should be treated as missing/null in output (e.g. `null_value = -327.68` for a sensor that uses that sentinel when disconnected).
- `mfct_tpl_status_bits` decodes manufacturer-specific error flags in the transport-layer header byte. The result is automatically merged into any `STATUS`/`INCLUDE_TPL_STATUS` field.
- `match_entire_payload = true` is for unusual meters that use a proprietary ixml-format payload instead of standard DIF/VIF records.
- New drivers must be written in XMQ. No new C++ drivers are accepted.

**lookup `map_type` values:**
- `BitToString` — each `map` entry tests whether a specific bit in the value is set or clear; matched names are concatenated
- `IndexToString` — the value is used as a direct index into the map entries (`value = 0`, `value = 1`, …); returns the matching name
- `DecimalsToString` — the value is treated as a decimal counter; each map entry's `value` is successively subtracted from the remainder; each successful subtraction appends that entry's name. Used for meters that encode multiple status conditions packed as a decimal number (e.g. error code `2800` = `VERIFICATION_EXPIRED` + `WIRELESS_ERROR`).

## Formula System

Fields can use a `calculate` expression to derive their value from other already-extracted fields rather than from a matched DV entry.

### Syntax

Every literal and field reference carries a unit. Units are written directly after the number with no space:

| Literal | Meaning |
|---------|---------|
| `42m3` | 42 cubic metres |
| `16counter` | 16 (dimensionless counter) |
| `1h` | 1 hour |
| `0.001kwh` | 0.001 kWh |
| `'2000-01-01 00:00:00'` | datetime literal |

Operators (standard C precedence): `+`, `-`, `*`, `/`, `%`, `>>`, `<<`, `==`, `!=`, `<`, `>`, `<=`, `>=`. Parentheses for grouping. `sqrt(expr)` is the only built-in function.

Field references are bare names of other fields defined in the same driver, e.g. `total_m3`, `packed_raw_counter`. The referenced field's current value (with its unit) is substituted.

### String interpolation in field names

A field `name` can contain `{formula}` — the result is evaluated and substituted at runtime to generate dynamic field names:

```xmq
name = historic_{storage_counter}_m3
```

### Typical pattern

Fields used only as formula inputs should carry `attributes = HIDE` so they are not printed. The field that computes the final value uses `calculate`:

```xmq
field {
    name       = raw_temp
    quantity   = Dimensionless
    attributes = HIDE
    match { difvifkey = 02FB01 }
}
field {
    name      = temp_room_avg
    quantity  = Temperature
    calculate = 'raw_temp_counter * 0.01c'
}
```

The formula engine lives in `src/formula.cc` / `src/formula_implementation.h`. Tests are in `src/testinternals.cc` under test names matching `formulas_*`.

## Library Fields

The `library { use = ... }` directive pulls in a pre-defined field with its standard `FieldMatcher` already set. Multiple fields can be listed in a single `use`: `use = fabrication_no,software_version`.

Available library fields (defined in `src/meters.cc` via `addOptionalLibraryFields`):

**Identity / metadata**
`access_counter`, `customer`, `enhanced_id`, `fabrication_no`, `location`, `manufacturer`, `model_version`, `parameter_set`, `software_version`, `firmware_version`

**Time**
`actuality_duration_h`, `actuality_duration_s`, `meter_date`, `meter_date_at_error`, `meter_datetime`, `meter_datetime_at_error`, `on_time_h`, `on_time_at_error_h`, `operating_time_h`

**Volume**
`target_m3`, `target_date`, `total_backward_m3`, `total_forward_m3`, `total_m3`, `volume_flow_m3h`

**Energy / Heat**
`target_hca`, `target_kwh`, `total_kwh`, `consumption_hca`

**Temperature**
`external_temperature_c`, `flow_return_temperature_difference_c`, `flow_temperature_c`, `return_temperature_c`

**Electrical**
`battery_v`

## Architecture Overview

### Data Flow
1. **Hardware layer** (`wmbus_*.cc`): reads raw telegrams from USB dongles (im871a, iu891a, amb8465, cul, rc1180), RTL-SDR (`wmbus_rtlwmbus.cc`, `wmbus_rtl433.cc`), or simulation/replay files
2. **Protocol parsing** (`wmbus.cc`, `dvparser.cc`): decodes wM-Bus/M-Bus frames, strips CRCs (format A/B), decrypts AES-128 (modes 3/5/7), parses DIF/VIF data records into typed values
3. **Driver matching** (`drivers.cc`): looks up the telegram's MVT triple in `builtins_mvts_[]` to find the driver name; loads it on demand
4. **Field extraction** (`driver_dynamic.cc`): the XMQ string for the matched driver is parsed at runtime into `DriverInfo` + `FieldInfo` structures; each field's `FieldMatcher` is evaluated against the parsed DV entries
5. **Output** (`printer.cc`): formats matched fields to JSON, CSV, or human-readable text; dispatches to MQTT, HTTP POST, files, or stdout

### Key Source Files
| File | Purpose |
|------|---------|
| `src/drivers.cc` / `.h` | Driver registry: `BuiltinDriver builtins_[]` (XMQ strings), `MapToDriver builtins_mvts_[]` (MVT→name index) |
| `src/driver_dynamic.cc` / `.h` | Parses XMQ content at runtime into `DriverInfo`; `DriverDynamic::load()` is the entry point |
| `src/dvparser.cc` / `.h` | DIF/VIF record parser; `LIST_OF_VIF_RANGES` and `LIST_OF_VIF_COMBINABLES` enumerate all valid `vif_range` / `add_combinable` values |
| `src/meters.cc` | Library field definitions (`addOptionalLibraryFields`); meter-level logic |
| `src/meters_common_implementation.h` | Base class `MeterCommonImplementation` for all meter instances |
| `src/formula.cc` / `formula_implementation.h` | Formula expression parser and evaluator for `calculate` fields |
| `src/translatebits.cc` | `BitToString` / `IndexToString` lookup translation for status/error flag fields |
| `src/printer.cc` | Output formatting and dispatch |
| `src/units.cc` / `.h` | `Quantity` and `Unit` enums, `LIST_OF_QUANTITIES`, unit conversion |
| `src/generated_database.cc` | Auto-generated — do not edit; regenerated by `cd drivers && make install` |
| `src/testinternals.cc` | C++ unit tests (no external framework; see below) |

### C++ Drivers (Legacy)
`driver_izar.cc`, `driver_hydroclima.cc`, `driver_qheat.cc`, `driver_qwater.cc` predate XMQ and handle meters requiring custom parsing logic. They are not being removed but no new ones should be added.

### Driver Loading
Drivers load on demand when the first matching telegram arrives. `findBuiltinDriver(mfct, ver, type)` scans `builtins_mvts_[]`; `loadBuiltinDriver(name)` finds the matching `builtins_[]` entry and calls `DriverDynamic::load()` to parse the embedded XMQ string. `forceLoadAllDrivers()` is expensive — only used for `--list-drivers` and long-running socket servers.

## Testing

### Driver Tests
Each driver's `tests { test { ... } }` block specifies: raw telegram hex, expected JSON (keys sorted), expected CSV fields. The `_` in a telegram hex string separates the DLL/TPL header from the encrypted payload; `NOKEY` means unencrypted.

`cd drivers && make testonly DRIVER=<name>` calls `test_case.sh` which:
1. Runs `build/wmbusmeters --format=json` and compares output (via `jq --sort-keys`) against the expected JSON
2. Runs `build/wmbusmeters --format=fields` and compares against the expected CSV

**When a driver test fails**, the diff is printed to stdout. To inspect visually with `meld`:
```bash
USE_MELD=true cd drivers && make testonly DRIVER=<name>
```

To manually reproduce what the test does:
```bash
echo "telegram=AABBCCDD..." > /tmp/sim.txt
build/wmbusmeters --driver=mydriver --format=json /tmp/sim.txt metername mydriver meterid NOKEY
```

**Updating test expectations after changing field logic:** There is no automated way to capture new expected output — you must update the `json` and `fields` values in the `.xmq` test block by hand.

1. Run the binary against the telegram and capture the new output:
   ```bash
   echo "telegram=AABBCCDD..." > /tmp/sim.txt
   build/wmbusmeters --driver=mydriver --format=json /tmp/sim.txt metername mydriver meterid NOKEY \
     | jq --sort-keys .
   build/wmbusmeters --driver=mydriver --format=fields /tmp/sim.txt metername mydriver meterid NOKEY
   ```
2. Verify the output is correct, then paste it into the `json = '...'` and `fields = '...'` lines of the test block.
3. Re-run `cd drivers && make testonly DRIVER=<name>` to confirm the test now passes.

### C++ Unit Tests (`testinternals`)
`src/testinternals.cc` is a standalone binary (no external test framework). Tests are plain `void test_<name>()` functions registered in the `LIST_OF_TESTS` X-macro. They use `assert()` and `printf("ERROR! ...")` for failure reporting.

To add a new test:
1. Add `X(my_test)` to `LIST_OF_TESTS` in `testinternals.cc`
2. Implement `void test_my_test()` in the same file
3. Run: `make && build/testinternals my_test` (the argument is a substring filter)

Run a specific test by name (substring match):
```bash
build/testinternals formulas_parsing
build/testinternals --verbose crc
```

## CI

GitHub Actions runs on every push and PR:

| Workflow | What it checks |
|----------|----------------|
| `build_ubuntu.yml` | `./configure && make && make test` |
| `build_macosx.yml` | Same on macOS |
| `lint.yml` | `cd drivers && make pretty` — fails if any XMQ file is not formatted |
| `build_docker.yml` | Docker image build |
| `build_snap.yml` | Snap package build |

To reproduce CI locally: `./configure && make && make test` plus `cd drivers && make pretty && git diff --quiet drivers`.

## Runtime Configuration

- Main config: `/etc/wmbusmeters.conf` — keys: `loglevel`, `device`, `logtelegrams`, `format`
- Per-meter configs: `/etc/wmbusmeters.d/<name>` — keys: `name`, `type` (driver name), `id`, `key` (32-hex AES key; omit for unencrypted)
- Simulation/replay: pass a `.txt` or `.msg` file as the device; lines contain `T1;...;hex` or `C1;...;hex` followed by the expected JSON

## Dependencies
- `xmq` CLI tool in PATH — required by `drivers/generate.sh`, `drivers/Makefile`, and all test scripts; built from `3rdparty/xmq/` via `make build/xmq`
- `jq` — required by `test.sh`
- `librtlsdr`, `libusb`, `libxml2`, `libxslt` — required by `./configure`
- C++17 compiler (g++ or clang++)
