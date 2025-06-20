/*
 Copyright (C) 2022-2025 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"manufacturer_specificities.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    protected:
        void processContent(Telegram *t) override;
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("qwater");
        di.setDefaultFields("name,id,total_m3,"
                            "due_date_m3,"
                            "due_date,"
                            "status,"
                            "timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::S1);
        di.addDetection(MANUFACTURER_QDS, 0x37,  0x33);
        di.addDetection(MANUFACTURER_QDS, 0x37,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x16);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x16);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x17);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x17);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x18);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x18);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x19);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x1a);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x1a);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x35);
        di.usesProcessContent();

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :
        MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime");
        addOptionalLibraryFields("total_m3");

        addStringField(
            "status",
            "Meter status tpl status field.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "due_date",
            "The water consumption at the due date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "due",
            "The due date for billing date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "due_17_date",
            "The water consumption at the 17 due date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(17))
            );

        addNumericFieldWithExtractor(
            "due_17",
            "The due date for billing date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(17)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "Media volume flow when duration exceeds lower last.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            .add(VIFCombinable::DurationExceedsLowerLast)
            );

        addNumericFieldWithExtractor(
            "error",
            "The date the error occurred at. If no error, reads 2127-15-31 (FFFF).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date),
            Unit::DateLT
            );
    }
}

void Driver::processContent(Telegram *t) {
    auto it = t->dv_entries.find("0DFF5F");
    if (it == t->dv_entries.end()) {
        return;
    }
    DVEntry entry = it->second.second;
    if (entry.value.length() != 53 * 2) {
        return;
    }
    qdsExtractWalkByField(t, this, entry, 24, 8, "0C13", "total", Quantity::Volume);
    qdsExtractWalkByField(t, this, entry, 32, 4, "426C", "due", Quantity::PointInTime);
    qdsExtractWalkByField(t, this, entry, 36, 8, "4C13", "due_date", Quantity::Volume);
    qdsExtractWalkByField(t, this, entry, 44, 4, "C2086C", "due_17", Quantity::PointInTime);
    qdsExtractWalkByField(t, this, entry, 48, 8, "CC0813", "due_17_date", Quantity::Volume);
}

// Test: MyQWater qwater 12353648 NOKEY
// telegram=|374493444836351218067ac70000200c13911900004c1391170000426cbf2ccc081391170000c2086cbf2c02bb560000326cffff046d1e02de21fed0|
// {"_":"telegram","media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","status":"OK","total_m3":1.991,"due_date_m3":1.791,"due_date":"2021-12-31","due_17_date_m3":1.791,"due_17_date":"2021-12-31","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-01-30 02:30","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;1.991;1.791;2021-12-31;OK;1111-11-11 11:11.11

// And a second telegram that only updates the device date time.
// telegram=|47449344483635121806780dff5f350082da0000600107c113ffff48200000bf2c91170000df2120200000008001000000060019001000160018000d001300350017002f046d370cc422c759|
// {"_":"telegram","media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","status":"OK","total_m3":2.048,"due_date_m3":1.791,"due_date":"2021-12-31","due_17_date_m3":2.02,"due_17_date":"2022-01-31","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-02-04 12:55","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;2.048;1.791;2021-12-31;OK;1111-11-11 11:11.11

// Test: AnotherQWater qwater 66666666 NOKEY
// telegram=|3C449344682268363537726666666693443507720000200C13670512004C1361100300426CBF2CCC081344501100C2086CDF28326CFFFF046D0813CF29|
// {"_":"telegram","media":"water","meter":"qwater","name":"AnotherQWater","id":"66666666","status":"OK","total_m3":120.567,"due_date_m3":31.061,"due_date":"2021-12-31","due_17_date_m3":115.044,"due_17_date":"2022-08-31","error_date":"2128-03-31","meter_datetime":"2022-09-15 19:08","timestamp":"1111-11-11T11:11:11Z"}
// |AnotherQWater;66666666;120.567;31.061;2021-12-31;OK;1111-11-11 11:11.11

// Test: YetAnoter qwater 33333333 NOKEY
// telegram=|3C449344333333333537723333333393443506B8000020_0C13350000004C1300000000426CBF2CCC081300000000C2086CDF25326CFFFF046D0516CE26|
// {"_":"telegram","media":"warm water","meter":"qwater","name":"YetAnoter","id":"33333333","status":"OK","total_m3":0.035,"due_date_m3":0,"due_date":"2021-12-31","due_17_date_m3":0,"due_17_date":"2022-05-31","error_date":"2128-03-31","meter_datetime":"2022-06-14 22:05","timestamp":"1111-11-11T11:11:11Z"}
// |YetAnoter;33333333;0.035;0;2021-12-31;OK;1111-11-11 11:11.11

// Test: QWater-7-18 qwater 12230094 NOKEY
// telegram=|394493449400231218077ad30000200c13536712004c1307920500426cBf2ccc081373621200c2086cde2B02BB560000326cffff046d3714c32c|
// {"_":"telegram","media":"water","meter":"qwater","name":"QWater-7-18","id":"12230094","status":"OK","total_m3":126.753,"due_date_m3":59.207,"due_date":"2021-12-31","due_17_date_m3":126.273,"due_17_date":"2022-11-30","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-12-03 20:55","timestamp":"1111-11-11T11:11:11Z"}
// |QWater-7-18;12230094;126.753;59.207;2021-12-31;OK;1111-11-11 11:11.11


// Test: QWoo qwater 13144514 NOKEY
// Comment: Warm water
// telegram=|394493441445141316067A31000020_0C13671605004C1348160500426CDF2CCC081348160500C2086CDF2C02BB560000326CFFFF046D3414E121|
// {"_":"telegram","due_17_date": "2022-12-31","due_17_date_m3": 51.648,"due_date": "2022-12-31","due_date_m3": 51.648,"error_date": "2128-03-31","id": "13144514","media": "warm water","meter": "qwater","meter_datetime": "2023-01-01 20:52","name": "QWoo","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 51.667,"volume_flow_m3h": 0}
// |QWoo;13144514;51.667;51.648;2022-12-31;OK;1111-11-11 11:11.11

// Test: QWooo qwater 13176890 NOKEY
// Comment: Cold water
// telegram=|394493449068171316077A0B000020_0C13358612004C1307851200426CDF2CCC081307851200C2086CDF2C02BB560000326CFFFF046D3014E121|
// {"_":"telegram","due_17_date": "2022-12-31","due_17_date_m3": 128.507,"due_date": "2022-12-31","due_date_m3": 128.507,"error_date": "2128-03-31","id": "13176890","media": "water","meter": "qwater","meter_datetime": "2023-01-01 20:48","name": "QWooo","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 128.635,"volume_flow_m3h": 0}
// |QWooo;13176890;128.635;128.507;2022-12-31;OK;1111-11-11 11:11.11

// Test: QWooo qwater 78563412 NOKEY
// Comment: Proprietary Q walk-by message
// telegram=|49449344123456781606780DFF5F3500824E00007F0007C113FFFF63961300DF2C82731200FE2463811300A400F200D100A900DD00E000E90006011601EA0027010F012F046D0211F225|
// {"_":"telegram","due_17_date": "2023-04-30","due_17_date_m3": 138.163,"due_date": "2022-12-31","due_date_m3": 127.382,"id": "78563412","media": "warm water","meter": "qwater","meter_datetime": "2023-05-18 17:02","name": "QWooo","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 139.663}
// |QWooo;78563412;139.663;127.382;2022-12-31;OK;1111-11-11 11:11.11

// Test: QWaaa qwater 51220588 NOKEY
// Comment:
// telegram=|4944934488052251190778_0DFF5F350082930000810007C113FFFF91670400FF2C265402001E34332204000000EE00F201A501DB01C1015401B70178019701B901C9012F046D06091D35|
// {"_":"telegram","due_17_date": "2024-04-30","due_17_date_m3": 42.233,"due_date": "2023-12-31","due_date_m3": 25.426,"id": "51220588","media": "water","meter": "qwater","meter_datetime": "2024-05-29 09:06","name": "QWaaa","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 46.791}
// |QWaaa;51220588;46.791;25.426;2023-12-31;OK;1111-11-11 11:11.11

// Test: QWccc qwater 13492674 NOKEY
// Comment:
// telegram=|394493447426491317077ADD0000200C13975710004C1330970700426CFF2CCC081387471000C2086C1E3B02BB560000326CFFFF046D1E0F113C|
// {"_": "telegram","due_17_date": "2024-11-30","due_17_date_m3": 104.787,"due_date": "2023-12-31","due_date_m3": 79.73,"error_date": "2128-03-31","id": "13492674","media": "water","meter": "qwater","meter_datetime": "2024-12-17 15:30","name": "QWccc","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 105.797,"volume_flow_m3h": 0}
// |QWccc;13492674;105.797;79.73;2023-12-31;OK;1111-11-11 11:11.11

// Test: QWddd qwater 13334995 NOKEY
// Comment:
// telegram=|49449344954933131706780DFF5F3500823A0000600107C113FFFF29970300FF2C580303001E3B269703000A006500750073005B0070007D0061005B004200160000002F046D1E0F113C|
// {"_": "telegram","due_17_date": "2024-11-30","due_17_date_m3": 39.726,"due_date": "2023-12-31","due_date_m3": 30.358,"id": "13334995","media": "warm water","meter": "qwater","meter_datetime": "2024-12-17 15:30","name": "QWddd","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 39.729}
// |QWddd;13334995;39.729;30.358;2023-12-31;OK;1111-11-11 11:11.11

// Test: QQ1 qwater 37439212 NOKEY
// Comment:
// telegram=|_5344934412924337353778077912924337934435070DFF5F3500828A0000100007C113FFFF966600001F3C000000003E3419580000008000800080008000800080008000005A0094009C00BB002F046D010F3235|
// {"_": "telegram","due_17_date": "2025-04-30","due_17_date_m3": 5.819,"due_date": "2024-12-31","due_date_m3": 0,"id": "37439212","media": "radio converter (meter side)","meter": "qwater","meter_datetime": "2025-05-18 15:01","name": "QQ1","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 6.696}
// |QQ1;37439212;6.696;0;2024-12-31;OK;1111-11-11 11:11.11

// Test: QQ2 qwater 37432649 NOKEY
// Comment:
// telegram=|_5344934449264337353778077949264337934435070DFF5F350082560000110007C113FFFF245300001F3C210400003E348946000000800080008000800080008000002A0066005F00730072002F046D000F3235|
// {"_": "telegram","due_17_date": "2025-04-30","due_17_date_m3": 4.689,"due_date": "2024-12-31","due_date_m3": 0.421,"id": "37432649","media": "radio converter (meter side)","meter": "qwater","meter_datetime": "2025-05-18 15:00","name": "QQ2","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 5.324}
// |QQ2;37432649;5.324;0.421;2024-12-31;OK;1111-11-11 11:11.11

// Test: QQ3 qwater 60101441 NOKEY
// Comment:
// telegram=|39449344411410601A067ADB000020_0C13780000004C1300000000426CFFFFCC081335000000C2086C3F3502BB560000326CFFFF046D14173136|
// {"_": "telegram","due_17_date": "2025-05-31","due_17_date_m3": 0.035,"due_date": "2128-03-31","due_date_m3": 0,"error_date": "2128-03-31","id": "60101441","media": "warm water","meter": "qwater","meter_datetime": "2025-06-17 23:20","name": "QQ3","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 0.078,"volume_flow_m3h": 0}
// |QQ3;60101441;0.078;0;2128-03-31;OK;1111-11-11 11:11.11

// Test: QQ4 qwater 60113189 NOKEY
// Comment:
// telegram=|39449344893111601a077a580000200c13200300004c1300000000426cffffcc081334000000c2086c3f3502BB560000326cffff046d13173136|
// {"_": "telegram","due_17_date": "2025-05-31","due_17_date_m3": 0.034,"due_date": "2128-03-31","due_date_m3": 0,"error_date": "2128-03-31","id": "60113189","media": "water","meter": "qwater","meter_datetime": "2025-06-17 23:19","name": "QQ4","status": "OK","timestamp": "1111-11-11T11:11:11Z","total_m3": 0.32,"volume_flow_m3h": 0}
// |QQ4;60113189;0.32;0;2128-03-31;OK;1111-11-11 11:11.11
