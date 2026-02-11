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
        di.setName("zenner_hca");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::C1);
        di.addMVT(MANUFACTURER_ZRI, 0x08, 0xfc);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x20 ,"REMOVAL", TestBit::Set))
                 .add(Translate::Map(0x40 ,"DUAL_SENSOR_MODE", TestBit::Set))
                 .add(Translate::Map(0x80 ,"PRODUCT_SCALE", TestBit::Set))));

        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES   |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "last_month_date",
            "The most recent monthly billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(8))
            );

        addNumericFieldWithExtractor(
            "last_month_consumption",
            "Heat cost allocation at the most recent monthly billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(8))
            );

        addStringField(
            "monthly_consumption",
            "Monthly consumption values from compact profile (14 months backward from last_month_date).",
            DEFAULT_PRINT_PROPERTIES);
    }

    void Driver::processContent(Telegram *t)
    {
        // Parse the inverse compact profile (DIF/VIF key 8D04EE13).
        // Contains 14 monthly BCD HCA values going backward from last_month_date.
        auto it = t->dv_entries.find("8D04EE13");
        if (it == t->dv_entries.end()) return;

        DVEntry entry = it->second.second;
        string hex = entry.value;

        // Expected: 2 bytes spacing control (4 hex chars) + 14 × 3 bytes BCD (84 hex chars) = 88 hex chars
        if (hex.length() < 88) return;

        // Skip spacing control bytes (0x3B = 3-byte register spacing, 0xFE = increment backward)
        string values = "";
        for (int i = 0; i < 14; i++)
        {
            int pos = 4 + i * 6; // Skip 4 hex chars (2 bytes spacing), each BCD value is 6 hex chars (3 bytes)
            string bcd_hex = hex.substr(pos, 6);

            if (i > 0) values += ",";

            if (bcd_hex == "FFFFFF")
            {
                values += "null";
            }
            else
            {
                // 6-digit BCD, little-endian: "XXYYZZ" → byte0=XX, byte1=YY, byte2=ZZ
                // Each hex char IS a BCD decimal digit (0-9).
                // Reversed: ZZ YY XX → e.g., 270600 → 00 06 27 → 627
                int d0 = (bcd_hex[0] - '0') * 10 + (bcd_hex[1] - '0');
                int d1 = (bcd_hex[2] - '0') * 10 + (bcd_hex[3] - '0');
                int d2 = (bcd_hex[4] - '0') * 10 + (bcd_hex[5] - '0');
                int value = d2 * 10000 + d1 * 100 + d0;

                values += tostrprintf("%d", value);
            }
        }

        string result = "[" + values + "]";
        setStringValue("monthly_consumption", result, NULL);

        t->addSpecialExplanation(entry.offset, entry.value.length()/2,
                                 KindOfData::CONTENT, Understanding::FULL,
                                 "*** %s monthly consumption compact profile (%s)",
                                 hex.c_str(), result.c_str());
    }
}

// Test: ZennerHCA zenner_hca 80081812 DC7C9EF16126348CDFD52CE6567A9FFD
// telegram=|5e44496a12180880fc087a7040500584a63344b5af071a23539c6020cbba81fa6adcfe738682a0924d4f6f7d89d165c9144e4918cdc2d5f86be7b5bd8143528273accc131a13d0b100f2540dc1f2c379dde4984236d3bca424113cf0ee1bbd|
// {"_":"telegram","media":"heat cost allocation","meter":"zenner_hca","name":"ZennerHCA","id":"80081812","consumption_at_set_date_hca":627,"current_consumption_hca":339,"last_month_consumption_hca":272,"last_month_date":"2026-02-01","monthly_consumption":"[627,395,176,7,null,null,null,null,null,null,null,null,null,null]","set_date":"2026-01-01","status":"DUAL_SENSOR_MODE","timestamp":"1111-11-11T11:11:11Z"}
// |ZennerHCA;80081812;339;2026-01-01;627;1111-11-11 11:11.11

// Test: ZennerHCA2 zenner_hca 80081907 750381240D0A7E371D4CB8D1869D8F9B
// telegram=|5e44496a07190880fc087a714050058922c9598bdddfbb5bbf44b9d54830830eb6be3ba8117dfc88ae1b251837dfecb04e071554125366ef9d72dce87c9a099ad0cd9bf70f4a2e9c4c58d780444219c3f546c887fbc2d93c272e314f925473|
// {"_":"telegram","media":"heat cost allocation","meter":"zenner_hca","name":"ZennerHCA2","id":"80081907","consumption_at_set_date_hca":2,"current_consumption_hca":0,"last_month_consumption_hca":0,"last_month_date":"2026-02-01","monthly_consumption":"[2,0,0,0,null,null,null,null,null,null,null,null,null,null]","set_date":"2026-01-01","status":"DUAL_SENSOR_MODE","timestamp":"1111-11-11T11:11:11Z"}
// |ZennerHCA2;80081907;0;2026-01-01;2;1111-11-11 11:11.11
