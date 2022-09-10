/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("weh_07");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_WEH, 0x07,  0xfe);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );
    }
}

// Test: Vatten weh_07 86868686 NOKEY
// Comment: Techem radio convert + Wehrle water meter combo.
// telegram=|494468509494949495377286868686A85CFE07A90030052F2F_0413100000000F52FCF6A52A90A8D83CA8F7FEAE86990502323D0C70EFF49833C7C1696F75BCABC1E52E6305308D0F31FB|
// {"media":"water","meter":"weh_07","name":"Vatten","id":"86868686","total_m3":0.016,"timestamp":"1111-11-11T11:11:11Z"}
// |Vatten;86868686;0.016;1111-11-11 11:11.11
