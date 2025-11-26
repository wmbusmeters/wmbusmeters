/*
 Copyright (C) 2025 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);
    };

    static bool ok = staticRegisterDriver([](DriverInfo&di)
    {
        di.setName("zenner0b");
        di.setDefaultFields("name,id,total_m3,monthly_m3,status,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        di.addMVT(MANUFACTURER_ZRI, 0x16, 0x0b);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField(
            "total",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "The total water consumption recorded by this meter.",
            Unit::M3);

        addNumericField(
            "monthly",
            Quantity::Volume,
            DEFAULT_PRINT_PROPERTIES,
            "The current month water consumption.",
            Unit::M3);

        addStringField(
            "status",
            "Meter status flags (bytes 0-3 from manufacturer data).",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS);
    }

    void Driver::processContent(Telegram *t)
    {
        // ZENNER EDC B.One wireless M-Bus module
        // Telegram scenario 322: Manufacturer specific data
        // The payload contains only a 0x0F marker followed by raw bytes:
        // bytes 0-3:   status (currently always 0x00000000)
        // bytes 4-7:   monthly consumption counter (little-endian, units of 1/256000 m³)
        // bytes 8-11:  total consumption counter (little-endian, units of 1/256000 m³)
        // bytes 12:    padding (0x00)

        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        if (bytes.size() < 12) return;

        // Extract total consumption (bytes 8-11, little-endian 32-bit)
        uint32_t total_raw = bytes[8] | (bytes[9] << 8) | (bytes[10] << 16) | (bytes[11] << 24);
        double total_m3 = total_raw / 256000.0;

        // Extract monthly consumption (bytes 4-7, little-endian 32-bit)
        uint32_t monthly_raw = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
        double monthly_m3 = monthly_raw / 256000.0;

        // Extract status (bytes 0-3, little-endian 32-bit)
        uint32_t status_raw = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        string status_str;
        if (status_raw == 0)
        {
            status_str = "OK";
        }
        else
        {
            // Format as hex for now until we decode the bit meanings
            status_str = tostrprintf("0x%08X", status_raw);
        }

        setNumericValue("total", Unit::M3, total_m3);
        setNumericValue("monthly", Unit::M3, monthly_m3);
        setStringValue("status", status_str, NULL);
    }
}

// Test: TestWater zenner0b 12345678 <AES_KEY_REQUIRED>
// telegram=|1E44496A677308500B167AD80010252F2F_0F0000000080BF1B0000A6420000|
// {"_":"telegram","media":"cold water","meter":"zenner0b","name":"TestWater","id":"12345678","total_m3":17.063,"monthly_m3":7.108,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |TestWater;12345678;17.063;7.108;OK;1111-11-11 11:11.11

