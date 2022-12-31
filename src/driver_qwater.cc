/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("qwater");
        di.setDefaultFields("name,id,total_m3,"
                            "due_date_m3,"
                            "due_date,"
                            "status,"
                            "timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::S1);
        di.addDetection(MANUFACTURER_QDS, 0x37,  0x33);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x18);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x18);
        di.addDetection(MANUFACTURER_QDS, 0x06,  0x35);
        di.addDetection(MANUFACTURER_QDS, 0x07,  0x35);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :
        MeterCommonImplementation(mi, di)
    {
        addOptionalCommonFields("meter_datetime");
        addOptionalFlowRelatedFields("total_m3");

        addStringField(
            "status",
            "Meter status tpl status field.",
            PrintProperty::JSON | PrintProperty::IMPORTANT | PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS);

        addNumericFieldWithExtractor(
            "due_date",
            "The water consumption at the due date.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "due",
            "The due date for billing date.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::PointInTime,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "due_17_date",
            "The water consumption at the 17 due date.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(17))
            );

        addNumericFieldWithExtractor(
            "due_17",
            "The due date for billing date.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::PointInTime,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(17)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "Media volume flow when duration exceeds lower last.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            .add(VIFCombinable::DurationExceedsLowerLast)
            );

        addNumericFieldWithExtractor(
            "error",
            "The date the error occured at. If no error, reads 2127-15-31 (FFFF).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::PointInTime,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date),
            Unit::DateLT
            );
    }
}

// Test: MyQWater qwater 12353648 NOKEY
// telegram=|374493444836351218067ac70000200c13911900004c1391170000426cbf2ccc081391170000c2086cbf2c02bb560000326cffff046d1e02de21fed0|
// {"media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","status":"OK","total_m3":1.991,"due_date_m3":1.791,"due_date":"2021-12-31","due_17_date_m3":1.791,"due_17_date":"2021-12-31","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-01-30 02:30","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;1.991;1.791;2021-12-31;OK;1111-11-11 11:11.11

// And a second telegram that only updates the device date time.
// telegram=|47449344483635121806780dff5f350082da0000600107c113ffff48200000bf2c91170000df2120200000008001000000060019001000160018000d001300350017002f046d370cc422c759|
// {"media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","status":"OK","total_m3":1.991,"due_date_m3":1.791,"due_date":"2021-12-31","due_17_date_m3":1.791,"due_17_date":"2021-12-31","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-02-04 12:55","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;1.991;1.791;2021-12-31;OK;1111-11-11 11:11.11

// Test: AnotherQWater qwater 66666666 NOKEY
// telegram=|3C449344682268363537726666666693443507720000200C13670512004C1361100300426CBF2CCC081344501100C2086CDF28326CFFFF046D0813CF29|
// {"media":"water","meter":"qwater","name":"AnotherQWater","id":"66666666","status":"OK","total_m3":120.567,"due_date_m3":31.061,"due_date":"2021-12-31","due_17_date_m3":115.044,"due_17_date":"2022-08-31","error_date":"2128-03-31","meter_datetime":"2022-09-15 19:08","timestamp":"1111-11-11T11:11:11Z"}
// |AnotherQWater;66666666;120.567;31.061;2021-12-31;OK;1111-11-11 11:11.11

// Test: YetAnoter qwater 33333333 NOKEY
// telegram=|3C449344333333333537723333333393443506B8000020_0C13350000004C1300000000426CBF2CCC081300000000C2086CDF25326CFFFF046D0516CE26|
// {"media":"warm water","meter":"qwater","name":"YetAnoter","id":"33333333","status":"OK","total_m3":0.035,"due_date_m3":0,"due_date":"2021-12-31","due_17_date_m3":0,"due_17_date":"2022-05-31","error_date":"2128-03-31","meter_datetime":"2022-06-14 22:05","timestamp":"1111-11-11T11:11:11Z"}
// |YetAnoter;33333333;0.035;0;2021-12-31;OK;1111-11-11 11:11.11

// Test: QWater-7-18 qwater 12230094 NOKEY
// telegram=|394493449400231218077ad30000200c13536712004c1307920500426cBf2ccc081373621200c2086cde2B02BB560000326cffff046d3714c32c|
// {"media":"water","meter":"qwater","name":"QWater-7-18","id":"12230094","status":"OK","total_m3":126.753,"due_date_m3":59.207,"due_date":"2021-12-31","due_17_date_m3":126.273,"due_17_date":"2022-11-30","error_date":"2128-03-31","volume_flow_m3h":0,"meter_datetime":"2022-12-03 20:55","timestamp":"1111-11-11T11:11:11Z"}
// |QWater-7-18;12230094;126.753;59.207;2021-12-31;OK;1111-11-11 11:11.11
