# Manual Guide: Building Custom wmbusmeters XMQ Drivers

> **Audience**: Human developers who need to manually create, build, test, and deploy custom wmbusmeters XMQ drivers for MBUS/wMBUS meters.

---

## Prerequisites

Before you begin, make sure you have:

- [ ] Access to the `wmbusmeters` repository (cloned locally)
- [ ] Build tools installed (`g++` or `clang++`, `make`)
- [ ] The `xmq` command-line tool installed and in your PATH
- [ ] The `jq` command-line tool installed (for JSON pretty-printing in tests)

---

## What You'll Need (Gather These First!)

### 1. MBUS Frame Format Document (Required)
This is the manufacturer's documentation (usually a PDF) that describes the telegram data format for the specific meter model. It tells you what each data record means — e.g., "Record 1 = Current Temperature", "Record 2 = Total Volume".

> **Don't have it?** Ask the meter manufacturer or check their website. Without this document, you'll be guessing what each field means.

### 2. MBUS Frame / Telegram (Required)
This is the raw hex-encoded telegram captured from the meter. It looks something like:
```
5e443330851416000a2a7a0d0000252f2f0265370842653208820165420802fB1a0a0242fB1a03028201fB1af20102fd3a270242fd3a2c028201fd3a0402c201fd3aa1018240fd3a196a0223f1010227f10102fd0f3e0001fd1B102f2f2f2f
```

> **Don't have it?** You need to capture one from the actual meter using an RTL-SDR dongle + wmbusmeters, or the customer/field engineer should provide it.

### 3. Decryption Key (Optional)
If the meter encrypts its telegrams, you'll need the 16-byte AES key (32 hex characters):
```
0DF3EC35064F8E975360A4CA5D7428CA
```

> **Don't have it?** If the telegram is unencrypted, use `NOKEY`. If it's encrypted but you don't have the key, you cannot proceed — contact the meter owner.

---

## Step 1: Analyze the Telegram

First, let's see what's inside the raw telegram. This step reveals the data records, their types, and values.

### Unencrypted telegram:
```bash
build/wmbusmeters --analyze YOUR_MBUS_FRAME_HERE
```

### Encrypted telegram:
```bash
build/wmbusmeters --analyze=YOUR_KEY_HERE YOUR_MBUS_FRAME_HERE
```

### Example:
```bash
build/wmbusmeters --analyze 5e443330851416000a2a7a0d0000252f2f0265370842653208820165420802fB1a0a0242fB1a03028201fB1af20102fd3a270242fd3a2c028201fd3a0402c201fd3aa1018240fd3a196a0223f1010227f10102fd0f3e0001fd1B102f2f2f2f
```

### What to look for in the output:
1. **Manufacturer code** (3 letters, e.g., `LAS` for Lansen)
2. **Version** (hex, e.g., `0A`)
3. **Device type** (hex, e.g., `2A`)
4. **Data records** — each one will show:
   - **DIF/VIF** hex codes
   - **What quantity** it represents (Temperature, Volume, etc.)
   - **The decoded value**
   - **Storage number** (if applicable)

Write down the **MVT triplet** — you'll need it: `<MANUFACTURER>,<VERSION>,<TYPE>`

---

## Step 2: Create the XMQ Driver File

Create a new file at:
```
drivers/src/<your_driver_name>.xmq
```

> **Naming**: Use lowercase, no spaces. Name it after the manufacturer/model (e.g., `lansenco2.xmq`, `sharky775.xmq`).

### Template — Start here and fill in:

```xmq
// Copyright (C) 2026 Your Name (gpl-3.0-or-later)
// Driver for <Meter Manufacturer Model>
driver {
    name           = your_driver_name
    meter_type     = HeatMeter        // See meter_type table below
    default_fields = name,id,timestamp // Add your important fields here

    detect {
        mvt = MFR,VER,TYPE            // From the --analyze output
    }

    fields {
        field {
            name         = temperature
            quantity     = Temperature
            display_unit = c
            match {
                measurement_type = Instantaneous
                vif_range        = ExternalTemperature
            }
        }
        // Add more fields...
    }

    tests {
        test {
            args     = 'TestMeter your_driver_name DEVICE_ID KEY_or_NOKEY'
            comment  = 'Basic test case'
            telegram = 'YOUR_HEX_TELEGRAM_HERE'
            json     = 'PASTE_THE_ACTUAL_JSON_OUTPUT_HERE_AFTER_TESTING'
            fields   = 'PASTE_THE_ACTUAL_FIELDS_OUTPUT_HERE'
        }
    }
}
```

### Meter Type Reference:

| `meter_type` | When to use |
|-------------|-------------|
| `WaterMeter` | Water meters (cold/hot) |
| `HeatMeter` | Heat energy meters |
| `HeatCoolingMeter` | Combined heat + cooling |
| `ElectricityMeter` | Electricity meters |
| `GasMeter` | Gas meters |
| `TempHygroMeter` | Temperature/humidity sensors, CO2 sensors |
| `PulseCounter` | Pulse counter devices |
| `PressureSensor` | Pressure sensors |
| `HeatCostAllocationMeter` | HCA devices |

### VIF Range Reference:

| `vif_range` | What it matches | Typical `quantity` |
|-------------|-----------------|-------------------|
| `EnergyWh` | Energy in Wh/kWh | `Energy` |
| `EnergyMJ` | Energy in MJ/GJ | `Energy` |
| `Volume` | Volume in m³ | `Volume` |
| `Mass` | Mass in kg | `Mass` |
| `OnTime` | On time | `Time` |
| `OperatingTime` | Operating time | `Time` |
| `PowerW` | Power in W/kW | `Power` |
| `VolumeFlow` | Volume flow rate | `Flow` |
| `FlowTemperature` | Flow temperature | `Temperature` |
| `ReturnTemperature` | Return temperature | `Temperature` |
| `TemperatureDifference` | Temperature difference | `Temperature` |
| `ExternalTemperature` | External temperature | `Temperature` |
| `Pressure` | Pressure in bar | `Pressure` |
| `DateTime` | Date and time | `PointInTime` |
| `RelativeHumidity` | Relative humidity | `RelativeHumidity` |
| `SoftwareVersion` | Software version | `Text` |
| `ErrorFlags` | Error/status flags | `Text` |
| `Dimensionless` | Counters/dimensionless | `Dimensionless` |
| `Voltage` | Voltage | `Voltage` |
| `Amperage` | Current/amperage | `Amperage` |
| `FabricationNo` | Fabrication number | `Text` |

### Using `storage_nr` to Differentiate Fields:
If the meter sends multiple values of the same type (e.g., current temp + average temp), they're distinguished by **storage number**:
```xmq
// Current temperature (storage 0)
field {
    name = temperature
    quantity = Temperature
    match {
        measurement_type = Instantaneous
        vif_range = ExternalTemperature
        storage_nr = 0
    }
}
// Average temperature (storage 1)
field {
    name = temperature_avg_1h
    quantity = Temperature
    match {
        measurement_type = Instantaneous
        vif_range = ExternalTemperature
        storage_nr = 1
    }
}
```

### Matching by Raw DIF/VIF Key:
When standard `vif_range` doesn't work (manufacturer-specific fields), use the hex key from `--analyze`:
```xmq
field {
    name = status
    quantity = Text
    match {
        difvifkey = 04FF22
    }
}
```

---

## Step 3: Build

From the **project root directory**:

```bash
# 1. Generate the driver database from all XMQ files
cd drivers && make install && cd ..

# 2. Rebuild wmbusmeters with the new driver
make -j$(nproc)
```

> **Build failed?** Check your XMQ file for syntax errors — unmatched braces `{}`, missing `=` signs, or typos in field names.

---

## Step 4: Test

### Quick test:
```bash
build/wmbusmeters --format=json --ppjson YOUR_MBUS_FRAME_HERE TestMeter auto DEVICE_ID KEY_or_NOKEY
```

### Example:
```bash
build/wmbusmeters --format=json --ppjson 5e443330851416000a2a7a0d0000252f2f0265370842653208820165420802fB1a0a0242fB1a03028201fB1af20102fd3a270242fd3a2c028201fd3a0402c201fd3aa1018240fd3a196a0223f1010227f10102fd0f3e0001fd1B102f2f2f2f metronix_perser auto 00161485 NOKEY
```

### What to check:
1. ✅ All expected fields appear in the JSON output
2. ✅ Values match what the frame format document says they should be
3. ✅ Units are correct (°C, m³, kWh, etc.)
4. ✅ No `null` values for fields that should have data

### If something is wrong:
- **Field missing?** → Check that your `vif_range` and `storage_nr` match the `--analyze` output
- **Wrong value?** → Try adding `vif_scaling = None` if it's a counter/dimensionless value
- **Driver not detected?** → Verify your `mvt` triplet matches exactly

### Filling in the test block:
Once you get the correct output, copy it back into your XMQ file's `tests {}` block:
1. Copy the JSON output → paste into the `json` field
2. Run with `--format=fields` to get the fields output → paste into the `fields` field
3. Replace the timestamp with the test placeholder `1111-11-11T11:11:11Z`

### Run the formal test suite:
```bash
cd drivers && ./test.sh ../build/wmbusmeters src/your_driver_name.xmq && cd ..
```

---

## Step 5: Deploy to Blackbox Server

Once everything tests correctly, it's time to deploy.

### 5a. Commit and Push to GitHub

```bash
# Stage your changes
git add drivers/src/your_driver_name.xmq
git add src/generated_database.cc
git add tests/generated_tests.xmq

# Commit
git commit -m "Add driver for <meter manufacturer/model>"

# Push to GitHub
git push origin <your-branch>
```

### 5b. SSH into the Blackbox Server

```bash
ssh <username>@<blackbox-server-ip>
```

### 5c. Rebuild the Docker Container

```bash
# Navigate to the project directory on the server
cd /path/to/project

# Rebuild the metronix-client-server container (includes wmbusmeters)
docker compose build --no-cache metronix-client-server
```

### 5d. Restart the Containers (if needed)

```bash
# Stop running containers
docker compose down

# Start them again with the new build
docker compose up -d
```

### ⚠️ Troubleshooting: Docker Build Fails

If the docker build fails with a **GitHub authentication error**, the GitHub token may have expired.

**Fix:**
1. Open the `docker/.env` file on the server
2. Update the GitHub token and username:
   ```env
   GITHUB_TOKEN=your_new_token_here
   GITHUB_USERNAME=your_username_here
   ```
3. Generate a new token at: https://github.com/settings/tokens
4. Re-run the build:
   ```bash
   docker compose build --no-cache metronix-client-server
   ```

---

## Quick Reference Card

```
┌──────────────────────────────────────────────────────┐
│                   QUICK COMMANDS                      │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ANALYZE (unencrypted):                              │
│  build/wmbusmeters --analyze <FRAME>                 │
│                                                      │
│  ANALYZE (encrypted):                                │
│  build/wmbusmeters --analyze=<KEY> <FRAME>           │
│                                                      │
│  BUILD:                                              │
│  cd drivers && make install && cd .. && make          │
│                                                      │
│  TEST:                                               │
│  build/wmbusmeters --format=json --ppjson \           │
│    <FRAME> <NAME> auto <ID> <KEY|NOKEY>              │
│                                                      │
│  DEPLOY:                                             │
│  git add . && git commit && git push                 │
│  ssh server                                          │
│  docker compose build --no-cache metronix-client-... │
│  docker compose down && docker compose up -d         │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Appendix: Full XMQ Example (Minimal Driver)

Here's a complete, working example — the `elster` gas meter driver:

```xmq
// Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)
driver {
    name           = elster
    meter_type     = GasMeter
    default_fields = name,id,total_m3,timestamp
    detect {
        mvt = ELS,81,03
    }
    library {
        use = actuality_duration_s
    }
    fields {
        field {
            name     = total
            quantity = Volume
            info     = 'The total water consumption.'
            match {
                measurement_type = Instantaneous
                vif_range        = Volume
            }
        }
    }
    tests {
        test {
            args     = 'Gas elster 05105025 NOKEY'
            telegram = 3644A511640010253837722550100593158103E70020052F2F_0374E602000C137034220302FD74EE0F2F2F2F2F2F2F2F2F2F2F2F2F2F2F
            json     = '{"_":"telegram","media":"gas","meter":"elster","name":"Gas","id":"05105025","actuality_duration_s":742,"total_m3":3223.47,"timestamp":"1111-11-11T11:11:11Z"}'
            fields   = 'Gas;05105025;3223.47;1111-11-11 11:11.11'
        }
    }
}
```
