# AI Agent Context: wmbusmeters Custom XMQ Driver Generation

> **Purpose**: This file is designed to be read by an AI agent to provide full context for generating custom wmbusmeters XMQ driver files, building the project, and testing drivers against MBUS frames. The agent should follow this document end-to-end.

---

## 1. Overview

**wmbusmeters** is an open-source tool that reads wireless M-Bus (wMBus) and wired M-Bus telegrams from utility meters (water, heat, gas, electricity, etc.) and outputs structured JSON data.

**Custom XMQ drivers** define how raw MBUS telegram bytes are parsed into human-readable fields (temperature, volume, energy, etc.). Each driver is an `.xmq` file placed in `drivers/src/`.

### Your Goal as an AI Agent
1. **Read** the MBUS frame format document (provided by the user) to understand the telegram structure
2. **Analyze** the raw MBUS frame using `--analyze` to identify data fields
3. **Generate** an XMQ driver file in `drivers/src/<driver_name>.xmq`
4. **Build** the driver database and rebuild wmbusmeters
5. **Test** the driver against the provided MBUS frame
6. **Iterate** until the JSON output matches expected values

---

## 2. Inputs You Will Need

Before starting, ensure you have the following from the user:

| Input | Required | Description |
|-------|----------|-------------|
| **MBUS Frame Format Document** | ✅ Yes | PDF/document describing the telegram structure, data records, and their meanings |
| **MBUS Frame (hex telegram)** | ✅ Yes | The raw hex-encoded telegram string to build the driver for |
| **Decryption Key** | ⬜ Optional | 16-byte hex key (32 hex chars) for encrypted telegrams. Use `NOKEY` if unencrypted |
| **Device ID** | ✅ Yes | The meter's device ID (typically 8 digits, extracted from the telegram header) |

---

## 3. Step 1 — Analyze the Raw Telegram

Before writing any driver code, always analyze the telegram to understand its structure.

### For unencrypted telegrams:
```bash
build/wmbusmeters --analyze <MBUS_FRAME_HEX>
```

### For encrypted telegrams:
```bash
build/wmbusmeters --analyze=<DECRYPTION_KEY> <MBUS_FRAME_HEX>
```

### What the `--analyze` output tells you:
- **Header fields**: Manufacturer code (3-letter), version, device type
- **MVT (Manufacturer/Version/Type)**: Used in the `detect` block — e.g., `LAS,0A,2A`
- **Data records (DRs)**: Each DR has a DIF/VIF combination that maps to a specific quantity
- **DIF** = Data Information Field (data length + type: instantaneous, minimum, maximum, etc.)
- **VIF** = Value Information Field (what physical quantity + unit: temperature, volume, energy, etc.)
- **Storage numbers, tariff numbers, subunit**: Used to distinguish between multiple values of the same type

**The analyze output is your primary source of truth for building the driver.**

---

## 4. Step 2 — Understand XMQ Driver Structure

Every XMQ driver file follows this structure:

```xmq
// Copyright notice
driver {
    name           = <driver_name>
    meter_type     = <MeterType>
    default_fields = <comma_separated_field_names>

    detect {
        mvt = <MFR>,<VER>,<TYPE>
    }

    // Optional: reuse common library fields
    library {
        use = meter_datetime
        use = total_m3
    }

    fields {
        field {
            name         = <field_name>
            quantity     = <Quantity>
            display_unit = <unit>        // optional
            vif_scaling  = <None|Auto>   // optional, default Auto
            info         = 'Description' // optional
            match {
                measurement_type = <MeasurementType>
                vif_range        = <VIFRange>
                storage_nr       = <number>    // optional
                tariff_nr        = <number>    // optional
                difvifkey        = <hex_key>   // alternative: match by raw DIF/VIF key
            }
        }
        // ... more fields
    }

    tests {
        test {
            args     = '<meter_name> <driver_name> <device_id> <key_or_NOKEY>'
            comment  = 'Description of test case'
            telegram = '<hex_telegram>'
            json     = '<expected_json_output>'
            fields   = '<expected_fields_output>'
        }
    }
}
```

### Key Properties

#### `meter_type` — Valid values:
| Value | Use For |
|-------|---------|
| `WaterMeter` | Water consumption meters |
| `HeatMeter` | Heat energy meters |
| `HeatCoolingMeter` | Combined heat/cooling meters |
| `ElectricityMeter` | Electricity meters |
| `GasMeter` | Gas consumption meters |
| `TempHygroMeter` | Temperature/humidity/CO2 sensors |
| `PulseCounter` | Generic pulse counter devices |
| `PressureSensor` | Pressure measurement devices |
| `HeatCostAllocationMeter` | Heat cost allocators (HCA) |

#### `measurement_type` — Valid values:
| Value | Description |
|-------|-------------|
| `Instantaneous` | Current/latest reading |
| `Minimum` | Minimum recorded value |
| `Maximum` | Maximum recorded value |
| `AtError` | Value recorded during error condition |

#### `vif_range` — Common VIF ranges:
| VIF Range | Quantity | Default Unit | Hex Range |
|-----------|----------|--------------|-----------|
| `EnergyWh` | Energy | KWH | 0x00–0x07 |
| `EnergyMJ` | Energy | MJ | 0x08–0x0F |
| `Volume` | Volume | M3 | 0x10–0x17 |
| `Mass` | Mass | KG | 0x18–0x1F |
| `OnTime` | Time | Hour | 0x20–0x23 |
| `OperatingTime` | Time | Hour | 0x24–0x27 |
| `PowerW` | Power | KW | 0x28–0x2F |
| `PowerJh` | Power | MJH | 0x30–0x37 |
| `VolumeFlow` | Flow | M3H | 0x38–0x3F |
| `FlowTemperature` | Temperature | C | 0x58–0x5B |
| `ReturnTemperature` | Temperature | C | 0x5C–0x5F |
| `TemperatureDifference` | Temperature | C | 0x60–0x63 |
| `ExternalTemperature` | Temperature | C | 0x64–0x67 |
| `Pressure` | Pressure | BAR | 0x68–0x6B |
| `Date` | PointInTime | DateLT | 0x6C |
| `DateTime` | PointInTime | DateTimeLT | 0x6D |
| `HeatCostAllocation` | HCA | HCA | 0x6E |
| `FabricationNo` | Text | TXT | 0x78 |
| `RelativeHumidity` | RH | RH | 0x7B1A–0x7B1B |
| `SoftwareVersion` | Text | TXT | 0x7D0F |
| `ErrorFlags` | Text | TXT | 0x7D17 |
| `Dimensionless` | Counter | COUNTER | 0x7D3A |
| `Voltage` | Voltage | Volt | 0x7D40–0x7D4F |
| `Amperage` | Amperage | Ampere | 0x7D50–0x7D5F |
| `RemainingBattery` | Time | Day | 0x7D74 |

#### `quantity` — Valid values:
`Volume`, `Mass`, `Energy`, `Power`, `Flow`, `Temperature`, `Pressure`, `Time`, `Voltage`, `Amperage`, `RH` (Relative Humidity), `HCA`, `Text`, `Dimensionless`, `Counter`, `PointInTime`, `RelativeHumidity`

#### `display_unit` — Common values:
`c` (Celsius), `f` (Fahrenheit), `m3` (cubic meters), `l` (liters), `kwh`, `mj`, `gj`, `w`, `kw`, `bar`, `d` (days), `h` (hours), `min` (minutes)

#### `vif_scaling`:
- `Auto` (default) — wmbusmeters automatically scales based on VIF exponent
- `None` — raw integer value, no scaling applied (useful for counters/dimensionless)

#### Matching by `difvifkey`:
When the standard `vif_range` matching isn't sufficient (e.g., manufacturer-specific VIFs), use:
```xmq
match {
    difvifkey = 04FF22   // Raw hex DIF+VIF key from --analyze output
}
```

#### `lookup` blocks (for status/error flags):
```xmq
field {
    name     = status
    quantity = Text
    attributes = STATUS,INCLUDE_TPL_STATUS
    match { difvifkey = 04FF22 }
    lookup {
        name            = ERROR_FLAGS
        map_type        = BitToString
        mask_bits       = 0xFFFFFFFF
        default_message = OK
        map { name = ERROR_NAME  value = 0x00000001  test = Set }
        map { name = ANOTHER     value = 0x00000002  test = Set }
    }
}
```

---

## 5. Step 3 — Generate the XMQ Driver File

### File location:
```
drivers/src/<driver_name>.xmq
```

### Naming conventions:
- Use lowercase, no spaces, no special characters
- Name should reflect the meter manufacturer/model (e.g., `lansenco2`, `dmesharky775`, `elster`)

### Process:
1. Parse the `--analyze` output to identify all data records
2. Cross-reference with the MBUS frame format document to understand field meanings
3. Map each data record to an XMQ `field {}` block
4. Set the correct `measurement_type`, `vif_range`, `storage_nr`, and other match criteria
5. Include at least one `test {}` block with the provided telegram and expected output

---

## 6. Step 4 — Build the Driver

After creating/modifying the XMQ file, you must regenerate the driver database and rebuild:

```bash
# Step 1: Navigate to the drivers directory and regenerate the database
cd drivers && make install && cd ..

# Step 2: Rebuild wmbusmeters from the project root
make -j$(nproc)
```

### What `make install` does in the drivers directory:
1. Runs `generate.sh` — compiles all `drivers/src/*.xmq` files into `src/generated_database.cc`
2. Runs test generation — compiles all test cases into `tests/generated_tests.xmq`
3. Copies the generated files to the main `src/` and `tests/` directories if changed

### Prerequisites:
- `xmq` command-line tool must be installed and in PATH
- `g++` or `clang++` compiler
- Standard build tools (make, etc.)

---

## 7. Step 5 — Test the Driver

### Quick test with a specific telegram:
```bash
build/wmbusmeters --format=json --ppjson <MBUS_FRAME_HEX> <meter_name> auto <DEVICE_ID> <KEY_or_NOKEY>
```

**Example:**
```bash
build/wmbusmeters --format=json --ppjson 5e443330851416000a2a7a0d0000252f2f0265370842653208820165420802fB1a0a0242fB1a03028201fB1af20102fd3a270242fd3a2c028201fd3a0402c201fd3aa1018240fd3a196a0223f1010227f10102fd0f3e0001fd1B102f2f2f2f metronix_perser auto 00161485 NOKEY
```

### Run the driver's built-in test cases:
```bash
cd drivers && make test && cd ..
```

### Run a specific driver's tests:
```bash
cd drivers && ./test.sh ../build/wmbusmeters src/<driver_name>.xmq && cd ..
```

### Verification checklist:
- [ ] JSON output contains all expected fields
- [ ] Field values match the MBUS frame format document
- [ ] Units are correct (check `display_unit`)
- [ ] No unexpected `null` or `0` values for fields that should have data
- [ ] The `--analyze` output fields all have corresponding driver fields (or are intentionally excluded)

---

## 8. Iteration & Debugging

### Common issues and fixes:

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Field shows `null` | Wrong `vif_range` or `storage_nr` | Re-check `--analyze` output for correct DIF/VIF |
| Field shows wrong value | Incorrect `vif_scaling` | Try `vif_scaling = None` for raw counters |
| Driver not detected | Wrong `mvt` in `detect` block | Verify manufacturer, version, type from `--analyze` |
| Build fails | Syntax error in XMQ | Check XMQ syntax (braces, quotes, equals signs) |
| Test case fails | Timestamp mismatch | Timestamps are normalized in tests; check other fields |

### Debugging tips:
- Always run `--analyze` first to understand the raw data
- Compare your XMQ field definitions against the `--analyze` key mappings
- Use `--ppjson` (pretty-print JSON) for readable output
- If a field uses manufacturer-specific VIF, use `difvifkey` matching instead of `vif_range`

---

## 9. Complete Workflow Summary

```
┌─────────────────────────────────────┐
│  1. Receive MBUS frame + format doc │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  2. Run --analyze on the frame      │
│     to identify data records        │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  3. Read frame format document      │
│     to understand field meanings    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  4. Create drivers/src/<name>.xmq   │
│     with fields matching the data   │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  5. cd drivers && make install      │
│     Regenerate driver database      │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  6. make -j$(nproc)                 │
│     Rebuild wmbusmeters binary      │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  7. Test with --format=json         │
│     Verify output matches expected  │
└──────────────┬──────────────────────┘
               │
               ▼
         ┌─────┴─────┐
         │ Output OK? │
         └─────┬─────┘
          No ┌─┘└─┐ Yes
             │    │
             ▼    ▼
     ┌───────────┐ ┌──────────────────┐
     │ Fix XMQ & │ │ Done! Ready for  │
     │ rebuild   │ │ git commit &     │
     └───────────┘ │ deployment       │
                   └──────────────────┘
```

---

## 10. Reference: Example XMQ Drivers

### Simple driver (Gas meter):
See `drivers/src/elster.xmq` — minimal driver with a single `Volume` field.

### Medium complexity (CO2 sensor):
See `drivers/src/lansenco2.xmq` — multiple temperature, humidity, and CO2 fields with storage numbers.

### Complex driver (Heat meter with error flags):
See `drivers/src/dmesharky775.xmq` — includes library references, lookup tables, and status flags.

### Pulse counter with multiple MVTs:
See `drivers/src/lansenop.xmq` — detects multiple meter type variants, uses dimensionless counters.

---

## 11. Important Notes

- **The AI agent CANNOT handle deployment.** After testing is complete, hand off to the human for git commit, push, and docker deployment steps.
- **Always preserve existing copyright headers** when editing files.
- **Never modify `src/generated_database.cc` directly** — it is auto-generated by `drivers/generate.sh`.
- **The `auto` keyword** in test commands tells wmbusmeters to auto-detect the driver based on the MVT. This works only after the driver database has been regenerated and the binary rebuilt.
- **Encrypted telegrams** will show garbled data without the correct key. Always pass the decryption key if the telegram is encrypted.
