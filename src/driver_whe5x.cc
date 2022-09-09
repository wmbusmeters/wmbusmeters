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
        di.setName("whe5x");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::S1);
        di.addDetection(MANUFACTURER_LSE, 0x08,  0x34);

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


// Test: HCA whe5x 91835132 NOKEY
// telegram=|244465323251839134087a4f0000000b6e0403004b6e660300426c9e29326cffff046d1416b921dd2f|
// {"media":"heat cost allocation","meter":"whe5x","name":"HCA","id":"91835132","status":"OK","current_consumption_hca":304,"set_date":"2020-09-30","consumption_at_set_date_hca":366,"set_date_1":"2020-09-30","consumption_at_set_date_1_hca":366,"error_date":"2127-15-31","device_date_time":"2021-01-25 22:20","timestamp":"1111-11-11T11:11:11Z"}
// |HCA;91835132;304;2020-09-30;366;1111-11-11 11:11.11
