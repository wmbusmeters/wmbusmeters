/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("qcaloric");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_LSE, 0x08,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x08,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x08,  0x34);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField(
            "status",
            "Meter status from tpl status field.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS);

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current temperature.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::HCA,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::HCA,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_1",
            "The most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_1",
            "Heat cost allocation at the most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::HCA,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "set_date_17",
            "The 17 billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(17))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_17",
            "Heat cost allocation at the 17 billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::HCA,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(17))
            );

        addStringFieldWithExtractor(
            "error_date",
            "Date when the meter entered an error state.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractor(
            "device_date_time",
            "Date and time when the meter sent the telegram.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

    }
}


// Test: MyElement qcaloric 78563412 NOKEY
// telegram=|314493441234567835087a740000200b6e2701004b6e450100426c5f2ccb086e790000c2086c7f21326cffff046d200b7422|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement","id":"78563412","status":"OK","current_consumption_hca":127,"set_date":"2018-12-31","consumption_at_set_date_hca":145,"set_date_1":"2018-12-31","consumption_at_set_date_1_hca":145,"set_date_17":"2019-01-31","consumption_at_set_date_17_hca":79,"error_date":"2127-15-31","device_date_time":"2019-02-20 11:32","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement;78563412;127;2018-12-31;145;1111-11-11 11:11.11

// Test: MyElement2 qcaloric 90919293 NOKEY
// Comment: Test mostly proprietary telegram without values
// telegram=|49449344939291903408780DFF5F350082180000800007B06EFFFF970000009F2C70020000BE26970000000000010018002E001F002E0023FF210008000500020000002F046D220FA227|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","current_consumption_hca":null,"set_date":null,"consumption_at_set_date_hca":null,"set_date_1":null,"consumption_at_set_date_1_hca":null,"set_date_17":null,"consumption_at_set_date_17_hca":null,"error_date":null,"device_date_time":"2021-07-02 15:34","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;null;null;null;1111-11-11 11:11.11

// Comment: Normal telegram that fills in values.
// telegram=|314493449392919034087a520000200b6e9700004b6e700200426c9f2ccb086e970000c2086cbe26326cffff046d2d16a227|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","current_consumption_hca":97,"set_date":"2020-12-31","consumption_at_set_date_hca":270,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":270,"set_date_17":"2021-06-30","consumption_at_set_date_17_hca":97,"error_date":"2127-15-31","device_date_time":"2021-07-02 22:45","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;97;2020-12-31;270;1111-11-11 11:11.11

// Comment: Another mostly empty telegram, but values are now valid.
// telegram=|49449344939291903408780DFF5F350082180000800007B06EFFFF970000009F2C70020000BE26970000000000010018002E001F002E0023FF210008000500020000002F046D220FA228|
// {"media":"heat cost allocation","meter":"qcaloric","name":"MyElement2","id":"90919293","status":"OK","current_consumption_hca":97,"set_date":"2020-12-31","consumption_at_set_date_hca":270,"set_date_1":"2020-12-31","consumption_at_set_date_1_hca":270,"set_date_17":"2021-06-30","consumption_at_set_date_17_hca":97,"error_date":"2127-15-31","device_date_time":"2021-08-02 15:34","timestamp":"1111-11-11T11:11:11Z"}
// |MyElement2;90919293;97;2020-12-31;270;1111-11-11 11:11.11
