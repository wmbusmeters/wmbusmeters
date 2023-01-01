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
        di.setName("hydrodigit");
        di.setDefaultFields("name,id,total_m3,meter_datetime,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMT,  0x06,  0x13);
        di.addDetection(MANUFACTURER_BMT,  0x07,  0x13);
        di.addDetection(MANUFACTURER_BMT,  0x07,  0x15);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "meter",
            "Meter timestamp for measurement.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime),
            Unit::DateTimeLT
            );
    }
}

// Test: HydrodigitWater hydrodigit 86868686 NOKEY
// telegram=|4E44B4098686868613077AF0004005_2F2F0C1366380000046D27287E2A0F150E00000000C10000D10000E60000FD00000C01002F0100410100540100680100890000A00000B30000002F2F2F2F2F2F|
// {"media":"water","meter":"hydrodigit","name":"HydrodigitWater","id":"86868686","total_m3":3.866,"meter_datetime":"2019-10-30 08:39","timestamp":"1111-11-11T11:11:11Z"}
// |HydrodigitWater;86868686;3.866;2019-10-30 08:39;1111-11-11 11:11.11
