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
        di.setDefaultFields("name,id,status,total_m3,target_m3,timestamp");
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
            "target",
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
        // bytes 4-7:   target consumption counter (little-endian, units of 1/256000 m³)
        // bytes 8-11:  total consumption counter (little-endian, units of 1/256000 m³)
        // bytes 12:    padding (0x00)

        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        if (bytes.size() < 12) return;

        int offset = t->header_size+t->mfct_0f_index; // This is where the mfct data starts in the telegram.

        // Extract total consumption (bytes 8-11, little-endian 32-bit)
        string total;
        strprintf(&total, "%02x%02x%02x%02x", bytes[8], bytes[9], bytes[10], bytes[11]);

        uint32_t total_raw = bytes[8] | (bytes[9] << 8) | (bytes[10] << 16) | (bytes[11] << 24);
        double total_m3 = total_raw / 256000.0;

        total = "*** "+total+" total consumption (%f m3)";
        t->addSpecialExplanation(offset+8, 4, KindOfData::CONTENT, Understanding::FULL, total.c_str(), total_m3);

        // Extract target consumption (bytes 4-7, little-endian 32-bit)
        string target;
        strprintf(&target, "%02x%02x%02x%02x", bytes[4], bytes[5], bytes[6], bytes[7]);

        uint32_t target_raw = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
        double target_m3 = target_raw / 256000.0;

        target = "*** "+target+" target consumption (%f m3)";
        t->addSpecialExplanation(offset+4, 4, KindOfData::CONTENT, Understanding::FULL, target.c_str(), target_m3);

        // Extract status (bytes 0-3, little-endian 32-bit)
        string status;
        strprintf(&status, "%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2], bytes[3]);

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

        status = "*** "+status+" status (%s)";
        t->addSpecialExplanation(offset+0, 4, KindOfData::CONTENT, Understanding::FULL, status.c_str(), status_str.c_str());

        setNumericValue("total", Unit::M3, total_m3);
        setNumericValue("target", Unit::M3, target_m3);
        setStringValue("status", status_str, NULL);
    }
}

// Test: TestWater zenner0b 50087367 NOKEY
// telegram=|1E44496A677308500B167AD80010252F2F_0F_00000000_80BF1B00_00A64200_00|
// {"_": "telegram","id": "50087367","media": "cold water","meter": "zenner0b","name": "TestWater","status": "OK","target_m3": 7.1035,"timestamp": "1111-11-11T11:11:11Z","total_m3": 17.062}
// |TestWater;50087367;OK;17.062;7.1035;1111-11-11 11:11.11
